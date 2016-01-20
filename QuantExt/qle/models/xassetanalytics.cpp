/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament
*/

#include <qle/models/xassetanalytics.hpp>
#include <qle/models/xassetmodel.hpp>

namespace QuantExt {

namespace XAssetAnalytics {

Real ir_expectation_1(const XAssetModel *x, const Size i, const Time t0,
                      const Real dt) {
    const Size na = Null<Size>();
    Real res = 0.0;
    if (i > 0) {
        res += -integral(x, i, na, i, i, na, na, t0, t0 + dt) -
               integral(x, na, na, i, na, na, i - 1, t0, t0 + dt) +
               integral(x, 0, na, 0, i, na, na, t0, t0 + dt);
    }
    return res;
}

Real ir_expectation_2(const XAssetModel *, const Size, const Real zi_0) {
    return zi_0;
}

Real fx_expectation_1(const XAssetModel *x, const Size i, const Time t0,
                      const Real dt) {
    const Size na = Null<Size>();
    Real H0_a = x->irlgm1f(0)->H(t0);
    Real Hi_a = x->irlgm1f(i + 1)->H(t0);
    Real H0_b = x->irlgm1f(0)->H(t0 + dt);
    Real Hi_b = x->irlgm1f(i + 1)->H(t0 + dt);
    Real zeta0_a = x->irlgm1f(0)->zeta(t0);
    Real zetai_a = x->irlgm1f(i + 1)->zeta(t0);
    Real zeta0_b = x->irlgm1f(0)->zeta(t0 + dt);
    Real zetai_b = x->irlgm1f(i + 1)->zeta(t0 + dt);
    Real res = std::log(x->irlgm1f(i + 1)->termStructure()->discount(t0 + dt) /
                        x->irlgm1f(i + 1)->termStructure()->discount(t0) *
                        x->irlgm1f(0)->termStructure()->discount(t0) /
                        x->irlgm1f(0)->termStructure()->discount(t0 + dt));
    res -= 0.5 * integral(x, na, na, na, na, i, i, t0, t0 + dt);
    res += 0.5 * (H0_b * H0_b * zeta0_b - H0_a * H0_a * zeta0_a -
                  integral(x, 0, 0, 0, 0, na, na, t0, t0 + dt));
    res -= 0.5 * (Hi_b * Hi_b * zetai_b - Hi_a * Hi_a * zetai_a -
                  integral(x, i + 1, i + 1, i + 1, i + 1, na, na, t0, t0 + dt));
    res += integral(x, 0, na, 0, na, na, i, t0, t0 + dt);
    res -= Hi_b * (-integral(x, i + 1, na, i + 1, i + 1, na, na, t0, t0 + dt) +
                   integral(x, 0, na, 0, i + 1, na, na, t0, t0 + dt) -
                   integral(x, na, na, i + 1, na, na, i, t0, t0 + dt));
    res += -integral(x, i + 1, i + 1, i + 1, i + 1, na, na, t0, t0 + dt) +
           integral(x, 0, i + 1, 0, i + 1, na, na, t0, t0 + dt) -
           integral(x, i + 1, na, i + 1, na, na, i, t0, t0 + dt);
    return res;
}

Real fx_expectation_2(const XAssetModel *x, const Size i, const Time t0,
                      const Real xi_0, const Real zi_0, const Real z0_0,
                      const Real dt) {
    Real res =
        xi_0 + (x->irlgm1f(0)->H(t0 + dt) - x->irlgm1f(0)->H(t0)) * z0_0 -
        (x->irlgm1f(i + 1)->H(t0 + dt) - x->irlgm1f(i + 1)->H(t0)) * zi_0;
    return res;
}

Real ir_ir_covariance(const XAssetModel *x, const Size i, const Size j,
                      const Time t0, Time dt) {
    const Size na = Null<Size>();
    Real res = integral(x, na, na, i, j, na, na, t0, t0 + dt);
    return res;
}

Real ir_fx_covariance(const XAssetModel *x, const Size i, const Size j,
                      const Time t0, Time dt) {
    const Size na = Null<Size>();
    Real res = x->irlgm1f(0)->H(t0 + dt) *
                   integral(x, na, na, 0, i, na, na, t0, t0 + dt) -
               integral(x, 0, na, 0, i, na, na, t0, t0 + dt) -
               x->irlgm1f(j + 1)->H(t0 + dt) *
                   integral(x, na, na, j + 1, i, na, na, t0, t0 + dt) +
               integral(x, j + 1, na, j + 1, i, na, na, t0, t0 + dt) +
               integral(x, na, na, i, na, na, j, t0, t0 + dt);
    return res;
}

Real fx_fx_covariance(const XAssetModel *x, const Size i, const Size j,
                      const Time t0, Time dt) {
    const Size na = Null<Size>();
    Real H0 = x->irlgm1f(0)->H(t0 + dt);
    Real Hi = x->irlgm1f(i + 1)->H(t0 + dt);
    Real Hj = x->irlgm1f(j + 1)->H(t0 + dt);
    Real res =
        // row 1
        H0 * H0 * integral(x, na, na, 0, 0, na, na, t0, t0 + dt) -
        2.0 * H0 * integral(x, 0, na, 0, 0, na, na, t0, t0 + dt) +
        integral(x, 0, 0, 0, 0, na, na, t0, t0 + dt) -
        // row 2
        H0 * Hj * integral(x, na, na, 0, j + 1, na, na, t0, t0 + dt) +
        Hj * integral(x, 0, na, 0, j + 1, na, na, t0, t0 + dt) +
        H0 * integral(x, j + 1, na, j + 1, 0, na, na, t0, t0 + dt) -
        integral(x, 0, j + 1, 0, j + 1, na, na, t0, t0 + dt) -
        // row 3
        H0 * Hi * integral(x, na, na, 0, i + 1, na, na, t0, t0 + dt) +
        Hi * integral(x, 0, na, 0, i + 1, na, na, t0, t0 + dt) +
        H0 * integral(x, i + 1, na, i + 1, 0, na, na, t0, t0 + dt) -
        integral(x, 0, i + 1, 0, i + 1, na, na, t0, t0 + dt) +
        // row 4
        H0 * integral(x, na, na, 0, na, na, j, t0, t0 + dt) -
        integral(x, 0, na, 0, na, na, j, t0, t0 + dt) +
        // row 5
        H0 * integral(x, na, na, 0, na, na, i, t0, t0 + dt) -
        integral(x, 0, na, 0, na, na, i, t0, t0 + dt) -
        // row 6
        Hi * integral(x, na, na, i + 1, na, na, j, t0, t0 + dt) +
        integral(x, i + 1, na, i + 1, na, na, j, t0, t0 + dt) -
        // row 7
        Hj * integral(x, na, na, j + 1, na, na, i, t0, t0 + dt) +
        integral(x, j + 1, na, j + 1, na, na, i, t0, t0 + dt) +
        // row 8
        Hi * Hj * integral(x, na, na, i + 1, j + 1, na, na, t0, t0 + dt) -
        Hj * integral(x, i + 1, na, i + 1, j + 1, na, na, t0, t0 + dt) -
        Hi * integral(x, j + 1, na, j + 1, i + 1, na, na, t0, t0 + dt) +
        integral(x, i + 1, j + 1, i + 1, j + 1, na, na, t0, t0 + dt) +
        // row 9
        integral(x, na, na, na, na, i, j, t0, t0 + dt);
    return res;
}

Real integral(const XAssetModel *x, const Size hi, const Size hj,
              const Size alphai, const Size alphaj, const Size sigmai,
              const Size sigmaj, const Real a, const Real b) {
    return x->integrator()->operator()(boost::bind(&integral_helper, x, hi, hj,
                                                   alphai, alphaj, sigmai,
                                                   sigmaj, _1),
                                       a, b);
}

Real integral_helper(const XAssetModel *x, const Size hi, const Size hj,
                     const Size alphai, const Size alphaj, const Size sigmai,
                     const Size sigmaj, const Real t) {
    const Size na = Null<Size>();
    Size i1 = Null<Size>(), j1 = Null<Size>();
    Size i2 = Null<Size>(), j2 = Null<Size>();
    Real res = 1.0;
    if (hi != na) {
        res *= x->irlgm1f(hi)->H(t);
        i1 = hi;
    }
    if (hj != na) {
        res *= x->irlgm1f(hj)->H(t);
        j1 = hj;
    }
    if (alphai != na) {
        res *= x->irlgm1f(alphai)->alpha(t);
        i1 = alphai;
    }
    if (alphaj != na) {
        res *= x->irlgm1f(alphaj)->alpha(t);
        j1 = alphaj;
    }
    if (sigmai != na) {
        res *= x->fxbs(sigmai)->sigma(t);
        i2 = sigmai;
    }
    if (sigmaj != na) {
        res *= x->fxbs(sigmaj)->sigma(t);
        j2 = sigmaj;
    }
    Size i = i1 != Null<Size>() ? i1 : i2 + x->currencies();
    Size j = j1 != Null<Size>() ? j1 : j2 + x->currencies();
    res *= x->correlation()[i][j];
    return res;
}

} // namesapce xassetanalytics
} // namespace QuantExt
