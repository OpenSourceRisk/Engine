/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

void BlackOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
    gearing_ = coupon.gearing();
    index_ = ext::dynamic_pointer_cast<OvernightIndex>(coupon.index());
    if (!index_) {
        // check if the coupon was right
        const CappedFlooredOvernightIndexedCoupon* c =
            dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon);
        QL_REQUIRE(c, "BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
        // coupon was right, index is not
        QL_FAIL("BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
    }
    Handle<YieldTermStructure> rateCurve = index_->forwardingTermStructure();
    swapletRate_ = coupon_->underlying()->rate();
    effectiveIndexFixing_ = coupon_->underlying()->effectiveIndexFixing();
}

Real BlackOvernightIndexedCouponPricer::optionletRate(Option::Type optionType, Real effStrike) const {
    Date lastRelevantFixingDate = coupon_->underlying()->fixingDate();
    if (lastRelevantFixingDate <= Settings::instance().evaluationDate()) {
        // the amount is determined
        Real a, b;
        if (optionType == Option::Call) {
            a = effectiveIndexFixing_;
            b = effStrike;
        } else {
            a = effStrike;
            b = effectiveIndexFixing_;
        }
        return std::max(a - b, 0.0);
    } else {
        // not yet determined, use Black model
        // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates, section 6.3.
        // the idea is to dampen the average volatility sigma between the fixing start and fixing end date by a
        // linear function going from (fixing start, 1) to (fixing end, 0)
        QL_REQUIRE(!capletVolatility().empty(), "BlackOvernightIndexedCouponPricer: missing optionlet volatility");
        std::vector<Date> fixingDates = coupon_->underlying()->fixingDates();
        QL_REQUIRE(!fixingDates.empty(), "BlackOvernightIndexedCouponPricer: empty fixing dates");
        Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
        Real fixingEndTime = capletVolatility()->timeFromReference(fixingDates.back());
        QL_REQUIRE(!close_enough(fixingEndTime, fixingStartTime),
                   "BlackOvernightIndexedCouponPricer: fixingStartTime = fixingEndTime = " << fixingStartTime);
        Real sigma = capletVolatility()->volatility(lastRelevantFixingDate, effStrike);
        Real stdDev = sigma * std::sqrt(std::max(fixingStartTime, 0.0) +
                                        std::pow(fixingEndTime - std::max(fixingStartTime, 0.0), 3.0) /
                                            std::pow(fixingEndTime - fixingStartTime, 2.0));
        Real shift = capletVolatility()->displacement();
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Rate fixing = shiftedLn ? blackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0, shift)
                                : bachelierBlackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0);
        return gearing_ * fixing;
    }
}

Rate BlackOvernightIndexedCouponPricer::swapletRate() const { return swapletRate_; }

Rate BlackOvernightIndexedCouponPricer::capletRate(Rate effectiveCap) const {
    return optionletRate(Option::Call, effectiveCap);
}

Rate BlackOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor) const {
    return optionletRate(Option::Put, effectiveFloor);
}

Real BlackOvernightIndexedCouponPricer::swapletPrice() const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::swapletPrice() not provided");
}
Real BlackOvernightIndexedCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::capletPrice() not provided");
}
Real BlackOvernightIndexedCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::floorletPrice() not provided");
}

} // namespace QuantExt
