/*
 Copyright (C) 2018 Quaternion Risk Management Ltd.
 All rights reserved.
 */

/*! \file scalablecoupon.hpp
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
    ScaledCashFlow(Real multiplier, boost::shared_ptr<CashFlow> underlying)
        : multiplier_(multiplier), underlying_(underlying) {}

    Date date() const override { return underlying_->date(); }
    Real amount() const override { return underlying_->amount() * multiplier_; }

private:
    Real multiplier_;
    boost::shared_ptr<CashFlow> underlying_;
};

//! scalable coupon
/*! %Coupon that returns the scaled results for an underlying coupon
    \ingroup cashflows
*/
class ScaledCoupon : public Coupon, public Observer {
public:
    ScaledCoupon(Real multiplier, boost::shared_ptr<Coupon> underlyingCoupon)
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
    const boost::shared_ptr<Coupon> underlyingCoupon() const { return underlyingCoupon_; }

private:
    Real multiplier_;
    boost::shared_ptr<Coupon> underlyingCoupon_;
};

} // namespace QuantExt
