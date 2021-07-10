/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/instruments/rebatedexercise.hpp>
#include <qle/pricingengines/numericlgmswaptionengine.hpp>

#include <ql/payoff.hpp>

using std::vector;

namespace QuantExt {

NumericLgmSwaptionEngineBase::NumericLgmSwaptionEngineBase(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                           const Real sy, const Size ny, const Real sx, const Size nx,
                                                           const Handle<YieldTermStructure>& discountCurve)
    : LgmConvolutionSolver(model, sy, ny, sx, nx) {}

Real NumericLgmSwaptionEngineBase::rebatePv(const Real x, const Real t, const Size exerciseIndex) const {
    boost::shared_ptr<QuantExt::RebatedExercise> rebatedExercise =
        boost::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);
    if (rebatedExercise) {
        return rebatedExercise->rebate(exerciseIndex) *
               model()->discountBond(t,
                                     model()->parametrization()->termStructure()->timeFromReference(
                                         rebatedExercise->rebatePaymentDate(exerciseIndex)),
                                     x) /
               model()->numeraire(t, x);
    } else {
        return 0.0;
    }
}

Real NumericLgmSwaptionEngineBase::calculate() const {

    iborModelCurve_ = boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model(), iborIndex_->forwardingTermStructure(),
                                                                       DayCounter(), false, true);

    iborIndexCorrected_ = iborIndex_->clone(Handle<YieldTermStructure>(iborModelCurve_));

    Date settlement = model()->parametrization()->termStructure()->referenceDate();

    if (exercise_->dates().back() <= settlement) { // swaption is expired, possibly generated swap is not
                                                   // valued
        return 0.0;
    }

    int idx = static_cast<int>(exercise_->dates().size()) - 1;
    int minIdxAlive =
        static_cast<int>(std::upper_bound(exercise_->dates().begin(), exercise_->dates().end(), settlement) -
                         exercise_->dates().begin());

    int options = idx - minIdxAlive + 1;

    // terminal payoff

    Real t =
        model()->parametrization()->termStructure()->timeFromReference(exercise_->dates()[minIdxAlive + options - 1]);
    std::vector<Real> x = stateGrid(t);
    std::vector<Real> v(x.size());
    for (Size k = 0; k < x.size(); ++k) {
        v[k] = std::max(conditionalSwapValue(x[k], t, exercise_->dates()[minIdxAlive + options - 1]) +
                            rebatePv(x[k], t, minIdxAlive + options - 1),
                        0.0);
    }

    // roll back

    for (int j = options - 1; j > 0; --j) {
        Real t_to =
            model()->parametrization()->termStructure()->timeFromReference(exercise_->dates()[minIdxAlive + j - 1]);
        v = rollback(v, t, t_to);
        x = stateGrid(t_to);
        for (Size k = 0; k < x.size(); ++k) {
            QL_REQUIRE(v[k] > 0 || close_enough(v[k], 0.0), "negative value in rollback: " << v[k]);
            // choose: continue or exercise
            v[k] = std::max(v[k], conditionalSwapValue(x[k], t_to, exercise_->dates()[minIdxAlive + j - 1]) +
                                      rebatePv(x[k], t_to, minIdxAlive + j - 1));
        }
        t = t_to;
    }
    v = rollback(v, t, 0.0);
    return v[0];

} // NumericLgmSwaptionEngineBase::calculate

std::map<std::string, boost::any> NumericLgmSwaptionEngineBase::additionalResults() const {
    return getAdditionalResultsMap(model()->getCalibrationInfo());
}

void NumericLgmSwaptionEngine::calculate() const {
    // TODO ParYieldCurve cash-settled swaptions are priced as if CollateralizedCashPrice, this can be refined
    iborIndex_ = arguments_.swap->iborIndex();
    exercise_ = arguments_.exercise;
    results_.value = NumericLgmSwaptionEngineBase::calculate();
    auto tmp = NumericLgmSwaptionEngineBase::additionalResults();
    results_.additionalResults.insert(tmp.begin(), tmp.end());
} // NumericLgmSwaptionEngine::calculate

Real NumericLgmSwaptionEngine::conditionalSwapValue(Real x, Real t, const Date expiry0) const {
    Schedule fixedSchedule = arguments_.swap->fixedSchedule();
    Schedule floatSchedule = arguments_.swap->floatingSchedule();
    Size j1 = std::upper_bound(fixedSchedule.dates().begin(), fixedSchedule.dates().end(), expiry0 - 1) -
              fixedSchedule.dates().begin();
    Size k1 = std::upper_bound(floatSchedule.dates().begin(), floatSchedule.dates().end(), expiry0 - 1) -
              floatSchedule.dates().begin();

    iborModelCurve_->move(expiry0, x);

    Real floatingLegNpv = 0.0;
    for (Size l = k1; l < arguments_.floatingCoupons.size(); l++) {
        Real T = model()->parametrization()->termStructure()->timeFromReference(arguments_.floatingPayDates[l]);
        floatingLegNpv +=
            arguments_.nominal * arguments_.floatingAccrualTimes[l] *
            (arguments_.floatingSpreads[l] + iborIndexCorrected_->fixing(arguments_.floatingFixingDates[l])) *
            model()->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Real fixedLegNpv = 0.0;
    for (Size l = j1; l < arguments_.fixedCoupons.size(); l++) {
        Real T = model()->parametrization()->termStructure()->timeFromReference(arguments_.fixedPayDates[l]);
        fixedLegNpv += arguments_.fixedCoupons[l] * model()->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    Real exerciseValue = (type == Option::Call ? 1.0 : -1.0) * (floatingLegNpv - fixedLegNpv);
    return exerciseValue;
} // NumericLgmSwaptionEngine::conditionalSwapValue

void NumericLgmNonstandardSwaptionEngine::calculate() const {
    iborIndex_ = arguments_.swap->iborIndex();
    exercise_ = arguments_.exercise;
    results_.value = NumericLgmSwaptionEngineBase::calculate();
    auto tmp = NumericLgmSwaptionEngineBase::additionalResults();
    results_.additionalResults.insert(tmp.begin(), tmp.end());
} // NumericLgmSwaptionEngine::calculate

Real NumericLgmNonstandardSwaptionEngine::conditionalSwapValue(Real x, Real t, const Date expiry0) const {
    Schedule fixedSchedule = arguments_.swap->fixedSchedule();
    Schedule floatSchedule = arguments_.swap->floatingSchedule();
    Size j1 = std::upper_bound(arguments_.fixedResetDates.begin(), arguments_.fixedResetDates.end(), expiry0 - 1) -
              arguments_.fixedResetDates.begin();
    Size k1 =
        std::upper_bound(arguments_.floatingResetDates.begin(), arguments_.floatingResetDates.end(), expiry0 - 1) -
        arguments_.floatingResetDates.begin();

    iborModelCurve_->move(expiry0, x);

    Real floatingLegNpv = 0.0;
    for (Size l = k1; l < arguments_.floatingCoupons.size(); l++) {
        Real T = model()->parametrization()->termStructure()->timeFromReference(arguments_.floatingPayDates[l]);
        if (arguments_.floatingIsRedemptionFlow[l]) {
            floatingLegNpv += arguments_.floatingCoupons[l] * model()->reducedDiscountBond(t, T, x, discountCurve_);
        } else {
            floatingLegNpv +=
                arguments_.floatingNominal[l] * arguments_.floatingAccrualTimes[l] *
                (arguments_.floatingSpreads[l] +
                 arguments_.floatingGearings[l] * iborIndexCorrected_->fixing(arguments_.floatingFixingDates[l])) *
                model()->reducedDiscountBond(t, T, x, discountCurve_);
        }
    }

    Real fixedLegNpv = 0.0;
    for (Size l = j1; l < arguments_.fixedCoupons.size(); l++) {
        Real T = model()->parametrization()->termStructure()->timeFromReference(arguments_.fixedPayDates[l]);
        fixedLegNpv += arguments_.fixedCoupons[l] * model()->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    Real exerciseValue = (type == Option::Call ? 1.0 : -1.0) * (floatingLegNpv - fixedLegNpv);
    return exerciseValue;
} // NumericLgmSwaptionEngine::conditionalSwapValue

} // namespace QuantExt
