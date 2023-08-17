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

/*! \file blackaveragebmacouponpricer.hpp
    \brief black average bma coupon pricer for capped / floored BMA coupons
*/

#pragma once

#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

class BlackAverageBMACouponPricer : public CapFlooredAverageBMACouponPricer {
public:
    using CapFlooredAverageBMACouponPricer::CapFlooredAverageBMACouponPricer;
    void initialize(const FloatingRateCoupon& coupon) override;
    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

private:
    Real optionletRate(Option::Type optionType, Real effStrike) const;

    Real gearing_;
    ext::shared_ptr<BMAIndex> index_;
    Real swapletRate_, forwardRate_;

    const CappedFlooredAverageBMACoupon* coupon_;
};

} // namespace QuantExt
