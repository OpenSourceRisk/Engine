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

/*! \file cpicouponpricer.cpp
    \brief CPI CashFlow and Coupon pricers that handle caps/floors
*/

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>

namespace QuantExt {

void BlackCPICashFlowPricer::initialize(const CappedFlooredCPICashFlow& cashflow) {
    cashflow_ = dynamic_cast<const CappedFlooredCPICashFlow*>(&cashflow);
    QL_REQUIRE(cashflow_, "CappedFlooredCPICashFlow needed");
}

Real BlackCPICashFlowPricer::amount() const {
    Real a = cashflow_->underlying()->amount();
    return a;
}

Real BlackCPICashFlowPricer::cap() const {
    Real c = 0.0;
    return c;
}

Real BlackCPICashFlowPricer::floor() const {
    Real f = 0.0;
    return f;
}

void BlackCPICouponPricer::initialize(const InflationCoupon& coupon) {
    coupon_ = dynamic_cast<const CappedFlooredCPICoupon*>(&coupon);
    QL_REQUIRE(coupon_, "CappedFlooredCPICoupon needed");
}

// TODO
Real BlackCPICouponPricer::swapletPrice() const { return 0.0; }
Real BlackCPICouponPricer::swapletRate() const { return 0.0; }
Real BlackCPICouponPricer::capletPrice(Rate effectiveCap) const { return 0.0; }
Real BlackCPICouponPricer::capletRate(Rate effectiveCap) const { return 0.0; }
Real BlackCPICouponPricer::floorletPrice(Rate effectiveFloor) const { return 0.0; }
Real BlackCPICouponPricer::floorletRate(Rate effectiveFloor) const { return 0.0; }

} // namespace QuantExt
