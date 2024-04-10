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

#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/models/lgmconvolutionsolver2.hpp>
#include <qle/models/lgmfdsolver.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/payoff.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

using namespace QuantLib;

bool NumericLgmMultiLegOptionEngineBase::CashflowInfo::isPartOfUnderlying(const Real optionTime) const {
    return optionTime < belongsToUnderlyingMaxTime_ || QuantLib::close_enough(optionTime, belongsToUnderlyingMaxTime_);
}

bool NumericLgmMultiLegOptionEngineBase::CashflowInfo::canBeEstimated(const Real time) const {
    if (maxEstimationTime_ != Null<Real>()) {
        return time < maxEstimationTime_ || QuantLib::close_enough(time, maxEstimationTime_);
    } else {
        return QuantLib::close_enough(time, exactEstimationTime_);
    }
}

bool NumericLgmMultiLegOptionEngineBase::CashflowInfo::mustBeEstimated(const Real time) const {
    if (maxEstimationTime_ != Null<Real>())
        return false;
    return time < exactEstimationTime_ || QuantLib::close_enough(time, exactEstimationTime_);
}

Real NumericLgmMultiLegOptionEngineBase::CashflowInfo::requiredSimulationTime() const { return exactEstimationTime_; }

Real NumericLgmMultiLegOptionEngineBase::CashflowInfo::couponRatio(const Real time) const {
    if (couponEndTime_ != Null<Real>() && couponStartTime_ != Null<Real>()) {
        return std::max(0.0, std::min(1.0, (couponEndTime_ - time) / (couponEndTime_ - couponStartTime_)));
    }
    return 1.0;
}

RandomVariable
NumericLgmMultiLegOptionEngineBase::CashflowInfo::pv(const LgmVectorised& lgm, const Real t,
                                                     const RandomVariable& state,
                                                     const Handle<YieldTermStructure>& discountCurve) const {
    return calculator_(lgm, t, state, discountCurve);
}

NumericLgmMultiLegOptionEngineBase::CashflowInfo
NumericLgmMultiLegOptionEngineBase::buildCashflowInfo(const Size i, const Size j) const {

    CashflowInfo info;
    auto const& ts = solver_->model()->parametrization()->termStructure();
    auto const& c = legs_[i][j];
    Real payrec = payer_[i] ? -1.0 : 1.0;

    Real T = solver_->model()->parametrization()->termStructure()->timeFromReference(c->date());

    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
        bool done = false;
        if (exercise_->type() == Exercise::American) {
            // american exercise implies that we can exercise into broken periods
            info.belongsToUnderlyingMaxTime_ = ts->timeFromReference(cpn->accrualEndDate());
        } else {
            // bermudan exercise implies that we always exercise into whole periods
            info.belongsToUnderlyingMaxTime_ = ts->timeFromReference(cpn->accrualStartDate());
        }
        info.couponStartTime_ = ts->timeFromReference(cpn->accrualStartDate());
        info.couponEndTime_ = ts->timeFromReference(cpn->accrualEndDate());
        if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c)) {
            info.maxEstimationTime_ = ts->timeFromReference(ibor->fixingDate());
            info.calculator_ = [ibor, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                 const Handle<YieldTermStructure>& discountCurve) {
                return (RandomVariable(x.size(), ibor->gearing()) *
                            lgm.fixing(ibor->index(), ibor->fixingDate(), t, x) +
                        RandomVariable(x.size(), ibor->spread())) *
                       RandomVariable(x.size(), ibor->accrualPeriod() * ibor->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto fix = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
            info.maxEstimationTime_ = ts->timeFromReference(fix->date());
            info.calculator_ = [fix, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                const Handle<YieldTermStructure>& discountCurve) {
                return RandomVariable(x.size(), fix->amount() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn)) {
            info.maxEstimationTime_ = ts->timeFromReference(on->fixingDates().front());
            info.calculator_ = [on, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                               const Handle<YieldTermStructure>& discountCurve) {
                return lgm.compoundedOnRate(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(on->index()), on->fixingDates(),
                                            on->valueDates(), on->dt(), on->rateCutoff(), on->includeSpread(),
                                            on->spread(), on->gearing(), on->lookback(), Null<Real>(), Null<Real>(),
                                            false, false, t, x) *
                       RandomVariable(x.size(), on->accrualPeriod() * on->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto av = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(cpn)) {
            info.maxEstimationTime_ = ts->timeFromReference(av->fixingDates().front());
            info.calculator_ = [av, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                               const Handle<YieldTermStructure>& discountCurve) {
                return lgm.averagedOnRate(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(av->index()), av->fixingDates(),
                                          av->valueDates(), av->dt(), av->rateCutoff(), false, av->spread(),
                                          av->gearing(), av->lookback(), Null<Real>(), Null<Real>(), false, false, t,
                                          x) *
                       RandomVariable(x.size(), av->accrualPeriod() * av->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto bma = QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(cpn)) {
            info.maxEstimationTime_ = ts->timeFromReference(bma->fixingDates().front());
            info.calculator_ = [bma, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                const Handle<YieldTermStructure>& discountCurve) {
                return lgm.averagedBmaRate(QuantLib::ext::dynamic_pointer_cast<BMAIndex>(bma->index()), bma->fixingDates(),
                                           bma->accrualStartDate(), bma->accrualEndDate(), false, bma->spread(),
                                           bma->gearing(), Null<Real>(), Null<Real>(), false, t, x) *
                       RandomVariable(x.size(), bma->accrualPeriod() * bma->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto cf = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(cpn)) {
            auto und = cf->underlying();
            if (auto undibor = QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(und)) {
                info.exactEstimationTime_ = ts->timeFromReference(und->fixingDate());
                info.calculator_ = [cf, undibor, T, payrec](const LgmVectorised& lgm, const Real t,
                                                            const RandomVariable& x,
                                                            const Handle<YieldTermStructure>& discountCurve) {
                    RandomVariable cap(x.size(), cf->cap() == Null<Real>() ? QL_MAX_REAL : cf->cap());
                    RandomVariable floor(x.size(), cf->floor() == Null<Real>() ? -QL_MAX_REAL : cf->floor());

                    return max(floor, min(cap, (RandomVariable(x.size(), undibor->gearing()) *
                                                    lgm.fixing(undibor->index(), undibor->fixingDate(), t, x) +
                                                RandomVariable(x.size(), undibor->spread())))) *
                           RandomVariable(x.size(), undibor->accrualPeriod() * undibor->nominal() * payrec) *
                           lgm.reducedDiscountBond(t, T, x, discountCurve);
                };
                done = true;
            }
        } else if (auto cfon = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(cpn)) {
            auto und = cfon->underlying();
            info.exactEstimationTime_ = ts->timeFromReference(und->fixingDates().front());
            info.calculator_ = [cfon, und, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                      const Handle<YieldTermStructure>& discountCurve) {
                return lgm.compoundedOnRate(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(und->index()),
                                            und->fixingDates(), und->valueDates(), und->dt(), und->rateCutoff(),
                                            und->includeSpread(), und->spread(), und->gearing(), und->lookback(),
                                            cfon->cap(), cfon->floor(), cfon->localCapFloor(), cfon->nakedOption(), t,
                                            x) *
                       RandomVariable(x.size(), cfon->accrualPeriod() * cfon->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto cfav = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(cpn)) {
            auto und = cfav->underlying();
            info.exactEstimationTime_ = ts->timeFromReference(und->fixingDates().front());
            info.calculator_ = [cfav, und, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                      const Handle<YieldTermStructure>& discountCurve) {
                return lgm.averagedOnRate(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(und->index()), und->fixingDates(),
                                          und->valueDates(), und->dt(), und->rateCutoff(), cfav->includeSpread(),
                                          und->spread(), und->gearing(), und->lookback(), cfav->cap(), cfav->floor(),
                                          cfav->localCapFloor(), cfav->nakedOption(), t, x) *
                       RandomVariable(x.size(), cfav->accrualPeriod() * cfav->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageBMACoupon>(cpn)) {
            auto und = cfbma->underlying();
            info.exactEstimationTime_ = ts->timeFromReference(und->fixingDates().front());
            info.calculator_ = [cfbma, und, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                       const Handle<YieldTermStructure>& discountCurve) {
                return lgm.averagedBmaRate(QuantLib::ext::dynamic_pointer_cast<BMAIndex>(und->index()), und->fixingDates(),
                                           und->accrualStartDate(), und->accrualEndDate(), cfbma->includeSpread(),
                                           und->spread(), und->gearing(), cfbma->cap(), cfbma->floor(),
                                           cfbma->nakedOption(), t, x) *
                       RandomVariable(x.size(), cfbma->accrualPeriod() * cfbma->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        } else if (auto sub = QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(cpn)) {
            info.maxEstimationTime_ = ts->timeFromReference(sub->fixingDates().front());
            info.calculator_ = [sub, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                                const Handle<YieldTermStructure>& discountCurve) {
                return lgm.subPeriodsRate(sub->index(), sub->fixingDates(), t, x) *
                       RandomVariable(x.size(), sub->accrualPeriod() * sub->nominal() * payrec) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            };
            done = true;
        }
        QL_REQUIRE(done, "NumericLgmMultiLegOptionEngineBase: coupon type not handled, supported coupon types: Fix, "
                         "(capfloored) Ibor, (capfloored) ON comp, (capfloored) ON avg, BMA/SIFMA, subperiod. leg = "
                             << i << " cf = " << j);
    } else {
        // can not cast to coupon
        info.belongsToUnderlyingMaxTime_ = ts->timeFromReference(c->date());
        info.maxEstimationTime_ = ts->timeFromReference(c->date());
        info.calculator_ = [c, T, payrec](const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                          const Handle<YieldTermStructure>& discountCurve) {
            return RandomVariable(x.size(), c->amount() * payrec) * lgm.reducedDiscountBond(t, T, x, discountCurve);
        };
    }

    // some postprocessing and checks

    info.maxEstimationTime_ = std::max(0.0, info.maxEstimationTime_);
    info.exactEstimationTime_ = std::max(0.0, info.exactEstimationTime_);

    QL_REQUIRE(
        info.belongsToUnderlyingMaxTime_ != Null<Real>(),
        "NumericLgmMultiLegOptionEngineBase: internal error: cashflow info: belongsToUnderlyingMaxTime_ is null. leg = "
            << i << " cf = " << j);
    QL_REQUIRE(info.maxEstimationTime_ != Null<Real>() || info.exactEstimationTime_ != Null<Real>(),
               "NumericLgmMultiLegOptionEngineBase: internal error: both maxEstimationTime_ and exactEstimationTime_ "
               "is null.  leg = "
                   << i << " cf = " << j);
    return info;
}

/* Get the rebate amount on an exercise  */
RandomVariable getRebatePv(const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                           const Handle<YieldTermStructure>& discountCurve,
                           const QuantLib::ext::shared_ptr<RebatedExercise>& exercise, const Date& d) {
    if (exercise == nullptr)
        return RandomVariable(x.size(), 0.0);
    if (exercise->type() == Exercise::American) {
        return RandomVariable(x.size(), exercise->rebate(0)) *
               lgm.reducedDiscountBond(
                   t, lgm.parametrization()->termStructure()->timeFromReference(exercise->rebatePaymentDate(d)), x);
    } else {
        auto f = std::find(exercise->dates().begin(), exercise->dates().end(), d);
        QL_REQUIRE(f != exercise->dates().end(), "NumericLgmMultiLegOptionEngine: internal error: exercise date "
                                                     << d << " from rebate payment not found amount exercise dates.");
        Size index = std::distance(exercise->dates().begin(), f);
        return RandomVariable(x.size(), exercise->rebate(index)) *
               lgm.reducedDiscountBond(
                   t, lgm.parametrization()->termStructure()->timeFromReference(exercise->rebatePaymentDate(index)), x);
    }
}

NumericLgmMultiLegOptionEngineBase::NumericLgmMultiLegOptionEngineBase(
    const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver, const Handle<YieldTermStructure>& discountCurve,
    const Size americanExerciseTimeStepsPerYear)
    : solver_(solver), discountCurve_(discountCurve),
      americanExerciseTimeStepsPerYear_(americanExerciseTimeStepsPerYear) {}

bool NumericLgmMultiLegOptionEngineBase::instrumentIsHandled(const MultiLegOption& m,
                                                             std::vector<std::string>& messages) {
    return instrumentIsHandled(m.legs(), m.payer(), m.currency(), m.exercise(), m.settlementType(),
                               m.settlementMethod(), messages);
}

bool NumericLgmMultiLegOptionEngineBase::instrumentIsHandled(
    const std::vector<Leg>& legs, const std::vector<bool>& payer, const std::vector<Currency>& currency,
    const QuantLib::ext::shared_ptr<Exercise>& exercise, const Settlement::Type& settlementType,
    const Settlement::Method& settlementMethod, std::vector<std::string>& messages) {

    bool isHandled = true;

    // is there a unique pay currency and all interest rate indices are in this same currency?

    for (Size i = 1; i < currency.size(); ++i) {
        if (currency[0] != currency[i]) {
            messages.push_back("NumericLgmMultilegOptionEngine: can only handle single currency underlyings, got " +
                               currency[0].code() + " on leg #1 and " + currency[i].code() + " on leg #" +
                               std::to_string(i + 1));
            isHandled = false;
        }
    }

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(legs[i][j])) {
                if (cpn->index()->currency() != currency[0]) {
                    messages.push_back("NumericLgmMultilegOptionEngine: can only handle indices (" +
                                       cpn->index()->name() + ") with same currency as unqiue pay currency (" +
                                       currency[0].code());
                }
            }
        }
    }

    // check coupon types

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto c = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs[i][j])) {
                if (!(QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c) || QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageBMACoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(c) ||
                      (QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(c) &&
                       QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(
                           QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(c)->underlying())))) {
                    messages.push_back(
                        "NumericLgmMultilegOptionEngine: coupon type not handled, supported coupon types: Fix, "
                        "(capfloored) Ibor, (capfloored) ON comp, (capfloored) ON avg, BMA/SIFMA, subperiod. leg = " +
                        std::to_string(i) + " cf = " + std::to_string(j));
                    isHandled = false;
                }
            }
        }
    }

    return isHandled;
}

void NumericLgmMultiLegOptionEngineBase::calculate() const {

    std::vector<std::string> messages;
    QL_REQUIRE(
        instrumentIsHandled(legs_, payer_, currency_, exercise_, settlementType_, settlementMethod_, messages),
        "NumericLgmMultiLegOptionEngineBase::calculate(): instrument is not handled: " << boost::join(messages, ", "));

    // handle empty exercise

    if (exercise_ == nullptr) {
        npv_ = 0.0;
        for (Size i = 0; i < legs_.size(); ++i) {
            for (Size j = 0; j < legs_[i].size(); ++j) {
                npv_ += legs_[i][j]->amount() * discountCurve_->discount(legs_[i][j]->date());
            }
        }
        underlyingNpv_ = npv_;
        return;
    }

    // we have a non-empty exercise

    auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);
    auto const& ts = solver_->model()->parametrization()->termStructure();
    Date refDate = ts->referenceDate();

    /* Build the cashflow info */

    enum class CashflowStatus { Open, Cached, Done };

    std::vector<CashflowInfo> cashflows;
    std::vector<CashflowStatus> cashflowStatus;

    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            cashflows.push_back(buildCashflowInfo(i, j));
            cashflowStatus.push_back(CashflowStatus::Open);
        }
    }

    /* Build the time grid containing the option times */

    std::set<Real> optionTimes;
    std::map<Real, Date> optionDates;

    if (exercise_->type() == Exercise::Bermudan || exercise_->type() == Exercise::European) {
        for (auto const& d : exercise_->dates()) {
            if (d > refDate) {
                optionTimes.insert(ts->timeFromReference(d));
                optionDates[ts->timeFromReference(d)] = d;
            }
        }
    } else if (exercise_->type() == Exercise::American) {
        QL_REQUIRE(exercise_->dates().size() == 2, "NumericLgmMultiLegOptionEngineBase::calculate(): internal error: "
                                                   "expected 2 dates for AmericanExercise, got "
                                                       << exercise_->dates().size());
        Real t1 = std::max(0.0, ts->timeFromReference(exercise_->dates().front()));
        Real t2 = std::max(t1, ts->timeFromReference(exercise_->dates().back()));
        Size steps =
            std::max<Size>(1, static_cast<Size>((t2 - t1) * static_cast<Real>(americanExerciseTimeStepsPerYear_)));
        optionTimes.insert(t1);
        for (Size i = 0; i <= steps; ++i) {
            optionTimes.insert(t1 + static_cast<Real>(i) * (t2 - t1) / static_cast<Real>(steps));
        }
    } else {
        QL_FAIL("NumericLgmMultiLegOptionEngineBase::calculate(): internal error: exercise type "
                << static_cast<int>(exercise_->type()) << " not handled.");
    }

    /* Add specific times required to simulate cashflows */

    std::set<Real> requiredCfSimTimes;

    for (auto const& c : cashflows) {
        if (Real t = c.requiredSimulationTime(); t != Null<Real>())
            requiredCfSimTimes.insert(t);
    }

    /* Join the two grids to get the time grid which we use for the backward run */

    std::set<Real> timeGrid{0.0};
    timeGrid.insert(optionTimes.begin(), optionTimes.end());
    timeGrid.insert(requiredCfSimTimes.begin(), requiredCfSimTimes.end());

    /* Step backwards through the grid and compute the option npv */

    LgmVectorised lgm(solver_->model()->parametrization());

    RandomVariable underlyingNpv(solver_->gridSize(), 0.0);
    RandomVariable optionNpv(solver_->gridSize(), 0.0);
    RandomVariable provisionalNpv(solver_->gridSize(), 0.0);

    std::vector<RandomVariable> cache(cashflows.size());

    for (auto it = timeGrid.rbegin(); it != timeGrid.rend(); ++it) {

        Real t_from = *it;
        Real t_to = (it != std::next(timeGrid.rend(), -1)) ? *std::next(it, 1) : t_from;

        RandomVariable state = solver_->stateGrid(t_from);

        // update cashflows on current time

        provisionalNpv = RandomVariable(solver_->gridSize(), 0.0);

        for (Size i = 0; i < cashflows.size(); ++i) {
            if (cashflowStatus[i] == CashflowStatus::Done)
                continue;
            if (cashflows[i].isPartOfUnderlying(t_from)) {
                RandomVariable cpnRatio(solver_->gridSize(), cashflows[i].couponRatio(t_from));
                bool isBrokenCoupon = !QuantLib::close_enough(cpnRatio.at(0), 1.0);
                if (cashflowStatus[i] == CashflowStatus::Cached) {
                    if (isBrokenCoupon) {
                        provisionalNpv += cache[i] * cpnRatio;
                    } else {
                        underlyingNpv += cache[i];
                        cache[i].clear();
                        cashflowStatus[i] = CashflowStatus::Done;
                    }
                } else if (cashflows[i].canBeEstimated(t_from)) {
                    if (isBrokenCoupon) {
                        cache[i] = cashflows[i].pv(lgm, t_from, state, discountCurve_);
                        cashflowStatus[i] = CashflowStatus::Cached;
                        provisionalNpv += cache[i] * cpnRatio;
                    } else {
                        underlyingNpv += cashflows[i].pv(lgm, t_from, state, discountCurve_);
                        cashflowStatus[i] = CashflowStatus::Done;
                    }
                } else {
                    provisionalNpv += cashflows[i].pv(lgm, t_from, state, discountCurve_) * cpnRatio;
                }
            } else if (cashflows[i].mustBeEstimated(t_from) && cashflowStatus[i] == CashflowStatus::Open) {
                cache[i] = cashflows[i].pv(lgm, t_from, state, discountCurve_);
                cashflowStatus[i] = CashflowStatus::Cached;
            }
        }

        // process optionality

        if (optionTimes.find(t_from) != optionTimes.end()) {
            auto rebateNpv =
                getRebatePv(lgm, t_from, state, discountCurve_, rebatedExercise,
                            exercise_->type() == Exercise::American ? Null<Date>() : optionDates.at(t_from));
            optionNpv = max(optionNpv, underlyingNpv + provisionalNpv + rebateNpv);
        }

        // roll back

        if (t_from != t_to) {
            underlyingNpv = solver_->rollback(underlyingNpv, t_from, t_to);
            optionNpv = solver_->rollback(optionNpv, t_from, t_to);
            for (auto& c : cache) {
                if (!c.initialised())
                    continue;
                c = solver_->rollback(c, t_from, t_to);
            }
            // need to roll back provisionalNpv only for the last step t_1 -> t_0 = 0
            if (it == std::next(timeGrid.rend(), -1))
                provisionalNpv = solver_->rollback(provisionalNpv, t_from, t_to);
        }
    }

    /* Set the results */

    npv_ = optionNpv.at(0);
    underlyingNpv_ = underlyingNpv.at(0);
    for (auto const& c : cache) {
        if (c.initialised())
            underlyingNpv_ += c.at(0);
    }
    underlyingNpv_ += provisionalNpv.at(0);

    additionalResults_ = getAdditionalResultsMap(solver_->model()->getCalibrationInfo());

    if (rebatedExercise) {
        for (Size i = 0; i < rebatedExercise->dates().size(); ++i) {
            std::ostringstream d;
            d << QuantLib::io::iso_date(rebatedExercise->dates()[i]);
            additionalResults_["exerciseFee_" + d.str()] = -rebatedExercise->rebate(i);
        }
    }

} // NumericLgmMultiLegOptionEngineBase::calculate()

NumericLgmMultiLegOptionEngine::NumericLgmMultiLegOptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                               const Real sy, const Size ny, const Real sx,
                                                               const Size nx,
                                                               const Handle<YieldTermStructure>& discountCurve,
                                                               const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(model, sy, ny, sx, nx),
                                         discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

NumericLgmMultiLegOptionEngine::NumericLgmMultiLegOptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                               const Real maxTime, const QuantLib::FdmSchemeDesc scheme,
                                                               const Size stateGridPoints, const Size timeStepsPerYear,
                                                               const Real mesherEpsilon,
                                                               const Handle<YieldTermStructure>& discountCurve,
                                                               const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(
          QuantLib::ext::make_shared<LgmFdSolver>(model, maxTime, scheme, stateGridPoints, timeStepsPerYear, mesherEpsilon),
          discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

void NumericLgmMultiLegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_ = arguments_.payer;
    currency_ = arguments_.currency;
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    NumericLgmMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.underlyingNpv = underlyingNpv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
} // NumericLgmSwaptionEngine::calculate

NumericLgmSwaptionEngine::NumericLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                   const Real sy, const Size ny, const Real sx, const Size nx,
                                                   const Handle<YieldTermStructure>& discountCurve,
                                                   const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(model, sy, ny, sx, nx),
                                         discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

NumericLgmSwaptionEngine::NumericLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                   const Real maxTime, const QuantLib::FdmSchemeDesc scheme,
                                                   const Size stateGridPoints, const Size timeStepsPerYear,
                                                   const Real mesherEpsilon,
                                                   const Handle<YieldTermStructure>& discountCurve,
                                                   const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(
          QuantLib::ext::make_shared<LgmFdSolver>(model, maxTime, scheme, stateGridPoints, timeStepsPerYear, mesherEpsilon),
          discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

void NumericLgmSwaptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    NumericLgmMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
} // NumericLgmSwaptionEngine::calculate

NumericLgmNonstandardSwaptionEngine::NumericLgmNonstandardSwaptionEngine(
    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<YieldTermStructure>& discountCurve, const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(model, sy, ny, sx, nx),
                                         discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

NumericLgmNonstandardSwaptionEngine::NumericLgmNonstandardSwaptionEngine(
    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real maxTime, const QuantLib::FdmSchemeDesc scheme,
    const Size stateGridPoints, const Size timeStepsPerYear, const Real mesherEpsilon,
    const Handle<YieldTermStructure>& discountCurve, const Size americanExerciseTimeStepsPerYear)
    : NumericLgmMultiLegOptionEngineBase(
          QuantLib::ext::make_shared<LgmFdSolver>(model, maxTime, scheme, stateGridPoints, timeStepsPerYear, mesherEpsilon),
          discountCurve, americanExerciseTimeStepsPerYear) {
    registerWith(solver_->model());
    registerWith(discountCurve_);
}

void NumericLgmNonstandardSwaptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    NumericLgmMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
} // NumericLgmSwaptionEngine::calculate

} // namespace QuantExt
