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

/*! \file equitycouponpricer.hpp
    \brief Pricer for equity coupons
    \ingroup cashflows
*/

#ifndef quantext_equity_coupon_pricer_hpp
#define quantext_equity_coupon_pricer_hpp

#include <ql/cashflows/couponpricer.hpp>
#include <qle/cashflows/equitycoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Pricer for equity coupons
/*! \ingroup cashflows
 */
class EquityCouponPricer : public virtual Observer, public virtual Observable {
public:
    struct AdditionalResultCache {
        Real initialPrice;
        Real startFixingTotal;
        Real startFixing;
        Real startFxFixing;
        Real endFixingTotal;
        Real endFixing;
        Real endFxFixing;
        Real pastDividends;
        Real forecastDividends;

        void clear();
    };

    virtual ~EquityCouponPricer() {}
    //! \name Interface
    //@{
    virtual Rate swapletRate();
    virtual void initialize(const EquityCoupon& coupon);
    const AdditionalResultCache& additionalResultCache() const { return additionalResultCache_; };
    //@}

    //! \name Observer interface
    //@{
    virtual void update() override { notifyObservers(); }
    //@}
protected:
    const EquityCoupon* coupon_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    EquityReturnType returnType_;
    Real dividendFactor_;
    AdditionalResultCache additionalResultCache_;
};
} // namespace QuantExt

#endif
