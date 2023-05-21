/*
 Copyright (C) 2020 Quaternion Risk Managment Ltd
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

#include <algorithm>
#include <qle/pricingengines/binomialconvertibleengine.hpp>
#include <qle/pricingengines/intrinsicascotengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

namespace QuantExt {

IntrinsicAscotEngine::IntrinsicAscotEngine(const Handle<YieldTermStructure>& discountCurve)
    : discountCurve_(discountCurve) {
    registerWith(discountCurve_);
}

void IntrinsicAscotEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::American, "not an American option");

    ConvertibleBond2 bond = *arguments_.bond;
    Real bondPrice = arguments_.bondQuantity * bond.NPV();

    Date referenceDate = discountCurve_->referenceDate();

    Leg notional;
    auto coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(bond.cashflows()[0]);
    QL_REQUIRE(coupon, "expected non-coupon legs");
    double initFlowAmt = coupon->nominal();
    Date initDate = coupon->accrualStartDate();
    initDate = bond.calendar().adjust(initDate, Following);
    if (initFlowAmt != 0)
        notional.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(initFlowAmt, initDate)));

    Real upfrontPayment = CashFlows::npv(notional, **discountCurve_, false, referenceDate, referenceDate);

    // includes redemption flows
    Real assetLeg = CashFlows::npv(bond.cashflows(), **discountCurve_, false, referenceDate, referenceDate);

    Real redemptionLeg = CashFlows::npv(bond.redemptions(), **discountCurve_, false, referenceDate, referenceDate);

    // multiplied by bondNotional already
    Real fundingLeg = CashFlows::npv(arguments_.fundingLeg, **discountCurve_, true, referenceDate, referenceDate);

    Real X = arguments_.bondQuantity * (upfrontPayment + assetLeg - redemptionLeg) - fundingLeg;

    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(arguments_.callPut, X));

    results_.value = (*payoff)(bondPrice);
}

} // namespace QuantExt
