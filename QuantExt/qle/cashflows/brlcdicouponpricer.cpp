/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/cashflows/brlcdicouponpricer.hpp>

using namespace QuantLib;
using std::vector;
using std::pow;

namespace QuantExt {

// The code here is very similar to QuantLib::OvernightIndexedCouponPricer. That class is in 
// an anonymous namespace so could not extend it. The altered code is to take account of the fact 
// that the BRL CDI coupon accrues as (1 + DI) ^ (1 / 252)
Rate BRLCdiCouponPricer::swapletRate() const {

    const vector<Date>& fixingDates = coupon_->fixingDates();
    const vector<Time>& dt = coupon_->dt();
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

        const vector<Date>& dates = coupon_->valueDates();
        DiscountFactor startDiscount = curve->discount(dates[i]);
        DiscountFactor endDiscount = curve->discount(dates[n]);

        compoundFactor *= startDiscount / endDiscount;
    }

    Rate rate = (compoundFactor - 1.0) / coupon_->accrualPeriod();
    return coupon_->gearing() * rate + coupon_->spread();
}

void BRLCdiCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    
    // Ensure that we have an overnight coupon and that the index is BRL DI
    coupon_ = dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "BRLCdiCouponPricer epxects an OvernightIndexedCoupon");

    boost::shared_ptr<InterestRateIndex> index = coupon_->index();
    index_ = boost::dynamic_pointer_cast<BRLCdi>(index);
    QL_REQUIRE(index_, "BRLCdiCouponPricer epxects the coupon's index to be BRLCdi");
}

Real BRLCdiCouponPricer::swapletPrice() const {
    QL_FAIL("swapletPrice not implemented for BRLCdiCouponPricer");
}

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

}
