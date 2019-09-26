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
#include <ql/time/calendars/weekendsonly.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

#include <regex>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                                     const CurveConfigurations& curveConfigs) {

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

void CommodityVolCurve::buildConstantVolatility(const Date& asof, CommodityVolatilityCurveConfig& config,
                                                const Loader& loader) {
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

void CommodityVolCurve::buildVolatilityCurve(const Date& asof, CommodityVolatilityCurveConfig& config,
                                             const Loader& loader) {

    // Loop over all market datums and find the quotes *in* the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)

    QL_REQUIRE(config.quotes().size() > 0, "No quotes specified in config " << config.curveID());

    bool foundRegex = false;
    regex reg1;

    // check for regex string in config
    for (Size i = 0; i < config.quotes().size(); i++) {
        foundRegex |= config.quotes()[i].find("*") != string::npos;
        if (foundRegex) {
            break;
        }
    }

    if (foundRegex) {
        QL_REQUIRE(config.quotes().size() == 1,
                   "Wild card specified in config " << config.curveID() << " but more quotes also specified.");

        // build regex string
        string regexstr = config.quotes()[0];
        boost::replace_all(regexstr, "*", ".*");
        reg1 = regex(regexstr);
    }

    map<Date, Real> curveData;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            // relevant quote?
            bool addQuote = false;
            vector<string>::const_iterator it;  // if not wild-card -> find quote.
            if (foundRegex) {
                if (regex_match(q->name(), reg1)) {
                    // quote only valid of expiry date is > asof
                    Date eDate;
                    Period ePeriod;
                    bool isDate;
                    parseDateOrPeriod(q->expiry(), eDate, ePeriod, isDate);
                    if (!isDate){
                        eDate = calendar.adjust(asof + ePeriod);
                    }
                    if (eDate > asof) {
                        addQuote = true;
                    }
                }
            } else {
                it = find(config.quotes().begin(), config.quotes().end(), q->name()); // find in config
                if (it != config.quotes().end()) {
                    addQuote = true;
                }
            }

            // add quote if relevant
            if (addQuote) {
                // Convert expiry to a date if necessary
                Date aDate;
                Period aPeriod;
                bool isDate;
                parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                if (isDate) {
                    QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q->name() << "' has expiry in the past ("
                                                                            << io::iso_date(aDate) << ")");
                } else {
                    aDate = calendar.adjust(asof + aPeriod);
                }

                // Add the quote to the curve data
                QL_REQUIRE(curveData.count(aDate) == 0,
                           "Quote " << *it << " provides a duplicate quote for the date " << io::iso_date(aDate));
                curveData[aDate] = q->quote()->value();
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << curveData.size() << " quotes.");

    if (foundRegex) {
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching " << config.quotes()[0]);
    } else {
        QL_REQUIRE(curveData.size() == config.quotes().size(), "Found " << curveData.size() << " quotes, but "
                                                                        << config.quotes().size()
                                                                        << " quotes given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
    }

    // Build the volatility structure curve
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    LOG("CommodityVolatilityCurve: Building BlackVarianceCurve term structure");
    volatility_ = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter);
}

void CommodityVolCurve::buildVolatilitySurface(const Date& asof, CommodityVolatilityCurveConfig& config,
                                               const Loader& loader) {

    map<Date, vector<Real>> surfaceData; // implicit order is assumed here for known stirkes and expiries from config.
    vector<Real> strikes;       // wild card case
    vector<Date> expiries;      // wild card case
    vector<Volatility> vols;    // wild card case

    // Assume the config has required strike strings to be sorted by value
    const vector<string>& configStrikes = config.strikes();

    // wild cards?
    vector<string>::iterator exprWcIt = find(config.expiries().begin(), config.expiries().end(), "*");
    vector<string>::iterator strkWcIt = find(config.strikes().begin(), config.strikes().end(), "*");
    bool expWc = exprWcIt != config.expiries().end();
    bool strkWc = strkWcIt != config.strikes().end();
    bool wildCard = expWc || strkWc;
    if (expWc) {
        QL_REQUIRE(config.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
    }
    if (strkWc) {
        QL_REQUIRE(config.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
    }

    // Loop over all market datums and find the quotes in the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)
    bool relevantQuote = false;
    bool strikeRelevant = strkWc;
    bool expiryRelevant = expWc;
    Size quotesAdded = 0;
    Calendar calendar = parseCalendar(config.calendar());
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {
            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
            relevantQuote = false;
            vector<string>::const_iterator it;

            // no wild cards
            if (!wildCard) {
                it = find(config.quotes().begin(), config.quotes().end(), q->name());
                relevantQuote = it != config.quotes().end();
                if (relevantQuote) {
                    // Convert expiry to date if necessary
                    Date aDate;
                    Period aPeriod;
                    bool isDate;
                    parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                    if (isDate) {
                        QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q->name() << "' has expiry in past ("
                                                                                << io::iso_date(aDate) << ")");
                    } else {
                        aDate = calendar.adjust(asof + aPeriod);
                    }

                    // Position of quote in vector
                    Size pos =
                        distance(configStrikes.begin(), find(configStrikes.begin(), configStrikes.end(), q->strike()));
                    QL_REQUIRE(pos < configStrikes.size(), "The quote '"
                                                               << q->name()
                                                               << "' is in the list of configured quotes "
                                                                  "but does not match any of the configured strikes");

                    // Add quote to surface
                    if (surfaceData.count(aDate) == 0)
                        surfaceData[aDate] = vector<Real>(configStrikes.size(), Null<Real>());
                    QL_REQUIRE(surfaceData[aDate][pos] == Null<Real>(),
                               "Quote " << q->name() << " provides a duplicate quote for the date "
                                        << io::iso_date(aDate) << " and the strike " << configStrikes[pos]);
                    surfaceData[aDate][pos] = q->quote()->value();
                    quotesAdded++;
                }
                // some wild card
            } else {
                if (!expWc) {
                    // strike only wild card
                    it = find(config.expiries().begin(), config.expiries().end(), q->expiry());
                    expiryRelevant = it != config.expiries().end();
                } else {
                    expiryRelevant = true;
                }
                if (!strkWc) {
                    // expiry only wild card
                    it = find(config.strikes().begin(), config.strikes().end(), q->strike());
                    strikeRelevant = it != config.strikes().end();
                } else {
                    // for now we ignore ATMF quotes
                    strikeRelevant = q->strike() != "ATMF";
                }
                relevantQuote = strikeRelevant && expiryRelevant;

                if (relevantQuote) {
                    //expiry (may be tenor or date)
                    Date eDate;
                    Period ePeriod;
                    bool isDate;
                    parseDateOrPeriod(q->expiry(), eDate, ePeriod,isDate);
                    if (!isDate) {
                        eDate = calendar.adjust(asof + ePeriod);
                    }
                    // strike + vol
                    strikes.push_back(parseReal(q->strike()));
                    vols.push_back(q->quote()->value());
                    expiries.push_back(eDate);
                    quotesAdded++;
                }
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << quotesAdded << " quotes.");

    // check loaded quotes
    if (!wildCard) {
        QL_REQUIRE(config.quotes().size() == quotesAdded, "Found " << quotesAdded << " quotes, but "
                                                                   << config.quotes().size()
                                                                   << " quotes required by config.");
    } else {
        QL_REQUIRE(quotesAdded > 0, "No quotes loaded for" << config.curveID());
        QL_REQUIRE(strikes.size() == vols.size() && vols.size() == expiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
    }

    // Build the volatility surface
    tuple<vector<Real>, vector<Date>> sAndE;
    Matrix volatilities;
    BlackVarianceSurface::Extrapolation lowerStrikeExtrap =
        config.lowerStrikeConstantExtrapolation()
            ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
            : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
    BlackVarianceSurface::Extrapolation upperStrikeExtrap =
        config.upperStrikeConstantExtrapolation()
            ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
            : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
    if (!wildCard) {
        // Vol matrix (strikes, dates), date & expiry vectors
        volatilities = Matrix(configStrikes.size(), surfaceData.size());
        get<1>(sAndE).reserve(surfaceData.size());
        get<0>(sAndE).reserve(configStrikes.size());
        Size counter = 0;
        for (const auto& datum : surfaceData) {
            get<1>(sAndE).push_back(datum.first);
            for (Size i = 0; i < configStrikes.size(); i++) {
                if (counter == 0)
                    get<0>(sAndE).push_back(parseReal(configStrikes[i]));
                volatilities[i][counter] = datum.second[i];
            }
            counter++;
        }
        LOG("CommodityVolatilityCurve: Building BlackVarianceSurface term structure");
        volatility_ =
            boost::make_shared<BlackVarianceSurface>(asof, calendar, get<1>(sAndE), get<0>(sAndE), volatilities,
                                                     dayCounter, lowerStrikeExtrap, upperStrikeExtrap);
    } else {
        bool lowerConstStrikeExtrap = lowerStrikeExtrap == BlackVarianceSurface::Extrapolation::ConstantExtrapolation;
        bool upperConstStrikeExtrap = upperStrikeExtrap == BlackVarianceSurface::Extrapolation::ConstantExtrapolation;
      
        volatility_ =
            boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar, expiries, strikes, vols,
                                                     dayCounter, lowerConstStrikeExtrap, upperConstStrikeExtrap); // Flat extraoplate strikes always good
    }
}

} // namespace data
} // namespace ore
