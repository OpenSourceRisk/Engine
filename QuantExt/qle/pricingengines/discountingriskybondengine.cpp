/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG
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

#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/instruments/cashflowresults.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                                       const Handle<Quote>& recoveryRate,
                                                       const Handle<Quote>& securitySpread, Period timestepPeriod,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : defaultCurve_(defaultCurve), recoveryRate_(recoveryRate), securitySpread_(securitySpread),
      timestepPeriod_(timestepPeriod), includeSettlementDateFlows_(includeSettlementDateFlows) {
    discountCurve_ =
        securitySpread_.empty()
            ? discountCurve
            : Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    registerWith(discountCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
}

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<Quote>& securitySpread, Period timestepPeriod,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : securitySpread_(securitySpread), timestepPeriod_(timestepPeriod),
      includeSettlementDateFlows_(includeSettlementDateFlows) {
    discountCurve_ =
        securitySpread_.empty()
            ? discountCurve
            : Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    registerWith(discountCurve_);
    registerWith(securitySpread_);
}

void DiscountingRiskyBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    results_.valuationDate = (*discountCurve_)->referenceDate();

    // the npv as of today, excluding cashflows before the settlement date

    DiscountingRiskyBondEngine::BondNPVCalculationResults npvResults = calculateNpv(
        results_.valuationDate, arguments_.settlementDate, arguments_.cashflows, includeSettlementDateFlows_);

    // the results value is set to the npv as of today including the cashflows before settlement

    results_.value = npvResults.npv + npvResults.cashflowsBeforeSettlementValue;

    // the settlement value is excluding cashflows before the settlement date and compounded to the settlement date

    results_.settlementValue = npvResults.npv * npvResults.compoundFactorSettlement;

    // set a few more additional results
    results_.additionalResults["cashFlowResults"] = npvResults.cashflowResults;
    results_.additionalResults["securitySpread"] = securitySpread_.empty() ? 0.0 : securitySpread_->value();
    Date maturity = CashFlows::maturityDate(arguments_.cashflows);
    if (maturity > results_.valuationDate) {
        Real t = discountCurve_->timeFromReference(maturity);
        results_.additionalResults["maturityTime"] = t;
        results_.additionalResults["maturityDiscountFactor"] = discountCurve_->discount(t);
        results_.additionalResults["maturitySurvivalProb"] =
            defaultCurve_.empty() ? 1.0 : defaultCurve_->survivalProbability(t);
        results_.additionalResults["recoveryRate"] = recoveryRate_.empty() ? 0.0 : recoveryRate_->value();
    }
}

DiscountingRiskyBondEngine::BondNPVCalculationResults
DiscountingRiskyBondEngine::calculateNpv(const Date& npvDate, const Date& settlementDate, const Leg& cashflows,
                                         boost::optional<bool> includeSettlementDateFlows,
                                         const Handle<YieldTermStructure>& incomeCurve,
                                         const bool conditionalOnSurvival, const bool additionalResults) const {

    bool includeRefDateFlows =
        includeSettlementDateFlows ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    Real npvValue = 0.0;
    DiscountingRiskyBondEngine::BondNPVCalculationResults calculationResults;
    calculationResults.cashflowsBeforeSettlementValue = 0.0;

    // handle case where we wish to price simply with benchmark curve and scalar security spread
    // i.e. credit curve term structure (and recovery) have not been specified
    // we set the default probability and recovery rate to zero in this instance (issuer credit worthiness already
    // captured within security spread)
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> creditCurvePtr =
        defaultCurve_.empty() ? QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(npvDate, 0.0, discountCurve_->dayCounter())
                              : defaultCurve_.currentLink();
    Rate recoveryVal = recoveryRate_.empty() ? 0.0 : recoveryRate_->value();

    // compounding factors for npv date
    Real dfNpv = incomeCurve.empty() ? discountCurve_->discount(npvDate) : incomeCurve->discount(npvDate);
    Real spNpv = conditionalOnSurvival ? creditCurvePtr->survivalProbability(npvDate) : 1.0;

    // compound factors for settlement date
    Real dfSettl =
        incomeCurve.empty() ? discountCurve_->discount(settlementDate) : incomeCurve->discount(settlementDate);
    Real spSettl = creditCurvePtr->survivalProbability(settlementDate);
    if (!conditionalOnSurvival)
        spSettl /= creditCurvePtr->survivalProbability(npvDate);

    // effective compound factor to get settlement npv from npv date npv
    calculationResults.compoundFactorSettlement = (dfNpv * spNpv) / (dfSettl * spSettl);

    Size numCoupons = 0;
    bool hasLiveCashFlow = false;
    for (Size i = 0; i < cashflows.size(); i++) {
        QuantLib::ext::shared_ptr<CashFlow> cf = cashflows[i];
        if (cf->hasOccurred(npvDate, includeRefDateFlows))
            continue;
        hasLiveCashFlow = true;

        DiscountFactor df = discountCurve_->discount(cf->date()) / dfNpv;
        // Coupon value is discounted future payment times the survival probability
        Probability S = creditCurvePtr->survivalProbability(cf->date()) / spNpv;
        Real tmp = cf->amount() * S * df;
        if (!cf->hasOccurred(settlementDate, includeRefDateFlows))
            npvValue += tmp;
        else
            calculationResults.cashflowsBeforeSettlementValue += tmp;

        /* The amount recovered in the case of default is the recoveryrate*Notional*Probability of
           Default; this is added to the NPV value. For coupon bonds the coupon periods are taken
           as the timesteps for integrating over the probability of default.
        */
        if (additionalResults) {
            CashFlowResults cfRes = populateCashFlowResultsFromCashflow(cf);
            cfRes.discountFactor = S * df;
            cfRes.presentValue = cfRes.amount * cfRes.discountFactor;
            calculationResults.cashflowResults.push_back(cfRes);
        }

        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            numCoupons++;
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, endDate) / spNpv;
            Real expectedRecoveryAmount = coupon->nominal() * recoveryVal;
            DiscountFactor recoveryDiscountFactor = discountCurve_->discount(defaultDate) / dfNpv;
            if (additionalResults && !close_enough(expectedRecoveryAmount * P * recoveryDiscountFactor, 0.0)) {
                // Add a new flow for the expected recovery conditional on the default during
                CashFlowResults recoveryResult;
                recoveryResult.amount = expectedRecoveryAmount;
                recoveryResult.payDate = defaultDate;
                recoveryResult.currency = "";
                recoveryResult.discountFactor = P * recoveryDiscountFactor;
                recoveryResult.presentValue = recoveryResult.discountFactor * recoveryResult.amount;
                recoveryResult.type = "ExpectedRecovery";
                calculationResults.cashflowResults.push_back(recoveryResult);
            }
            npvValue += expectedRecoveryAmount * P * recoveryDiscountFactor;
        }
    }

    // the ql instrument might not yet be expired and still have not anything to value if
    // the npvDate > evaluation date
    if (!hasLiveCashFlow) {
        calculationResults.npv = 0.0;
        return calculationResults;
    }

    if (cashflows.size() > 1 && numCoupons == 0) {
        QL_FAIL("DiscountingRiskyBondEngine does not support bonds with multiple cashflows but no coupons");
    }

    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (cashflows.size() == 1) {
        QuantLib::ext::shared_ptr<Redemption> redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(cashflows[0]);
        if (redemption) {
            Date startDate = npvDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate) / spNpv;
                if (additionalResults) {
                    CashFlowResults recoveryResult;
                    recoveryResult.amount = redemption->amount() * recoveryVal;
                    recoveryResult.payDate = defaultDate;
                    recoveryResult.currency = "";
                    recoveryResult.discountFactor = P * discountCurve_->discount(defaultDate) / dfNpv;
                    recoveryResult.presentValue = recoveryResult.discountFactor * recoveryResult.amount;
                    recoveryResult.type = "ExpectedRecovery";
                    calculationResults.cashflowResults.push_back(recoveryResult);
                }
                npvValue += redemption->amount() * recoveryVal * P * discountCurve_->discount(defaultDate) / dfNpv;
                startDate = stepDate;
            }
        }
    }
    calculationResults.npv = npvValue;
    return calculationResults;
} // namespace QuantExt
} // namespace QuantExt
