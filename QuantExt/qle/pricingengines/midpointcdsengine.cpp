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

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008, 2009 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/midpointcdsengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

MidPointCdsEngine::MidPointCdsEngine(const Handle<DefaultProbabilityTermStructure>& probability, Real recoveryRate,
                                     const Handle<YieldTermStructure>& discountCurve,
                                     boost::optional<bool> includeSettlementDateFlows)
    : MidPointCdsEngineBase(discountCurve, includeSettlementDateFlows), probability_(probability),
      recoveryRate_(recoveryRate) {
    registerWith(discountCurve_);
    registerWith(probability_);
}

Real MidPointCdsEngine::survivalProbability(const Date& d) const { return probability_->survivalProbability(d); }

Real MidPointCdsEngine::defaultProbability(const Date& d1, const Date& d2) const {
    return probability_->defaultProbability(d1, d2);
}

Real MidPointCdsEngine::expectedLoss(const Date& defaultDate, const Date& d1, const Date& d2,
                                     const Real notional) const {
    return arguments_.claim->amount(defaultDate, notional, recoveryRate_) * probability_->defaultProbability(d1, d2);
}

void MidPointCdsEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "no discount term structure set");
    QL_REQUIRE(!probability_.empty(), "no probability term structure set");
    MidPointCdsEngineBase::calculate(probability_->referenceDate(), arguments_, results_);
}

void MidPointCdsEngineBase::calculate(const Date& refDate, const CreditDefaultSwap::arguments& arguments,
                                      CreditDefaultSwap::results& results) const {
    Date today = Settings::instance().evaluationDate();
    Date settlementDate = discountCurve_->referenceDate();

    // Upfront Flow NPV and accrual rebate NPV. Either we are on-the-run (no flow)
    // or we are forward start

    // date determining the probability survival so we have to pay
    // the upfront flows (did not knock out)
    Date effectiveProtectionStart = arguments.protectionStart > refDate ? arguments.protectionStart : refDate;
    Probability nonKnockOut = survivalProbability(effectiveProtectionStart);

    Real upfPVO1 = 0.0;
    results.upfrontNPV = 0.0;
    if (arguments.upfrontPayment &&
        !arguments.upfrontPayment->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        upfPVO1 = nonKnockOut * discountCurve_->discount(arguments.upfrontPayment->date());
        results.upfrontNPV = upfPVO1 * arguments.upfrontPayment->amount();
    }

    results.accrualRebateNPV = 0.;
    if (arguments.accrualRebate && !arguments.accrualRebate->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        results.accrualRebateNPV =
            nonKnockOut * discountCurve_->discount(arguments.accrualRebate->date()) * arguments.accrualRebate->amount();
    }

    results.couponLegNPV = 0.0;
    results.defaultLegNPV = 0.0;
    for (Size i = 0; i < arguments.leg.size(); ++i) {
        if (arguments.leg[i]->hasOccurred(settlementDate, includeSettlementDateFlows_))
            continue;

        boost::shared_ptr<FixedRateCoupon> coupon = boost::dynamic_pointer_cast<FixedRateCoupon>(arguments.leg[i]);

        // In order to avoid a few switches, we calculate the NPV
        // of both legs as a positive quantity. We'll give them
        // the right sign at the end.

        Date paymentDate = coupon->date(), startDate = coupon->accrualStartDate(), endDate = coupon->accrualEndDate();
        // this is the only point where it might not coincide
        if (i == 0)
            startDate = arguments.protectionStart;
        Date effectiveStartDate = (startDate <= today && today <= endDate) ? today : startDate;
        Date defaultDate = // mid-point
            effectiveStartDate + (endDate - effectiveStartDate) / 2;

        Probability S = survivalProbability(paymentDate);
        Probability P = defaultProbability(effectiveStartDate, endDate);

        // on one side, we add the fixed rate payments in case of
        // survival...
        results.couponLegNPV += S * coupon->amount() * discountCurve_->discount(paymentDate);
        // ...possibly including accrual in case of default.
        if (arguments.settlesAccrual) {
            if (arguments.paysAtDefaultTime) {
                results.couponLegNPV += P * coupon->accruedAmount(defaultDate) * discountCurve_->discount(defaultDate);
            } else {
                // pays at the end
                results.couponLegNPV += P * coupon->amount() * discountCurve_->discount(paymentDate);
            }
        }

        // on the other side, we add the payment in case of default.
        results.defaultLegNPV += expectedLoss(defaultDate, effectiveStartDate, endDate, arguments.notional) *
                                 discountCurve_->discount(arguments.paysAtDefaultTime ? defaultDate : paymentDate);
    }

    Real upfrontSign = 1.0;
    switch (arguments.side) {
    case Protection::Seller:
        results.defaultLegNPV *= -1.0;
        results.accrualRebateNPV *= -1.0;
        break;
    case Protection::Buyer:
        results.couponLegNPV *= -1.0;
        results.upfrontNPV *= -1.0;
        upfrontSign = -1.0;
        break;
    default:
        QL_FAIL("unknown protection side");
    }

    results.value = results.defaultLegNPV + results.couponLegNPV + results.upfrontNPV + results.accrualRebateNPV;
    results.errorEstimate = Null<Real>();

    if (results.couponLegNPV != 0.0) {
        results.fairSpread =
            -results.defaultLegNPV * arguments.spread / (results.couponLegNPV + results.accrualRebateNPV);
    } else {
        results.fairSpread = Null<Rate>();
    }

    Real upfrontSensitivity = upfPVO1 * arguments.notional;
    if (upfrontSensitivity != 0.0) {
        results.fairUpfront = -upfrontSign * (results.defaultLegNPV + results.couponLegNPV + results.accrualRebateNPV) /
                              upfrontSensitivity;
    } else {
        results.fairUpfront = Null<Rate>();
    }

    static const Rate basisPoint = 1.0e-4;

    if (arguments.spread != 0.0) {
        results.couponLegBPS = results.couponLegNPV * basisPoint / arguments.spread;
    } else {
        results.couponLegBPS = Null<Rate>();
    }

    if (arguments.upfront && *arguments.upfront != 0.0) {
        results.upfrontBPS = results.upfrontNPV * basisPoint / (*arguments.upfront);
    } else {
        results.upfrontBPS = Null<Rate>();
    }

} // MidPointCdsEngineBase::calculate()

} // namespace QuantExt
