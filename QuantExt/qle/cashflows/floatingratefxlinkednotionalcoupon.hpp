/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file floatingratefxlinkednotionalcoupon.hpp
    \brief Coupon paying a Libor-type index but with an FX linked notional

    \ingroup cashflows
*/

#ifndef quantext_floating_rate_fx_linked_notional_coupon_hpp
#define quantext_floating_rate_fx_linked_notional_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

using namespace QuantLib;

namespace QuantExt {

//! %Coupon paying a Libor-type index on an fx-linked nominal
//! \ingroup cashflows
class FloatingRateFXLinkedNotionalCoupon : public FloatingRateCoupon {
public:
    //! FloatingRateFXLinkedNotionalCoupon
    /*! Note that if you ask this coupon for it's nominal, you will get 0 back as the nominal is
     *  variable (and Coupon::nominal() is not virtual). To get the actual nominal call fxLinkedCashFlow().amount()
     */
    FloatingRateFXLinkedNotionalCoupon(Real foreignAmount, const Date& fxFixingDate, boost::shared_ptr<FxIndex> fxIndex,
                                       bool invertFxIndex, const Date& paymentDate, const Date& startDate,
                                       const Date& endDate, Natural fixingDays,
                                       const boost::shared_ptr<InterestRateIndex>& index, Real gearing = 1.0,
                                       Spread spread = 0.0, const Date& refPeriodStart = Date(),
                                       const Date& refPeriodEnd = Date(), const DayCounter& dayCounter = DayCounter(),
                                       bool isInArrears = false)
        : FloatingRateCoupon(paymentDate, 0.0, startDate, endDate, fixingDays, index, gearing, spread, refPeriodStart,
                             refPeriodEnd, dayCounter, isInArrears),
          notional_(paymentDate, fxFixingDate, foreignAmount, fxIndex, invertFxIndex) {}

    //! \name CashFlow interface
    //@{
    /*! We override FloatingRateCoupon::amount() here as we need to use the variable notional from
        the fxLinkedCashflow.
     */
    Real amount() const { return rate() * accrualPeriod() * notional_.amount(); }
    //@}

    //! Return the underlying FX linked notional
    const FXLinkedCashFlow& fxLinkedCashFlow() { return notional_; }

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

private:
    FXLinkedCashFlow notional_;
};

// inline definitions
inline void FloatingRateFXLinkedNotionalCoupon::accept(AcyclicVisitor& v) {
    Visitor<FloatingRateFXLinkedNotionalCoupon>* v1 = dynamic_cast<Visitor<FloatingRateFXLinkedNotionalCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}
} // namespace QuantExt

#endif
