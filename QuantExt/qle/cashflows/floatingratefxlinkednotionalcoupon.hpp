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

namespace QuantExt {
using namespace QuantLib;

//! %Coupon paying a Libor-type index on an fx-linked nominal
//! \ingroup cashflows
class FloatingRateFXLinkedNotionalCoupon : public FloatingRateCoupon, public FXLinked {
public:
    //! FloatingRateFXLinkedNotionalCoupon
    FloatingRateFXLinkedNotionalCoupon(const Date& fxFixingDate, Real foreignAmount, boost::shared_ptr<FxIndex> fxIndex,
                                       const boost::shared_ptr<FloatingRateCoupon>& underlying)
        : FloatingRateCoupon(underlying->date(), Null<Real>(), underlying->accrualStartDate(),
                             underlying->accrualEndDate(), underlying->fixingDays(), underlying->index(),
                             underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                             underlying->referencePeriodEnd(), underlying->dayCounter(), underlying->isInArrears()),
          FXLinked(fxFixingDate, foreignAmount, fxIndex), underlying_(underlying) {
        registerWith(FXLinked::fxIndex());
        registerWith(underlying_);
    }

    //! \name Coupon interface
    //@{
    Rate nominal() const { return foreignAmount() * fxRate(); }
    Rate rate() const { return underlying_->rate(); }
    //@}

    //! \name FloatingRateCoupon interface
    //@{
    Rate indexFixing() const { return underlying_->indexFixing(); } // might be overwritten in underlying
    void setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& p) { underlying_->setPricer(p); }
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

    //! more inspectors
    boost::shared_ptr<FloatingRateCoupon> underlying() const { return underlying_; }

private:
    const boost::shared_ptr<FloatingRateCoupon> underlying_;
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
