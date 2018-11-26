/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file floatingannuitynominal.hpp
    \brief Nominal flow associated with a floating annuity coupon
    \ingroup cashflows
*/

#ifndef quantext_floating_annuity_nominal_hpp
#define quantext_floating_annuity_nominal_hpp

#include <ql/cashflow.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Nominal flows associated with the FloatingAnnuityCoupon
//! \ingroup cashflows
class FloatingAnnuityNominal : public CashFlow {
public:
    FloatingAnnuityNominal(const boost::shared_ptr<FloatingAnnuityCoupon>& floatingAnnuityCoupon)
        : coupon_(floatingAnnuityCoupon) {}

    //! \name Cashflow interface
    Rate amount() const;
    Date date() const;

private:
    boost::shared_ptr<FloatingAnnuityCoupon> coupon_;
};

inline Date FloatingAnnuityNominal::date() const { return coupon_->accrualStartDate(); }

inline Real FloatingAnnuityNominal::amount() const { return coupon_->previousNominal() - coupon_->nominal(); }

Leg makeFloatingAnnuityNominalLeg(const Leg& floatingAnnuityLeg);

} // namespace QuantExt

#endif
