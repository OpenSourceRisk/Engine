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

/*! \file cappedflooredaveragebmacoupon.hpp
    \brief coupon paying a capped / floored average bma rate
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/couponpricer.hpp>

namespace QuantExt {
using namespace QuantLib;

class CappedFlooredAverageBMACoupon : public FloatingRateCoupon {
public:
    CappedFlooredAverageBMACoupon(const ext::shared_ptr<AverageBMACoupon>& underlying, Real cap = Null<Real>(),
                                  Real floor = Null<Real>(), bool nakedOption = false, bool includeSpread = false);

    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    void alwaysForwardNotifications() override;
    //@}
    //! \name Coupon interface
    //@{
    Rate rate() const override;
    Rate convexityAdjustment() const override;
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    Date fixingDate() const override { return underlying_->fixingDate(); }
    //@}
    //! cap
    Rate cap() const;
    //! floor
    Rate floor() const;
    //! effective cap of fixing
    Rate effectiveCap() const;
    //! effective floor of fixing
    Rate effectiveFloor() const;
    //! effective caplet volatility
    Real effectiveCapletVolatility() const;
    //! effective floorlet volatility
    Real effectiveFloorletVolatility() const;
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;

    bool isCapped() const { return cap_ != Null<Real>(); }
    bool isFloored() const { return floor_ != Null<Real>(); }

    ext::shared_ptr<AverageBMACoupon> underlying() const { return underlying_; }
    bool nakedOption() const { return nakedOption_; }
    bool includeSpread() const { return includeSpread_; }

protected:
    ext::shared_ptr<AverageBMACoupon> underlying_;
    Rate cap_, floor_;
    bool nakedOption_;
    bool includeSpread_;
    mutable Real effectiveCapletVolatility_;
    mutable Real effectiveFloorletVolatility_;
};

//! capped floored averaged indexed coupon pricer base class
class CapFlooredAverageBMACouponPricer : public FloatingRateCouponPricer {
public:
    CapFlooredAverageBMACouponPricer(const Handle<OptionletVolatilityStructure>& v,
                                     const bool effectiveVolatilityInput = false);
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    bool effectiveVolatilityInput() const;
    Real effectiveCapletVolatility() const;   // only available after capletRate() was called
    Real effectiveFloorletVolatility() const; // only available after floorletRate() was called

protected:
    Handle<OptionletVolatilityStructure> capletVol_;
    bool effectiveVolatilityInput_;
    mutable Real effectiveCapletVolatility_;
    mutable Real effectiveFloorletVolatility_;
};

} // namespace QuantExt
