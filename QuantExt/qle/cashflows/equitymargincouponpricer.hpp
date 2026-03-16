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

/*! \file equitymargincouponpricer.hpp
    \brief Pricer for equity margin coupons
    \ingroup cashflows
*/

#ifndef quantext_equity_margin_coupon_pricer_hpp
#define quantext_equity_margin_coupon_pricer_hpp

#include <ql/cashflows/couponpricer.hpp>
#include <qle/cashflows/equitymargincoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Pricer for equity margin coupons
/*! \ingroup cashflows
 */
class EquityMarginCouponPricer : public virtual Observer, public virtual Observable {
public:
    virtual ~EquityMarginCouponPricer() {}
    //! \name Interface
    //@{
    virtual Rate rate() const;
    virtual void initialize(const EquityMarginCoupon& coupon);
    //@}

    //! \name Observer interface
    //@{
    virtual void update() override { notifyObservers(); }
    //@}
protected:
    const EquityMarginCoupon* coupon_;
    Real marginFactor_;
    InterestRate fixedRate_;
    QuantLib::ext::shared_ptr<EquityIndex2> equityCurve_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    bool isTotalReturn_;
    Real dividendFactor_;
    Real initialPrice_;
};
} // namespace QuantExt

#endif
