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

#include <qle/pricingengines/midpointcdoengine.hpp>

#ifndef QL_PATCH_SOLARIS

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::cpu_timer;

namespace QuantExt {

void MidPointCDOEngine::calculate() const {

    cpu_timer timer;
    timer.start();

    Date today = Settings::instance().evaluationDate();

    results_.premiumValue = 0.0;
    results_.protectionValue = 0.0;
    results_.upfrontPremiumValue = 0.0;
    results_.error = 0;
    results_.expectedTrancheLoss.clear();
    // todo Should be remaining when considering realized loses
    results_.xMin = arguments_.basket->attachmentAmount();
    results_.xMax = arguments_.basket->detachmentAmount();
    results_.remainingNotional = results_.xMax - results_.xMin;
    const Real inceptionTrancheNotional = arguments_.basket->trancheNotional();

    // Upfront Flow NPV and accrual rebate NPV. Either we are on-the-run (no flow)
    // or we are forward start

    // date determining the probability survival so we have to pay
    // the upfront flows (did not knock out)
    Date refDate = discountCurve_->referenceDate();
    // Date effectiveProtectionStart = arguments_.protectionStart > refDate ? arguments_.protectionStart : refDate ;
    Probability nonKnockOut = 1.0; // FIXME

    Real upfPVO1 = 0.0;
    results_.upfrontPremiumValue = 0.0;
    if (arguments_.upfrontPayment && !arguments_.upfrontPayment->hasOccurred(refDate, includeSettlementDateFlows_)) {
        upfPVO1 = nonKnockOut * discountCurve_->discount(arguments_.upfrontPayment->date());
        results_.upfrontPremiumValue = upfPVO1 * arguments_.upfrontPayment->amount();
    }

    results_.accrualRebateValue = 0.;
    if (arguments_.accrualRebate && !arguments_.accrualRebate->hasOccurred(refDate, includeSettlementDateFlows_)) {
        results_.accrualRebateValue = nonKnockOut * discountCurve_->discount(arguments_.accrualRebate->date()) *
                                      arguments_.accrualRebate->amount();
    }

    // compute expected loss at the beginning of first relevant period
    Real zeroRecovery_e1 = 0, recovery_e1 = 0;
    // todo add includeSettlement date flows variable to engine.
    // RL: comment the following out, thows a negative time error
    // if (!arguments_.normalizedLeg[0]->hasOccurred(today))
    //     // Notice that since there might be a gap between the end of
    //     // acrrual and payment dates and today be in between
    //     // the tranche loss on that date might not be contingent but
    //     // realized:
    //     e1 = arguments_.basket->expectedTrancheLoss(
    //         QuantLib::ext::dynamic_pointer_cast<Coupon>(
    //             arguments_.normalizedLeg[0])->accrualStartDate());
    results_.expectedTrancheLoss.push_back(recovery_e1);
    //'e1'  should contain the existing loses.....? use remaining amounts?
    for (Size i = 0; i < arguments_.normalizedLeg.size(); i++) {
        if (arguments_.normalizedLeg[i]->hasOccurred(today)) {
            results_.expectedTrancheLoss.push_back(0.);
            continue;
        }
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(arguments_.normalizedLeg[i]);
        Date paymentDate = coupon->date();
        Date startDate = std::max(coupon->accrualStartDate(), discountCurve_->referenceDate());
        Date endDate = coupon->accrualEndDate();
        // we assume the loss within the period took place on this date:
        Date defaultDate = startDate + (endDate - startDate) / 2;

        Real zeroRecovery_e2 =
            arguments_.basket->expectedTrancheLoss(endDate, true); // zero recoveries for the coupon leg

        Real recovery_e2 =
            arguments_.basket->expectedTrancheLoss(endDate, false); // non-zero recovery for the default leg

        results_.expectedTrancheLoss.push_back(recovery_e2);
        results_.premiumValue += ((inceptionTrancheNotional - zeroRecovery_e2) / inceptionTrancheNotional) *
                                 coupon->amount() * discountCurve_->discount(paymentDate);

        // default flows:
        Date protectionPaymentDate;
        if (arguments_.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atDefault) {
            protectionPaymentDate = defaultDate;
        } else if (arguments_.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd) {
            protectionPaymentDate = paymentDate;
        } else if (arguments_.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atMaturity) {
            protectionPaymentDate = arguments_.maturity;
        } else {
            QL_FAIL("protectionPaymentTime not handled");
        }
        const Real discount = discountCurve_->discount(protectionPaymentDate);

        // Accrual removed till the argument flag is implemented
        // pays accrued on defaults' date
        if (arguments_.settlesAccrual)
            results_.premiumValue += coupon->accruedAmount(defaultDate) * discount *
                                     (zeroRecovery_e2 - zeroRecovery_e1) / inceptionTrancheNotional;

        results_.protectionValue += discount * (recovery_e2 - recovery_e1);
        /* use it in a future version for coherence with the integral engine
         * arguments_.leverageFactor;
         */
        recovery_e1 = recovery_e2;
        zeroRecovery_e1 = zeroRecovery_e2;
    }

    /* use it in a future version for coherence with the integral engine
        arguments_.leverageFactor * ;
    */
    if (arguments_.side == Protection::Buyer) {
        results_.premiumValue *= -1;
        results_.upfrontPremiumValue *= -1;
    } else {
        results_.protectionValue *= -1;
        results_.accrualRebateValue *= -1;
    }
    results_.value =
        results_.premiumValue + results_.protectionValue + results_.upfrontPremiumValue + results_.accrualRebateValue;
    results_.errorEstimate = Null<Real>();
    // Fair spread GIVEN the upfront
    Real fairSpread = 0.;
    if (results_.premiumValue != 0.0) {
        fairSpread = -(results_.protectionValue + results_.upfrontPremiumValue + results_.accrualRebateValue) *
                     arguments_.runningRate / results_.premiumValue;
    }

    timer.stop();

    results_.additionalResults["attachment"] = arguments_.basket->attachmentRatio();
    results_.additionalResults["detachment"] = arguments_.basket->detachmentRatio();
    results_.additionalResults["fixedRate"] = arguments_.runningRate;
    results_.additionalResults["fairSpread"] = fairSpread;
    results_.additionalResults["upfrontPremium"] = arguments_.upfrontPayment->amount();
    Real correlation = arguments_.basket->correlation();
    if (correlation != Null<Real>())
        results_.additionalResults["correlation"] = correlation;
    results_.additionalResults["upfrontPremiumNPV"] = results_.upfrontPremiumValue;
    results_.additionalResults["premiumLegNPV"] = results_.premiumValue;
    results_.additionalResults["accrualRebateNPV"] = results_.accrualRebateValue;
    results_.additionalResults["protectionLegNPV"] = results_.protectionValue;
    results_.additionalResults["calculationTime"] = timer.elapsed().wall * 1e-9;
}

} // namespace QuantExt

#endif
