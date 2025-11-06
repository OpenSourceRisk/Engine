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
#include <ored/utilities/log.hpp>

#include <qle/cashflows/interpolatediborcouponpricer.hpp>

#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

namespace {
template <class T>
boost::shared_ptr<FloatingRateCouponPricer>
getEngine(const std::string& index, const boost::shared_ptr<Market>& market, const std::string& configuration,
          const bool zeroVolatility, const std::string& timingAdjustmentStr, const Real correlationValue,
          std::map<string, boost::shared_ptr<FloatingRateCouponPricer>>& engines) {

    std::string ccyCode = parseIborIndex(index)->currency().code();
    Handle<YieldTermStructure> yts = market->discountCurve(ccyCode, configuration);
    Handle<OptionletVolatilityStructure> ovs;
    if (zeroVolatility) {
        ovs = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    } else {
        ovs = market->capFloorVol(index, configuration);
    }
    typename T::TimingAdjustment timingAdjustment = T::Black76;
    QuantLib::ext::shared_ptr<SimpleQuote> correlation = QuantLib::ext::make_shared<SimpleQuote>(1.0);
    // for backwards compatibility we do not require the additional timing adjustment fields
    if (timingAdjustmentStr == "Black76")
        timingAdjustment = T::Black76;
    else if (timingAdjustmentStr == "BivariateLognormal")
        timingAdjustment = T::BivariateLognormal;
    else {
        QL_FAIL("timing adjustment parameter (" << timingAdjustmentStr << ") not recognised.");
    }
    correlation->setValue(correlationValue);
    return QuantLib::ext::make_shared<T>(ovs, timingAdjustment, Handle<Quote>(correlation));
}
} // namespace

boost::shared_ptr<FloatingRateCouponPricer> CapFlooredIborLegEngineBuilder::engineImpl(const std::string& index) {
    return getEngine<BlackIborCouponPricer>(index, market_, configuration(MarketContext::pricing),
                                            parseBool(engineParameter("ZeroVolatility", {}, false, "false")),
                                            engineParameter("TimingAdjustment", {}, false, "Black76"),
                                            parseReal(engineParameter("Correlation", {}, false, "0.0")),
                                            engines_);
}

boost::shared_ptr<FloatingRateCouponPricer>
CapFlooredInterpolatedIborLegEngineBuilder::engineImpl(const std::string& index) {
    return getEngine<QuantExt::BlackInterpolatedIborCouponPricer>(
        index, market_, configuration(MarketContext::pricing),
        parseBool(engineParameter("ZeroVolatility", {}, false, "false")),
        engineParameter("TimingAdjustment", {}, false, "Black76"),
        parseReal(engineParameter("Correlation", {}, false, "0.0")), engines_);
}

} // namespace data
} // namespace ore
