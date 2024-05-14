/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/scaledcoupon.hpp
    \brief Coupon / Cashflow paying scaled amounts
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflows/coupon.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! scalable cashflow
/*! %CashFlow that returns the scaled results for an underlying cashflow
    \ingroup cashflows
*/
class ScaledCashFlow : public CashFlow {
public:
    ScaledCashFlow(Real multiplier, QuantLib::ext::shared_ptr<CashFlow> underlying)
        : multiplier_(multiplier), underlying_(underlying) {}

    Date date() const override { return underlying_->date(); }
    Real amount() const override { return underlying_->amount() * multiplier_; }

private:
    Real multiplier_;
    QuantLib::ext::shared_ptr<CashFlow> underlying_;
};

//! scalable coupon
/*! %Coupon that returns the scaled results for an underlying coupon
    \ingroup cashflows
*/
class ScaledCoupon : public Coupon, public Observer {
public:
    ScaledCoupon(Real multiplier, QuantLib::ext::shared_ptr<Coupon> underlyingCoupon)
        : Coupon(underlyingCoupon->date(), underlyingCoupon->nominal(), underlyingCoupon->accrualStartDate(),
                 underlyingCoupon->accrualEndDate()),
          multiplier_(multiplier), underlyingCoupon_(underlyingCoupon) {
        registerWith(underlyingCoupon_);
    };
    //! \name Observer interface
    void update() override { notifyObservers(); }
    //! \name Cashflow interface
    Rate amount() const override { return multiplier_ * underlyingCoupon_->amount(); }

    //! \name Inspectors
    Real accruedAmount(const Date& d) const override { return multiplier_ * underlyingCoupon_->accruedAmount(d); }
    Rate nominal() const override { return multiplier_ * underlyingCoupon_->nominal(); }
    Rate rate() const override { return underlyingCoupon_->rate(); }
    DayCounter dayCounter() const override { return underlyingCoupon_->dayCounter(); }

    const Real multiplier() const { return multiplier_; }
    const QuantLib::ext::shared_ptr<Coupon> underlyingCoupon() const { return underlyingCoupon_; }

private:
    Real multiplier_;
    QuantLib::ext::shared_ptr<Coupon> underlyingCoupon_;
};

} // namespace QuantExt
