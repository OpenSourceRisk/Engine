/*
Copyright (C) 2018 Quaternion Risk Management Ltd
All rights reserved.

This file is part of ORE, a free-software/open-source library
for transparent pricing and risk analysis - http://opensourcerisk.org

ORE is free software: you can redistribute it and/or modify it
under the terms of the Modified BSD License.  You should have received a
copy of the license along with this program.
The license is also available online at <http://opensourcerisk.org>

This program is distributed on the basis that it will form a useful
contribution to risk analytics and model standardisation, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/marketdata/commodityvolcurve.hpp>

#include <algorithm>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, 
    const Loader& loader, const CurveConfigurations& curveConfigs) {

    try {
        boost::shared_ptr<CommodityVolatilityCurveConfig> config =
            curveConfigs.commodityVolatilityCurveConfig(spec.curveConfigID());

        // Do different things depending on the type of volatility configured
        switch (config->type()) {
        case CommodityVolatilityCurveConfig::Type::Constant:
            buildConstantVolatility(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Curve:
            buildVolatilityCurve(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Surface:
            buildVolatilitySurface(asof, *config, loader);
            break;
        default:
            QL_FAIL("Commodity volatility type not recognised");
        }

        // Enable/disable extrapolation as per config
        volatility_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("Commodity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Commodity volatility curve building failed: unknown error");
    }
}

void CommodityVolCurve::buildConstantVolatility(const Date& asof, CommodityVolatilityCurveConfig& config, const Loader& loader) {
    // Should have a single quote for constant volatility structure
    QL_REQUIRE(config.quotes().size() == 1, "Expected a single quote when the commodity volatility type is constant");
    string quoteId = config.quotes()[0];

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {
            
            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
            
            if (q->name() == quoteId) {
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << quoteId);
                quoteValue = q->quote()->value();
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << quoteId);

    // Build the constant volatility structure
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    Calendar calendar = parseCalendar(config.calendar());
    LOG("CommodityVolatilityCurve: Building BlackConstantVol term structure");
    volatility_ = boost::make_shared<BlackConstantVol>(asof, calendar, quoteValue, dayCounter);
}

void CommodityVolCurve::buildVolatilityCurve(const Date& asof, CommodityVolatilityCurveConfig& config, const Loader& loader) {

    // Loop over all market datums and find the quotes in the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)
    map<Date, Real> curveData;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            vector<string>::const_iterator it = find(config.quotes().begin(), config.quotes().end(), q->name());
            if (it != config.quotes().end()) {
                // The quote expiry is a string that may be a period or a date
                // Convert it to a date here
                Date aDate;
                Period aPeriod;
                bool isDate;
                parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                if (isDate) {
                    QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << *it << 
                        "' is for a past date (" << io::iso_date(aDate) << ")");
                } else {
                    aDate = calendar.adjust(asof + aPeriod);
                }
                
                // Add the quote to the curve data
                QL_REQUIRE(curveData.count(aDate) == 0, "Quote " << *it << 
                    " provides a duplicate quote for the date " << io::iso_date(aDate));
                curveData[aDate] = q->quote()->value();
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << curveData.size() << " quotes.");

    QL_REQUIRE(curveData.size() == config.quotes().size(),
        "Found " << curveData.size() << " quotes, but " << config.quotes().size() << " quotes given in config.");

    // Create the dates and volatility vector
    vector<Date> dates; dates.reserve(curveData.size());
    vector<Volatility> volatilities; volatilities.reserve(curveData.size());
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
    }

    // Build the volatility structure curve
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    LOG("CommodityVolatilityCurve: Building BlackVarianceCurve term structure");
    volatility_ = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter);
}

void CommodityVolCurve::buildVolatilitySurface(const Date& asof, CommodityVolatilityCurveConfig& config, const Loader& loader) {

    // To hold dates x strike surface volatilities
    // dates will be sorted since map key
    map<Date, vector<Real>> surfaceData;

    // Assume the config has required strike strings to be sorted by value
    const vector<string>& configStrikes = config.strikes();

    // Loop over all market datums and find the quotes in the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)
    Size quotesAdded = 0;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            vector<string>::const_iterator it = find(config.quotes().begin(), config.quotes().end(), q->name());
            if (it != config.quotes().end()) {
                // The quote expiry is a string that may be a period or a date
                // Convert it to a date here
                Date aDate;
                Period aPeriod;
                bool isDate;
                parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                if (isDate) {
                    QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << *it <<
                        "' is for a past date (" << io::iso_date(aDate) << ")");
                } else {
                    aDate = calendar.adjust(asof + aPeriod);
                }

                // Position of quote in vector
                Size pos = distance(configStrikes.begin(), find(configStrikes.begin(), configStrikes.end(), q->strike()));
                QL_REQUIRE(pos < configStrikes.size(), "The quote '" << *it << "' is in the list of configured quotes "
                    "but does not match any of the configured strikes");
                
                // Add quote to surface
                if (surfaceData.count(aDate) == 0) surfaceData[aDate] = vector<Real>(configStrikes.size(), Null<Real>());
                QL_REQUIRE(surfaceData[aDate][pos] == Null<Real>(), "Quote " << *it << " provides a duplicate quote for the date " 
                    << io::iso_date(aDate) << " and the strike " << configStrikes[pos]);
                surfaceData[aDate][pos] = q->quote()->value();
                quotesAdded++;
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << quotesAdded << " quotes.");

    QL_REQUIRE(config.quotes().size() == quotesAdded,
        "Found " << quotesAdded << " quotes, but " << config.quotes().size() << " quotes required by config.");

    // Create the volatility matrix (rows are strikes, columns are dates)
    Matrix volatilities(configStrikes.size(), surfaceData.size());
    vector<Date> dates; dates.reserve(surfaceData.size());
    vector<Real> strikes; strikes.reserve(configStrikes.size());
    Size counter = 0;
    for (const auto& datum : surfaceData) {
        dates.push_back(datum.first);
        for (Size i = 0; i < configStrikes.size(); i++) {
            if (counter == 0) strikes.push_back(parseReal(configStrikes[i]));
            volatilities[i][counter] = datum.second[i];
        }
        counter++;
    }

    // Build the volatility surface
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    BlackVarianceSurface::Extrapolation lowerStrikeExtrap = config.lowerStrikeConstantExtrapolation() ? 
        BlackVarianceSurface::Extrapolation::ConstantExtrapolation : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
    BlackVarianceSurface::Extrapolation upperStrikeExtrap = config.upperStrikeConstantExtrapolation() ?
        BlackVarianceSurface::Extrapolation::ConstantExtrapolation : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;

    LOG("CommodityVolatilityCurve: Building BlackVarianceSurface term structure");
    volatility_ = boost::make_shared<BlackVarianceSurface>(asof, calendar, dates, strikes, 
        volatilities, dayCounter, lowerStrikeExtrap, upperStrikeExtrap);
}

}
}
