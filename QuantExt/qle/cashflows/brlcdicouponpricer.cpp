/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/cashflows/brlcdicouponpricer.hpp>

using namespace QuantLib;
using std::pow;
using std::vector;

namespace QuantExt {

// The code here is very similar to QuantLib::OvernightIndexedCouponPricer. That class is in
// an anonymous namespace so could not extend it. The altered code is to take account of the fact
// that the BRL CDI coupon accrues as (1 + DI) ^ (1 / 252)
Rate BRLCdiCouponPricer::swapletRate() const {

    const vector<Date>& fixingDates = couponQl_ ? couponQl_->fixingDates() : couponQle_->fixingDates();
    const vector<Time>& dt = couponQl_ ? couponQl_->dt() : couponQle_->dt();
    Size n = dt.size();
    Size i = 0;
    Real compoundFactor = 1.0;
    Date today = Settings::instance().evaluationDate();

    // Already fixed part of the coupon
    while (i < n && fixingDates[i] < today) {
        Rate pastFixing = IndexManager::instance().getHistory(index_->name())[fixingDates[i]];
        QL_REQUIRE(pastFixing != Null<Real>(), "Missing " << index_->name() << " fixing for " << fixingDates[i]);
        compoundFactor *= pow(1.0 + pastFixing, dt[i]);
        ++i;
    }

    // Today is a border case. If there is a fixing use it. If not, it will be projected in the next block.
    if (i < n && fixingDates[i] == today) {
        try {
            Rate pastFixing = IndexManager::instance().getHistory(index_->name())[fixingDates[i]];
            if (pastFixing != Null<Real>()) {
                compoundFactor *= pow(1.0 + pastFixing, dt[i]);
                ++i;
            }
        } catch (Error&) {
        }
    }

    // Use telescopic property for piece of coupon in the future
    // \Pi_{i=0}^{n-1} \left( 1 + DI(t, t_{i}, t_{i+1}) \right) ^ \delta = \frac{P(t, t_0)}{P(t, t_n)}
    if (i < n) {
        Handle<YieldTermStructure> curve = index_->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(), "BRLCdiCouponPricer needs the index to have a forwarding term structure");

        const vector<Date>& dates = couponQl_ ? couponQl_->valueDates() : couponQle_->valueDates();
        DiscountFactor startDiscount = curve->discount(dates[i]);
        DiscountFactor endDiscount = curve->discount(dates[n]);

        compoundFactor *= startDiscount / endDiscount;
    }

    Rate rate = (compoundFactor - 1.0) / (couponQl_ ? couponQl_->accrualPeriod() : couponQle_->accrualPeriod());
    return (couponQl_ ? couponQl_->gearing() : couponQle_->gearing()) * rate +
           (couponQl_ ? couponQl_->spread() : couponQle_->spread());
}

void BRLCdiCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    // Ensure that we have an overnight coupon and that the index is BRL DI
    couponQl_ = dynamic_cast<const QuantLib::OvernightIndexedCoupon*>(&coupon);
    couponQle_ = dynamic_cast<const QuantExt::OvernightIndexedCoupon*>(&coupon);
    QL_REQUIRE(couponQl_ || couponQle_, "BRLCdiCouponPricer expects an OvernightIndexedCoupon");

    QuantLib::ext::shared_ptr<InterestRateIndex> index = couponQl_ ? couponQl_->index() : couponQle_->index();
    index_ = QuantLib::ext::dynamic_pointer_cast<BRLCdi>(index);
    QL_REQUIRE(index_, "BRLCdiCouponPricer expects the coupon's index to be BRLCdi");
}

Real BRLCdiCouponPricer::swapletPrice() const { QL_FAIL("swapletPrice not implemented for BRLCdiCouponPricer"); }

Real BRLCdiCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("capletPrice not implemented for BRLCdiCouponPricer");
}

Real BRLCdiCouponPricer::capletRate(Rate effectiveCap) const {
    QL_FAIL("capletRate not implemented for BRLCdiCouponPricer");
}

Real BRLCdiCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("floorletPrice not implemented for BRLCdiCouponPricer");
}

Real BRLCdiCouponPricer::floorletRate(Rate effectiveFloor) const {
    QL_FAIL("floorletRate not implemented for BRLCdiCouponPricer");
}

} // namespace QuantExt
