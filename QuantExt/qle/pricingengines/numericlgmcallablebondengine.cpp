/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file fdlgmcallablebondengine.hpp */

#include <qle/pricingengines/numericlgmcallablebondengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/fdcallablebondevents.hpp>

namespace QuantExt {

NumericLgmCallableBondEngineBase::NumericLgmCallableBondEngineBase(
    const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver, const Size americanExerciseTimeStepsPerYear,
    const Handle<QuantLib::YieldTermStructure>& referenceCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : solver_(solver), americanExerciseTimeStepsPerYear_(americanExerciseTimeStepsPerYear),
      referenceCurve_(referenceCurve), discountingSpread_(discountingSpread), creditCurve_(creditCurve),
      incomeCurve_(incomeCurve), recoveryRate_(recoveryRate) {}

void NumericLgmCallableBondEngineBase::calculate() const {

    // 0 if there are no cashflows in the underlying bond, we do not calculate anyything

    if (instrArgs_->cashflows.empty())
        return;

    // 1 build the cashflow info

    enum class CashflowStatus { Open, Cached, Done };

    std::vector<NumericLgmMultiLegOptionEngine::CashflowInfo> cashflows;
    std::vector<CashflowStatus> cashflowStatus;

    for (Size i = 0; i < instrArgs_->cashflows.size(); ++i) {
        cashflows.push_back(NumericLgmMultiLegOptionEngine::buildCashflowInfo(
            instrArgs_->cashflows[i], 1.0,
            [this](const Date& d) {
                return solver_->model()->parametrization()->termStructure()->timeFromReference(d);
            },
            Exercise::Type::American, true, 0 * Days, NullCalendar(), Unadjusted, "cashflow " + std::to_string(i)));
        cashflowStatus.push_back(CashflowStatus::Open);
    }

    // 2 set up events

    Date today = Settings::instance().evaluationDate();
    FdCallableBondEvents events(today, solver_->model()->parametrization()->termStructure()->dayCounter());

    // 2a bond cashflows

    for (Size i = 0; i < cashflows.size(); ++i) {
        if (cashflows[i].payDate > today) {
            events.registerBondCashflow(cashflows[i]);
        }
    }

    // 2b call and put data

    for (auto const& c : instrArgs_->callData) {
        events.registerCall(c);
    }
    for (auto const& c : instrArgs_->putData) {
        events.registerPut(c);
    }

    // 3 set up time grid

    QL_REQUIRE(!events.times().empty(),
               "NumericLgmCallableBondEngine: internal error, times are empty");

    Size effectiveTimeStepsPerYear;
    if(events.hasAmericanExercise())
        effectiveTimeStepsPerYear = std::max(americanExerciseTimeStepsPerYear_, solver_->timeStepsPerYear());
    else
        effectiveTimeStepsPerYear = solver_->timeStepsPerYear();

    TimeGrid grid;
    if (effectiveTimeStepsPerYear == 0) {
        grid = TimeGrid(events.times().begin(), events.times().end());
    } else {
        Size steps = std::max<Size>(std::lround(effectiveTimeStepsPerYear * (*events.times().rbegin()) + 0.5), 1);
        grid = TimeGrid(events.times().begin(), events.times().end(), steps);
    }

    // 4 finalize event processor

    events.finalise(grid);

    // 5 set up functions accrualFraction(t), notional(t)

    Real N0 = instrArgs_->notionals.front();
    std::vector<Real> notionalTimes = {0.0};
    std::vector<Real> notionals = {N0};
    std::vector<Real> couponAccrualStartTimes, couponAccrualEndTimes, couponPayTimes;
    for (auto const& c : instrArgs_->cashflows) {
        if (c->date() <= today)
            continue;
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
            if (!QuantLib::close_enough(cpn->nominal(), notionals.back())) {
                notionalTimes.push_back(solver_->model()->parametrization()->termStructure()->timeFromReference(c->date()));
                notionals.push_back(cpn->nominal());
            }
            couponAccrualStartTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->accrualStartDate()));
            couponAccrualEndTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->accrualEndDate()));
            couponPayTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->date()));
        }
    }

    [[maybe_unused]] auto notional = [&notionalTimes, &notionals](const Real t) {
        auto cn = std::upper_bound(notionalTimes.begin(), notionalTimes.end(), t,
                                   [](Real s, Real t) { return s < t && !QuantLib::close_enough(s, t); });
        return notionals[std::max<Size>(std::distance(notionalTimes.begin(), cn), 1) - 1];
    };

    [[maybe_unused]] auto accrualFraction = [&couponPayTimes, &couponAccrualStartTimes, &couponAccrualEndTimes](const Real t) {
        Real accrualFraction = 0.0;
        auto f = std::upper_bound(couponPayTimes.begin(), couponPayTimes.end(), t,
                                  [](Real s, Real t) { return s < t && !QuantLib::close_enough(s, t); });
        if (f != couponPayTimes.end()) {
            Size i = std::distance(couponPayTimes.begin(), f);
            if (t > couponAccrualStartTimes[i]) {
                accrualFraction =
                    (t - couponAccrualStartTimes[i]) / (couponAccrualEndTimes[i] - couponAccrualStartTimes[i]);
            }
        }
        return accrualFraction;
    };

    

}

NumericLgmCallableBondEngine::NumericLgmCallableBondEngine(
    const Handle<LGM>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Size americanExerciseTimeStepsPerYear, const Handle<QuantLib::YieldTermStructure>& referenceCurve,
    const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(*model, sy, ny, sx, nx),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate) {
    registerWith(solver_->model());
    registerWith(referenceCurve_);
    registerWith(discountingSpread_);
    registerWith(creditCurve_);
    registerWith(incomeCurve_);
    registerWith(recoveryRate_);
}

NumericLgmCallableBondEngine::NumericLgmCallableBondEngine(
    const Handle<LGM>& model, const Real maxTime, const QuantLib::FdmSchemeDesc scheme, const Size stateGridPoints,
    const Size timeStepsPerYear, const Real mesherEpsilon, const Size americanExerciseTimeStepsPerYear,
    const Handle<QuantLib::YieldTermStructure>& referenceCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmFdSolver>(*model, maxTime, scheme, stateGridPoints,
                                                                               timeStepsPerYear, mesherEpsilon),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate) {
    registerWith(solver_->model());
    registerWith(referenceCurve_);
    registerWith(discountingSpread_);
    registerWith(creditCurve_);
    registerWith(incomeCurve_);
    registerWith(recoveryRate_);
}

std::pair<Real, Real> NumericLgmCallableBondEngine::forwardPrice(const QuantLib::Date& forwardNpvDate,
                                                                 const QuantLib::Date& settlementDate,
                                                                 const bool conditionalOnSurvival,
                                                                 std::vector<CashFlowResults>* const cfResults) const {

    npvDate_ = forwardNpvDate;
    settlementDate_ = settlementDate;
    conditionalOnSurvival_ = true;
    cfResults_ = cfResults;
    instrArgs_ = &arguments_;

    calculate();

    return std::make_pair(npv_, settlementValue_);
}

void NumericLgmCallableBondEngine::calculate() const {
    std::vector<CashFlowResults> cfResults;

    npvDate_ = referenceCurve_->referenceDate();
    settlementDate_ = arguments_.settlementDate;
    conditionalOnSurvival_ = false; // does not matter, since npvDate_ = today
    cfResults_ = &cfResults;
    instrArgs_ = &arguments_;

    NumericLgmCallableBondEngineBase::calculate();

    results_.value = npv_;
    results_.settlementValue = settlementValue_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["cashFlowResults"] = cfResults;
}

} // namespace QuantExt
