/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file durationadjustedcms.hpp
    \brief coupon pricer builder for duration adjusted cms coupons
    \ingroup builders
*/

#include <ored/portfolio/builders/durationadjustedcms.hpp>

#include <qle/cashflows/durationadjustedcmscoupontsrpricer.hpp>
#include <qle/models/linearannuitymapping.hpp>

#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

QuantLib::ext::shared_ptr<FloatingRateCouponPricer>
LinearTsrDurationAdjustedCmsCouponPricerBuilder::engineImpl(const std::string& key) {
    std::string ccy = key;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(key, index))
        ccy = index->currency().code();
    Real reversion = parseReal(engineParameter("MeanReversion", {key, ccy}, true));

    Handle<Quote> reversionQuote(QuantLib::ext::make_shared<SimpleQuote>(reversion));
    Handle<SwaptionVolatilityStructure> vol;
    if (parseBool(engineParameter("ZeroVolatility", {}, false, "false"))) {
        vol = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    } else {
        vol = market_->swaptionVol(key, configuration(MarketContext::pricing));
    }

    string lowerBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "LowerRateBoundLogNormal" : "LowerRateBoundNormal";
    string upperBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "UpperRateBoundLogNormal" : "UpperRateBoundNormal";

    Real lower = parseReal(engineParameter(lowerBoundStr));
    Real upper = parseReal(engineParameter(upperBoundStr));

    return QuantLib::ext::make_shared<DurationAdjustedCmsCouponTsrPricer>(
        vol, QuantLib::ext::make_shared<LinearAnnuityMappingBuilder>(reversionQuote), lower, upper);
};

} // namespace data
} // namespace ore
