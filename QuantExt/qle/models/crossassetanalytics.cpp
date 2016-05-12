/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament
*/

#include <qle/models/crossassetanalytics.hpp>

namespace QuantExt {

namespace CrossAssetAnalytics {

Real ir_expectation_1(const CrossAssetModel *x, const Size i, const Time t0,
                      const Real dt) {
    Real res = 0.0;
    if (i > 0) {
        res += -integral(x, P(Hz(i), az(i), az(i)), t0, t0 + dt) -
               integral(x, P(az(i), sx(i - 1), rzx(i, i - 1)), t0, t0 + dt) +
               integral(x, P(Hz(0), az(0), az(i), rzz(0, i)), t0, t0 + dt);
    }
    return res;
}

Real ir_expectation_2(const CrossAssetModel *, const Size, const Real zi_0) {
    return zi_0;
}

Real fx_expectation_1(const CrossAssetModel *x, const Size i, const Time t0,
                      const Real dt) {
    Real H0_a = Hz(0).eval(x, t0);
    Real Hi_a = Hz(i + 1).eval(x, t0);
    Real H0_b = Hz(0).eval(x, t0 + dt);
    Real Hi_b = Hz(i + 1).eval(x, t0 + dt);
    Real zeta0_a = zeta(0).eval(x, t0);
    Real zetai_a = zeta(i + 1).eval(x, t0);
    Real zeta0_b = zeta(0).eval(x, t0 + dt);
    Real zetai_b = zeta(i + 1).eval(x, t0 + dt);
    Real res = std::log(x->irlgm1f(i + 1)->termStructure()->discount(t0 + dt) /
                        x->irlgm1f(i + 1)->termStructure()->discount(t0) *
                        x->irlgm1f(0)->termStructure()->discount(t0) /
                        x->irlgm1f(0)->termStructure()->discount(t0 + dt));
    res -= 0.5 * (x->fxbs(i)->variance(t0+dt)-x->fxbs(i)->variance(t0));
    res += 0.5 * (H0_b * H0_b * zeta0_b - H0_a * H0_a * zeta0_a -
                  integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt));
    res -= 0.5 * (Hi_b * Hi_b * zetai_b - Hi_a * Hi_a * zetai_a -
                  integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0,
                           t0 + dt));
    res += integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t0 + dt);
    res -=
        Hi_b *
        (-integral(x, P(Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt) +
         integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) -
         integral(x, P(az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt));
    res +=
        -integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0,
                  t0 + dt) +
        integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)), t0,
                 t0 + dt) -
        integral(x, P(Hz(i + 1), az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt);
    return res;
}

Real fx_expectation_2(const CrossAssetModel *x, const Size i, const Time t0,
                      const Real xi_0, const Real zi_0, const Real z0_0,
                      const Real dt) {
    Real res = xi_0 + (Hz(0).eval(x, t0 + dt) - Hz(0).eval(x, t0)) * z0_0 -
               (Hz(i + 1).eval(x, t0 + dt) - Hz(i + 1).eval(x, t0)) * zi_0;
    return res;
}

Real ir_ir_covariance(const CrossAssetModel *x, const Size i, const Size j,
                      const Time t0, const Time dt) {
    Real res = integral(x, P(az(i), az(j), rzz(i, j)), t0, t0 + dt);
    return res;
}

Real ir_fx_covariance(const CrossAssetModel *x, const Size i, const Size j,
                      const Time t0, const Time dt) {
    Real res =
        Hz(0).eval(x, t0 + dt) *
            integral(x, P(az(0), az(i), rzz(0, i)), t0, t0 + dt) -
        integral(x, P(Hz(0), az(0), az(i), rzz(0, i)), t0, t0 + dt) -
        Hz(j + 1).eval(x, t0 + dt) *
            integral(x, P(az(j + 1), az(i), rzz(j + 1, i)), t0, t0 + dt) +
        integral(x, P(Hz(j + 1), az(j + 1), az(i), rzz(j + 1, i)), t0,
                 t0 + dt) +
        integral(x, P(az(i), sx(j), rzx(i, j)), t0, t0 + dt);
    return res;
}

Real fx_fx_covariance(const CrossAssetModel *x, const Size i, const Size j,
                      const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real Hj = Hz(j + 1).eval(x, t0 + dt);
    Real res =
        // row 1
        H0 * H0 * (x->irlgm1f(0)->zeta(t0 + dt) - x->irlgm1f(0)->zeta(t0)) -
        2.0 * H0 * integral(x, P(Hz(0), az(0), az(0)), t0, t0 + dt) +
        integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt) -
        // row 2
        H0 * Hj * integral(x, P(az(0), az(j + 1), rzz(0, j + 1)), t0, t0 + dt) +
        Hj * integral(x, P(Hz(0), az(0), az(j + 1), rzz(0, j + 1)), t0,
                      t0 + dt) +
        H0 * integral(x, P(Hz(j + 1), az(j + 1), az(0), rzz(j + 1, 0)), t0,
                      t0 + dt) -
        integral(x, P(Hz(0), Hz(j + 1), az(0), az(j + 1), rzz(0, j + 1)), t0,
                 t0 + dt) -
        // row 3
        H0 * Hi * integral(x, P(az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt) +
        Hi * integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0,
                      t0 + dt) +
        H0 * integral(x, P(Hz(i + 1), az(i + 1), az(0), rzz(i + 1, 0)), t0,
                      t0 + dt) -
        integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)), t0,
                 t0 + dt) +
        // row 4
        H0 * integral(x, P(az(0), sx(j), rzx(0, j)), t0, t0 + dt) -
        integral(x, P(Hz(0), az(0), sx(j), rzx(0, j)), t0, t0 + dt) +
        // row 5
        H0 * integral(x, P(az(0), sx(i), rzx(0, i)), t0, t0 + dt) -
        integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t0 + dt) -
        // row 6
        Hi * integral(x, P(az(i + 1), sx(j), rzx(i + 1, j)), t0, t0 + dt) +
        integral(x, P(Hz(i + 1), az(i + 1), sx(j), rzx(i + 1, j)), t0,
                 t0 + dt) -
        // row 7
        Hj * integral(x, P(az(j + 1), sx(i), rzx(j + 1, i)), t0, t0 + dt) +
        integral(x, P(Hz(j + 1), az(j + 1), sx(i), rzx(j + 1, i)), t0,
                 t0 + dt) +
        // row 8
        Hi * Hj * integral(x, P(az(i + 1), az(j + 1), rzz(i + 1, j + 1)), t0,
                           t0 + dt) -
        Hj * integral(x, P(Hz(i + 1), az(i + 1), az(j + 1), rzz(i + 1, j + 1)),
                      t0, t0 + dt) -
        Hi * integral(x, P(Hz(j + 1), az(j + 1), az(i + 1), rzz(j + 1, i + 1)),
                      t0, t0 + dt) +
        integral(
            x, P(Hz(i + 1), Hz(j + 1), az(i + 1), az(j + 1), rzz(i + 1, j + 1)),
            t0, t0 + dt) +
        // row 9
        integral(x, P(sx(i), sx(j), rxx(i, j)), t0, t0 + dt);
    return res;
}

} // namesapce crossassetanalytics
} // namespace QuantExt
