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

#include <qle/models/crossassetanalytics.hpp>

namespace QuantExt {

namespace CrossAssetAnalytics {

Real ir_expectation_1(const CrossAssetModel* x, const Size i, const Time t0, const Real dt) {
    Real res = 0.0;
    if (i > 0) {
        res += -integral(x, P(Hz(i), az(i), az(i)), t0, t0 + dt) -
               integral(x, P(az(i), sx(i - 1), rzx(i, i - 1)), t0, t0 + dt) +
               integral(x, P(Hz(0), az(0), az(i), rzz(0, i)), t0, t0 + dt);
    }
    return res;
}

Real ir_expectation_2(const CrossAssetModel*, const Size, const Real zi_0) { return zi_0; }

Real fx_expectation_1(const CrossAssetModel* x, const Size i, const Time t0, const Real dt) {
    Real H0_a = Hz(0).eval(x, t0);
    Real Hi_a = Hz(i + 1).eval(x, t0);
    Real H0_b = Hz(0).eval(x, t0 + dt);
    Real Hi_b = Hz(i + 1).eval(x, t0 + dt);
    Real zeta0_a = zetaz(0).eval(x, t0);
    Real zetai_a = zetaz(i + 1).eval(x, t0);
    Real zeta0_b = zetaz(0).eval(x, t0 + dt);
    Real zetai_b = zetaz(i + 1).eval(x, t0 + dt);
    Real res = std::log(
        x->irlgm1f(i + 1)->termStructure()->discount(t0 + dt) / x->irlgm1f(i + 1)->termStructure()->discount(t0) *
        x->irlgm1f(0)->termStructure()->discount(t0) / x->irlgm1f(0)->termStructure()->discount(t0 + dt));
    res -= 0.5 * (vx(i).eval(x, t0 + dt) - vx(i).eval(x, t0));
    res +=
        0.5 * (H0_b * H0_b * zeta0_b - H0_a * H0_a * zeta0_a - integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt));
    res -= 0.5 * (Hi_b * Hi_b * zetai_b - Hi_a * Hi_a * zetai_a -
                  integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt));
    res += integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t0 + dt);
    res -= Hi_b * (-integral(x, P(Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt) +
                   integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) -
                   integral(x, P(az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt));
    res += -integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt) +
           integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) -
           integral(x, P(Hz(i + 1), az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt);
    return res;
}

Real fx_expectation_2(const CrossAssetModel* x, const Size i, const Time t0, const Real xi_0, const Real zi_0,
                      const Real z0_0, const Real dt) {
    Real res = xi_0 + (Hz(0).eval(x, t0 + dt) - Hz(0).eval(x, t0)) * z0_0 -
               (Hz(i + 1).eval(x, t0 + dt) - Hz(i + 1).eval(x, t0)) * zi_0;
    return res;
}

Real eq_expectation_1(const CrossAssetModel* x, const Size k, const Time t0, const Real dt) {
    Size i = x->ccyIndex(x->eqbs(k)->currency());
    Size eps_i = (i == 0) ? 0 : 1;
    Real Hi_a = Hz(i).eval(x, t0);
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real zetai_a = zetaz(i).eval(x, t0);
    Real zetai_b = zetaz(i).eval(x, t0 + dt);
    Real res =
        std::log(x->eqbs(k)->equityDivYieldCurveToday()->discount(t0 + dt) /
                 x->eqbs(k)->equityDivYieldCurveToday()->discount(t0) * x->eqbs(k)->equityIrCurveToday()->discount(t0) /
                 x->eqbs(k)->equityIrCurveToday()->discount(t0 + dt));
    res -= 0.5 * (vs(k).eval(x, t0 + dt) - vs(k).eval(x, t0));
    res +=
        0.5 * (Hi_b * Hi_b * zetai_b - Hi_a * Hi_a * zetai_a - integral(x, P(Hz(i), Hz(i), az(i), az(i)), t0, t0 + dt));
    res += integral(x, P(rzs(0, k), Hz(0), az(0), ss(k)), t0, t0 + dt);
    if (eps_i > 0) {
        res -= integral(x, P(rxs(i - 1, k), sx(i - 1), ss(k)), t0, t0 + dt);
    }
    // expand gamma term
    if (eps_i > 0) {
        res += Hi_b * (-integral(x, P(Hz(i), az(i), az(i)), t0, t0 + dt) -
                       integral(x, P(rzx(i, i - 1), sx(i - 1), az(i)), t0, t0 + dt) +
                       integral(x, P(rzz(0, i), az(i), az(0), Hz(0)), t0, t0 + dt));
        res -= (-integral(x, P(Hz(i), Hz(i), az(i), az(i)), t0, t0 + dt) -
                integral(x, P(Hz(i), rzx(i, i - 1), sx(i - 1), az(i)), t0, t0 + dt) +
                integral(x, P(Hz(i), rzz(0, i), az(i), az(0), Hz(0)), t0, t0 + dt));
    }
    return res;
}

Real eq_expectation_2(const CrossAssetModel* x, const Size k, const Time t0, const Real sk_0, const Real zi_0,
                      const Real dt) {
    Size i = x->ccyIndex(x->eqbs(k)->currency());
    Real Hi_a = Hz(i).eval(x, t0);
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real res = sk_0 + (Hi_b - Hi_a) * zi_0;
    return res;
}

Real ir_ir_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(az(i), az(j), rzz(i, j)), t0, t0 + dt);
    return res;
}

Real ir_fx_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = Hz(0).eval(x, t0 + dt) * integral(x, P(az(0), az(i), rzz(0, i)), t0, t0 + dt) -
               integral(x, P(Hz(0), az(0), az(i), rzz(0, i)), t0, t0 + dt) -
               Hz(j + 1).eval(x, t0 + dt) * integral(x, P(az(j + 1), az(i), rzz(j + 1, i)), t0, t0 + dt) +
               integral(x, P(Hz(j + 1), az(j + 1), az(i), rzz(j + 1, i)), t0, t0 + dt) +
               integral(x, P(az(i), sx(j), rzx(i, j)), t0, t0 + dt);
    return res;
}

Real fx_fx_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real Hj = Hz(j + 1).eval(x, t0 + dt);
    Real res =
        // row 1
        H0 * H0 * (zetaz(0).eval(x, t0 + dt) - zetaz(0).eval(x, t0)) -
        2.0 * H0 * integral(x, P(Hz(0), az(0), az(0)), t0, t0 + dt) +
        integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt) -
        // row 2
        H0 * Hj * integral(x, P(az(0), az(j + 1), rzz(0, j + 1)), t0, t0 + dt) +
        Hj * integral(x, P(Hz(0), az(0), az(j + 1), rzz(0, j + 1)), t0, t0 + dt) +
        H0 * integral(x, P(Hz(j + 1), az(j + 1), az(0), rzz(j + 1, 0)), t0, t0 + dt) -
        integral(x, P(Hz(0), Hz(j + 1), az(0), az(j + 1), rzz(0, j + 1)), t0, t0 + dt) -
        // row 3
        H0 * Hi * integral(x, P(az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) +
        Hi * integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) +
        H0 * integral(x, P(Hz(i + 1), az(i + 1), az(0), rzz(i + 1, 0)), t0, t0 + dt) -
        integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) +
        // row 4
        H0 * integral(x, P(az(0), sx(j), rzx(0, j)), t0, t0 + dt) -
        integral(x, P(Hz(0), az(0), sx(j), rzx(0, j)), t0, t0 + dt) +
        // row 5
        H0 * integral(x, P(az(0), sx(i), rzx(0, i)), t0, t0 + dt) -
        integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t0 + dt) -
        // row 6
        Hi * integral(x, P(az(i + 1), sx(j), rzx(i + 1, j)), t0, t0 + dt) +
        integral(x, P(Hz(i + 1), az(i + 1), sx(j), rzx(i + 1, j)), t0, t0 + dt) -
        // row 7
        Hj * integral(x, P(az(j + 1), sx(i), rzx(j + 1, i)), t0, t0 + dt) +
        integral(x, P(Hz(j + 1), az(j + 1), sx(i), rzx(j + 1, i)), t0, t0 + dt) +
        // row 8
        Hi * Hj * integral(x, P(az(i + 1), az(j + 1), rzz(i + 1, j + 1)), t0, t0 + dt) -
        Hj * integral(x, P(Hz(i + 1), az(i + 1), az(j + 1), rzz(i + 1, j + 1)), t0, t0 + dt) -
        Hi * integral(x, P(Hz(j + 1), az(j + 1), az(i + 1), rzz(j + 1, i + 1)), t0, t0 + dt) +
        integral(x, P(Hz(i + 1), Hz(j + 1), az(i + 1), az(j + 1), rzz(i + 1, j + 1)), t0, t0 + dt) +
        // row 9
        integral(x, P(sx(i), sx(j), rxx(i, j)), t0, t0 + dt);
    return res;
}

Real infz_infz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryy(i, j), ay(i), ay(j)), t0, t0 + dt);
    return res;
}

Real infz_infy_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryy(i, j), ay(i), Hy(j), ay(j)), t0, t0 + dt);
    return res;
}

Real infy_infy_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryy(i, j), Hy(i), ay(i), Hy(j), ay(j)), t0, t0 + dt);
    return res;
}

Real ir_infz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzy(i, j), az(i), ay(j)), t0, t0 + dt);
    return res;
}

Real ir_infy_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzy(i, j), az(i), Hy(j), ay(j)), t0, t0 + dt);
    return res;
}

Real fx_infz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzy(0, j), Hz(0), az(0), ay(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzy(0, j), az(0), ay(j)), t0, t0 + dt) +
               integral(x, P(rzy(i + 1, j), Hz(i + 1), az(i + 1), ay(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzy(i + 1, j), az(i + 1), ay(j)), t0, t0 + dt) +
               integral(x, P(rxy(i, j), sx(i), ay(j)), t0, t0 + dt);
    return res;
}

Real fx_infy_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzy(0, j), Hz(0), az(0), Hy(j), ay(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzy(0, j), az(0), Hy(j), ay(j)), t0, t0 + dt) +
               integral(x, P(rzy(i + 1, j), Hz(i + 1), az(i + 1), Hy(j), ay(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzy(i + 1, j), az(i + 1), Hy(j), ay(j)), t0, t0 + dt) +
               integral(x, P(rxy(i, j), sx(i), Hy(j), ay(j)), t0, t0 + dt);
    return res;
}

Real crz_crz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), al(i), al(j)), t0, t0 + dt);
    return res;
}

Real crz_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), al(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real cry_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), Hl(i), al(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real ir_crz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzl(i, j), az(i), al(j)), t0, t0 + dt);
    return res;
}

Real ir_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzl(i, j), az(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real fx_crz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzl(0, j), Hz(0), az(0), al(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzl(0, j), az(0), al(j)), t0, t0 + dt) +
               integral(x, P(rzl(i + 1, j), Hz(i + 1), az(i + 1), al(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzl(i + 1, j), az(i + 1), al(j)), t0, t0 + dt) +
               integral(x, P(rxl(i, j), sx(i), al(j)), t0, t0 + dt);
    return res;
}

Real fx_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzl(0, j), Hz(0), az(0), Hl(j), al(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzl(0, j), az(0), Hl(j), al(j)), t0, t0 + dt) +
               integral(x, P(rzl(i + 1, j), Hz(i + 1), az(i + 1), Hl(j), al(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzl(i + 1, j), az(i + 1), Hl(j), al(j)), t0, t0 + dt) +
               integral(x, P(rxl(i, j), sx(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real infz_crz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), ay(i), al(j)), t0, t0 + dt);
    return res;
}

Real infz_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), ay(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real infy_crz_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), Hy(i), ay(i), al(j)), t0, t0 + dt);
    return res;
}

Real infy_cry_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), Hy(i), ay(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real ir_eq_covariance(const CrossAssetModel* x, const Size j, const Size k, const Time t0, const Time dt) {
    Size i = x->ccyIndex(x->eqbs(k)->currency()); // the equity underlying currency
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real res = Hi_b * integral(x, P(rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res -= integral(x, P(Hz(i), rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res += integral(x, P(rzs(j, k), az(j), ss(k)), t0, t0 + dt);
    return res;
}

Real fx_eq_covariance(const CrossAssetModel* x, const Size j, const Size k, const Time t0, const Time dt) {
    Size i = x->ccyIndex(x->eqbs(k)->currency()); // the equity underlying currency
    const Size& j_lgm = j + 1;                    // indexing of the FX currency for extracting the LGM terms
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real Hj_b = Hz(j_lgm).eval(x, t0 + dt);
    Real H0_b = Hz(0).eval(x, t0 + dt);
    Real res = 0.;
    res += Hi_b * H0_b * integral(x, P(rzz(0, i), az(0), az(i)), t0, t0 + dt);
    res -= Hi_b * integral(x, P(Hz(0), rzz(0, i), az(0), az(i)), t0, t0 + dt);
    res -= H0_b * integral(x, P(Hz(i), rzz(0, i), az(0), az(i)), t0, t0 + dt);
    res += integral(x, P(Hz(0), Hz(i), rzz(0, i), az(0), az(i)), t0, t0 + dt);
    res -= Hi_b * Hj_b * integral(x, P(rzz(j_lgm, i), az(j_lgm), az(i)), t0, t0 + dt);
    res += Hi_b * integral(x, P(Hz(j_lgm), rzz(j_lgm, i), az(j_lgm), az(i)), t0, t0 + dt);
    res += Hj_b * integral(x, P(Hz(i), rzz(j_lgm, i), az(j_lgm), az(i)), t0, t0 + dt);
    res -= integral(x, P(Hz(j_lgm), Hz(i), rzz(j_lgm, i), az(j_lgm), az(i)), t0, t0 + dt);

    res += Hi_b * integral(x, P(rzx(i, j), sx(j), az(i)), t0, t0 + dt);
    res -= integral(x, P(Hz(i), rzx(i, j), sx(j), az(i)), t0, t0 + dt);

    res += H0_b * integral(x, P(rzs(0, k), az(0), ss(k)), t0, t0 + dt);
    res -= integral(x, P(Hz(0), rzs(0, k), az(0), ss(k)), t0, t0 + dt);
    res -= Hj_b * integral(x, P(rzs(j_lgm, k), az(j_lgm), ss(k)), t0, t0 + dt);
    res += integral(x, P(Hz(j_lgm), rzs(j_lgm, k), az(j_lgm), ss(k)), t0, t0 + dt);

    res += integral(x, P(rxs(j, k), sx(j), ss(k)), t0, t0 + dt);
    return res;
}

Real infz_eq_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x->ccyIndex(x->eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzy(k, i), az(k), ay(i)), t0, t0 + dt) -
               integral(x, P(rzy(k, i), Hz(k), az(k), ay(i)), t0, t0 + dt) +
               integral(x, P(rys(i, j), ay(i), ss(j)), t0, t0 + dt);
    return res;
}

Real infy_eq_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x->ccyIndex(x->eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzy(k, i), az(k), Hy(i), ay(i)), t0, t0 + dt) -
               integral(x, P(rzy(k, i), Hz(k), az(k), Hy(i), ay(i)), t0, t0 + dt) +
               integral(x, P(rys(i, j), Hy(i), ay(i), ss(j)), t0, t0 + dt);
    return res;
}

Real crz_eq_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x->ccyIndex(x->eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzl(k, i), az(k), al(i)), t0, t0 + dt) -
               integral(x, P(rzl(k, i), Hz(k), az(k), al(i)), t0, t0 + dt) +
               integral(x, P(rls(i, j), al(i), ss(j)), t0, t0 + dt);
    return res;
}

Real cry_eq_covariance(const CrossAssetModel* x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x->ccyIndex(x->eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzl(k, i), az(k), Hl(i), al(i)), t0, t0 + dt) -
               integral(x, P(rzl(k, i), Hz(k), az(k), Hl(i), al(i)), t0, t0 + dt) +
               integral(x, P(rls(i, j), Hl(i), al(i), ss(j)), t0, t0 + dt);
    return res;
}

Real eq_eq_covariance(const CrossAssetModel* x, const Size k, const Size l, const Time t0, const Time dt) {
    Size i = x->ccyIndex(x->eqbs(k)->currency()); // ccy underlying equity k
    Size j = x->ccyIndex(x->eqbs(l)->currency()); // ccy underlying equity l
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real Hj_b = Hz(j).eval(x, t0 + dt);
    Real res = integral(x, P(rss(k, l), ss(k), ss(l)), t0, t0 + dt);
    res += Hj_b * integral(x, P(rzs(j, k), az(j), ss(k)), t0, t0 + dt);
    res -= integral(x, P(Hz(j), rzs(j, k), az(j), ss(k)), t0, t0 + dt);
    res += Hi_b * integral(x, P(rzs(i, l), az(i), ss(l)), t0, t0 + dt);
    res -= integral(x, P(Hz(i), rzs(i, l), az(i), ss(l)), t0, t0 + dt);
    res += Hi_b * Hj_b * integral(x, P(rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res -= Hi_b * integral(x, P(Hz(j), rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res -= Hj_b * integral(x, P(Hz(i), rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res += integral(x, P(Hz(i), Hz(j), rzz(i, j), az(i), az(j)), t0, t0 + dt);
    return res;
}

} // namespace CrossAssetAnalytics
} // namespace QuantExt
