/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/pricingengines/numericlgmswaptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/payoff.hpp>

using std::vector;

namespace QuantExt {

Real NumericLgmSwaptionEngine::conditionalSwapValue(Real x, Real t,
                                                    const Date expiry0) const {
    Schedule fixedSchedule = arguments_.swap->fixedSchedule();
    Schedule floatSchedule = arguments_.swap->floatingSchedule();
    Size j1 = std::upper_bound(fixedSchedule.dates().begin(),
                               fixedSchedule.dates().end(), expiry0 - 1) -
              fixedSchedule.dates().begin();
    Size k1 = std::upper_bound(floatSchedule.dates().begin(),
                               floatSchedule.dates().end(), expiry0 - 1) -
              floatSchedule.dates().begin();

    iborModelCurve_->move(t, x);

    Real floatingLegNpv = 0.0;
    for (Size l = k1; l < arguments_.floatingCoupons.size(); l++) {
        Real T = model_->parametrization()->termStructure()->timeFromReference(
            arguments_.floatingPayDates[l]);
        floatingLegNpv +=
            arguments_.nominal * arguments_.floatingAccrualTimes[l] *
            (arguments_.floatingSpreads[l] +
             iborIndex_->fixing(arguments_.floatingFixingDates[l])) *
            model_->reducedDiscountBond(x, t, T, discountCurve_);
    }

    Real fixedLegNpv = 0.0;
    for (Size l = j1; l < arguments_.fixedCoupons.size(); l++) {
        Real T = model_->parametrization()->termStructure()->timeFromReference(
            arguments_.fixedPayDates[l]);
        fixedLegNpv += arguments_.fixedCoupons[l] *
                       model_->reducedDiscountBond(x, t, T, discountCurve_);
    }

    Option::Type type =
        arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    Real exerciseValue = (type == Option::Call ? 1.0 : -1.0) *
                         (floatingLegNpv - fixedLegNpv) /
                         model_->numeraire(t, x, discountCurve_);
    return exerciseValue;
}

void NumericLgmSwaptionEngine::calculate() const {

    QL_REQUIRE(arguments_.settlementType == Settlement::Physical,
               "cash-settled swaptions not yet implemented ...");

    Date settlement =
        model_->parametrization()->termStructure()->referenceDate();

    if (arguments_.exercise->dates().back() <=
        settlement) { // swaption is expired, possibly generated swap is not
                      // valued
        results_.value = 0.0;
        return;
    }

    int idx = static_cast<int>(arguments_.exercise->dates().size()) - 1;
    int minIdxAlive = static_cast<int>(
        std::upper_bound(arguments_.exercise->dates().begin(),
                         arguments_.exercise->dates().end(), settlement) -
        arguments_.exercise->dates().begin());

    int options = idx - minIdxAlive + 1;

    // x grid for each expiry

    vector<Real> te(options);    // time to expiry
    vector<Real> sigma(options); // standard deviation of x
    vector<Real> dx(options);    // x-grid spacing
    Matrix x(options,
             2 * mx_ + 1); // x-coordinate of grid points for each expiry
    Matrix u(options,
             2 * mx_ + 1); // conditional underlying value in grid point i
    Matrix v(options,
             2 * mx_ + 1); // conditional continuation value in grid point i
    for (int j = 0; j < options; j++) {
        te[j] = model_->parametrization()->termStructure()->timeFromReference(
            arguments_.exercise->dates()[idx - minIdxAlive + 1 + j]);
        sigma[j] = sqrt(model_->parametrization()->zeta(te[j]));
        dx[j] = sigma[j] / nx_;
        for (int k = 0; k <= 2 * mx_; k++) {
            x[j][k] = dx[j] * (k - mx_);
            // Payoff goes here
            u[j][k] = conditionalSwapValue(
                x[j][k], te[j],
                arguments_.exercise->dates()[idx - minIdxAlive + 1 + j]);
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
            results_.value = value;
            return;
        }
        // intermediate rollback
        else {
            Real std = sqrt(model_->parametrization()->zeta(te[j]) -
                            model_->parametrization()->zeta(te[j - 1]));
            Real previousValue = 0;
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
                    Real kp =
                        (dx[j - 1] * (k - mx_) + y_[i] * std) / dx[j] + mx_;
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
                        vp = (kp - kk) * v[j][kk + 1] +
                             (1.0 + kk - kp) * v[j][kk];

                    value += w_[i] * vp;
                    QL_REQUIRE(vp >= 0, "negative value in rollback");
                }
                // choose: continue (value) or exercise (u[j-1][k])
                v[j - 1][k] = std::max(value, u[j - 1][k]);
                previousValue = value;
            }
        }
    } // for options

    QL_FAIL("This point should never be reached");

} // calculate

} // namespace QuantExt
