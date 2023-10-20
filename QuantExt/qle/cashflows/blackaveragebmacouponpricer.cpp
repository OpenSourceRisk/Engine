/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/cashflows/blackaveragebmacouponpricer.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

void BlackAverageBMACouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const CappedFlooredAverageBMACoupon*>(&coupon);
    QL_REQUIRE(coupon_, "BlackAverageBMACouponPricer: CappedFlooredAverageBMACoupon required");
    gearing_ = coupon.gearing();
    index_ = ext::dynamic_pointer_cast<BMAIndex>(coupon.index());
    if (!index_) {
        // check if the coupon was right
        const CappedFlooredAverageBMACoupon* c = dynamic_cast<const CappedFlooredAverageBMACoupon*>(&coupon);
        QL_REQUIRE(c, "BlackAverageBMACouponPricer: CappedFlooredAverageBMACoupon required");
        // coupon was right, index is not
        QL_FAIL("BlackAverageBMACouponPricer: CappedFlooredAverageBMACoupon required");
    }
    swapletRate_ = coupon_->underlying()->rate();
    forwardRate_ = (swapletRate_ - coupon_->underlying()->spread()) / coupon_->underlying()->gearing();
    effectiveCapletVolatility_ = effectiveFloorletVolatility_ = Null<Real>();
}

Real BlackAverageBMACouponPricer::optionletRate(Option::Type optionType, Real effStrike) const {
    Date lastRelevantFixingDate = coupon_->underlying()->fixingDate();
    if (lastRelevantFixingDate <= Settings::instance().evaluationDate()) {
        // the amount is determined
        Real a, b;
        if (optionType == Option::Call) {
            a = forwardRate_;
            b = effStrike;
        } else {
            a = effStrike;
            b = forwardRate_;
        }
        return gearing_ * std::max(a - b, 0.0);
    } else {
        // not yet determined, use Black model
        QL_REQUIRE(!capletVolatility().empty(), "BlackAverageBMACouponPricer: missing optionlet volatility");
        std::vector<Date> fixingDates = coupon_->underlying()->fixingDates();
        QL_REQUIRE(!fixingDates.empty(),
                   "BlackAverageBMACouponPricer: internal error, got empty fixingDates, contact dev.");
        fixingDates.erase(std::next(fixingDates.end(), -1)); // there is one additional date returned!
        QL_REQUIRE(!fixingDates.empty(), "BlackAverageBMACouponPricer: empty fixing dates");
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Real shift = capletVolatility()->displacement();
        Real stdDev;
        Real effectiveTime = capletVolatility()->timeFromReference(fixingDates.back());
        if (effectiveVolatilityInput()) {
            // vol input is effective, i.e. we use a plain black model
            stdDev = capletVolatility()->volatility(fixingDates.back(), effStrike) * std::sqrt(effectiveTime);
        } else {
            // vol input is not effective: we proceed similarly to average on coupon pricing:
            // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates,
            // section 6.3. the idea is to dampen the average volatility sigma between the fixing start and fixing end
            // date by a linear function going from (fixing start, 1) to (fixing end, 0)
            Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
            Real fixingEndTime = capletVolatility()->timeFromReference(fixingDates.back());
            Real sigma = capletVolatility()->volatility(
                std::max(fixingDates.front(), capletVolatility()->referenceDate() + 1), effStrike);
            Real T = std::max(fixingStartTime, 0.0);
            if (!close_enough(fixingEndTime, T))
                T += std::pow(fixingEndTime - T, 3.0) / std::pow(fixingEndTime - fixingStartTime, 2.0) / 3.0;
            stdDev = sigma * std::sqrt(T);
        }
        if (optionType == Option::Type::Call)
            effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
        else
            effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);
        Real fixing = shiftedLn ? blackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0, shift)
                                : bachelierBlackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0);
        return gearing_ * fixing;
    }
}

Rate BlackAverageBMACouponPricer::swapletRate() const { return swapletRate_; }

Rate BlackAverageBMACouponPricer::capletRate(Rate effectiveCap) const {
    return optionletRate(Option::Call, effectiveCap);
}

Rate BlackAverageBMACouponPricer::floorletRate(Rate effectiveFloor) const {
    return optionletRate(Option::Put, effectiveFloor);
}

Real BlackAverageBMACouponPricer::swapletPrice() const {
    QL_FAIL("BlackAverageBMACouponPricer::swapletPrice() not provided");
}
Real BlackAverageBMACouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("BlackAverageBMACouponPricer::capletPrice() not provided");
}
Real BlackAverageBMACouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("BlackAverageBMACouponPricer::floorletPrice() not provided");
}

} // namespace QuantExt
