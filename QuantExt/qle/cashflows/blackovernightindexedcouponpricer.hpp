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

/*! \file blackovernightindexedcouponpricer.hpp
    \brief black coupon pricer for capped / floored ON indexed coupons
*/

#pragma once

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Black overnight coupon pricer
class BlackOvernightIndexedCouponPricer : public CappedFlooredOvernightIndexedCouponPricer {
public:
    using CappedFlooredOvernightIndexedCouponPricer::CappedFlooredOvernightIndexedCouponPricer;
    virtual void initialize(const FloatingRateCoupon& coupon);
    Real swapletPrice() const;
    Rate swapletRate() const;
    Real capletPrice(Rate effectiveCap) const;
    Rate capletRate(Rate effectiveCap) const;
    Real floorletPrice(Rate effectiveFloor) const;
    Rate floorletRate(Rate effectiveFloor) const;

protected:
    Real optionletRate(Option::Type optionType, Real effStrike) const;

    Real gearing_;
    ext::shared_ptr<IborIndex> index_;
    Real effectiveIndexFixing_, swapletRate_, fixedAccrualPeriodRatio_;

    const CappedFlooredOvernightIndexedCoupon* coupon_;
};

} // namespace QuantExt
