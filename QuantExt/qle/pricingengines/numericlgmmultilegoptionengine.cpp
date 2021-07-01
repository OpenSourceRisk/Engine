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

#include <orepscriptedtrade/ored/pricingengines/numericlgmswaptionengineplus.hpp>

#include <qle/instruments/rebatedexercise.hpp>

#include <ql/payoff.hpp>

namespace QuantExt {

namespace {
// return simulation date for cashflow c or null if c is not relevant (pay date <= today)
Date getSimulationDate(const Date& today, const Date& latestOptionDate, const boost::shared_ptr<CashFlow>& c) {}

// true if cashflow is part of the exercise right
bool isCashflowRelevantForExercise(const Date& today, const Date& exercise, const boost::shared_ptr<CashFlow>& c) {}

} // namespace

NumericLgmMultiLegOptionEnginePlus::NumericLgmMultiLegOptionEnginePlus(
    const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<YieldTermStructure>& discountCurve)
    : LgmConvolutionSolver2(model, sy, ny, sx, nx), discountCurve_(discountCurve) {
    registerWith(LgmConvolutionSolver2::model());
    registerWith(discountCurve_);
}

Real NumericLgmMultiLegOptionEnginePlus::calculate() const {

    // TODO ParYieldCurve cash-settled swaptions are priced as if CollateralizedCashPrice, this can be refined

    // the reference date of the valuation

    Date refDate = model()->parametrization()->termStructure()->referenceDate();

    // we might have to handle an exercise with rebate

    auto rebatedExercise = boost::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);

    /* check that the engine can handle the instrument, i.e. that there is a unique pay currency and all interest rate
       indices are in this same currency */

    for (Size i = 1; i < arguments_.currency_.size(); ++i) {
        QL_REQUIRE(arguments_.currency[i] == arguments_.currency[0],
                   "NumericLgmMultilegOptionEngine: can only handle single currency underlyings, got at least two pay "
                   "currencies: "
                       << arguments_.currency[0] << "; " << arguments_.currency[i]);
    }

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        for (Size j = 0; j < arguments_.legs[i].size(); ++j) {
            if (auto cpn = boost::dynamic_pointer_cast<FloatingRateCoupon>(arguments_.legs[i][j])) {
                QL_REQUIRE(cpn->index()->currency() == arguments_.currency[0],
                           "NumericLgmMultilegOptionEngine: can only handle indices ("
                               << cpn->index()->name() << ") with same currency as unqiue pay currency ("
                               << arguments_.currency[0]);
            }
        }
    }

    /* map an option date to the set of relevant cashflows to exercise into, the set will not contain cashflows
       that are relevant for later option dates */

    std::map<Date, std::set<std::pair<Size, Size>>> optionDates;

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        for (Size j = 0; j < arguments_.legs[i].size(); ++j) {
            for (Size k = exercise_->dates().size(); k > 0; --k) {
                Date exerciseDate = arguments_.exercise_->dates()[k - 1];
                if (cashflowIsRelevantForExercise(arguments_.legs[i][j], exerciseDate))
                    optionDates[exerciseDates].insert(std::make_pair(i, j));
                break;
            }
        }
    }

    /* map a simulation date => cashflows that are determined on that date */

    std::map<Date, std::set<std::pair<Size, Size>>> cashflowDates;

    /* Each underlying coupon has a date on which we can efficiently calculate its amount. We have to approximate
       certain coupon amounts due to the 1d backward pricing approach, which
       - does not allow to access relevant fixing values before the option date
       - can not handle path-dependent payoffs
       In this step we associate a simulation date to each coupon. */

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        for (Size j = 0; j < arguments_.legs[i].size(); ++j) {
            Date latestOptionDate = Date::minDate();
            for (auto const& d : optionDates) {
                if (d->second.find(std::make_pair(i, j)) != d->second.end())
                    latestOptionDate = std::max(latestOptionDate, d->first);
            }
            Date d = getSimulationDate(refDate, latestOptionDate, arguments_.legs[i][j]);
            if (d != Null<Date>())
                cashflowDates[d].insert(std::make_pair(i, j));
        }
    }

    /* Compile the list of simulation dates */

    std::set<Date> simulationDates;

    for (auto const& d : optionDates)
        simluationDates.insert(d.first);

    for (auto const& d : couponSimulationDates)
        simulationDates.insert(d.first);

    /* Step backwards through simulation dates and compute the option npv */

    LgmVectorised lgm(model()->parametrization());

    RandomVariable optionNpv(gridSize(), 0.0);
    RandomVariable underlyingNpv1(gridSize(), 0.0);
    std::map<std::pair<Size, Size>> underlyingNpv2(gridSize(), 0.0);

    for (auto it = simulationDates.rbegin(); it != simulationDates.rend(); ++it) {

        auto option = optionDates.find(*it);
        auto cashflow = cashflowDates.find(*it);

        // collect cashflow amounts on simulation date

        if (cashflow != cashflowDates.end()) {
            for (auto const& cf : cashflow->second) {
                underlyingNpv2[cf.first] = getUnderlyingCashflowAmount(cf.first);
            }
        }

        // handle option date

        if (option != optionDates.end()) {

            // transfer relevant cashflow amounts to exercise into npv

            for (auto const& cf : option->second) {
                underlyingNpv1 += underlyingNpv2[cf.first];
                underyingNpv2.erase(cf.first);
            }

            // update the option value

            optionNpv = max(optionNpv, underlyingNpv1);
        }

        // rollback

        underlyingNpv = rollback(underlyingNpv, t_from, t);
        optionValue = rollback(optionValue, t_from, t);
    }

    /* Set the results */

    results_.value = optionNpv;

} // calculate()

} // namespace QuantExt
