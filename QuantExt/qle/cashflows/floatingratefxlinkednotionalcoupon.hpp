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
    FloatingRateFXLinkedNotionalCoupon(const Date& fxFixingDate, Real foreignAmount, QuantLib::ext::shared_ptr<FxIndex> fxIndex,
                                       const QuantLib::ext::shared_ptr<FloatingRateCoupon>& underlying)
        : FloatingRateCoupon(underlying->date(), Null<Real>(), underlying->accrualStartDate(),
                             underlying->accrualEndDate(), underlying->fixingDate(), underlying->index(),
                             underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                             underlying->referencePeriodEnd(), underlying->dayCounter(), underlying->isInArrears()),
          FXLinked(fxFixingDate, foreignAmount, fxIndex), underlying_(underlying) {
        fixingDays_ = underlying->fixingDays() == Null<Natural>()
                          ? (underlying->index() ? underlying->index()->fixingDays() : 0)
                                                                  : underlying->fixingDays();
        registerWith(FXLinked::fxIndex());
        registerWith(underlying_);
    }

    //! \name FXLinked interface
    //@{
    QuantLib::ext::shared_ptr<FXLinked> clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) override;
    //@}

    //! \name Obverver interface
    //@{
    void deepUpdate() override {
        update();
        underlying_->deepUpdate();
    }
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override { rate_ = underlying_->rate(); }
    void alwaysForwardNotifications() override {
	LazyObject::alwaysForwardNotifications();
        underlying_->alwaysForwardNotifications();
    }
    //@}
    //! \name Coupon interface
    //@{
    Rate nominal() const override { return foreignAmount() * fxRate(); }
    //@}

    //! \name FloatingRateCoupon interface
    //@{
    Rate indexFixing() const override { return underlying_->indexFixing(); } // might be overwritten in underlying
    void setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& p) override { underlying_->setPricer(p); }
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

    //! more inspectors
    QuantLib::ext::shared_ptr<FloatingRateCoupon> underlying() const { return underlying_; }

private:
    const QuantLib::ext::shared_ptr<FloatingRateCoupon> underlying_;
};

// inline definitions
inline void FloatingRateFXLinkedNotionalCoupon::accept(AcyclicVisitor& v) {
    Visitor<FloatingRateFXLinkedNotionalCoupon>* v1 = dynamic_cast<Visitor<FloatingRateFXLinkedNotionalCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

inline QuantLib::ext::shared_ptr<FXLinked> FloatingRateFXLinkedNotionalCoupon::clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) {
    return QuantLib::ext::make_shared<FloatingRateFXLinkedNotionalCoupon>(fxFixingDate(), foreignAmount(), fxIndex,
                                                                  underlying());
}

} // namespace QuantExt

#endif
