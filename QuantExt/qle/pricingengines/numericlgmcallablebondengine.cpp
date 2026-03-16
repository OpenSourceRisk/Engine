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
#include <qle/utilities/callablebond.hpp>

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

    QuantExt::CallableBondNotionalAndAccrualCalculator notionalAccrualCalc(
        today, instrArgs_->notionals.front(), instrArgs_->cashflows,
        solver_->model()->parametrization()->termStructure().currentLink());

    // 7 init variables for rollback with bounday value at last grid point

    RandomVariable underlyingNpv(solver_->gridSize(), 0.0);
    RandomVariable optionNpv(solver_->gridSize(), 0.0);
    RandomVariable provisionalNpv;

    std::vector<RandomVariable> exercisedCall(grid.size(), RandomVariable(solver_->gridSize(), 0.0));
    std::vector<RandomVariable> exercisedPut(grid.size(), RandomVariable(solver_->gridSize(), 0.0));

    std::vector<RandomVariable> cache(cashflows.size());

    // 8 determine fwd cutoff point for forward price calculation

    Real t_fwd_cutoff = solver_->model()->parametrization()->termStructure()->timeFromReference(npvDate_);

    // 9 perform the backward pricing using the backward solver

    LgmVectorised lgmv(solver_->model()->parametrization());

    for (Size i = grid.size() - 1; i > 0; --i) {

        // 9.1 we will roll back from t_i = t_from to t_{i-1} = t_to in this step

        Real t_from = grid[i];
        Real t_to = grid[i - 1];

        if (t_from > t_fwd_cutoff) {

            RandomVariable state = solver_->stateGrid(t_from);

            // 9.2 update cashflows on current time

            provisionalNpv = RandomVariable(solver_->gridSize(), 0.0);

            for (Size j = 0; j < cashflows.size(); ++j) {
                if (cashflowStatus[j] == CashflowStatus::Done)
                    continue;

                /* Compare this to NumericLgmMultilegOptionEngineBase:

                   Since we incorporate an accrual payment in the call / put payments below, we do not consider
                   the coupon ratio (exercise into short broken coupon) as in the swaption case. Instead we account
                   for the full coupon value in the underlying. The only caveat is the case couponRatio = 0.0 where
                   the coupon is still considered to be part of the underlying in the swaption case (but with
                   weight 0.0) whereas the accruals that we add to the call / put price here are zero already.
                   Therefore we need to exclude a coupon in this case from the underlying.
                */

                if (cashflows[j].isPartOfUnderlying(t_from) && cashflows[j].couponRatio(t_from) > 0.0) {
                    if (cashflowStatus[j] == CashflowStatus::Cached) {
                        underlyingNpv += cache[j];
                        cache[j].clear();
                        cashflowStatus[j] = CashflowStatus::Done;
                    } else if (cashflows[j].canBeEstimated(t_from)) {
                        underlyingNpv += cashflows[j].pv(lgmv, t_from, state, effDiscountCurve);
                        cashflowStatus[j] = CashflowStatus::Done;
                    } else {
                        provisionalNpv += cashflows[j].pv(lgmv, t_from, state, effDiscountCurve);
                    }
                } else if (cashflows[j].mustBeEstimated(t_from) && cashflowStatus[j] == CashflowStatus::Open) {
                    cache[j] = cashflows[j].pv(lgmv, t_from, state, effDiscountCurve);
                    cashflowStatus[j] = CashflowStatus::Cached;
                }
            }

            // 9.3 handle call, put on t_i (assume put overrides call, should both be exercised)

            if (events.hasCall(i)) {

                RandomVariable f = RandomVariable(
                    solver_->gridSize(), getCallPriceAmount(events.getCallData(i), notionalAccrualCalc.notional(t_from), notionalAccrualCalc.accrual(t_from)));
                RandomVariable num = lgmv.numeraire(t_from, solver_->stateGrid(t_from), effDiscountCurve);
                RandomVariable c = f / num;

                RandomVariable tmp = c - (underlyingNpv + provisionalNpv);
                optionNpv = min(tmp, optionNpv);

                if (expectedCashflows_) {
                    Filter exercisedNow = close_enough(tmp, optionNpv);
                    exercisedCall[i] = RandomVariable(solver_->gridSize(), 1.0) / num;
                    exercisedCall[i] = applyFilter(exercisedCall[i], exercisedNow);
                    for (Size j = i + 1; j < grid.size(); ++j) {
                        exercisedCall[j] = applyInverseFilter(exercisedCall[j], exercisedNow);
                        exercisedPut[j] = applyInverseFilter(exercisedPut[j], exercisedNow);
                    }
                }
            }

            if (events.hasPut(i)) {

                RandomVariable f = RandomVariable(
                    solver_->gridSize(), getCallPriceAmount(events.getPutData(i), notionalAccrualCalc.notional(t_from), notionalAccrualCalc.accrual(t_from)));
                RandomVariable num = lgmv.numeraire(t_from, solver_->stateGrid(t_from), effDiscountCurve);
                RandomVariable p = f / num;

                RandomVariable tmp = p - underlyingNpv + provisionalNpv;
                optionNpv = max(tmp, optionNpv);

                if (expectedCashflows_) {
                    Filter exercisedNow = close_enough(tmp, optionNpv);
                    exercisedPut[i] = RandomVariable(solver_->gridSize(), 1.0) / num;
                    exercisedPut[i] = applyFilter(exercisedPut[i], exercisedNow);
                    exercisedCall[i] = applyInverseFilter(exercisedPut[i], exercisedNow);
                    for (Size j = i + 1; j < grid.size(); ++j) {
                        exercisedCall[j] = applyInverseFilter(exercisedCall[j], exercisedNow);
                        exercisedPut[j] = applyInverseFilter(exercisedPut[j], exercisedNow);
                    }
                }
            }
        }

        // 9.4 roll back from t_i to t_{i-1}

        if (t_from != t_to) {
            optionNpv = solver_->rollback(optionNpv, t_from, t_to, 1);
            underlyingNpv = solver_->rollback(underlyingNpv, t_from, t_to, 1);
            for (auto& c : cache) {
                if (!c.initialised())
                    continue;
                c = solver_->rollback(c, t_from, t_to);
            }

            // need to roll back all future exercise indicators
            for (Size j = i; j < grid.size(); ++j) {
                exercisedCall[j] = solver_->rollback(exercisedCall[j], t_from, t_to);
                exercisedPut[j] = solver_->rollback(exercisedPut[j], t_from, t_to);
            }

            // need to roll back provisionalNpv, but only for part of the steps
            if (i == 1 || t_from <= t_fwd_cutoff)
                provisionalNpv = solver_->rollback(provisionalNpv, t_from, t_to);
        }
    }

    // 10 set expected cashflows if required

    if (expectedCashflows_) {

        // 10.1 init a vector on the time grid with underlying discounted bond cashflows

        std::vector<double> gridCashflows(grid.size(), 0.0);

        for (Size j = 0; j < cashflows.size(); ++j) {
            Size index = grid.closestIndex(events.time(cashflows[j].payDate));
            if (grid[index] > t_fwd_cutoff) {
                gridCashflows[index] +=
                    cashflows[j].qlCf->amount() * effDiscountCurve->discount(cashflows[j].qlCf->date());
            }
        }

        // 10.2 incorporate call and put exercises

        double cumCallProb = 0.0;
        double cumPutProb = 0.0;

        for (Size i = 1; i < grid.size(); ++i) {

            cumCallProb += exercisedCall[i].at(0) / effDiscountCurve->discount(grid[i]);
            cumPutProb += exercisedPut[i].at(0) / effDiscountCurve->discount(grid[i]);

            if (grid[i] > t_fwd_cutoff)
                gridCashflows[i] = (1.0 - (cumCallProb + cumPutProb)) * gridCashflows[i] +
                                   exercisedCall[i].at(0) *
                                       getCallPriceAmount(events.getCallData(i), notionalAccrualCalc.notional(grid[i]), notionalAccrualCalc.accrual(grid[i])) +
                                   exercisedPut[i].at(0) *
                                       getCallPriceAmount(events.getPutData(i), notionalAccrualCalc.notional(grid[i]), notionalAccrualCalc.accrual(grid[i]));
        }

        // 10.3 allocate cashflows back to dates and store expectation in result vector

        std::map<Date, Real> cf;
        Date currentDate = events.latestRelevantDate();
        for (Size i = grid.size() - 1; i > 0; --i) {
            if (Date tmp = events.getAssociatedDate(i); tmp != Date())
                currentDate = tmp;
            if (Real tmp = gridCashflows[i]; !QuantLib::close_enough(tmp, 0.0)) {
                cf[currentDate] += tmp;
            }
        }

        for (auto& [d, v] : cf) {
            expectedCashflows_->push_back(
                QuantLib::ext::make_shared<SimpleCashFlow>(v / effDiscountCurve->discount(d), d));
        }

    }

    // 11 set the cf results if required

    if (cfResults_) {
        for (Size j = 0; j < cashflows.size(); ++j) {
            if (cashflows[j].payDate > npvDate_)
                cfResults_->push_back(
                    standardCashFlowResults(cashflows[j].qlCf, 1.0, std::string(), 0, Currency(), effDiscountCurve));
        }
    }

    // 12 set the result values

    Real totalUnderlyingNpv = underlyingNpv.at(0);
    for (auto const& c : cache) {
        if (c.initialised())
            totalUnderlyingNpv += c.at(0);
    }
    totalUnderlyingNpv += provisionalNpv.at(0);

    npv_ = (totalUnderlyingNpv + optionNpv.at(0)) / effIncomeCurve->discount(npvDate_);

    if (conditionalOnSurvival_)
        npv_ /= effCreditCurve->survivalProbability(npvDate_);

    settlementValue_ = npv_ / effIncomeCurve->discount(settlementDate_) /
                       effCreditCurve->survivalProbability(settlementDate_) *
                       effCreditCurve->survivalProbability(settlementDate_);


    // 13 set additional results if requested

    if (!generateAdditionalResults_)
        return;

    // 13.1 additional results from lgm model

    additionalResults_ = getAdditionalResultsMap(solver_->model()->getCalibrationInfo());

    // 13.2 stripped underlying bond, and call / put option

    Real npvStripped = totalUnderlyingNpv / effIncomeCurve->discount(npvDate_);
    if (conditionalOnSurvival_)
        npvStripped /= effCreditCurve->survivalProbability(npvDate_);
    Real settlementValueStripped = npvStripped / effIncomeCurve->discount(settlementDate_) /
                                   effCreditCurve->survivalProbability(settlementDate_) *
                                   effCreditCurve->survivalProbability(settlementDate_);

    additionalResults_["strippedBondNpv"] = npvStripped;
    additionalResults_["strippedBondSettlementValue"] = settlementValueStripped;
    additionalResults_["callPutValue"] = settlementValueStripped - settlementValue_;

    // 13.3 event table

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
                         << "|" << std::setw(width) << notionalAccrualCalc.notional(grid[i])
                         << "|" << std::setw(width) << notionalAccrualCalc.accrual(grid[i]) 
                         << "|" << std::setw(width) << bondFlowStr.str() << "|"
                         << std::setw(width) << callStr.str() << "|" << std::setw(width) << putStr.str() << "|"
                         << std::setw(width) << refDisc << "|" << std::setw(width) << survProb << "|"
                         << std::setw(width) << secDisc << "|" << std::setw(width) << effDisc << "|";
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
