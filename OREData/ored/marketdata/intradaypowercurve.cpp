/*
 Copyright (C) 2026 AcadiaSoft Inc
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

#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/intradaypowercurve.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/wildcard.hpp>

#include <map>
#include <sstream>

using QuantExt::IntradayPowerIndex;
using QuantExt::IntradayPowerPriceTermStructure;
using QuantExt::IntradayShapeTermstructure;
using QuantExt::PriceTermStructure;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Real;
using std::map;
using std::string;

namespace ore {
namespace data {

IntradayPowerCurve::IntradayPowerCurve(const Date& asof, const IntradayPowerCurveSpec& spec, const Loader& loader,
                                       const CurveConfigurations& curveConfigs,
                                       const map<string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves)
    : spec_(spec) {

    try {
        QuantLib::ext::shared_ptr<IntradayPowerCurveConfig> config =
            curveConfigs.intradayPowerCurveConfig(spec_.curveConfigID());

        // Look up the underlying daily average commodity price curve from the curves this curve depends on.
        // The configuration holds the full curve spec, e.g. "Commodity/USD/PJM_WH_RT_AVG".
        auto ccSpec = parseCurveSpec(config->dailyAveragePriceCurve());
        DLOG("IntradayPowerCurve: looking for daily average price curve with spec " << ccSpec->name() << ".");
        auto it = commodityCurves.find(ccSpec->name());
        QL_REQUIRE(it != commodityCurves.end(),
                   "IntradayPowerCurve: can't find daily average price curve with id "
                       << config->dailyAveragePriceCurve());
        Handle<PriceTermStructure> underlying(it->second->commodityPriceCurve());

        // Build the intraday shape term structure from the shape factor quotes.
        auto shape = buildShape(asof, config->shapeQuoteName(), loader);

        // Build the intraday power price term structure wrapping the daily average curve with the shape.
        curve_ = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shape);

        // Build the intraday power index with the same id as the curve.
        Handle<IntradayPowerPriceTermStructure> ts(curve_);
        index_ = parseIntradayPowerIndex(config->indexName().empty() ? spec_.curveConfigID() : config->indexName(), false, ts);

    } catch (std::exception& e) {
        QL_FAIL("intraday power curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("intraday power curve building failed: unknown error");
    }
}

QuantLib::ext::shared_ptr<IntradayShapeTermstructure>
IntradayPowerCurve::buildShape(const Date& asof, const string& shapeQuoteName, const Loader& loader) const {

    // Always load all shape factor quotes for the configured shape quote name using a wildcard:
    // SHAPE_PROFILE/SHAPE_FACTOR/<shapeQuoteName>/*
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::SHAPE_PROFILE << "/" << MarketDatum::QuoteType::SHAPE_FACTOR << "/"
       << shapeQuoteName << "/*";
    Wildcard w(ss.str());
    auto data = loader.get(w, asof);

    // Group the shape factors per delivery date and intraday start time.
    map<Date, map<int, Real>> shapeFactors;
    map<Date, map<int, Real>>
        shapeFactorsDST; // DST shape factors are not supported through this quote format yet, keep empty.
    for (const auto& md : data) {
        auto q = QuantLib::ext::dynamic_pointer_cast<IntradayPowerCurveQuote>(md);
        if (!q)
            continue;
        auto start = static_cast<int>(q->startTimeInSec()) * static_cast<int>(q->timeUnit());
        
        QL_REQUIRE(start >= 0 && start < 86400,
                   "IntradayPowerCurve: start time " << start << " is out of range for quote " << q->name());
        if (!q->isDST())
            shapeFactors[q->deliveryDate()][start] = q->quote()->value();
        else {
            QL_REQUIRE(
                start >= 2 * 3600 && start < 3 * 3600,
                "IntradayPowerCurve: DST shape factors are only supported for the hour between 2 and 3 am, but quote "
                    << q->name() << " has start time " << start);
            shapeFactorsDST[q->deliveryDate()][start] = q->quote()->value();
        }
        TLOG("IntradayPowerCurve: loaded shape factor quote " << q->name() << " with delivery date " << q->deliveryDate()
             << ", start time " << start << " and value " << q->quote()->value() << (q->isDST() ? " (DST)" : ""));
    }

    // Perform some basic checks on the shape factors
    QL_REQUIRE(!shapeFactors.empty(), "IntradayPowerCurve: no SHAPE_PROFILE/SHAPE_FACTOR/"
                                          << shapeQuoteName << "/* quotes found for asof "
                                          << QuantLib::io::iso_date(asof));
    for (const auto& [d, factors] : shapeFactorsDST) {
        QL_REQUIRE(factors.empty() || factors.begin()->first == 2 * 3600,
                   "IntradayPowerCurve: DST shape factors need to start with 2am for date "
                       << d << " but quote has start time " << factors.begin()->first);
    }

    return QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeFactors, shapeFactorsDST);
}

} // namespace data
} // namespace ore
