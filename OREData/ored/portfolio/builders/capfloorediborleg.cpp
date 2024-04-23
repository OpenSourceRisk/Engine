/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ored/utilities/log.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> CapFlooredIborLegEngineBuilder::engineImpl(const std::string& index) {

    std::string ccyCode = parseIborIndex(index)->currency().code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    Handle<OptionletVolatilityStructure> ovs;
    if (parseBool(engineParameter("ZeroVolatility", {}, false, "false"))) {
        ovs = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    } else {
        ovs = market_->capFloorVol(index, configuration(MarketContext::pricing));
    }
    BlackIborCouponPricer::TimingAdjustment timingAdjustment = BlackIborCouponPricer::Black76;
    QuantLib::ext::shared_ptr<SimpleQuote> correlation = QuantLib::ext::make_shared<SimpleQuote>(1.0);
    // for backwards compatibility we do not require the additional timing adjustment fields
    if (engineParameters_.find("TimingAdjustment") != engineParameters_.end()) {
        string adjStr = engineParameter("TimingAdjustment");
        if (adjStr == "Black76")
            timingAdjustment = BlackIborCouponPricer::Black76;
        else if (adjStr == "BivariateLognormal")
            timingAdjustment = BlackIborCouponPricer::BivariateLognormal;
        else {
            QL_FAIL("timing adjustment parameter (" << adjStr << ") not recognised.");
        }
        correlation->setValue(parseReal(engineParameter("Correlation")));
    }
    return QuantLib::ext::make_shared<BlackIborCouponPricer>(ovs, timingAdjustment, Handle<Quote>(correlation));
}

} // namespace data
} // namespace ore
