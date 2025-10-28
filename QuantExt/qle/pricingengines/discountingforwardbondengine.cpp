/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/instruments/cashflowresults.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>

#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>
#include <boost/tuple/tuple.hpp>

namespace QuantExt {

DiscountingForwardBondEngine::DiscountingForwardBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                           const Handle<DefaultProbabilityTermStructure>& creditCurve,
                                                           const Handle<Quote>& conversionFactor)
    : discountCurve_(discountCurve), creditCurve_(creditCurve), conversionFactor_(conversionFactor) {
    registerWith(discountCurve_);
    registerWith(conversionFactor_);
}

void DiscountingForwardBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "DiscountingForwardBondEngine::calculate(): discountCurve is empty.");
    QL_REQUIRE(arguments_.lockRate == Null<Real>() || arguments_.knockOut,
               "DiscountingForwardBondEngine::calculate(): lock is given, expected knockOut to be true");

    Date bondSettlementDate = arguments_.underlying->settlementDate(arguments_.fwdMaturityDate);
    Real accruedAmount = arguments_.underlying->accruedAmount(bondSettlementDate) / 100.0;
    Real accruedAmountDollar =
        accruedAmount * arguments_.underlying->notional(bondSettlementDate) * arguments_.bondNotional;

    std::vector<CashFlowResults> cfResults;

    Real forwardPrice = QuantExt::forwardPrice(arguments_.underlying, arguments_.fwdMaturityDate,
                                               arguments_.fwdMaturityDate, arguments_.knockOut, &cfResults)
                            .second *
                        arguments_.bondNotional;

    if (!conversionFactor_.empty()) {
        forwardPrice = (forwardPrice - accruedAmountDollar) / conversionFactor_->value() + accruedAmountDollar;
    }

    Real strikeAmount;

    if (arguments_.lockRate == Null<Real>()) {

        // plain vanilla payoff

        strikeAmount = arguments_.strikeAmount + (arguments_.settlementDirty ? 0.0 : accruedAmountDollar);

    } else {

        // t-lock payoff using hardcoded conventions (compounded / semi annual) for treasury bonds here
        Real yield = QuantExt::yield(arguments_.underlying,
                                     forwardPrice / arguments_.bondNotional /
                                         arguments_.underlying->notional(bondSettlementDate) * 100.0,
                                     arguments_.lockRateDayCounter, Compounded, Semiannual, arguments_.fwdMaturityDate,
                                     bondSettlementDate, 1E-10, 100, 0.05, QuantLib::Bond::Price::Dirty);

        Real dv01, modDur = Null<Real>();
        if (arguments_.dv01 != Null<Real>()) {
            dv01 = arguments_.dv01;
        } else {
            modDur = QuantExt::duration(arguments_.underlying, yield, arguments_.lockRateDayCounter, Compounded,
                                        Semiannual, Duration::Modified, arguments_.fwdMaturityDate, bondSettlementDate);
            dv01 =
                forwardPrice / arguments_.bondNotional / arguments_.underlying->notional(bondSettlementDate) * modDur;

        }

        forwardPrice = yield * dv01 * arguments_.underlying->notional(bondSettlementDate) * arguments_.bondNotional;
        strikeAmount =
            arguments_.lockRate * dv01 * arguments_.underlying->notional(bondSettlementDate) * arguments_.bondNotional;

        results_.additionalResults["forwardYield"] = yield;
        results_.additionalResults["forwardDv01"] = dv01;
        results_.additionalResults["lockRate"] = arguments_.lockRate;

    }

    Date effSettlementDate = arguments_.isPhysicallySettled ? bondSettlementDate : arguments_.fwdSettlementDate;

    Real effDiscount = discountCurve_->discount(effSettlementDate);
    if (arguments_.knockOut)
        effDiscount *= creditCurve_.empty() ? 1.0 : creditCurve_->survivalProbability(arguments_.fwdMaturityDate);

    results_.value = (arguments_.isLong ? 1.0 : -1.0) * (forwardPrice - strikeAmount) * effDiscount;

    Real effCmpPayment = arguments_.compensationPayment == Null<Real>() ? 0.0 : arguments_.compensationPayment;
    Date effCmpPaymentDate = arguments_.compensationPaymentDate == Null<Date>() ? arguments_.fwdMaturityDate
                                                                                : arguments_.compensationPaymentDate;
    if (effCmpPaymentDate > discountCurve_->referenceDate()) {
        results_.value +=
            (arguments_.isLong ? -1.0 : 1.0) * effCmpPayment * discountCurve_->discount(effCmpPaymentDate);
        results_.additionalResults["compensationPayment"] = effCmpPayment;
        results_.additionalResults["compensationPaymentDate"] = effCmpPaymentDate;
        results_.additionalResults["compensationPaymentDiscount"] = discountCurve_->discount(effCmpPaymentDate);
    }

    // relabel cashflow results from underlying bond, scale with bond notional

    for (auto& r : cfResults) {
        r.type = "Bond_" + r.type;
        if (r.amount != Null<Real>())
            r.amount *= arguments_.bondNotional;
        if (r.notional != Null<Real>())
            r.notional *= arguments_.bondNotional;
        if (r.presentValue != Null<Real>())
            r.presentValue *= arguments_.bondNotional;
        if (r.presentValueBase != Null<Real>())
            r.presentValueBase *= arguments_.bondNotional;
    }

    // add cashflow for forward

    CashFlowResults tmp;
    tmp.amount = results_.value / effDiscount;
    tmp.payDate = effSettlementDate;
    tmp.legNumber = 1;
    tmp.type = "ForwardPayoff";
    tmp.discountFactor = effDiscount;
    cfResults.push_back(tmp);

    // set add results and exit

    results_.additionalResults["cashFlowResults"] = cfResults;

    results_.additionalResults["forwardPrice"] = forwardPrice;
    results_.additionalResults["strikeAmount"] = strikeAmount;
    results_.additionalResults["accruedAmount"] = accruedAmountDollar;
    results_.additionalResults["position"] = arguments_.isLong ? "long" : "short";
    results_.additionalResults["settlement"] = arguments_.isPhysicallySettled ? "physical" : "cash";
    results_.additionalResults["forwardContractDiscountFactor"] = discountCurve_->discount(effSettlementDate);
    results_.additionalResults["fowardContractSettlementDate"] = effSettlementDate;
    results_.additionalResults["bondSettlementDate"] = bondSettlementDate;
    results_.additionalResults["contractNotional"] = arguments_.bondNotional;
    results_.additionalResults["effectiveNotional"] =
        arguments_.underlying->notional(bondSettlementDate) * arguments_.bondNotional;
}

std::pair<QuantLib::Real, QuantLib::Real> DiscountingForwardBondEngine::forwardPrice(
    const QuantLib::Date& forwardNpvDate, const QuantLib::Date& settlementDate, const bool conditionalOnSurvival,
    std::vector<CashFlowResults>* const cfResults, QuantLib::Leg* const expectedCashflows) const {
    return QuantExt::forwardPrice(arguments_.underlying, forwardNpvDate, settlementDate, conditionalOnSurvival,
                                  cfResults, expectedCashflows);
}

} // namespace QuantExt
