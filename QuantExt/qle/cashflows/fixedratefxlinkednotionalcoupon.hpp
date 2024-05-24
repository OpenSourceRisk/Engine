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

/*! \file fixedratefxlinkednotionalcoupon.hpp
    \brief Coupon paying a fixed rate but with an FX linked notional

    \ingroup cashflows
*/

#ifndef quantext_fixed_rate_fx_linked_notional_coupon_hpp
#define quantext_fixed_rate_fx_linked_notional_coupon_hpp

#include <ql/cashflows/fixedratecoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

namespace QuantExt {

//! %Coupon paying a Libor-type index on an fx-linked nominal
//! \ingroup cashflows
class FixedRateFXLinkedNotionalCoupon : public QuantLib::Observer, public FixedRateCoupon, public FXLinked {
public:
    //! FloatingRateFXLinkedNotionalCoupon
    FixedRateFXLinkedNotionalCoupon(const QuantLib::Date& fxFixingDate, QuantLib::Real foreignAmount,
        QuantLib::ext::shared_ptr<FxIndex> fxIndex, const QuantLib::ext::shared_ptr<FixedRateCoupon>& underlying);
    
    //! \name FXLinked interface
    //@{
    QuantLib::ext::shared_ptr<FXLinked> clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) override;
    //@}

    //! \name Coupon interface
    //@{
    QuantLib::Rate nominal() const override;
    QuantLib::Rate rate() const override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&) override;
    //@}

    //! more inspectors
    QuantLib::ext::shared_ptr<FixedRateCoupon> underlying() const;

private:
    const QuantLib::ext::shared_ptr<FixedRateCoupon> underlying_;
};

} // namespace QuantExt

#endif
