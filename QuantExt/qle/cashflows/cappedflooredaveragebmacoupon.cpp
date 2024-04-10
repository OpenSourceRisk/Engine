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

#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

void CappedFlooredAverageBMACoupon::alwaysForwardNotifications() {
    LazyObject::alwaysForwardNotifications();
    underlying_->alwaysForwardNotifications();
}

void CappedFlooredAverageBMACoupon::deepUpdate() {
    update();
    underlying_->deepUpdate();
}

void CappedFlooredAverageBMACoupon::performCalculations() const {
    QL_REQUIRE(underlying_->pricer(), "pricer not set");
    Rate swapletRate = nakedOption_ ? 0.0 : underlying_->rate();
    if (floor_ != Null<Real>() || cap_ != Null<Real>())
        pricer()->initialize(*this);
    Rate floorletRate = 0.;
    if (floor_ != Null<Real>())
        floorletRate = pricer()->floorletRate(effectiveFloor());
    Rate capletRate = 0.;
    if (cap_ != Null<Real>())
        capletRate = (nakedOption_ && floor_ == Null<Real>() ? -1.0 : 1.0) * pricer()->capletRate(effectiveCap());
    rate_ = swapletRate + floorletRate - capletRate;
    auto p = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageBMACouponPricer>(pricer());
    QL_REQUIRE(p, "CapFlooredAverageBMACoupon::performCalculations(): internal error, could not cast to "
                  "CapFlooredAverageBMACouponPricer");
    effectiveCapletVolatility_ = p->effectiveCapletVolatility();
    effectiveFloorletVolatility_ = p->effectiveFloorletVolatility();
}

Rate CappedFlooredAverageBMACoupon::cap() const { return gearing_ > 0.0 ? cap_ : floor_; }

Rate CappedFlooredAverageBMACoupon::floor() const { return gearing_ > 0.0 ? floor_ : cap_; }

Rate CappedFlooredAverageBMACoupon::rate() const {
    calculate();
    return rate_;
}

Rate CappedFlooredAverageBMACoupon::convexityAdjustment() const { return underlying_->convexityAdjustment(); }

Rate CappedFlooredAverageBMACoupon::effectiveCap() const {
    if (cap_ == Null<Real>())
        return Null<Real>();

    if (includeSpread()) {
        // A = \min \left( \max \left( \cdot \frac{\prod (1 + \tau_i(f_i + s)) - 1}{\tau}, F \right), C \right)
        return (cap_ / gearing() - underlying_->spread());
    } else {
        // A = \min \left( \max \left( g \cdot \frac{\prod (1 + \tau_i f_i) - 1}{\tau} + s, F \right), C \right)
        return (cap_ - underlying_->spread()) / gearing();
    }
}

Rate CappedFlooredAverageBMACoupon::effectiveFloor() const {
    if (floor_ == Null<Real>())
        return Null<Real>();
    if (includeSpread()) {
        return (floor_ - underlying_->spread());
    } else {
        return (floor_ - underlying_->spread()) / gearing();
    }
}

Real CappedFlooredAverageBMACoupon::effectiveCapletVolatility() const {
    calculate();
    return effectiveCapletVolatility_;
}

Real CappedFlooredAverageBMACoupon::effectiveFloorletVolatility() const {
    calculate();
    return effectiveFloorletVolatility_;
}

void CappedFlooredAverageBMACoupon::accept(AcyclicVisitor& v) {
    Visitor<CappedFlooredAverageBMACoupon>* v1 = dynamic_cast<Visitor<CappedFlooredAverageBMACoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

CappedFlooredAverageBMACoupon::CappedFlooredAverageBMACoupon(const ext::shared_ptr<AverageBMACoupon>& underlying,
                                                             Real cap, Real floor, bool nakedOption, bool includeSpread)
    : FloatingRateCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                         underlying->accrualEndDate(), underlying->fixingDays(), underlying->index(),
                         underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                         underlying->referencePeriodEnd(), underlying->dayCounter(), false),
      underlying_(underlying), cap_(cap), floor_(floor), nakedOption_(nakedOption), includeSpread_(includeSpread) {
    QL_REQUIRE(!includeSpread_ || close_enough(underlying_->gearing(), 1.0),
               "CappedFlooredAverageBMACoupon: if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");
    registerWith(underlying_);
    if (nakedOption_)
        underlying_->alwaysForwardNotifications();
}

// capped floored average on coupon pricer base class implementation

CapFlooredAverageBMACouponPricer::CapFlooredAverageBMACouponPricer(const Handle<OptionletVolatilityStructure>& v,
                                                                   const bool effectiveVolatilityInput)
    : capletVol_(v), effectiveVolatilityInput_(effectiveVolatilityInput) {
    registerWith(capletVol_);
}

bool CapFlooredAverageBMACouponPricer::effectiveVolatilityInput() const { return effectiveVolatilityInput_; }

Real CapFlooredAverageBMACouponPricer::effectiveCapletVolatility() const { return effectiveCapletVolatility_; }

Real CapFlooredAverageBMACouponPricer::effectiveFloorletVolatility() const { return effectiveFloorletVolatility_; }

Handle<OptionletVolatilityStructure> CapFlooredAverageBMACouponPricer::capletVolatility() const { return capletVol_; }

} // namespace QuantExt
