/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepscriptedtrade/ored/pricingengines/numericlgmswaptionengineplus.hpp>

#include <qle/instruments/rebatedexercise.hpp>

#include <ql/payoff.hpp>

namespace QuantExt {

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

    // initialise the index of the current exercise date

    int optionIndex = static_cast<int>(exercise_->dates().size()) - 1;

    // the minimum relevant exercise index

    int minOptionIndexAlive = static_cast<int>(
        std::upper_bound(exercise_->dates().begin(), exercise_->dates().end(), refDate) - exercise_->dates().begin());

    // setup the vectorised LGM model which we use for the calculations below

    LgmVectorised lgm(model()->parametrization());

    // the npv of all coupons we have handled so far
    RandomVariable underlyingNpv(gridSize(), 0.0);

    // the current option value
    RandomVariable optionValue(gridSize(), 0.0);

    // we might have to handle an exercise with rebate
    auto rebatedExercise = boost::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);

    while (optionIndex >= minOptionIndexAlive) {

        // compute conditional PV of coupons as seen from the current exercise date that are
        // a) part of the current exercise into right (i.e. the coupon accrual start dates >= exercise date)
        // b) are not part of future exercise into rights (those are incorporated into underlyingNpv already)

        // current exercise time
        Real t = model()->parametrization()->termStructure()->timeFromReference(exercise_->dates()[optionIndex]);

        // state grid at the exercise time
        RandomVariable x = stateGrid(t);

        // rebate value for this exercise
        RandomVariable rebateValue(x.size(), 0.0);
        if (rebatedExercise) {
            Real T = model()->parametrization()->termStructure()->timeFromReference(
                rebatedExercise->rebatePaymentDate(optionIndex));
            rebateValue =
                RandomVariable(gridSize(), rebatedExercise->rebate(optionIndex)) * lgm.reducedDiscountBond(t, T, x);
        }

        // npv of the coupons we are handling for this exercise date
        RandomVariable currentCouponNpv(x.size(), 0.0);

        // handle floating coupons (as received at this point)
        while (floatingCouponIndex >= 0 &&
               floatingResetDates_[floatingCouponIndex] >= exercise_->dates()[optionIndex]) {
            Real T =
                model()->parametrization()->termStructure()->timeFromReference(floatingPayDates_[floatingCouponIndex]);
            // the floating fixing date might fall before the exericse date, in this case we apply the modelling
            // assumption that the original fixing can be approximated by the fixing on the exercise date
            Date effectiveFixingDate = iborIndex_->fixingCalendar().adjust(
                std::max(floatingFixingDates_[floatingCouponIndex], exercise_->dates()[optionIndex]));
            currentCouponNpv += RandomVariable(x.size(), floatingNominal_[floatingCouponIndex] *
                                                             floatingAccrualTimes_[floatingCouponIndex]) *
                                (RandomVariable(x.size(), floatingSpreads_[floatingCouponIndex]) +
                                 lgm.fixing(iborIndex_, effectiveFixingDate, t, x)) *
                                lgm.reducedDiscountBond(t, T, x, discountCurve_);
            --floatingCouponIndex;
        }

        // handle fixed coupons (as paid at this point)
        while (fixedCouponIndex >= 0 && fixedResetDates_[fixedCouponIndex] >= exercise_->dates()[optionIndex]) {
            Real T = model()->parametrization()->termStructure()->timeFromReference(fixedPayDates_[fixedCouponIndex]);
            currentCouponNpv -= RandomVariable(x.size(), fixedCoupons_[fixedCouponIndex]) *
                                lgm.reducedDiscountBond(t, T, x, discountCurve_);
            --fixedCouponIndex;
        }

        // roll back
        // a) previously computed coupon npvs (if any) from the next exercise date to the current one
        // b) the current option value
        if (optionIndex < static_cast<int>(exercise_->dates().size()) - 1) {
            Real t_from =
                model()->parametrization()->termStructure()->timeFromReference(exercise_->dates()[optionIndex + 1]);
            underlyingNpv = rollback(underlyingNpv, t_from, t);
            optionValue = rollback(optionValue, t_from, t);
        }

        // add the npv of the newly added coupons for this exercise date
        underlyingNpv += currentCouponNpv;

        // update the option value (exercise or continue)
        optionValue =
            max(optionValue,
                RandomVariable(x.size(), swapType_ == VanillaSwap::Payer ? 1.0 : -1.0) * underlyingNpv + rebateValue);

        // go to next option date (back in time)
        --optionIndex;
    }

    // last roll back from first alive exercise date to the reference date
    Real t = model()->parametrization()->termStructure()->timeFromReference(exercise_->dates()[minOptionIndexAlive]);
    optionValue = rollback(optionValue, t, 0.0);

    // return the result
    return optionValue[0];
} // NumericLgmSwaptionEngineBase::calculate

} // namespace QuantExt
