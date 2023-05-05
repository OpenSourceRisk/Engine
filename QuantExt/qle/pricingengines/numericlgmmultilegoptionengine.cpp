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
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/payoff.hpp>

namespace QuantExt {

using namespace QuantLib;

#define THROW_ERROR_CPN_NOT_SUPPORTED                                                                                  \
    QL_FAIL("NumericLgmMultiLegOptionEngineBase: coupon type not handled, supported coupon types: Fix, (capfloored) "  \
            "Ibor, (capfloored) ON comp, (capfloored) ON avg, BMA/SIFMA")

/* For a given cashflow and option exercise date, return the simulation date on which we determine the coupon amout.
   - If the pay date is <= today, null is returned.
   - The latestOptionDate is the latest option date for which the cashflow is relevant. This can be Date::minDate(),
     if the cashflow is not relevant for any option date.
   The return value will be >= today (or null) always. */
Date getSimulationDates(const Date& today, const Date& latestOptionDate, const boost::shared_ptr<CashFlow>& c) {
    if (c->date() <= today)
        return Date();
    Date minDate = std::max(today, latestOptionDate);
    if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
        if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(c)) {
            return minDate;
        } else if (auto fix = boost::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
            return minDate;
        } else if (auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn)) {
            return minDate;
        } else if (auto av = boost::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(cpn)) {
            return minDate;
        } else if (auto av = boost::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(cpn)) {
            return minDate;
        } else if (auto cf = boost::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(cpn)) {
            auto und = cf->underlying();
            if (auto cfibor = boost::dynamic_pointer_cast<QuantLib::IborCoupon>(und)) {
                return std::max(minDate, und->fixingDate());
            }
        } else if (auto cfon = boost::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(cpn)) {
            return std::max(minDate, cfon->underlying()->fixingDates().front());
        } else if (auto cfav = boost::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(cpn)) {
            return std::max(minDate, cfav->underlying()->fixingDates().front());
        }
        THROW_ERROR_CPN_NOT_SUPPORTED;
    } else {
        return minDate;
    }
}

/* For a given cashflow and exercise date, return true if cashflow is part of the exercise right, otherwise false.
   If exercise dates <= today or null, false is returned. */
bool isCashflowRelevantForExercise(const Date& today, const Date& exercise, const boost::shared_ptr<CashFlow>& c) {
    if (exercise <= today)
        return false;
    if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
        /* In case we have a coupon, it belongs to the exercise right if and only if exercise <= accrual start,
           this is a definition. */
        return exercise <= cpn->accrualStartDate();
    } else {
        /* In case we have a cashflow only, it belongs to the exercise right, if and only if exercise <= pay date,
           again, this is a definition */
        return exercise <= c->date();
    }
}

/* Get the model-estimate of a cashflow as observed from model time t, state x, deflated by model numeraire */
RandomVariable getUnderlyingCashflowPv(const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                                       const Handle<YieldTermStructure>& discountCurve,
                                       const boost::shared_ptr<CashFlow>& c) {
    Real T = lgm.parametrization()->termStructure()->timeFromReference(c->date());
    if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
        if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(c)) {
            return (RandomVariable(x.size(), ibor->gearing()) * lgm.fixing(ibor->index(), ibor->fixingDate(), t, x) +
                    RandomVariable(x.size(), ibor->spread())) *
                   RandomVariable(x.size(), ibor->accrualPeriod() * ibor->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto fix = boost::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
            return RandomVariable(x.size(), fix->amount()) * lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn)) {
            return lgm.compoundedOnRate(boost::dynamic_pointer_cast<OvernightIndex>(on->index()), on->fixingDates(),
                                        on->valueDates(), on->dt(), on->rateCutoff(), on->includeSpread(), on->spread(),
                                        on->gearing(), on->lookback(), on->dayCounter(),
                                        Null<Real>(), Null<Real>(), false, false, t, x) *
                   RandomVariable(x.size(), on->accrualPeriod() * on->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto av = boost::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(cpn)) {
            return lgm.averagedOnRate(boost::dynamic_pointer_cast<OvernightIndex>(av->index()), av->fixingDates(),
                                      av->valueDates(), av->dt(), av->rateCutoff(), false, av->spread(), av->gearing(),
                                      av->lookback(), av->dayCounter(), Null<Real>(), Null<Real>(),
                                      false, false, t, x) *
                   RandomVariable(x.size(), av->accrualPeriod() * av->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto bma = boost::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(cpn)) {
            return (RandomVariable(x.size(), bma->gearing()) *
                        lgm.averagedBmaRate(boost::dynamic_pointer_cast<BMAIndex>(bma->index()), bma->fixingDates(),
                                            bma->accrualStartDate(), bma->accrualEndDate(), t, x) +
                    RandomVariable(x.size(), bma->spread())) *
                   RandomVariable(x.size(), bma->accrualPeriod() * bma->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto cf = boost::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(cpn)) {
            auto und = cf->underlying();
            RandomVariable cap(x.size(), cf->cap() == Null<Real>() ? QL_MAX_REAL : cf->cap());
            RandomVariable floor(x.size(), cf->floor() == Null<Real>() ? -QL_MAX_REAL : cf->floor());
            if (auto undibor = boost::dynamic_pointer_cast<QuantLib::IborCoupon>(und)) {
                return max(floor, min(cap, (RandomVariable(x.size(), undibor->gearing()) *
                                                lgm.fixing(undibor->index(), undibor->fixingDate(), t, x) +
                                            RandomVariable(x.size(), undibor->spread())))) *
                       RandomVariable(x.size(), undibor->accrualPeriod() * undibor->nominal()) *
                       lgm.reducedDiscountBond(t, T, x, discountCurve);
            }
        } else if (auto cfon = boost::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(cpn)) {
            auto und = cfon->underlying();
            return lgm.compoundedOnRate(boost::dynamic_pointer_cast<OvernightIndex>(und->index()), und->fixingDates(),
                                        und->valueDates(), und->dt(), und->rateCutoff(), und->includeSpread(),
                                        und->spread(), und->gearing(), und->lookback(),
                                        und->dayCounter(), cfon->cap(), cfon->floor(), cfon->localCapFloor(),
                                        cfon->nakedOption(), t, x) *
                   RandomVariable(x.size(), cfon->accrualPeriod() * cfon->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto cfav = boost::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(cpn)) {
            auto und = cfav->underlying();
            return lgm.averagedOnRate(boost::dynamic_pointer_cast<OvernightIndex>(und->index()), und->fixingDates(),
                                      und->valueDates(), und->dt(), und->rateCutoff(), cfav->includeSpread(),
                                      und->spread(), und->gearing(), und->lookback(),
                                      und->dayCounter(), cfav->cap(), cfav->floor(), cfav->localCapFloor(),
                                      cfav->nakedOption(), t, x) *
                   RandomVariable(x.size(), cfav->accrualPeriod() * cfav->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        }
        THROW_ERROR_CPN_NOT_SUPPORTED;
    } else {
        return RandomVariable(x.size(), c->amount()) * lgm.reducedDiscountBond(t, T, x, discountCurve);
    }
}

/* Get the rebate amount on an exercise  */
RandomVariable getRebatePv(const LgmVectorised& lgm, const Real t, const RandomVariable& x,
                           const Handle<YieldTermStructure>& discountCurve,
                           const boost::shared_ptr<RebatedExercise>& exercise, const Date& d) {
    if (exercise == nullptr)
        return RandomVariable(x.size(), 0.0);
    auto f = std::find(exercise->dates().begin(), exercise->dates().end(), d);
    QL_REQUIRE(f != exercise->dates().end(), "NumericLgmMultiLegOptionEngine: internal error: exercise date "
                                                 << d << " from rebate payment not found amount exercise dates.");
    Size index = std::distance(exercise->dates().begin(), f);
    return RandomVariable(x.size(), exercise->rebate(index)) *
           lgm.reducedDiscountBond(
               t, lgm.parametrization()->termStructure()->timeFromReference(exercise->rebatePaymentDate(index)), x);
}

NumericLgmMultiLegOptionEngineBase::NumericLgmMultiLegOptionEngineBase(
    const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<YieldTermStructure>& discountCurve)
    : LgmConvolutionSolver2(model, sy, ny, sx, nx), discountCurve_(discountCurve) {}

void NumericLgmMultiLegOptionEngineBase::calculate() const {

    /* TODO ParYieldCurve cash-settled swaptions are priced as if CollateralizedCashPrice, this can be refined. */

    /* The reference date of the valuation. Exercise dates <= refDate are treated as if not present, i.e. it is
       assumed that these rights were not exercised.
       TODO we should add exercise data to indicate past exercises and value outstanding cash settlement amounts,
       or - possibly (?) - the physical underlying in that we exercised into? Or would that be a separate trade? */

    Date refDate = model()->parametrization()->termStructure()->referenceDate();

    /* we might have to handle an exercise with rebate, init a variable for that (null if not applicable) */

    auto rebatedExercise = boost::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);

    /* Check that the engine can handle the instrument, i.e. that there is a unique pay currency and all interest rate
       indices are in this same currency. */

    for (Size i = 1; i < currency_.size(); ++i) {
        QL_REQUIRE(currency_[i] == currency_[0],
                   "NumericLgmMultilegOptionEngine: can only handle single currency underlyings, got at least two pay "
                   "currencies: "
                       << currency_[0] << "; " << currency_[i]);
    }

    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            if (auto cpn = boost::dynamic_pointer_cast<FloatingRateCoupon>(legs_[i][j])) {
                QL_REQUIRE(cpn->index()->currency() == currency_[0],
                           "NumericLgmMultilegOptionEngine: can only handle indices ("
                               << cpn->index()->name() << ") with same currency as unqiue pay currency ("
                               << currency_[0]);
            }
        }
    }

    /* Map an option date to the set of relevant cashflows to exercise into, the set will not contain cashflows
       that are relevant for later option dates. */

    std::map<Date, std::set<std::pair<Size, Size>>> optionDates;

    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            for (Size k = exercise_->dates().size(); k > 0; --k) {
                Date exerciseDate = exercise_->dates()[k - 1];
                if (exerciseDate > refDate) {
                    if (isCashflowRelevantForExercise(refDate, exerciseDate, legs_[i][j])) {
                        optionDates[exerciseDate].insert(std::make_pair(i, j));
                        break;
                    }
                }
            }
        }
    }

    /* map a actual simulation date => set of (cf leg, cf number) */

    std::map<Date, std::set<std::pair<Size, Size>>> cashflowDates;

    /* Each underlying cashflow has a date on which we can efficiently calculate its amount, depending on
       the option date for which the cashflow is relevant. We estimate a cashflow amount for the latest
       option date it is relevant for and then roll it back. */

    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            Date latestOptionDate = Date::minDate();
            for (auto const& d : optionDates) {
                if (d.second.find(std::make_pair(i, j)) != d.second.end())
                    latestOptionDate = std::max(latestOptionDate, d.first);
            }
            auto d = getSimulationDates(refDate, latestOptionDate, legs_[i][j]);
            if (d != Null<Date>())
                cashflowDates[d].insert(std::make_pair(i, j));
        }
    }

    /* Compile the list of simulation dates, include the ref date always */

    std::set<Date> simulationDates;

    for (auto const& d : optionDates)
        simulationDates.insert(d.first);

    for (auto const& d : cashflowDates)
        simulationDates.insert(d.first);

    simulationDates.insert(refDate);

    /* Step backwards through simulation dates and compute the option npv */

    LgmVectorised lgm(model()->parametrization());

    // the current option npv that we roll back through the grid
    RandomVariable optionNpv(gridSize(), 0.0);
    // cashflow npvs that are part of the latest exercise into option
    RandomVariable underlyingNpv1(gridSize(), 0.0);
    // cashflow npvs that are estimated, but not yet part of the latest exercise into option
    std::map<std::pair<Size, Size>, RandomVariable> underlyingNpv2;

    for (auto it = simulationDates.rbegin(); it != simulationDates.rend(); ++it) {

        Real t_from = model()->parametrization()->termStructure()->timeFromReference(*it);
        Real t_to = (it != std::next(simulationDates.rend(), -1))
                        ? model()->parametrization()->termStructure()->timeFromReference(*std::next(it, 1))
                        : t_from;

        RandomVariable state = stateGrid(t_from);

        auto option = optionDates.find(*it);
        auto cashflow = cashflowDates.find(*it);

        // collect cashflow amounts on simulation date

        if (cashflow != cashflowDates.end()) {
            for (auto const& cf : cashflow->second) {
                auto tmp = getUnderlyingCashflowPv(lgm, t_from, state, discountCurve_, legs_[cf.first][cf.second]) *
                           RandomVariable(gridSize(), payer_[cf.first]);
                auto it = underlyingNpv2.find(cf);
                if (it != underlyingNpv2.end())
                    it->second += tmp;
                else
                    underlyingNpv2[cf] = tmp;
            }
        }

        // handle option date

        if (option != optionDates.end()) {

            // transfer relevant cashflow amounts to exercise into npv

            for (auto const& cf : option->second) {
                underlyingNpv1 += underlyingNpv2[cf];
                underlyingNpv2.erase(cf);
            }

            // update the option value (exercise value including rebate vs. continuation value)

            auto rebateNpv = getRebatePv(lgm, t_from, state, discountCurve_, rebatedExercise, option->first);
            optionNpv = max(optionNpv, underlyingNpv1 + rebateNpv);
        }

        // rollback

        if (t_from != t_to) {
            underlyingNpv1 = rollback(underlyingNpv1, t_from, t_to);
            for (auto& c : underlyingNpv2) {
                c.second = rollback(c.second, t_from, t_to);
            }
            optionNpv = rollback(optionNpv, t_from, t_to);
        }
    }

    /* Set the results */

    npv_ = optionNpv.at(0);
    underlyingNpv_ = underlyingNpv1.at(0);
    for (auto const& c : underlyingNpv2) {
        underlyingNpv_ += c.second.at(0);
    }

    additionalResults_ = getAdditionalResultsMap(model()->getCalibrationInfo());

    if (rebatedExercise) {
        for (Size i = 0; i < rebatedExercise->dates().size(); ++i) {
            std::ostringstream d;
            d << QuantLib::io::iso_date(rebatedExercise->dates()[i]);
            additionalResults_["exerciseFee_" + d.str()] = -rebatedExercise->rebate(i);
        }
    }

} // NumericLgmMultiLegOptionEngineBase::calculate()

NumericLgmMultiLegOptionEngine::NumericLgmMultiLegOptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                               const Real sy, const Size ny, const Real sx,
                                                               const Size nx,
                                                               const Handle<YieldTermStructure>& discountCurve)
    : NumericLgmMultiLegOptionEngineBase(model, sy, ny, sx, nx, discountCurve) {
    registerWith(LgmConvolutionSolver2::model());
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

NumericLgmSwaptionEngine::NumericLgmSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                   const Real sy, const Size ny, const Real sx, const Size nx,
                                                   const Handle<YieldTermStructure>& discountCurve)
    : NumericLgmMultiLegOptionEngineBase(model, sy, ny, sx, nx, discountCurve) {
    registerWith(LgmConvolutionSolver2::model());
    registerWith(discountCurve_);
}

void NumericLgmSwaptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_ = arguments_.payer;
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
    const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<YieldTermStructure>& discountCurve)
    : NumericLgmMultiLegOptionEngineBase(model, sy, ny, sx, nx, discountCurve) {
    registerWith(LgmConvolutionSolver2::model());
    registerWith(discountCurve_);
}

void NumericLgmNonstandardSwaptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_ = arguments_.payer;
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
