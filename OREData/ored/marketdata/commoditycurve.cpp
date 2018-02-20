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

#include <algorithm>

#include <boost/algorithm/string.hpp>

#include <ql/time/date.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>

#include <ored/marketdata/commoditycurve.hpp>
#include <ored/utilities/log.hpp>

using QuantExt::InterpolatedPriceCurve;

namespace ore {
namespace data {

CommodityCurve::CommodityCurve(const Date& asof, const CommodityCurveSpec& spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, const Conventions& conventions) : spec_(spec) {

    try {

        boost::shared_ptr<CommodityCurveConfig> config = curveConfigs.commodityCurveConfig(spec_.curveConfigId());

        // Loop over all market data looking for the quotes
        map<Date, Real> curveData;

        for (auto& md : loader.loadQuotes(asof)) {
            // Only looking for quotes on asof date with quote type PRICE
            if (md->asofDate() == asof && md->quoteType() == MarketDatum::QuoteType::PRICE) {

                // Looking for Commodity spot price from the commodity curve config
                if (md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_SPOT) {

                    boost::shared_ptr<CommoditySpotQuote> q = boost::dynamic_pointer_cast<CommoditySpotQuote>(md);

                    if (q->name() == config->commoditySpotQuoteId()) {
                        QL_REQUIRE(curveData.find(asof) == curveData.end(), "duplicate commodity spot quote " << q->name() << " found.");
                        curveData[asof] = q->quote()->value();
                    }
                }

                // Looking for Commodity forward prices from the commodity curve config
                if (md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_FWD) {

                    boost::shared_ptr<CommodityForwardQuote> q = boost::dynamic_pointer_cast<CommodityForwardQuote>(md);

                    vector<string>::const_iterator it = find(config->quotes().begin(), config->quotes().end(), q->name());
                    if (it != config->quotes().end()) {
                        QL_REQUIRE(curveData.find(q->expiryDate()) == curveData.end(), "duplicate market datum found for " << *it);
                        curveData[q->expiryDate()] = q->quote()->value();
                    }
                }
            }
        }

        LOG("CommodityCurve: read " << curveData.size() << " quotes.");

        QL_REQUIRE(curveData.size() == config->quotes().size(),
            "Found " << curveData.size() << " quotes, but " << config->quotes().size() << " quotes given in config.");

        // curveData is already ordered by date. Make sure asof is first entry.
        QL_REQUIRE(curveData.begin()->first == asof, "All quotes must be after the asof date, " << asof << ".");

        // Create the commodity price curve
        vector<Date> curveDates;
        curveDates.reserve(curveData.size());
        vector<Real> curvePrices;
        curvePrices.reserve(curveData.size());
        for (auto const& datum : curveData) {
            curveDates.push_back(datum.first);
            curvePrices.push_back(datum.second);
        }

        DayCounter dc = config->dayCountId() == "" ? Actual365Fixed() : parseDayCounter(config->dayCountId());
        string interpolationMethod = config->interpolationMethod() == "" ? "Linear" : config->interpolationMethod();
        boost::algorithm::to_upper(interpolationMethod);

        // Would like a better way of switching between interpolation methods
        if (interpolationMethod == "LINEAR") {
            commodityPriceCurve_ = boost::make_shared<InterpolatedPriceCurve<Linear>>(curveDates, curvePrices, dc);
        } else if (interpolationMethod == "LOGLINEAR") {
            commodityPriceCurve_ = boost::make_shared<InterpolatedPriceCurve<LogLinear>>(curveDates, curvePrices, dc);
        } else {
            QL_FAIL("The interpolation method, " << interpolationMethod << ", is not supported. Needs to be Linear or LogLinear");
        }

        // Apply extrapolation from the curve configuration
        commodityPriceCurve_->enableExtrapolation(config->extrapolation());

    } catch (std::exception& e) {
        QL_FAIL("commodity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("commodity curve building failed: unknown error");
    }
}

}
}
