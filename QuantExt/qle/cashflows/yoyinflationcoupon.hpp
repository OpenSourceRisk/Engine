/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/indexedcoupon.hpp
    \brief coupon with an indexed notional
    \ingroup cashflows
*/

#ifndef quantext_yoy_inflation_coupon_hpp
#define quantext_yoy_inflation_coupon_hpp

#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/cashflow.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Extend the QuantLib YoYInflationCoupon, now the payoff is based on growth only (default behaviour) (I_t / I_{t-1} -
//! 1) or I_t / I_{t-1}

class YoYInflationCoupon : public QuantLib::YoYInflationCoupon {
public:
    explicit YoYInflationCoupon(const ext::shared_ptr<QuantLib::YoYInflationCoupon> underlying, bool growthOnly = true);

    // ! \name Coupon interface
    Rate rate() const;
    //@}
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
    void setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer);

private:
    const ext::shared_ptr<QuantLib::YoYInflationCoupon> underlying_;
    bool growthOnly_;
};


class CappedFlooredYoYInflationCoupon : public QuantLib::CappedFlooredYoYInflationCoupon {
public:
    explicit CappedFlooredYoYInflationCoupon(
        const ext::shared_ptr<QuantLib::CappedFlooredYoYInflationCoupon> underlying,
                                             bool growthOnly = true);

    // ! \name Coupon interface
    Rate rate() const;
    //@}
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
    void setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer);

private:
    const ext::shared_ptr<QuantLib::CappedFlooredYoYInflationCoupon> underlying_;
    bool growthOnly_;
};



class YoYCouponLeg {
public:
    explicit YoYCouponLeg(const Leg& underlyingLeg, bool growthOnly = true);
    operator Leg() const;

private:
    Leg underlyingLeg_;
    bool growthOnly_;
};
} // namespace QuantExt
#endif

