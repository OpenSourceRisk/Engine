/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/discountingcreditlinkedswapengine.hpp>

#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/timegrid.hpp>

#include <iostream>

namespace QuantExt {

using namespace QuantLib;

DiscountingCreditLinkedSwapEngine::DiscountingCreditLinkedSwapEngine(
    const Handle<YieldTermStructure>& irCurve, const Handle<DefaultProbabilityTermStructure>& creditCurve,
    const Handle<Quote>& marketRecovery, const Size timeStepsPerYear, const bool generateAdditionalResults)
    : irCurve_(irCurve), creditCurve_(creditCurve), marketRecovery_(marketRecovery),
      timeStepsPerYear_(timeStepsPerYear), generateAdditionalResults_(generateAdditionalResults) {
    registerWith(irCurve_);
    registerWith(creditCurve_);
    registerWith(marketRecovery_);
}

void DiscountingCreditLinkedSwapEngine::calculate() const {
    QL_REQUIRE(!irCurve_.empty(), "DiscountingCreditLinkedSwapEngine::calculate(): ir curve is empty");
    QL_REQUIRE(!creditCurve_.empty(), "DiscountingCreditLinkedSwapEngine::calculate(): ir curve is empty");
    QL_REQUIRE(!marketRecovery_.empty(), "DiscountingCreditLinkedSwapEngine::calculate(): market recovery is empty");

    Date today = Settings::instance().evaluationDate();
    Real npv_independent = 0.0, npv_contingent = 0.0, default_accrual_contingent = 0.0, npv_default_payments = 0.0,
         npv_recovery_payments = 0.0;

    std::vector<QuantExt::CashFlowResults> cfResults;

    // handle the independent and contingent payments (including an accrual settlement for the latter, if active)

    for (Size i = 0; i < arguments_.legs.size(); ++i) {

        Real multiplier = (arguments_.legPayers[i] ? -1.0 : 1.0);

        switch (arguments_.legTypes[i]) {
        case CreditLinkedSwap::LegType::IndependentPayments: {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() <= today)
                    continue;
                npv_independent += multiplier * c->amount() * irCurve_->discount(c->date());
                if (generateAdditionalResults_) {
                    cfResults.push_back(
                        standardCashFlowResults(c, multiplier, "Independent", 0, arguments_.currency, irCurve_));
                }
            }
            break;
        }

        case CreditLinkedSwap::LegType::ContingentPayments: {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() <= today)
                    continue;
                Real dsc = irCurve_->discount(c->date());
                Real P = creditCurve_->survivalProbability(c->date());
                npv_contingent += multiplier * c->amount() * dsc * P;

                if (generateAdditionalResults_) {
                    cfResults.push_back(standardCashFlowResults(c, multiplier, "CreditLinked", 1, arguments_.currency));
                    cfResults.back().amount *= P;
                    cfResults.back().discountFactor = dsc;
                    cfResults.back().presentValue = cfResults.back().amount * dsc;
                }

                if (arguments_.settlesAccrual) {
                    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                        Date start = std::max(cpn->accrualStartDate(), today);
                        Date end = cpn->accrualEndDate();
                        if (end > today) {
                            Date mid = Date((start.serialNumber() + end.serialNumber()) / 2);
                            Real P = creditCurve_->survivalProbability(start) - creditCurve_->survivalProbability(end);
                            Real dsc = irCurve_->discount(mid);
                            default_accrual_contingent += multiplier * cpn->accruedAmount(mid) * P * dsc;

                            if (generateAdditionalResults_) {
                                cfResults.push_back(CashFlowResults());
                                cfResults.back().amount = multiplier * cpn->accruedAmount(mid) * P;
                                cfResults.back().accrualStartDate = cpn->accrualStartDate();
                                cfResults.back().accrualEndDate = cpn->accrualEndDate();
                                cfResults.back().payDate = mid;
                                cfResults.back().currency = arguments_.currency.code();
                                cfResults.back().legNumber = 2;
                                cfResults.back().type = "CreditLinkedDefaultAccrual";
                                cfResults.back().discountFactor = dsc;
                                cfResults.back().presentValue = cfResults.back().amount * dsc;
                            }
                        }
                    }
                }
            }
        }

        default: {
            break;
        }
        }
    }

    // handle the default and recovery payments

    std::set<Real> defaultPaymentTimes, recoveryPaymentTimes, allTimes;
    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        if (arguments_.legTypes[i] == CreditLinkedSwap::LegType::DefaultPayments) {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() > today) {
                    defaultPaymentTimes.insert(creditCurve_->timeFromReference(c->date()));
                }
            }
        } else if (arguments_.legTypes[i] == CreditLinkedSwap::LegType::RecoveryPayments) {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() > today) {
                    recoveryPaymentTimes.insert(creditCurve_->timeFromReference(c->date()));
                }
            }
        }
    }

    std::vector<Real> defaultPaymentAmounts(defaultPaymentTimes.size() + 1, 0.0),
        recoveryPaymentAmounts(recoveryPaymentTimes.size() + 1, 0.0);
    std::vector<Date> defaultPeriodEndDates(defaultPaymentTimes.size() + 1, Null<Date>()),
        recoveryPeriodEndDates(recoveryPaymentTimes.size() + 1, Null<Date>());

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        Real multiplier = (arguments_.legPayers[i] ? -1.0 : 1.0);
        if (arguments_.legTypes[i] == CreditLinkedSwap::LegType::DefaultPayments) {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() > today) {
                    Size index = std::distance(defaultPaymentTimes.begin(),
                                               defaultPaymentTimes.find(creditCurve_->timeFromReference(c->date())));
                    QL_REQUIRE(
                        index < defaultPaymentTimes.size(),
                        "DiscountingCreditLinkedSwapEngine: internal error: default payment times index out of range");
                    defaultPaymentAmounts[index] += c->amount() * multiplier;
                    defaultPeriodEndDates[index] = c->date();
                }
            }
        } else if (arguments_.legTypes[i] == CreditLinkedSwap::LegType::RecoveryPayments) {
            for (auto const& c : arguments_.legs[i]) {
                if (c->date() > today) {
                    Size index = std::distance(recoveryPaymentTimes.begin(),
                                               recoveryPaymentTimes.find(creditCurve_->timeFromReference(c->date())));
                    QL_REQUIRE(
                        index < recoveryPaymentTimes.size(),
                        "DiscountingCreditLinkedSwapEngine: internal error: recovery payment times index out of range");
                    recoveryPaymentAmounts[index] += c->amount() * multiplier;
                    recoveryPeriodEndDates[index] = c->date();
                }
            }
        }
    }

    allTimes.insert(defaultPaymentTimes.begin(), defaultPaymentTimes.end());
    allTimes.insert(recoveryPaymentTimes.begin(), recoveryPaymentTimes.end());

    if (!allTimes.empty()) {
        TimeGrid grid(
            allTimes.begin(), allTimes.end(),
            std::max<Size>(1, static_cast<Size>(0.5 + static_cast<Real>(timeStepsPerYear_) * (*allTimes.rbegin()))));

        Real rr =
            arguments_.fixedRecoveryRate != Null<Real>() ? arguments_.fixedRecoveryRate : marketRecovery_->value();

        for (Size i = 0; i < grid.size() - 1; ++i) {
            Real t0 = grid[i];
            Real t1 = grid[i + 1];

            auto defaultPayTime = defaultPaymentTimes.lower_bound(t1);
            auto recoveryPayTime = recoveryPaymentTimes.lower_bound(t1);
            Size index_d = std::distance(defaultPaymentTimes.begin(), defaultPayTime);
            Size index_r = std::distance(recoveryPaymentTimes.begin(), recoveryPayTime);

            Real P = creditCurve_->survivalProbability(t0) - creditCurve_->survivalProbability(t1);

            Real dscDefault = 0.0, dscRecovery = 0.0;
            Date payDateDefault, payDateRecovery;
            if (arguments_.defaultPaymentTime == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault) {
                dscDefault = dscRecovery = irCurve_->discount(0.5 * (t0 + t1));
                // interpolate a date for display in the cashflow report, since we can not determine it exactly
                if (defaultPayTime != defaultPaymentTimes.end()) {
                    Date start = today;
                    Real ts = 0;
                    if (index_d > 0) {
                        start = defaultPeriodEndDates[index_d - 1];
                        ts = *(std::next(defaultPayTime, -1));
                    }
                    Date end = defaultPeriodEndDates[index_d];
                    payDateDefault =
                        start + static_cast<Size>(0.5 + static_cast<Real>(end.serialNumber() - start.serialNumber()) *
                                                            (t1 - ts) / (*defaultPayTime - ts));
                }
                if (recoveryPayTime != recoveryPaymentTimes.end()) {
                    Date start = today;
                    Real ts = 0;
                    if (index_r > 0) {
                        start = recoveryPeriodEndDates[index_r - 1];
                        ts = *(std::next(recoveryPayTime, -1));
                    }
                    Date end = recoveryPeriodEndDates[index_r];
                    payDateRecovery =
                        start + static_cast<Size>(0.5 + static_cast<Real>(end.serialNumber() - start.serialNumber()) *
                                                            (t1 - ts) / (*recoveryPayTime - ts));
                }
            } else if (arguments_.defaultPaymentTime ==
                       QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd) {
                if (defaultPayTime != defaultPaymentTimes.end()) {
                    dscDefault = irCurve_->discount(*defaultPayTime);
                    payDateDefault = defaultPeriodEndDates[index_d];
                }
                if (recoveryPayTime != recoveryPaymentTimes.end()) {
                    dscRecovery = irCurve_->discount(*recoveryPayTime);
                    payDateRecovery = recoveryPeriodEndDates[index_r];
                }
            } else if (arguments_.defaultPaymentTime ==
                       QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity) {
                dscDefault = dscRecovery = irCurve_->discount(arguments_.maturityDate);
                payDateDefault = payDateRecovery = arguments_.maturityDate;
            } else {
                QL_FAIL("DiscountingCreditLinkedSwapEngine: internal error: unhandled default payment time");
            }

            npv_default_payments += defaultPaymentAmounts[index_d] * P * (1.0 - rr) * dscDefault;
            npv_recovery_payments += recoveryPaymentAmounts[index_r] * P * rr * dscRecovery;

            if (generateAdditionalResults_) {
                if (!close_enough(defaultPaymentAmounts[index_d], 0.0)) {
                    cfResults.push_back(CashFlowResults());
                    cfResults.back().amount = defaultPaymentAmounts[index_d] * P * (1.0 - rr);
                    cfResults.back().payDate = payDateDefault;
                    cfResults.back().currency = arguments_.currency.code();
                    cfResults.back().legNumber = 3;
                    cfResults.back().type = "DefaultPayment";
                    cfResults.back().discountFactor = dscDefault;
                    cfResults.back().presentValue = cfResults.back().discountFactor * cfResults.back().amount;
                }
                if (!close_enough(recoveryPaymentAmounts[index_r], 0.0)) {
                    cfResults.push_back(CashFlowResults());
                    cfResults.back().amount = recoveryPaymentAmounts[index_r] * P * rr;
                    cfResults.back().payDate = payDateRecovery;
                    cfResults.back().currency = arguments_.currency.code();
                    cfResults.back().legNumber = 3;
                    cfResults.back().type = "RecoveryPayment";
                    cfResults.back().discountFactor = dscRecovery;
                    cfResults.back().presentValue = cfResults.back().discountFactor * cfResults.back().amount;
                }
            }
        }
    }

    // set results

    results_.value =
        npv_independent + npv_contingent + default_accrual_contingent + npv_default_payments + npv_recovery_payments;

    if (generateAdditionalResults_) {
        results_.additionalResults["npv_independent"] = npv_independent;
        results_.additionalResults["npv_credit_linked"] = npv_contingent;
        results_.additionalResults["npv_credit_linked_accruals"] = default_accrual_contingent;
        results_.additionalResults["npv_default_payments"] = npv_default_payments;
        results_.additionalResults["npv_recovery_payments"] = npv_recovery_payments;
        results_.additionalResults["cashFlowResults"] = cfResults;
    }

} // calculate()

} // namespace QuantExt
