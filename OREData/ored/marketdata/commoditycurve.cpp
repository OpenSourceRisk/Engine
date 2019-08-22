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

#include <ored/marketdata/commoditycurve.hpp>
#include <ored/utilities/log.hpp>
#include <qle/termstructures/crosscurrencypricetermstructure.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/date.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <regex>

using QuantExt::InterpolatedPriceCurve;
using QuantExt::CrossCurrencyPriceTermStructure;
using QuantExt::PriceTermStructure;
using std::map;
using std::regex;
using std::string;

namespace ore {
namespace data {

CommodityCurve::CommodityCurve(const Date& asof,
    const CommodityCurveSpec& spec,
    const Loader& loader,
    const CurveConfigurations& curveConfigs,
    const Conventions& conventions,
    const FXTriangulation& fxSpots,
    const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves)
    : spec_(spec), commoditySpot_(Null<Real>()), onValue_(Null<Real>()), tnValue_(Null<Real>()) {

    try {

        boost::shared_ptr<CommodityCurveConfig> config = curveConfigs.commodityCurveConfig(spec_.curveConfigID());

        if (config->type() == CommodityCurveConfig::Type::Direct) {
            
            // Populate the raw price curve data
            map<Date, Real> data;
            populateData(data, asof, config, loader, conventions);

            // Create the commodity price curve
            buildCurve(asof, data, config);

        } else {

            // We have a cross currency type commodity curve configuration
            boost::shared_ptr<CommodityCurveConfig> baseConfig = 
                curveConfigs.commodityCurveConfig(config->basePriceCurveId());
            
            buildCrossCurrencyPriceCurve(asof, config, baseConfig, fxSpots, yieldCurves, commodityCurves);

        }

        // Apply extrapolation from the curve configuration
        commodityPriceCurve_->enableExtrapolation(config->extrapolation());

    } catch (std::exception& e) {
        QL_FAIL("commodity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("commodity curve building failed: unknown error");
    }
}

void CommodityCurve::populateData(map<Date, Real>& data, const Date& asof, 
    const boost::shared_ptr<CommodityCurveConfig>& config, const Loader& loader, const Conventions& conventions) {

    // Some default conventions for building the commodity curve
    Period spotTenor = 2 * Days;
    Real pointsFactor = 1.0;
    Calendar cal = parseCalendar(config->currency());
    bool spotRelative = true;
    BusinessDayConvention bdc = Following;
    bool outright = true;

    // Overwrite the default conventions if the commodity curve config provides explicit conventions
    if (!config->conventionsId().empty()) {
        QL_REQUIRE(conventions.has(config->conventionsId()), "Commodity conventions " << config->conventionsId()
            << " requested by commodity config " << config->curveID() << " not found");
        boost::shared_ptr<CommodityConvention> convention = boost::dynamic_pointer_cast<CommodityConvention>(
            conventions.get(config->conventionsId()));
        QL_REQUIRE(convention, "Convention " << config->conventionsId()
            << " not of expected type CommodityConvention");

        spotTenor = convention->spotDays() * Days;
        pointsFactor = convention->pointsFactor();
        if (convention->advanceCalendar() != NullCalendar())
            cal = convention->advanceCalendar();
        spotRelative = convention->spotRelative();
        bdc = convention->bdc();
        outright = convention->outright();
    }

    // Check for regex string in config
    bool foundRegex = false;
    for (Size i = 0; i < config->fwdQuotes().size(); i++) {
        foundRegex |= config->fwdQuotes()[i].find("*") != string::npos;
    }

    // If we find a regex in forward quotes, check there is only one and initialise the regex
    regex reg;
    if (foundRegex) {
        QL_REQUIRE(config->fwdQuotes().size() == 1, "Regular expression specified in commodity curve " <<
            config->curveID() << " but more quotes also specified.");
        LOG("Regular expression specified for forward quotes for commodity curve " << config->curveID());
        string regexstr = config->fwdQuotes()[0];
        boost::replace_all(regexstr, "*", ".*");
        reg = regex(regexstr);
    }

    // Commodity spot quote if provided by the configuration
    Date spotDate = cal.advance(asof, spotTenor);
    if (!config->commoditySpotQuoteId().empty()) {
        commoditySpot_ = loader.get(config->commoditySpotQuoteId(), asof)->quote()->value();
        data[spotDate] = commoditySpot_;
    } else {
        QL_REQUIRE(outright, "If the commodity forward quotes are not outright," <<
            " a commodity spot quote needs to be configured");
    }

    // Add the forward quotes to the curve data
    for (auto& md : loader.loadQuotes(asof)) {

        // Only looking for quotes on asof date, with quote type PRICE and instrument type commodity forward
        if (md->asofDate() == asof && md->quoteType() == MarketDatum::QuoteType::PRICE &&
            md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_FWD) {

            boost::shared_ptr<CommodityForwardQuote> q = boost::dynamic_pointer_cast<CommodityForwardQuote>(md);

            // Check if the quote is requested by the config and if it isn't continue to the next quote
            if (!foundRegex) {
                vector<string>::const_iterator it = find(
                    config->fwdQuotes().begin(), config->fwdQuotes().end(), q->name());
                if (it == config->fwdQuotes().end())
                    continue;
            } else {
                if (!regex_match(q->name(), reg))
                    continue;
            }

            // The quote is requested by the config so process it
            // We add ON and TN quotes after this loop if they are given and not outright quotes
            TLOG("Commodity Forward Price found for quote: " << q->name());
            Date expiry;
            Real value = q->quote()->value();
            if (!q->tenorBased()) {
                expiry = q->expiryDate();
                add(asof, expiry, value, data, outright, pointsFactor);
            } else {
                if (q->startTenor() == boost::none) {
                    expiry = cal.advance(spotRelative ? spotDate : asof, q->tenor(), bdc);
                    add(asof, expiry, value, data, outright, pointsFactor);
                } else {
                    if (*q->startTenor() == 0 * Days && q->tenor() == 1 * Days) {
                        onValue_ = q->quote()->value();
                        if (outright)
                            add(asof, asof, value, data, outright);
                    } else if (*q->startTenor() == 1 * Days && q->tenor() == 1 * Days) {
                        tnValue_ = q->quote()->value();
                        if (outright) {
                            expiry = cal.advance(asof, 1 * Days, bdc);
                            add(asof, expiry, value, data, outright);
                        }
                    } else {
                        expiry = cal.advance(cal.advance(asof, *q->startTenor(), bdc), q->tenor(), bdc);
                        add(asof, expiry, value, data, outright, pointsFactor);
                    }
                }
            }
        }
    }

    // Deal with ON and TN if quotes are not outright quotes
    if (spotTenor == 2 * Days && tnValue_ != Null<Real>() && !outright) {
        add(asof, cal.advance(asof, 1 * Days, bdc), -tnValue_, data, outright, pointsFactor);
        if (onValue_ != Null<Real>()) {
            add(asof, asof, -onValue_ - tnValue_, data, outright, pointsFactor);
        }
    }

    // Some logging and checks
    LOG("Read " << data.size() << " quotes for commodity curve " << config->curveID());
    if (!foundRegex) {
        QL_REQUIRE(data.size() == config->quotes().size(), "Found " << data.size() << " quotes, but "
            << config->quotes().size() << " quotes given in config " << config->curveID());
    } else {
        QL_REQUIRE(data.size() > 0, "Regular expression specified in commodity config " <<
            config->curveID() << " but no quotes read");
    }
}

void CommodityCurve::add(const Date& asof, const Date& expiry, Real value, 
    map<Date, Real>& data, bool outright, Real pointsFactor) {
    
    if (expiry < asof)
        return;

    QL_REQUIRE(data.find(expiry) == data.end(), "Duplicate quote for expiry " << io::iso_date(expiry) << " found.");

    if (!outright) {
        QL_REQUIRE(commoditySpot_ != Null<Real>(), "Can't use forward points without a commodity spot value");
        value = commoditySpot_ + value / pointsFactor;
    }

    data[expiry] = value;
}

void CommodityCurve::buildCurve(const Date& asof, 
    const map<Date, Real>& data, const boost::shared_ptr<CommodityCurveConfig>& config) {

    vector<Date> curveDates;
    curveDates.reserve(data.size());
    vector<Real> curvePrices;
    curvePrices.reserve(data.size());
    for (auto const& datum : data) {
        curveDates.push_back(datum.first);
        curvePrices.push_back(datum.second);
    }

    DayCounter dc = config->dayCountId() == "" ? Actual365Fixed() : parseDayCounter(config->dayCountId());
    string interpolationMethod = config->interpolationMethod() == "" ? "Linear" : config->interpolationMethod();
    boost::algorithm::to_upper(interpolationMethod);

    if (interpolationMethod == "LINEAR") {
        commodityPriceCurve_ = boost::make_shared<InterpolatedPriceCurve<Linear>>(asof, curveDates, curvePrices, dc);
    } else if (interpolationMethod == "LOGLINEAR") {
        commodityPriceCurve_ = boost::make_shared<InterpolatedPriceCurve<LogLinear>>(asof, curveDates, curvePrices, dc);
    } else {
        QL_FAIL("The interpolation method, " << interpolationMethod <<
            ", is not supported. Needs to be Linear or LogLinear");
    }

}

void CommodityCurve::buildCrossCurrencyPriceCurve(const Date& asof,
    const boost::shared_ptr<CommodityCurveConfig>& config,
    const boost::shared_ptr<CommodityCurveConfig>& baseConfig, const FXTriangulation& fxSpots,
    const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves) {

    // Look up the required base price curve in the commodityCurves map
    // We pass in the commodity curve ID only in the member basePriceCurveId of config e.g. PM:XAUUSD.
    // But, the map commodityCurves is keyed on the spec name e.g. Commodity/USD/PM:XAUUSD
    auto commIt = commodityCurves.find(CommodityCurveSpec(baseConfig->currency(), baseConfig->curveID()).name());
    QL_REQUIRE(commIt != commodityCurves.end(), "Could not find base commodity curve with id " << baseConfig->curveID()
        << " required in the building of commodity curve with id " << config->curveID());

    // Look up the two yield curves in the yieldCurves map
    auto baseYtsIt = yieldCurves.find(YieldCurveSpec(baseConfig->currency(), config->baseYieldCurveId()).name());
    QL_REQUIRE(baseYtsIt != yieldCurves.end(), "Could not find base yield curve with id " << 
        config->baseYieldCurveId() << " and currency " << baseConfig->currency() << 
        " required in the building of commodity curve with id " << config->curveID());

    auto ytsIt = yieldCurves.find(YieldCurveSpec(config->currency(), config->yieldCurveId()).name());
    QL_REQUIRE(ytsIt != yieldCurves.end(), "Could not find yield curve with id " <<
        config->yieldCurveId() << " and currency " << config->currency() <<
        " required in the building of commodity curve with id " << config->curveID());

    // Get the FX spot rate, number of units of this currency per unit of base currency
    Handle<Quote> fxSpot = fxSpots.getQuote(baseConfig->currency() + config->currency());

    // Populate the commodityPriceCurve_ member
    commodityPriceCurve_ = boost::make_shared<CrossCurrencyPriceTermStructure>(asof, 
        QuantLib::Handle<PriceTermStructure>(commIt->second->commodityPriceCurve()), 
        fxSpot, baseYtsIt->second->handle(), ytsIt->second->handle());

}

}
}
