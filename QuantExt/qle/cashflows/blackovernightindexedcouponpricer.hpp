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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Black compounded overnight coupon pricer
/* The methods that are implemented here to price capped / floored compounded ON coupons are
   highly experimental and ad-hoc. As soon as a market best practice has evolved, the pricer
   should be revised. */
class BlackOvernightIndexedCouponPricer : public CappedFlooredOvernightIndexedCouponPricer {
public:
    using CappedFlooredOvernightIndexedCouponPricer::CappedFlooredOvernightIndexedCouponPricer;
    void initialize(const FloatingRateCoupon& coupon) override;
    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

private:
    Real optionletRateGlobal(Option::Type optionType, Real effStrike) const;
    Real optionletRateLocal(Option::Type optionType, Real effStrike) const;

    Real gearing_;
    ext::shared_ptr<IborIndex> index_;
    Real effectiveIndexFixing_, swapletRate_;

    const CappedFlooredOvernightIndexedCoupon* coupon_;
};

//! Black averaged overnight coupon pricer
/* The methods that are implemented here to price capped / floored avergad ON coupons are
   highly experimental and ad-hoc. As soon as a market best practice has evolved, the pricer
   should be revised. */
class BlackAverageONIndexedCouponPricer : public CapFlooredAverageONIndexedCouponPricer {
public:
    using CapFlooredAverageONIndexedCouponPricer::CapFlooredAverageONIndexedCouponPricer;
    void initialize(const FloatingRateCoupon& coupon) override;
    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

private:
    Real optionletRateGlobal(Option::Type optionType, Real effStrike) const;
    Real optionletRateLocal(Option::Type optionType, Real effStrike) const;

    Real gearing_;
    ext::shared_ptr<IborIndex> index_;
    Real swapletRate_;

    const CappedFlooredAverageONIndexedCoupon* coupon_;
};

} // namespace QuantExt
