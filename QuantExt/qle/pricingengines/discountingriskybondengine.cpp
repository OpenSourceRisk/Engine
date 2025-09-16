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

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
    const Handle<Quote>& recoveryRate, const Handle<Quote>& securitySpread, Period timestepPeriod,
    boost::optional<bool> includeSettlementDateFlows, const bool includePastCashflows,
    const Handle<YieldTermStructure>& incomeCurve, const bool conditionalOnSurvival, const bool spreadOnIncome)
    : defaultCurve_(defaultCurve), recoveryRate_(recoveryRate), securitySpread_(securitySpread),
      timestepPeriod_(timestepPeriod), includeSettlementDateFlows_(includeSettlementDateFlows),
      includePastCashflows_(includePastCashflows), incomeCurve_(incomeCurve),
      conditionalOnSurvival_(conditionalOnSurvival), spreadOnIncome_(spreadOnIncome) {
    discountCurve_ = securitySpread_.empty()
                         ? discountCurve
                         : Handle<YieldTermStructure>(
                               QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    incomeCurve_ = incomeCurve.empty() ? discountCurve_ : incomeCurve;
    incomeCurve_ = securitySpread_.empty() || !spreadOnIncome_
                       ? incomeCurve_
                       : Handle<YieldTermStructure>(
                             QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(incomeCurve_, securitySpread));
    registerWith(discountCurve_);
    registerWith(incomeCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
}

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<Quote>& securitySpread, Period timestepPeriod,
                                                       boost::optional<bool> includeSettlementDateFlows,
                                                       const Handle<YieldTermStructure>& incomeCurve,
                                                       const bool conditionalOnSurvival, const bool spreadOnIncome)
    : securitySpread_(securitySpread), timestepPeriod_(timestepPeriod),
      includeSettlementDateFlows_(includeSettlementDateFlows), incomeCurve_(incomeCurve),
      conditionalOnSurvival_(conditionalOnSurvival), spreadOnIncome_(spreadOnIncome) {
    discountCurve_ = securitySpread_.empty()
                         ? discountCurve
                         : Handle<YieldTermStructure>(
                               QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    incomeCurve_ = incomeCurve.empty() ? discountCurve_ : incomeCurve;
    incomeCurve_ = securitySpread_.empty() || !spreadOnIncome_
                       ? incomeCurve_
                       : Handle<YieldTermStructure>(
                             QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(incomeCurve_, securitySpread));
    registerWith(discountCurve_);
    registerWith(incomeCurve_);
    registerWith(securitySpread_);
}

void DiscountingRiskyBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    results_.valuationDate = (*discountCurve_)->referenceDate();

    // the npv as of today, excluding cashflows before the settlement date

    DiscountingRiskyBondEngine::BondNPVCalculationResults npvResults = calculateNpv(
        results_.valuationDate, arguments_.settlementDate, includeSettlementDateFlows_, conditionalOnSurvival_, true);

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

std::pair<Real, Real> DiscountingRiskyBondEngine::forwardPrice(const QuantLib::Date& forwardNpvDate,
                                                               const QuantLib::Date& settlementDate,
                                                               const bool conditionalOnSurvival,
                                                               std::vector<CashFlowResults>* const cfResults,
                                                               QuantLib::Leg* const expectedCashflows) const {
    auto res = calculateNpv(forwardNpvDate, settlementDate, includeSettlementDateFlows_, conditionalOnSurvival,
                            cfResults != nullptr);
    if (cfResults != nullptr)
        *cfResults = res.cashflowResults;
    return std::make_pair(res.npv + res.cashflowsBeforeSettlementValue, res.npv * res.compoundFactorSettlement);
}

DiscountingRiskyBondEngine::RecoveryContribution DiscountingRiskyBondEngine::recoveryContribution(
    const Real dfNpv, const Real spNpv, const Real effRecovery,
    const QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure>& effCreditCurve, const bool additionalResults,
    const Real nominal, const QuantLib::Date& startDate, const QuantLib::Date& endDate) const {
    RecoveryContribution result;
    if (startDate >= endDate)
        return result;
    Date defaultDate = startDate + (endDate - startDate) / 2;
    Probability P = effCreditCurve->defaultProbability(startDate, endDate) / spNpv;
    Real expectedRecoveryAmount = nominal * effRecovery;
    DiscountFactor recoveryDiscountFactor = discountCurve_->discount(defaultDate) / dfNpv;
    if (additionalResults && !close_enough(expectedRecoveryAmount * P * recoveryDiscountFactor, 0.0)) {
        CashFlowResults recoveryResult;
        recoveryResult.amount = expectedRecoveryAmount;
        recoveryResult.payDate = defaultDate;
        recoveryResult.currency = "";
        recoveryResult.discountFactor = P * recoveryDiscountFactor;
        recoveryResult.presentValue = recoveryResult.discountFactor * recoveryResult.amount;
        recoveryResult.type = "ExpectedRecovery";
        result.cashflowResults.push_back(recoveryResult);
    }
    result.value = expectedRecoveryAmount * P * recoveryDiscountFactor;
    return result;
}

DiscountingRiskyBondEngine::BondNPVCalculationResults
DiscountingRiskyBondEngine::calculateNpv(const Date& npvDate, const Date& settlementDate,
                                         boost::optional<bool> includeSettlementDateFlows,
                                         const bool conditionalOnSurvival, const bool additionalResults) const {

    bool includeRefDateFlows =
        includeSettlementDateFlows ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    DiscountingRiskyBondEngine::BondNPVCalculationResults result;

    auto effCreditCurve =
        defaultCurve_.empty()
            ? QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(npvDate, 0.0, discountCurve_->dayCounter())
            : defaultCurve_.currentLink();
    Rate effRecovery = recoveryRate_.empty() ? 0.0 : recoveryRate_->value();

    Real dfNpv = incomeCurve_->discount(npvDate);
    Real spNpv = conditionalOnSurvival ? effCreditCurve->survivalProbability(npvDate) : 1.0;
    Real dfSettl = incomeCurve_->discount(settlementDate);
    Real spSettl = effCreditCurve->survivalProbability(settlementDate);

    if (!conditionalOnSurvival)
        spSettl /= effCreditCurve->survivalProbability(npvDate);

    result.compoundFactorSettlement = (dfNpv * spNpv) / (dfSettl * spSettl);
    result.accruedAmountSettlement =
        CashFlows::accruedAmount(arguments_.cashflows, includeRefDateFlows, settlementDate);

    for (Size i = 0; i < arguments_.cashflows.size(); i++) {
        QuantLib::ext::shared_ptr<CashFlow> cf = arguments_.cashflows[i];

        if (cf->hasOccurred(npvDate, includeRefDateFlows)) {
            if (!includePastCashflows_ || !additionalResults)
                continue;
            CashFlowResults cfRes = populateCashFlowResultsFromCashflow(cf);
            result.cashflowResults.push_back(cfRes);
            continue;
        }

        DiscountFactor df = discountCurve_->discount(cf->date()) / dfNpv;
        Probability S = effCreditCurve->survivalProbability(cf->date()) / spNpv;

        Real tmp = cf->amount() * S * df;
        if (!cf->hasOccurred(settlementDate, includeRefDateFlows))
            result.npv += tmp;
        else
            result.cashflowsBeforeSettlementValue += tmp;

        if (additionalResults) {
            CashFlowResults cfRes = populateCashFlowResultsFromCashflow(cf);
            cfRes.discountFactor = S * df;
            cfRes.presentValue = cfRes.amount * cfRes.discountFactor;
            result.cashflowResults.push_back(cfRes);
        }

        if (auto coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf)) {
            auto rec =
                recoveryContribution(dfNpv, spNpv, effRecovery, effCreditCurve, additionalResults, coupon->nominal(),
                                     std::max(npvDate, coupon->accrualStartDate()), coupon->accrualEndDate());
            result.npv += rec.value;
            result.cashflowResults.insert(result.cashflowResults.end(), rec.cashflowResults.begin(),
                                          rec.cashflowResults.end());
        }
    }

    /* calculate recovery in case there are no coupons covering the lifetime of the bond */

    if (auto redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(arguments_.cashflows[0]);
        redemption && arguments_.cashflows.size() == 1) {
        Date startDate = npvDate;
        while (startDate < redemption->date()) {
            Date stepDate = startDate + timestepPeriod_;
            auto rec = recoveryContribution(dfNpv, spNpv, effRecovery, effCreditCurve, additionalResults,
                                            redemption->amount(), startDate, std::min(redemption->date(), stepDate));
            result.npv += rec.value;
            result.cashflowResults.insert(result.cashflowResults.end(), rec.cashflowResults.begin(),
                                          rec.cashflowResults.end());
            startDate = stepDate;
        }
    }

    /* If conditionalOnSurvival = false, we add the recovery contribution between reference date and npv date */

    if (!conditionalOnSurvival_) {
        Real recovery0 = 0.0;
        if (auto redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(arguments_.cashflows[0]);
            redemption && arguments_.cashflows.size() == 1) {
            Date startDate = discountCurve_->referenceDate();
            while (startDate < std::min(redemption->date(), npvDate)) {
                Date stepDate = startDate + timestepPeriod_;
                auto rec = recoveryContribution(1.0, 1.0, effRecovery, effCreditCurve, false, redemption->amount(),
                                                startDate, std::min(npvDate, redemption->date()));
                recovery0 += rec.value;
                startDate = stepDate;
            }
        } else {
            for (Size i = 0; i < arguments_.cashflows.size(); i++) {
                QuantLib::ext::shared_ptr<CashFlow> cf = arguments_.cashflows[i];
                if (auto coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf)) {
                    auto rec =
                        recoveryContribution(1.0, 1.0, effRecovery, effCreditCurve, false, coupon->nominal(),
                                             std::max(discountCurve_->referenceDate(), coupon->accrualStartDate()),
                                             std::min(npvDate, coupon->accrualEndDate()));
                    recovery0 += rec.value;
                }
            }
        }
        // compound recovery from reference date back to forwardDate, on reference curve
        result.npv += recovery0 / discountCurve_->discount(npvDate);
    }

    /* Return the result */

    return result;
}

} // namespace QuantExt
