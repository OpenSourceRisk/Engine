/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file durationadjustedcms.hpp
    \brief coupon pricer builder for duration adjuted cms coupons
    \ingroup builders
*/

#include <orepbase/ored/portfolio/builders/durationadjustedcms.hpp>

#include <orepbase/qle/cashflows/durationadjustedcmscoupontsrpricer.hpp>
#include <orepbase/qle/models/linearannuitymapping.hpp>

#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

boost::shared_ptr<FloatingRateCouponPricer>
LinearTsrDurationAdjustedCmsCouponPricerBuilder::engineImpl(const Currency& ccy) {

    Real reversion = parseReal(engineParameter("MeanReversion", ccy.code(), true));

    Handle<Quote> reversionQuote(boost::make_shared<SimpleQuote>(reversion));
    Handle<SwaptionVolatilityStructure> vol;
    if (parseBool(engineParameter("ZeroVolatility", "", false, "false"))) {
        vol = Handle<SwaptionVolatilityStructure>(boost::make_shared<ConstantSwaptionVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    } else {
        vol = market_->swaptionVol(ccy.code(), configuration(MarketContext::pricing));
    }

    string lowerBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "LowerRateBoundLogNormal" : "LowerRateBoundNormal";
    string upperBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "UpperRateBoundLogNormal" : "UpperRateBoundNormal";

    Real lower = parseReal(engineParameter(lowerBoundStr));
    Real upper = parseReal(engineParameter(upperBoundStr));

    return boost::make_shared<DurationAdjustedCmsCouponTsrPricer>(
        vol, boost::make_shared<LinearAnnuityMappingBuilder>(reversionQuote), lower, upper);
};

} // namespace data
} // namespace ore
