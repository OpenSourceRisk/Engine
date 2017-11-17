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

#include <qle/pricingengines/numericlgmswaptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/payoff.hpp>

using std::vector;

namespace QuantExt {

Real NumericLgmSwaptionEngineBase::calculate() const {

    iborModelCurve_ = boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_, iborIndex_->forwardingTermStructure());

    iborIndexCorrected_ = iborIndex_->clone(Handle<YieldTermStructure>(iborModelCurve_));

    Date settlement = model_->parametrization()->termStructure()->referenceDate();

    if (exercise_->dates().back() <= settlement) { // swaption is expired, possibly generated swap is not
                                                   // valued
        return 0.0;
    }

    int idx = static_cast<int>(exercise_->dates().size()) - 1;
    int minIdxAlive =
        static_cast<int>(std::upper_bound(exercise_->dates().begin(), exercise_->dates().end(), settlement) -
                         exercise_->dates().begin());

    int options = idx - minIdxAlive + 1;

    // x grid for each expiry

    vector<Real> te(options);       // time to expiry
    vector<Real> sigma(options);    // standard deviation of x
    vector<Real> dx(options);       // x-grid spacing
    Matrix x(options, 2 * mx_ + 1); // x-coordinate of grid points for each expiry
    Matrix u(options, 2 * mx_ + 1); // conditional underlying value in grid point i
    Matrix v(options, 2 * mx_ + 1); // conditional continuation value in grid point i
    for (int j = 0; j < options; j++) {
        te[j] = model_->parametrization()->termStructure()->timeFromReference(exercise_->dates()[minIdxAlive + j]);
        sigma[j] = sqrt(model_->parametrization()->zeta(te[j]));
        dx[j] = sigma[j] / nx_;
        for (int k = 0; k <= 2 * mx_; k++) {
            x[j][k] = dx[j] * (k - mx_);
            // Payoff goes here
            u[j][k] = conditionalSwapValue(x[j][k], te[j], exercise_->dates()[minIdxAlive + j]);
            // Continuation value is zero at final expiry.
            // This will be updated for j < jmax in the rollback loop
            v[j][k] = std::max(u[j][k], 0.0);
        }
    }

    // roll back

    for (int j = options - 1; j >= 0; j--) {
        if (j == 0) {
            Real value = 0.0;
            for (int i = 0; i <= 2 * my_; i++) {
                // Map y index to x index, not integer in general
                Real kp = y_[i] * sigma[j] / dx[j] + mx_;
                // Adjacent integer x index <= k
                int kk = int(floor(kp));
                // Get value at kp by linear interpolation on
                // kk <= kp <= kk + 1 with flat extrapolation
                Real vp;
                if (kk < 0)
                    vp = v[0][0];
                else if (kk + 1 > 2 * mx_)
                    vp = v[0][2 * mx_];
                else
                    vp = (kp - kk) * v[0][kk + 1] + (1.0 + kk - kp) * v[0][kk];

                value += w_[i] * vp;
            }
            return value;
        }
        // intermediate rollback
        else {
            Real std = sqrt(model_->parametrization()->zeta(te[j]) - model_->parametrization()->zeta(te[j - 1]));
            //            Real previousValue = 0;
            for (int k = 0; k <= 2 * mx_; k++) {
                int imin = 0;
                int imax = 2 * my_;
                Real value = 0.0;
                /*
                int i1 = int(floor((- dx[j] * mx - dx[j-1] * (k - mx))/(h*std)))
                + my;
                int i2 = int(floor((+ dx[j] * mx - dx[j-1] * (k - mx))/(h*std)))
                + my;
                int imin = std::max(i1, 0);
                int imax = std::min(i2, 2*my);
                if (imin > 0)     value += v[j][0] * wsum[imin];
                if (imax < 2*my)  value += v[j][2*my] * (1.0 - wsum[imax]);
                */
                for (int i = imin; i <= imax; i++) {
                    // Map y index to x index, not integer in general
                    Real kp = (dx[j - 1] * (k - mx_) + y_[i] * std) / dx[j] + mx_;
                    // Adjacent integer x index <= k
                    int kk = int(floor(kp));
                    // Get value at kp by linear interpolation on
                    // kk <= kp <= kk + 1 with flat extrapolation
                    Real vp;
                    if (kk < 0)
                        vp = v[j][0];
                    else if (kk + 1 > 2 * mx_)
                        vp = v[j][2 * mx_];
                    else
                        vp = (kp - kk) * v[j][kk + 1] + (1.0 + kk - kp) * v[j][kk];

                    value += w_[i] * vp;
                    QL_REQUIRE(vp >= 0, "negative value in rollback");
                }
                // choose: continue (value) or exercise (u[j-1][k])
                v[j - 1][k] = std::max(value, u[j - 1][k]);
                // previousValue = value;
            }
        }
    } // for options

    QL_FAIL("This point should never be reached");

} // NumericLgmSwaptionEngineBase::calculate

void NumericLgmSwaptionEngine::calculate() const {
    // TODO cash-settled swaptions are priced as if physically settled, this can be refined
    iborIndex_ = arguments_.swap->iborIndex();
    exercise_ = arguments_.exercise;
    results_.value = NumericLgmSwaptionEngineBase::calculate();
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
        Real T = model_->parametrization()->termStructure()->timeFromReference(arguments_.floatingPayDates[l]);
        floatingLegNpv +=
            arguments_.nominal * arguments_.floatingAccrualTimes[l] *
            (arguments_.floatingSpreads[l] + iborIndexCorrected_->fixing(arguments_.floatingFixingDates[l])) *
            model_->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Real fixedLegNpv = 0.0;
    for (Size l = j1; l < arguments_.fixedCoupons.size(); l++) {
        Real T = model_->parametrization()->termStructure()->timeFromReference(arguments_.fixedPayDates[l]);
        fixedLegNpv += arguments_.fixedCoupons[l] * model_->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    Real exerciseValue = (type == Option::Call ? 1.0 : -1.0) * (floatingLegNpv - fixedLegNpv);
    return exerciseValue;
} // NumericLgmSwaptionEngine::conditionalSwapValue

void NumericLgmNonstandardSwaptionEngine::calculate() const {
    // FIXME handle cash settled swaption properly in this engine, for the time being we treat cash settlement
    // the same as physical settlement
    iborIndex_ = arguments_.swap->iborIndex();
    exercise_ = arguments_.exercise;
    results_.value = NumericLgmSwaptionEngineBase::calculate();
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
        Real T = model_->parametrization()->termStructure()->timeFromReference(arguments_.floatingPayDates[l]);
        if (arguments_.floatingIsRedemptionFlow[l]) {
            floatingLegNpv += arguments_.floatingCoupons[l] * model_->reducedDiscountBond(t, T, x, discountCurve_);
        } else {
            floatingLegNpv +=
                arguments_.floatingNominal[l] * arguments_.floatingAccrualTimes[l] *
                (arguments_.floatingSpreads[l] +
                 arguments_.floatingGearings[l] * iborIndexCorrected_->fixing(arguments_.floatingFixingDates[l])) *
                model_->reducedDiscountBond(t, T, x, discountCurve_);
        }
    }

    Real fixedLegNpv = 0.0;
    for (Size l = j1; l < arguments_.fixedCoupons.size(); l++) {
        Real T = model_->parametrization()->termStructure()->timeFromReference(arguments_.fixedPayDates[l]);
        fixedLegNpv += arguments_.fixedCoupons[l] * model_->reducedDiscountBond(t, T, x, discountCurve_);
    }

    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    Real exerciseValue = (type == Option::Call ? 1.0 : -1.0) * (floatingLegNpv - fixedLegNpv);
    return exerciseValue;
} // NumericLgmSwaptionEngine::conditionalSwapValue

} // namespace QuantExt
