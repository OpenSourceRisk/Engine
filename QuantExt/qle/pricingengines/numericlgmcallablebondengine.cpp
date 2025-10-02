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

#include <qle/pricingengines/fdcallablebondevents.hpp>
#include <qle/pricingengines/numericlgmcallablebondengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/termstructures/effectivebonddiscountcurve.hpp>

#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <ql/cashflows/simplecashflow.hpp>

namespace QuantExt {

namespace {
// get amount to be paid on call (or put) exercise dependent on outstanding notional and call details
Real getCallPriceAmount(const FdCallableBondEvents::CallData& cd, Real notional, Real accruals) {
    Real price = cd.price * notional;
    if (cd.priceType == CallableBond::CallabilityData::PriceType::Clean)
        price += accruals;
    if (!cd.includeAccrual)
        price -= accruals;
    return price;
}
} // namespace

NumericLgmCallableBondEngineBase::NumericLgmCallableBondEngineBase(
    const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver, const Size americanExerciseTimeStepsPerYear,
    const Handle<QuantLib::YieldTermStructure>& referenceCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate,
    const bool spreadOnIncome)
    : solver_(solver), americanExerciseTimeStepsPerYear_(americanExerciseTimeStepsPerYear),
      referenceCurve_(referenceCurve), discountingSpread_(discountingSpread), creditCurve_(creditCurve),
      incomeCurve_(incomeCurve), recoveryRate_(recoveryRate), spreadOnIncome_(spreadOnIncome) {}

void NumericLgmCallableBondEngineBase::calculate() const {

    // 0 if there are no cashflows in the underlying bond, we do not calculate anyything

    if (instrArgs_->cashflows.empty())
        return;

    // 1 set effective discount, income and credit curve

    QL_REQUIRE(!referenceCurve_.empty(), "NumericLgmCallableBondEngineBase::calculate(): reference curve is empty. "
                                         "Check reference data and errors from curve building.");

    auto effCreditCurve =
        creditCurve_.empty()
            ? Handle<DefaultProbabilityTermStructure>(
                  QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(npvDate_, 0.0, referenceCurve_->dayCounter()))
            : creditCurve_;

    auto effIncomeCurve = incomeCurve_.empty() ? referenceCurve_ : incomeCurve_;
    if (spreadOnIncome_ && !discountingSpread_.empty())
        effIncomeCurve = Handle<YieldTermStructure>(
            QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(effIncomeCurve, discountingSpread_));

    auto effDiscountCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<EffectiveBondDiscountCurve>(
        referenceCurve_, creditCurve_, discountingSpread_, recoveryRate_));

    // 2 build the cashflow info

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

    // 3 set up events

    Date today = Settings::instance().evaluationDate();
    FdCallableBondEvents events(today, solver_->model()->parametrization()->termStructure()->dayCounter());

    // 3a bond cashflows

    for (Size i = 0; i < cashflows.size(); ++i) {
        if (cashflows[i].payDate > today) {
            events.registerBondCashflow(cashflows[i]);
        }
    }

    // 3b call and put data

    for (auto const& c : instrArgs_->callData) {
        events.registerCall(c);
    }
    for (auto const& c : instrArgs_->putData) {
        events.registerPut(c);
    }

    // 4 set up time grid

    QL_REQUIRE(!events.times().empty(), "NumericLgmCallableBondEngine: internal error, times are empty");

    Size effectiveTimeStepsPerYear;
    if (events.hasAmericanExercise())
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

    // 5 finalize event processor

    events.finalise(grid);

    // 6 set up functions accrualFraction(t), notional(t)

    Real N0 = instrArgs_->notionals.front();
    std::vector<Real> notionalTimes = {0.0};
    std::vector<Real> notionals = {N0};
    std::vector<Real> couponAmounts, couponAccrualStartTimes, couponAccrualEndTimes, couponPayTimes;
    for (auto const& c : instrArgs_->cashflows) {
        if (c->date() <= today)
            continue;
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
            if (!QuantLib::close_enough(cpn->nominal(), notionals.back())) {
                notionalTimes.push_back(
                    solver_->model()->parametrization()->termStructure()->timeFromReference(c->date()));
                notionals.push_back(cpn->nominal());
            }
            couponAmounts.push_back(cpn->amount());
            couponAccrualStartTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->accrualStartDate()));
            couponAccrualEndTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->accrualEndDate()));
            couponPayTimes.push_back(
                solver_->model()->parametrization()->termStructure()->timeFromReference(cpn->date()));
        }
    }

    auto notional = [&notionalTimes, &notionals](const Real t) {
        auto cn = std::upper_bound(notionalTimes.begin(), notionalTimes.end(), t,
                                   [](Real s, Real t) { return s < t && !QuantLib::close_enough(s, t); });
        return notionals[std::max<Size>(std::distance(notionalTimes.begin(), cn), 1) - 1];
    };

    auto accrual = [&couponAmounts, &couponPayTimes, &couponAccrualStartTimes, &couponAccrualEndTimes](const Real t) {
        Real accruals = 0.0;
        for (Size i = 0; i < couponAmounts.size(); ++i) {
            if (couponPayTimes[i] > t && t > couponAccrualStartTimes[i]) {
                accruals += (t - couponAccrualStartTimes[i]) / (couponAccrualEndTimes[i] - couponAccrualStartTimes[i]) *
                            couponAmounts[i];
            }
        }
        return accruals;
    };

    // 7 set bounday value at last grid point

    RandomVariable value(solver_->gridSize(), 0.0);
    RandomVariable valueStripped(solver_->gridSize(), 0.0);

    // 8 determine fwd cutoff point for forward price calculation

    Real t_fwd_cutoff = solver_->model()->parametrization()->termStructure()->timeFromReference(npvDate_);

    // 9 init cashflow variables for expected cashflow calculation if required

    std::vector<RandomVariable> rvCashflows;

    if (expectedCashflows_) {
        rvCashflows.resize(grid.size() - 1, RandomVariable(solver_->gridSize(), 0.0));
    }

    // 10 perform the backward pricing using the backward solver

    LgmVectorised lgmv(solver_->model()->parametrization());

    for (Size i = grid.size() - 1; i > 0; --i) {

        // 7.1 we will roll back from t_i = t_from to t_{i-1} = t_to in this step

        Real t_from = grid[i];
        Real t_to = grid[i - 1];

        if (t_from > t_fwd_cutoff) {

            // 7.2 handle call, put on t_i (assume put overrides call, should both be exercised)

            if (events.hasCall(i)) {

                RandomVariable c =
                    RandomVariable(solver_->gridSize(),
                                   getCallPriceAmount(events.getCallData(i), notional(t_from), accrual(t_from))) /
                    lgmv.numeraire(t_from, solver_->stateGrid(t_from), effDiscountCurve);

                value = min(c, value);

                if (expectedCashflows_) {
                    Filter exercised = close_enough(c, value);
                    rvCashflows[i - 1] = conditionalResult(exercised, c, rvCashflows[i - 1]);
                    for (Size j = i + 1; j < grid.size(); ++j)
                        rvCashflows[i - 1] = applyInverseFilter(rvCashflows[i - 1], exercised);
                }
            }

            if (events.hasPut(i)) {

                RandomVariable c =
                    RandomVariable(solver_->gridSize(),
                                   getCallPriceAmount(events.getPutData(i), notional(t_from), accrual(t_from))) /
                    lgmv.numeraire(t_from, solver_->stateGrid(t_from), effDiscountCurve);

                value = max(c, value);

                if (expectedCashflows_) {
                    Filter exercised = close_enough(c, value);
                    rvCashflows[i - 1] = conditionalResult(exercised, c, rvCashflows[i - 1]);
                    for (Size j = i + 1; j < grid.size(); ++j)
                        rvCashflows[i - 1] = applyInverseFilter(rvCashflows[i - 1], exercised);
                }
            }

            // 7.3 handle bond cashflows on t_i

            if (events.hasBondCashflow(i)) {

                for (auto const& f : events.getBondCashflow(i)) {
                    auto tmp = f.pv(lgmv, t_from, solver_->stateGrid(t_from), effDiscountCurve);
                    value += tmp;

                    if (generateAdditionalResults_)
                        valueStripped += tmp;

                    if (cfResults_)
                        cfResults_->insert(cfResults_->begin(), standardCashFlowResults(f.qlCf, 1.0, std::string(), 0,
                                                                                        Currency(), effDiscountCurve));

                    if (expectedCashflows_) {
                        rvCashflows[i - 1] = tmp;
                    }
                }
            }
        }

        // 7.4 roll back from t_i to t_{i-1}

        value = solver_->rollback(value, t_from, t_to, 1);

        if (generateAdditionalResults_)
            valueStripped = solver_->rollback(valueStripped, t_from, t_to, 1);

        for (auto c : rvCashflows) {
            c = solver_->rollback(c, t_from, t_to, 1);
        }
    }

    // 11 set the result values

    npv_ = value.at(0) / effIncomeCurve->discount(npvDate_);

    if (conditionalOnSurvival_)
        npv_ /= effCreditCurve->survivalProbability(npvDate_);

    settlementValue_ = npv_ / effIncomeCurve->discount(settlementDate_) /
                       effCreditCurve->survivalProbability(settlementDate_) *
                       effCreditCurve->survivalProbability(settlementDate_);

    if (expectedCashflows_) {
        std::map<Date, Real> cf;
        Date currentDate = events.latestRelevantDate();
        for (Size i = grid.size() - 1; i > 0; --i) {
            if (Date tmp = events.getAssociatedDate(i); tmp != Date())
                currentDate = tmp;
            if (Real tmp = rvCashflows[i - 1].at(0); !QuantLib::close_enough(tmp, 0.0)) {
                cf[currentDate] += tmp;
            }
        }
        for (auto& [d, v] : cf) {
            v /= effDiscountCurve->discount(d);
            expectedCashflows_->push_back(QuantLib::ext::make_shared<SimpleCashFlow>(v, d));
        }
    }

    // 12 set additional results if requested

    if (!generateAdditionalResults_)
        return;

    // 12.1 additional results from lgm model

    additionalResults_ = getAdditionalResultsMap(solver_->model()->getCalibrationInfo());

    // 12.2 stripped underlying bond, and call / put option

    Real npvStripped = valueStripped.at(0) / effIncomeCurve->discount(npvDate_);
    if (conditionalOnSurvival_)
        npvStripped /= effCreditCurve->survivalProbability(npvDate_);
    Real settlementValueStripped = npvStripped / effIncomeCurve->discount(settlementDate_) /
                                   effCreditCurve->survivalProbability(settlementDate_) *
                                   effCreditCurve->survivalProbability(settlementDate_);

    additionalResults_["strippedBondNpv"] = npvStripped;
    additionalResults_["strippedBondSettlementValue"] = settlementValueStripped;
    additionalResults_["callPutValue"] = settlementValueStripped - settlementValue_;

    // 12.3 event table

    constexpr Size width = 12;
    std::ostringstream header;
    header << std::left << "|" << std::setw(width) << "time"
           << "|" << std::setw(width) << "date"
           << "|" << std::setw(width) << "notional"
           << "|" << std::setw(width) << "accrual"
           << "|" << std::setw(width) << "flow"
           << "|" << std::setw(width) << "call"
           << "|" << std::setw(width) << "put"
           << "|" << std::setw(width) << "refDsc"
           << "|" << std::setw(width) << "survProb"
           << "|" << std::setw(width) << "secSprdDsc"
           << "|" << std::setw(width) << "effDsc"
           << "|";

    additionalResults_["event_0000!"] = header.str();

    Size counter = 0;
    for (Size i = 0; i < grid.size(); ++i) {
        std::ostringstream dateStr, bondFlowStr, callStr, putStr;

        if (events.getAssociatedDate(i) != Null<Date>()) {
            dateStr << QuantLib::io::iso_date(events.getAssociatedDate(i));
        }

        Real flow = 0.0;
        for (auto const& f : events.getBondCashflow(i))
            flow += f.qlCf->amount();
        if (!QuantLib::close_enough(flow, 0.0))
            bondFlowStr << flow;

        if (events.hasCall(i)) {
            const auto& cd = events.getCallData(i);
            callStr << "@" << cd.price;
        }

        if (events.hasPut(i)) {
            const auto& cd = events.getPutData(i);
            putStr << "@" << cd.price;
        }

        Real refDisc = referenceCurve_->discount(grid[i]);
        Real survProb = effCreditCurve->survivalProbability(grid[i]);
        Real secDisc = discountingSpread_.empty() ? 1.0 : std::exp(-discountingSpread_->value() * grid[i]);
        Real effDisc = effDiscountCurve->discount(grid[i]);

        std::ostringstream eventDescription;
        eventDescription << std::left << "|" << std::setw(width) << grid[i] << "|" << std::setw(width) << dateStr.str()
                         << "|" << std::setw(width) << notional(grid[i]) << "|" << std::setw(width) << accrual(grid[i])
                         << "|" << std::setw(width) << bondFlowStr.str() << "|" << std::setw(width) << callStr.str()
                         << "|" << std::setw(width) << putStr.str() << "|" << std::setw(width) << refDisc << "|"
                         << std::setw(width) << survProb << "|" << std::setw(width) << secDisc << "|"
                         << std::setw(width) << effDisc << "|";
        std::string label = "0000" + std::to_string(counter++);
        additionalResults_["event_" + label.substr(label.size() - 5)] = eventDescription.str();
        // do not log more than 100k events, unlikely that this is ever necessary
        if (counter >= 100000)
            break;
    }

} // NumericLgmCallableBondEngineBase::calculate()

NumericLgmCallableBondEngine::NumericLgmCallableBondEngine(
    const Handle<LGM>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Size americanExerciseTimeStepsPerYear, const Handle<QuantLib::YieldTermStructure>& referenceCurve,
    const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate,
    const bool spreadOnIncome, const bool generateAdditionalResults)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(*model, sy, ny, sx, nx),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate, spreadOnIncome),
      generateAdditionalResultsInput_(generateAdditionalResults) {
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
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate,
    const bool spreadOnIncome, const bool generateAdditionalResults)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmFdSolver>(*model, maxTime, scheme, stateGridPoints,
                                                                               timeStepsPerYear, mesherEpsilon),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate, spreadOnIncome),
      generateAdditionalResultsInput_(generateAdditionalResults) {
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
                                                                 std::vector<CashFlowResults>* const cfResults,
                                                                 QuantLib::Leg* const expectedCashflows) const {
    npvDate_ = forwardNpvDate;
    settlementDate_ = settlementDate;
    conditionalOnSurvival_ = conditionalOnSurvival;
    cfResults_ = cfResults;
    expectedCashflows_ = expectedCashflows;
    instrArgs_ = &arguments_;
    generateAdditionalResults_ = false;

    NumericLgmCallableBondEngineBase::calculate();

    return std::make_pair(npv_, settlementValue_);
}

void NumericLgmCallableBondEngine::calculate() const {
    std::vector<CashFlowResults> cfResults;

    npvDate_ = referenceCurve_->referenceDate();
    settlementDate_ = arguments_.settlementDate;
    conditionalOnSurvival_ = false; // does not matter, since npvDate_ = today
    cfResults_ = &cfResults;
    expectedCashflows_ = nullptr;
    instrArgs_ = &arguments_;
    generateAdditionalResults_ = generateAdditionalResultsInput_;

    NumericLgmCallableBondEngineBase::calculate();

    results_.value = npv_;
    results_.settlementValue = settlementValue_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["cashFlowResults"] = cfResults;
}

} // namespace QuantExt
