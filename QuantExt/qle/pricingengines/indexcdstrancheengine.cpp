/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/indexcdstrancheengine.hpp>

#ifndef QL_PATCH_SOLARIS

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using boost::timer::cpu_timer;
using std::vector;

namespace QuantExt {

IndexCdsTrancheEngine::IndexCdsTrancheEngine(const Handle<YieldTermStructure>& discountCurve,
    boost::optional<bool> includeSettlementDateFlows)
    : discountCurve_(discountCurve),
      includeSettlementDateFlows_(includeSettlementDateFlows) {
    registerWith(discountCurve);
}

void IndexCdsTrancheEngine::calculate() const {

    cpu_timer timer;
    timer.start();

    // Upfront premium
    results_.upfrontPremiumValue = 0.0;
    Real upfrontPremiumAmount = arguments_.upfrontPayment->amount();
    if (arguments_.upfrontPayment && !arguments_.upfrontPayment->hasOccurred(
        discountCurve_->referenceDate(), includeSettlementDateFlows_)) {
        results_.upfrontPremiumValue = upfrontPremiumAmount *
            discountCurve_->discount(arguments_.upfrontPayment->date());
    }

    // Accrual rebate
    results_.accrualRebateValue = 0.0;
    if (arguments_.accrualRebate && !arguments_.accrualRebate->hasOccurred(
        discountCurve_->referenceDate(), includeSettlementDateFlows_)) {
        results_.accrualRebateValue = arguments_.accrualRebate->amount() *
            discountCurve_->discount(arguments_.accrualRebate->date());
    }

    // Accrual rebate current
    results_.accrualRebateCurrentValue = 0.0;
    if (arguments_.accrualRebateCurrent &&
        !arguments_.accrualRebateCurrent->hasOccurred(discountCurve_->referenceDate(), includeSettlementDateFlows_)) {
        results_.accrualRebateCurrentValue =
            discountCurve_->discount(arguments_.accrualRebateCurrent->date()) * arguments_.accrualRebateCurrent->amount();
    }

    // Final results, not updated below.
    // FD TODO: check again when testing tranches with existing losses.
    const auto& basket = arguments_.basket;
    QL_REQUIRE(basket, "IndexCdsTrancheEngine expects a non-null basket.");
    results_.xMin = basket->attachmentAmount();
    results_.xMax = basket->detachmentAmount();
    results_.remainingNotional = results_.xMax - results_.xMin;

    // Record the expected tranche loss up to inception and up to the end of each coupon period.
    // FD TODO: Recheck 0's for past coupons when testing tranches with existing losses. The loss model gives 
    //          accumulated losses so we should in theory be able to use these.
    vector<Real>& etls = results_.expectedTrancheLoss;
    etls.push_back(0.0);

    // Variables used in the loop below.
    Date today = Settings::instance().evaluationDate();
    results_.premiumValue = 0.0;
    results_.protectionValue = 0.0;
    Real inceptionTrancheNotional = arguments_.basket->trancheNotional();

    // Value the premium and protection leg.
    for (Size i = 0; i < arguments_.normalizedLeg.size(); i++) {

        // Zero expected loss on coupon end dates that have already occured.
        // FD TODO: check again when testing tranches with existing losses.
        if (arguments_.normalizedLeg[i]->hasOccurred(today)) {
            etls.push_back(0.0);
            continue;
        }

        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(arguments_.normalizedLeg[i]);
        QL_REQUIRE(coupon, "IndexCdsTrancheEngine expects leg to have Coupon cashflow type.");

        // Relevant dates with assumption that future defaults occur at midpoint of (remaining) coupon period.
        Date paymentDate = coupon->date();
        Date startDate = std::max(coupon->accrualStartDate(), today);
        Date endDate = coupon->accrualEndDate();
        Date defaultDate = startDate + (endDate - startDate) / 2;

        // Expected loss on the tranche up to the end of the current period.
        Real etl = basket->expectedTrancheLoss(endDate, arguments_.recoveryRate);

        // Update protection leg value
        results_.protectionValue += discountCurve_->discount(defaultDate) * (etl - etls.back());

        // Update the premium leg value. If settling accruals, which is standard, assume that losses are evenly 
        // distributed over the coupon period, as per Andersen, Sidenius, Basu Nov 2003 paper for example. If not 
        // settling accruals, just use the tranche notional at period end.
        Real effNtl = 0.0;
        if (arguments_.settlesAccrual) {
            effNtl = inceptionTrancheNotional - (etl + etls.back()) / 2.0;
        } else {
            effNtl = inceptionTrancheNotional - etl;
        }
        results_.premiumValue += (coupon->amount() / inceptionTrancheNotional) *
            effNtl * discountCurve_->discount(paymentDate);

        // Update the expected tranche loss results vector.
        etls.push_back(etl);
    }

    // Apply correct sign to each PV'ed quantity depending on whether buying or selling protection on the tranche.
    if (arguments_.side == Protection::Buyer) {
        results_.premiumValue *= -1;
        results_.upfrontPremiumValue *= -1;
        upfrontPremiumAmount *= -1;
    } else {
        results_.protectionValue *= -1;
        results_.accrualRebateValue *= -1;
        results_.accrualRebateCurrentValue *= -1;
    }

    // Final tranche NPV.
    results_.value = results_.premiumValue + results_.protectionValue +
        results_.upfrontPremiumValue + results_.accrualRebateValue;

    // Fair tranche spread.
    Real fairSpread = 0.0;
    if (results_.premiumValue != 0.0) {
        fairSpread = -(results_.protectionValue + results_.upfrontPremiumValue) *
                     arguments_.runningRate / (results_.premiumValue + results_.accrualRebateValue);
    }

    timer.stop();

    // Populate the additional results.
    results_.additionalResults["attachment"] = arguments_.basket->attachmentRatio();
    results_.additionalResults["detachment"] = arguments_.basket->detachmentRatio();
    results_.additionalResults["fixedRate"] = arguments_.runningRate;
    results_.additionalResults["fairSpread"] = fairSpread;
    results_.additionalResults["upfrontPremium"] = upfrontPremiumAmount;
    Real correlation = arguments_.basket->correlation();
    if (correlation != Null<Real>())
        results_.additionalResults["correlation"] = correlation;
    results_.additionalResults["upfrontPremiumNPV"] = results_.upfrontPremiumValue;
    results_.additionalResults["premiumLegNPV"] = results_.premiumValue;
    results_.additionalResults["accrualRebateNPV"] = results_.accrualRebateValue;
    results_.additionalResults["accrualRebateCurrentNPV"] = results_.accrualRebateCurrentValue;

    results_.additionalResults["protectionLegNPV"] = results_.protectionValue;
    results_.additionalResults["calculationTime"] = timer.elapsed().wall * 1e-9;
}

}

#endif
