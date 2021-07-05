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

#include <qle/instruments/rebatedexercise.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/payoff.hpp>

namespace QuantExt {

using namespace QuantLib;

namespace {

/* For a given cashflow and option exercise date, return the
   - earliest
   - lastest
   - actual
   simulation date that can (or is, including approximations) used in the 1D backward scheme applied in this engine.
   The earliest and latest date should be determined, so that the amount can be esimated accurately. The actual date
   might include approximations though, this is what is used in the end in the engine to simulate the amount.
   If the pay date is <= today, (null, null, null) is returned. If the lastest option date is <= today or null, it is
   assumed, there is no relevant option date for this cashflow, but nevertheless the triplet of dates is returned.
*/
std::tuple<Date, Date, Date> getSimulationDates(const Date& today, const Date& latestOptionDate,
                                                const boost::shared_ptr<CashFlow>& c) {
    if (c->date() <= today)
        return std::make_tuple(Date(), Date(), Date());
    if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
        if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(c)) {
            /* Handle Ibor coupon */
            Date fixingDate = ibor->fixingDate();
            return std::make_tuple(fixingDate, fixingDate, std::max(latestOptionDate, fixingDate));
        } else if (auto fix = boost::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
            /* Handle Fixed Rate coupon */
            return std::make_tuple(today, fix->date(), latestOptionDate);
        } else {
            /* Other coupons are not supported at the moment */
            QL_FAIL("NumericLgmMultiLegOptionEngine: coupon type not handled, supported are Ibor and Fixed coupons");
        }
    } else {
        /* Handle non-coupon, i.e. a cashflow */
        return std::make_tuple(today, cpn->date(), latestOptionDate);
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
                                       const std::tuple<Date, Date, Date>& simulationDates,
                                       const boost::shared_ptr<CashFlow>& c) {
    Real T = lgm.parametrization()->termStructure()->timeFromReference(c->date());
    if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
        if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(c)) {
            /* Handle Ibor coupon */
            return lgm.fixing(ibor->index(), ibor->fixingDate(), t, x) *
                   RandomVariable(x.size(), ibor->accrualPeriod() * ibor->nominal()) *
                   lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else if (auto fix = boost::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
            /* Handle Fixed Rate coupon */
            return RandomVariable(x.size(), fix->amount()) * lgm.reducedDiscountBond(t, T, x, discountCurve);
        } else {
            /* Other coupons are not supported at the moment */
            QL_FAIL("NumericLgmMultiLegOptionEngine: coupon type not handled, supported are Ibor and Fixed coupons");
        }
    } else {
        /* Handle non-coupon, i.e. a cashflow */
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

} // namespace

NumericLgmMultiLegOptionEngineBase::NumericLgmMultiLegOptionEngineBase(
    const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<YieldTermStructure>& discountCurve)
    : LgmConvolutionSolver2(model, sy, ny, sx, nx), discountCurve_(discountCurve) {
    registerWith(LgmConvolutionSolver2::model());
    registerWith(discountCurve_);
}

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
                if (isCashflowRelevantForExercise(refDate, exerciseDate, legs_[i][j])) {
                    optionDates[exerciseDate].insert(std::make_pair(i, j));
                    break;
                }
            }
        }
    }

    /* map a actual simulation date => ((cf leg, cf number), (earliest sim date, latest sim date))  */

    std::map<Date, std::set<std::pair<std::pair<Size, Size>, std::tuple<Date, Date, Date>>>> cashflowDates;

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
            if (std::get<2>(d) != Null<Date>())
                cashflowDates[std::get<2>(d)].insert(std::make_pair(
                    std::make_pair(i, j), std::make_tuple(std::get<0>(d), std::get<1>(d), std::get<2>(d))));
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

    for (auto it = simulationDates.rbegin(); it != std::next(simulationDates.rend(), -1); ++it) {

        Real t_from = model()->parametrization()->termStructure()->timeFromReference(*it);
        Real t_to = model()->parametrization()->termStructure()->timeFromReference(*std::next(it, 1));

        RandomVariable state = stateGrid(t_from);

        auto option = optionDates.find(*it);
        auto cashflow = cashflowDates.find(*it);

        // collect cashflow amounts on simulation date

        if (cashflow != cashflowDates.end()) {
            Size i, j;
            for (auto const& cf : cashflow->second) {
                std::tie(i, j) = cf.first;
                auto tmp = getUnderlyingCashflowPv(lgm, t_from, state, discountCurve_, cf.second, legs_[i][j]) *
                           RandomVariable(gridSize(), payer_[i]);
                auto it = underlyingNpv2.find(cf.first);
                if (it != underlyingNpv2.end())
                    it->second += tmp;
                else
                    underlyingNpv2[cf.first] = tmp;
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

        underlyingNpv1 = rollback(underlyingNpv1, t_from, t_to);
        for (auto& c : underlyingNpv2) {
            c.second = rollback(c.second, t_from, t_to);
        }
        optionNpv = rollback(optionNpv, t_from, t_to);
    }

    /* Set the results */

    npv_ = optionNpv.at(0);
    underlyingNpv_ = underlyingNpv1.at(0);
    for (auto& c : underlyingNpv2) {
        underlyingNpv_ += c.second.at(0);
    }

    additionalResults_ = getAdditionalResultsMap(model()->getCalibrationInfo());

} // NumericLgmMultiLegOptionEngineBase::calculate()

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
