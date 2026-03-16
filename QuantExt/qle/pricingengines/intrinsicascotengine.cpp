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
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/pricingengines/binomialconvertibleengine.hpp>
#include <qle/pricingengines/intrinsicascotengine.hpp>

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
    Date settlementDate = bond.calendar().advance(referenceDate, bond.settlementDays(), QuantLib::Days);

    Real currentNotional = Null<Real>();
    for (auto const& c : bond.cashflows()) {
        if (auto coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(c)) {
            currentNotional = coupon->nominal();
            if (c->date() > referenceDate)
                break;
        }
    }

    QL_REQUIRE(currentNotional != Null<Real>(), "IntrinsicAscotEngine::calculate(): could not determine current "
                                                "notional, underlying bond must have at least one coupon");

    Leg upfrontLeg;
    upfrontLeg.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(currentNotional, settlementDate));
    Real upfrontLegNpv = CashFlows::npv(upfrontLeg, **discountCurve_, false, referenceDate, referenceDate);

    // includes redemption flows
    Real assetLegNpv = CashFlows::npv(bond.cashflows(), **discountCurve_, false, referenceDate, referenceDate);

    Real redemptionLegNpv = CashFlows::npv(bond.redemptions(), **discountCurve_, false, referenceDate, referenceDate);

    // multiplied by bondNotional already
    Real fundingLegNpv = CashFlows::npv(arguments_.fundingLeg, **discountCurve_, true, referenceDate, referenceDate);

    Real strike = arguments_.bondQuantity * (upfrontLegNpv + assetLegNpv - redemptionLegNpv) - fundingLegNpv;

    results_.value = PlainVanillaPayoff(arguments_.callPut, strike)(bondPrice);
    results_.additionalResults["bondPrice"] = bondPrice;
    results_.additionalResults["strike"] = strike;
    results_.additionalResults["fundingLegNpv"] = fundingLegNpv;
    results_.additionalResults["redemptionLegNpv"] = redemptionLegNpv * arguments_.bondQuantity;
    results_.additionalResults["assetLegNpv"] = assetLegNpv * arguments_.bondQuantity;
    results_.additionalResults["upfrontLegNpv"] = upfrontLegNpv * arguments_.bondQuantity;
    results_.additionalResults["bondQuantity"] = arguments_.bondQuantity;
}

} // namespace QuantExt
