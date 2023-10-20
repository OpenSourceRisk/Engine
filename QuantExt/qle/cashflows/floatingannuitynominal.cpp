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

#include <qle/cashflows/floatingannuitynominal.hpp>

namespace QuantExt {

Leg makeFloatingAnnuityNominalLeg(const Leg& floatingAnnuityLeg) {
    Leg leg;
    for (Size i = 0; i < floatingAnnuityLeg.size(); i++) {
        boost::shared_ptr<FloatingAnnuityCoupon> coupon =
            boost::dynamic_pointer_cast<FloatingAnnuityCoupon>(floatingAnnuityLeg[i]);
        if (coupon)
            leg.push_back(boost::shared_ptr<CashFlow>(new FloatingAnnuityNominal(coupon)));
    }
    return leg;
}

} // namespace QuantExt
