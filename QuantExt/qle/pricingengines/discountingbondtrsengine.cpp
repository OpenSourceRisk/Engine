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

#include <qle/cashflows/bondtrscashflow.hpp>
#include <qle/pricingengines/discountingbondtrsengine.hpp>
#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>

namespace QuantExt {

namespace {
std::string ccyStr(const Currency& c) {
    if (c.empty())
        return "NA";
    else
        return c.code();
}
} // namespace

DiscountingBondTRSEngine::DiscountingBondTRSEngine(const Handle<YieldTermStructure>& discountCurve)
    : discountCurve_(discountCurve) {
    registerWith(discountCurve_);
}

void DiscountingBondTRSEngine::calculate() const {

    Date today = Settings::instance().evaluationDate();

    Handle<Quote> bondSpread = arguments_.bondIndex->securitySpread();
    Handle<Quote> bondRecoveryRate = arguments_.bondIndex->recoveryRate();
    Handle<YieldTermStructure> bondReferenceYieldCurve =
        bondSpread.empty() ? arguments_.bondIndex->discountCurve()
                           : Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                                 arguments_.bondIndex->discountCurve(), bondSpread));
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> bondDefaultCurve =
        arguments_.bondIndex->defaultCurve().empty()
            ? QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(today, 0.0, Actual365Fixed())
            : arguments_.bondIndex->defaultCurve().currentLink();
    Rate recoveryVal =
        arguments_.bondIndex->recoveryRate().empty() ? 0.0 : arguments_.bondIndex->recoveryRate()->value();

    // 1 initialise additional result vectors

    std::vector<QuantExt::CashFlowResults> cfResults;
    std::vector<Date> returnStartDates, returnEndDates;
    std::vector<Real> returnFxStarts, returnFxEnds, returnBondStarts, returnBondEnds, returnBondNotionals;
    std::vector<Date> bondCashflowOriginalPayDates, bondCashflowReturnPayDates, bondCashflowFxFixingDate;
    std::vector<Real> bondCashflows, bondCashflowFxRate, bondCashflowSurvivalProbability;

    // 2 do some checks on data

    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    QL_REQUIRE(!arguments_.bondIndex->conditionalOnSurvival(),
               "DiscountingBondTRSEngine::calculate(): bondIndex should not be computed with conditionalOnSurvival = "
               "true in this engine");

    Real mult = arguments_.payTotalReturnLeg ? -1.0 : 1.0;

    // 3 handle funding leg(s) (leg #2, 3, ...)

    Real fundingLeg = 0.0;
    Size fundingLegNo = 2;
    for (auto const& l : arguments_.fundingLeg) {
        fundingLeg += CashFlows::npv(l, **discountCurve_, false);
        for (auto const& c : l) {
            if (c->hasOccurred(today))
                continue;
            cfResults.emplace_back();
            cfResults.back().amount = -mult * c->amount();
            cfResults.back().payDate = c->date();
            cfResults.back().currency = ccyStr(arguments_.fundingCurrency);
            cfResults.back().legNumber = fundingLegNo;
            cfResults.back().type = "Funding";
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                cfResults.back().rate = cpn->rate();
                cfResults.back().accrualPeriod = cpn->accrualPeriod();
                cfResults.back().accrualStartDate = cpn->accrualStartDate();
                cfResults.back().accrualEndDate = cpn->accrualEndDate();
                cfResults.back().accruedAmount = cpn->accruedAmount(today);
                cfResults.back().notional = cpn->nominal();
            }
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                cfResults.back().fixingDate = cpn->fixingDate();
                cfResults.back().fixingValue = cpn->index()->fixing(cpn->fixingDate());
            }
        }
        ++fundingLegNo;
    }

    // 4 handle total return leg (leg #0)

    Real returnLeg = CashFlows::npv(arguments_.returnLeg, **discountCurve_, false);

    for (auto const& c : arguments_.returnLeg) {
        if (c->hasOccurred(today))
            continue;
        cfResults.emplace_back();
        cfResults.back().amount = mult * c->amount();
        cfResults.back().payDate = c->date();
        cfResults.back().currency = ccyStr(arguments_.fundingCurrency);
        cfResults.back().legNumber = 0;
        cfResults.back().type = "Return";
        if (auto bc = QuantLib::ext::dynamic_pointer_cast<BondTRSCashFlow>(c)) {
            cfResults.back().fixingDate = bc->fixingEndDate();
            cfResults.back().fixingValue = bc->assetEnd();
            cfResults.back().accrualStartDate = bc->fixingStartDate();
            cfResults.back().accrualEndDate = bc->fixingEndDate();
            cfResults.back().notional = bc->notional();
            returnStartDates.push_back(bc->fixingStartDate());
            returnEndDates.push_back(bc->fixingEndDate());
            returnFxStarts.push_back(bc->fxStart());
            returnFxEnds.push_back(bc->fxEnd());
            returnBondStarts.push_back(bc->assetStart());
            returnBondEnds.push_back(bc->assetEnd());
            returnBondNotionals.push_back(bc->notional());
        }
    }

    // 5 handle bond cashflows (leg #1)

    QuantLib::ext::shared_ptr<Bond> bd = arguments_.bondIndex->bond();

    Date start = bd->settlementDate(arguments_.valuationDates.front());
    Date end = bd->settlementDate(arguments_.valuationDates.back());

    Real bondPayments = 0.0, bondRecovery = 0.0;
    bool hasLiveCashFlow = false;
    Size numCoupons = 0;

    

    for (Size i = 0; i < bd->cashflows().size(); i++) {

        // 5a skip bond cashflows that are outside the total return valuation schedule

        if (bd->cashflows()[i]->date() <= start || bd->cashflows()[i]->date() > end)
            continue;

        // 5b determine bond cf pay date

        Date bondFlowPayDate;
        Date bondFlowValuationDate;
        bool paymentAfterMaturityButWithinBondSettlement =
            bd->cashflows()[i]->date() > arguments_.valuationDates.back() && bd->cashflows()[i]->date() <= end;
        if (arguments_.payBondCashFlowsImmediately || paymentAfterMaturityButWithinBondSettlement) {
            bondFlowPayDate = bd->cashflows()[i]->date();
            bondFlowValuationDate = bd->cashflows()[i]->date();
        } else {
            const auto& payDates = arguments_.paymentDates;
            auto nextPayDate = std::lower_bound(payDates.begin(), payDates.end(), bd->cashflows()[i]->date());
            QL_REQUIRE(nextPayDate != payDates.end(), "DiscountingBondTRSEngine::calculate(): unexpected, could "
                                                      "not determine next pay date for bond cashflow date "
                                                          << bd->cashflows()[i]);
            bondFlowPayDate = *nextPayDate;

            const auto& valDates = arguments_.valuationDates;
            auto nextValDate = std::upper_bound(valDates.begin(), valDates.end(), bondFlowPayDate);

            if (nextValDate == valDates.begin()) {
                nextValDate = valDates.end();
            } else {
                nextValDate--;
            }

            QL_REQUIRE(nextValDate != valDates.end(), "DiscountingBondTRSEngine::calculate(): unexpected, could "
                                                      "not determine next valuation date for bond cashflow date "
                                                          << bondFlowPayDate);
            bondFlowValuationDate = *nextValDate;
        }

        // 5c skip cashflows that are paid <= today

        if (bondFlowPayDate <= today)
            continue;

        hasLiveCashFlow = true;

        // 5d determine survivial prob S and fx conversion rate for bond cashflow

        Probability S = bondDefaultCurve->survivalProbability(bondFlowPayDate);
        // FIXME which fixing date should we use for the fx conversion
        Date fxFixingDate = bondFlowValuationDate;
        if (arguments_.fxIndex)
            fxFixingDate = arguments_.fxIndex->fixingCalendar().adjust(fxFixingDate, Preceding);
        Real fx = arguments_.fxIndex ? arguments_.fxIndex->fixing(fxFixingDate) : 1.0;

        // 5e set bond cashflow and additional results

        cfResults.emplace_back();
        cfResults.back().amount = mult * bd->cashflows()[i]->amount() * fx * arguments_.bondNotional;
        cfResults.back().discountFactor = discountCurve_->discount(bondFlowPayDate) * S;
        cfResults.back().payDate = bondFlowPayDate;
        cfResults.back().currency = ccyStr(arguments_.fundingCurrency);
        cfResults.back().legNumber = 1;
        cfResults.back().type = "BondCashFlowReturn";
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[i])) {
            cfResults.back().rate = cpn->rate();
            cfResults.back().accrualPeriod = cpn->accrualPeriod();
            cfResults.back().accrualStartDate = cpn->accrualStartDate();
            cfResults.back().accrualEndDate = cpn->accrualEndDate();
            cfResults.back().accruedAmount = cpn->accruedAmount(today);
            cfResults.back().notional = cpn->nominal();
        }
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(bd->cashflows()[i])) {
            cfResults.back().fixingDate = cpn->fixingDate();
            cfResults.back().fixingValue = cpn->index()->fixing(cpn->fixingDate());
        }

        bondCashflows.push_back(mult * bd->cashflows()[i]->amount() * arguments_.bondNotional);
        bondCashflowOriginalPayDates.push_back(bd->cashflows()[i]->date());
        bondCashflowReturnPayDates.push_back(bondFlowPayDate);
        bondCashflowFxRate.push_back(fx);
        bondCashflowFxFixingDate.push_back(fxFixingDate);
        bondCashflowSurvivalProbability.push_back(S);

        // 5f bond cashflow npv contribution

        // Real spreadFactor = exp(- bondSpread->value() * discountCurve_->timeFromReference(bondFlowPayDate));
        bondPayments +=
            bd->cashflows()[i]->amount() * S * discountCurve_->discount(bondFlowPayDate) * fx; // * spreadFactor;

        // 5g bond cashflow recovery contribution

        if (auto coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[i])) {
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= start && start <= endDate) ? start : startDate;
            if (effectiveStartDate < today)
                effectiveStartDate = today;
            if (endDate > effectiveStartDate) {
                Probability P = bondDefaultCurve->defaultProbability(effectiveStartDate, endDate);
                Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
                // FIXME which fixing date should we use for the fx conversion?
                Real fx = arguments_.fxIndex ? arguments_.fxIndex->fixing(arguments_.fxIndex->fixingCalendar().adjust(
                                                   coupon->date(), Preceding))
                                             : 1.0;
                bondRecovery += coupon->nominal() * recoveryVal * P * discountCurve_->discount(defaultDate) * fx;
            }
            ++numCoupons;
        }

    } // loop over bond cashflows

    // 5h multiply bond payments and recovery contributions by bond notional
    bondPayments *= arguments_.bondNotional;
    bondRecovery *= arguments_.bondNotional;

    // 5i bond recovery value (special treatment for zero bonds)

    if (hasLiveCashFlow) {
        if (bd->cashflows().size() > 1 && numCoupons == 0) {
            QL_FAIL("DiscountingBondTRSEngine: no support of bonds with multiple cashflows but no coupons");
        }
        /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
           maturity. The timestepPeriod specified is used as provide the steps for the integration. This only
           applies to bonds with 1 cashflow, identified as a final redemption payment. */
        if (bd->cashflows().size() == 1) {
            QuantLib::ext::shared_ptr<Redemption> redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
            if (redemption) {
                Date startDate = (start < today ? today : start);
                while (startDate < redemption->date()) {
                    Date stepDate = startDate + 1 * Months; // hardcoded period
                    Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                    Date defaultDate = startDate + (endDate - startDate) / 2;
                    Probability P = bondDefaultCurve->defaultProbability(startDate, endDate);
                    // FIXME which fixing date should we use for the fx conversion?
                    Real fx = arguments_.fxIndex
                                  ? arguments_.fxIndex->fixing(
                                        arguments_.fxIndex->fixingCalendar().adjust(redemption->date(), Preceding))
                                  : 1.0;
                    bondRecovery += redemption->amount() * recoveryVal * P * discountCurve_->discount(defaultDate) * fx;
                    startDate = stepDate;
                }
            }
        }
    }

    // 6 set results

    results_.value = mult * (returnLeg + bondPayments + bondRecovery - fundingLeg);

    results_.additionalResults["returnLegNpv"] = mult * (returnLeg + bondPayments + bondRecovery);
    results_.additionalResults["returnLegNpvReturnPaymentsContribtion"] = mult * returnLeg;
    results_.additionalResults["returnLegNpvBondPaymentsContribtion"] = mult * bondPayments;
    results_.additionalResults["returnLegNpvBondRecoveryContribution"] = mult * bondRecovery;
    results_.additionalResults["fundingLegNpv"] = -mult * fundingLeg;

    results_.additionalResults["cashFlowResults"] = cfResults;

    results_.additionalResults["returnStartDate"] = returnStartDates;
    results_.additionalResults["returnEndDate"] = returnEndDates;
    results_.additionalResults["returnFxStart"] = returnFxStarts;
    results_.additionalResults["returnFxEnd"] = returnFxEnds;
    results_.additionalResults["returnBondStart"] = returnBondStarts;
    results_.additionalResults["returnBondEnd"] = returnBondEnds;

    results_.additionalResults["bondCashflow"] = bondCashflows;
    results_.additionalResults["bondCashflowOriginalPayDate"] = bondCashflowOriginalPayDates;
    results_.additionalResults["bondCashflowReturnPayDate"] = bondCashflowReturnPayDates;
    results_.additionalResults["bondCashflowFxRate"] = bondCashflowFxRate;
    results_.additionalResults["bondCashflowFxFixingDate"] = bondCashflowFxFixingDate;
    results_.additionalResults["bondCashflowSurvivalProbability"] = bondCashflowSurvivalProbability;

    results_.additionalResults["bondNotional"] = returnBondNotionals;
    results_.additionalResults["bondCurrency"] = ccyStr(arguments_.bondCurrency);
    results_.additionalResults["returnCurrency"] = ccyStr(arguments_.fundingCurrency);

    results_.additionalResults["bondCleanPrice"] = arguments_.bondIndex->bond()->cleanPrice();
    results_.additionalResults["bondDirtyPrice"] = arguments_.bondIndex->bond()->dirtyPrice();
    results_.additionalResults["bondSpread"] = bondSpread->value();
    results_.additionalResults["bondRecovery"] = recoveryVal;
}

} // namespace QuantExt
