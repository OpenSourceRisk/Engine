/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/ratedigitaloption.hpp
    \brief Engine builder for European digital (cash-or-nothing) option on interest rates
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/ratedigitalcallspreadengine.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

// ---------------------------------------------------------------------------
//  RateDigitalOptionEngineBuilder
// ---------------------------------------------------------------------------
//! Engine Builder for European rate digital options (call-spread replication)
/*! Retrieves the optionlet (capFloor) vol surface, forward rate and discount
    factor, then constructs a RateDigitalCallSpreadEngine.

    Engine parameters:
    - CallSpreadEps (optional, default 1e-4): width of the call spread in rate
      terms.  Smaller values approach the analytical digital; larger values
      smooth the replication.

    \ingroup builders
*/
class RateDigitalOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::string, const std::string&, QuantLib::Real,
                                                    const QuantLib::Date&, const QuantLib::Date&> {
public:
    RateDigitalOptionEngineBuilder()
        : CachingEngineBuilder("Black", "CallSpreadEngine", {"RateDigitalOption"}) {}

protected:
    std::string keyImpl(const std::string& index, QuantLib::Real strike, const QuantLib::Date& fixingDate,
                        const QuantLib::Date& paymentDate) override {
        return index + "/" + std::to_string(strike) + "/" + ore::data::to_string(fixingDate) + "/" +
               ore::data::to_string(paymentDate);
    }

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const std::string& index, QuantLib::Real strike, const QuantLib::Date& fixingDate,
               const QuantLib::Date& paymentDate) override {

        auto config = configuration(MarketContext::pricing);

        Handle<IborIndex> hIndex = market_->iborIndex(index, config);
        auto isFlatVol = parseBool(engineParameter("withFlatVol", {}, false, "false"));
        QL_REQUIRE(!hIndex.empty(), "RateDigitalOptionEngineBuilder: could not find index " << index);
        ext::shared_ptr<IborIndex> iborIndex = hIndex.currentLink();

        std::string ccy = iborIndex->currency().code();
        Handle<YieldTermStructure> discountCurve = market_->discountCurve(ccy, config);

        Real forward = iborIndex->forecastFixing(fixingDate);

        DiscountFactor dfPayment = discountCurve->discount(paymentDate);

        Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(index, config);
        if (isFlatVol) {
            // Replace market vol with a flat zero surface → digital valued at intrinsic
            ovs = Handle<OptionletVolatilityStructure>(
                ext::make_shared<ConstantOptionletVolatility>(
                    ovs->referenceDate(), ovs->calendar(), ovs->businessDayConvention(),
                    0.0, ovs->dayCounter(), ovs->volatilityType(), ovs->displacement()));
        }
        QL_REQUIRE(!ovs.empty(), "RateDigitalOptionEngineBuilder: no capFloor vol for " << index);

        // Call-spread eps (configurable, default 1e-4 = 1 bp)
        Real eps = 1.0e-4;
        if (engineParameter("CallSpreadEps", {}, false, "") != "")
            eps = parseReal(engineParameter("CallSpreadEps"));

        return ext::make_shared<QuantExt::RateDigitalCallSpreadEngine>(
            forward, dfPayment, ovs, fixingDate, eps);
    }
};

} // namespace data
} // namespace ore
