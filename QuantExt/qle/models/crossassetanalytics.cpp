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
#include <qle/utilities/inflation.hpp>

using std::make_pair;
using std::pair;

namespace QuantExt {

namespace CrossAssetAnalytics {

Real ir_expectation_1(const CrossAssetModel& x, const Size i, const Time t0, const Real dt) {
    Real res = 0.0;
    if (i == 0) {
        if (x.measure() == IrModel::Measure::BA)
            res -= integral(x, P(Hz(i), az(i), az(i)), t0, t0 + dt);
    } else {
        res -= integral(x, P(Hz(i), az(i), az(i)), t0, t0 + dt);
        res -= integral(x, P(az(i), sx(i - 1), rzx(i, i - 1)), t0, t0 + dt);
        if (x.measure() != IrModel::Measure::BA)
            res += integral(x, P(Hz(0), az(0), az(i), rzz(0, i)), t0, t0 + dt);
    }
    return res;
}

Real ir_expectation_2(const CrossAssetModel&, const Size, const Real zi_0) { return zi_0; }

pair<Real, Real> inf_jy_expectation_1(const CrossAssetModel& x, Size i, Time t0, Real dt) {

    QL_REQUIRE(x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::JY,
               "inf_jy_expectation_1: should only be used for JY CAM inflation component.");

    auto res = make_pair(0.0, 0.0);

    // 1) Real rate process drift
    res.first = -integral(x, P(Hy(i), ay(i), ay(i)), t0, t0 + dt) +
                integral(x, P(rzy(0, i), Hz(0), az(0), ay(i)), t0, t0 + dt) -
                integral(x, P(ryy(i, i, 0, 1), ay(i), sy(i)), t0, t0 + dt);

    // i_i - index of i-th inflation component's currency.
    Size i_i = x.ccyIndex(x.infjy(i)->currency());
    if (i_i > 0) {
        res.first -= integral(x, P(rxy(i_i - 1, i, 0), ay(i), sx(i_i - 1)), t0, t0 + dt);
    }

    // 2) Inflation index process drift
    const auto& zts = x.infjy(i)->realRate()->termStructure();
    bool indexIsInterpolated = true; // FIXME, see also crossassetmodel.cpp line 706ff
    res.second =
        std::log(inflationGrowth(zts, t0 + dt, indexIsInterpolated) / inflationGrowth(zts, t0, indexIsInterpolated));

    res.second -= 0.5 * (vy(i).eval(x, t0 + dt) - vy(i).eval(x, t0));

    // Final _s means start of period i.e. t_0 and _e means end of period i.e. t_0 + dt
    Real Hi_i_s = Hz(i_i).eval(x, t0);
    Real Hi_s = Hy(i).eval(x, t0);
    Real Hi_i_e = Hz(i_i).eval(x, t0 + dt);
    Real Hi_e = Hy(i).eval(x, t0 + dt);
    Real zetai_i_s = zetaz(i_i).eval(x, t0);
    Real zetai_s = zetay(i).eval(x, t0);
    Real zetai_i_e = zetaz(i_i).eval(x, t0 + dt);
    Real zetai_e = zetay(i).eval(x, t0 + dt);
    res.second += 0.5 * (Hi_i_e * Hi_i_e * zetai_i_e - Hi_i_s * Hi_i_s * zetai_i_s);
    res.second -= 0.5 * integral(x, P(Hz(i_i), Hz(i_i), az(i_i), az(i_i)), t0, t0 + dt);
    res.second -= 0.5 * (Hi_e * Hi_e * zetai_e - Hi_s * Hi_s * zetai_s);
    res.second += 0.5 * integral(x, P(Hy(i), Hy(i), ay(i), ay(i)), t0, t0 + dt);

    res.second += integral(x, P(rzy(0, i, 1), Hz(0), az(0), sy(i)), t0, t0 + dt);

    res.second -=
        integral(x,
                 P(LC(Hi_e, -1.0, Hy(i)), LC(0.0, -1.0, P(Hy(i), ay(i), ay(i)), 1.0, P(Hz(0), az(0), ay(i), rzy(0, i)),
                                             -1.0, P(ryy(i, i, 0, 1), ay(i), sy(i)))),
                 t0, t0 + dt);

    if (i_i > 0) {
        res.second += integral(x,
                               P(LC(Hi_i_e, -1.0, Hz(i_i)),
                                 LC(0.0, -1.0, P(Hz(i_i), az(i_i), az(i_i)), 1.0, P(Hz(0), az(0), az(i_i), rzz(0, i_i)),
                                    -1.0, P(rzx(i_i, i_i - 1), az(i_i), sx(i_i - 1)))),
                               t0, t0 + dt);
        res.second -= integral(x, P(rxy(i_i - 1, i, 1), sy(i), sx(i_i - 1)), t0, t0 + dt);
        res.second += integral(x, P(LC(Hi_e, -1.0, Hy(i)), ay(i), sx(i_i - 1), rxy(i_i - 1, i)), t0, t0 + dt);
    }

    return res;
}

pair<Real, Real> inf_jy_expectation_2(const CrossAssetModel& x, Size i, Time t0, const pair<Real, Real>& state_0,
                                      Real zi_i_0, Real dt) {

    QL_REQUIRE(x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::JY,
               "inf_jy_expectation_2: should only be used for JY CAM inflation component.");

    // i_i - index of i-th inflation component's currency.
    Size i_i = x.ccyIndex(x.infjy(i)->currency());

    // Real rate portion of the result i.e. first element in pair is not state dependent so nothing needed.
    // The inflation index portion of the result i.e. second element needs to be updated.
    auto res = state_0;
    res.second += (Hz(i_i).eval(x, t0 + dt) - Hz(i_i).eval(x, t0)) * zi_i_0;
    res.second -= (Hy(i).eval(x, t0 + dt) - Hy(i).eval(x, t0)) * state_0.first;

    return res;
}

Real fx_expectation_1(const CrossAssetModel& x, const Size i, const Time t0, const Real dt) {
    bool bam = (x.measure() == IrModel::Measure::BA);
    Real H0_a = Hz(0).eval(x, t0);
    Real Hi_a = Hz(i + 1).eval(x, t0);
    Real H0_b = Hz(0).eval(x, t0 + dt);
    Real Hi_b = Hz(i + 1).eval(x, t0 + dt);
    Real zeta0_a = zetaz(0).eval(x, t0);
    Real zetai_a = zetaz(i + 1).eval(x, t0);
    Real zeta0_b = zetaz(0).eval(x, t0 + dt);
    Real zetai_b = zetaz(i + 1).eval(x, t0 + dt);
    Real res = std::log(x.irlgm1f(i + 1)->termStructure()->discount(t0 + dt) /
                        x.irlgm1f(i + 1)->termStructure()->discount(t0) * x.irlgm1f(0)->termStructure()->discount(t0) /
                        x.irlgm1f(0)->termStructure()->discount(t0 + dt));
    res -= 0.5 * (vx(i).eval(x, t0 + dt) - vx(i).eval(x, t0));
    res +=
        0.5 * (H0_b * H0_b * zeta0_b - H0_a * H0_a * zeta0_a - integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt));
    res -= 0.5 * (Hi_b * Hi_b * zetai_b - Hi_a * Hi_a * zetai_a -
                  integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt));
    res += (bam ? 0.0 : integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t0 + dt));
    res -= Hi_b * (-integral(x, P(Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt) +
                   (bam ? 0.0 : integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt)) -
                   integral(x, P(az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt));
    res += -integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0, t0 + dt) +
           (bam ? 0.0 : integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)), t0, t0 + dt)) -
           integral(x, P(Hz(i + 1), az(i + 1), sx(i), rzx(i + 1, i)), t0, t0 + dt);
    if (bam) {
        res -= H0_b * integral(x, P(Hz(0), az(0), az(0)), t0, t0 + dt);
        res += integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt);
    }
    return res;
}

Real fx_expectation_2(const CrossAssetModel& x, const Size i, const Time t0, const Real xi_0, const Real zi_0,
                      const Real z0_0, const Real dt) {
    Real res = xi_0 + (Hz(0).eval(x, t0 + dt) - Hz(0).eval(x, t0)) * z0_0 -
               (Hz(i + 1).eval(x, t0 + dt) - Hz(i + 1).eval(x, t0)) * zi_0;
    return res;
}

Real eq_expectation_1(const CrossAssetModel& x, const Size k, const Time t0, const Real dt) {
    Size i = x.ccyIndex(x.eqbs(k)->currency());
    Size eps_i = (i == 0) ? 0 : 1;
    Real Hi_a = Hz(i).eval(x, t0);
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real zetai_a = zetaz(i).eval(x, t0);
    Real zetai_b = zetaz(i).eval(x, t0 + dt);
    Real res = std::log(
        x.eqbs(k)->equityDivYieldCurveToday()->discount(t0 + dt) / x.eqbs(k)->equityDivYieldCurveToday()->discount(t0) *
        x.eqbs(k)->equityIrCurveToday()->discount(t0) / x.eqbs(k)->equityIrCurveToday()->discount(t0 + dt));
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

Real eq_expectation_2(const CrossAssetModel& x, const Size k, const Time t0, const Real sk_0, const Real zi_0,
                      const Real dt) {
    Size i = x.ccyIndex(x.eqbs(k)->currency());
    Real Hi_a = Hz(i).eval(x, t0);
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real res = sk_0 + (Hi_b - Hi_a) * zi_0;
    return res;
}

Real com_expectation_1(const CrossAssetModel& x, const Size k, const Time t0, const Real dt) {
    Size i = x.ccyIndex(x.comModel(k)->currency());
    Real res = integral(x, P(rzc(0, k), Hz(0), az(0), comDiffusionIntegrand(t0 + dt, k)), t0, t0 + dt);
    if (i > 0) {
        res -= integral(x, P(rxc(i - 1, k), sx(i - 1), comDiffusionIntegrand(t0 + dt, k)), t0, t0 + dt);
    }
    return res;
}

Real ir_ir_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real rzzij = x.correlation(CrossAssetModel::AssetType::IR, i, CrossAssetModel::AssetType::IR, j, 0, 0);
    const auto lgmi = x.irlgm1f(i);
    const auto lgmj = x.irlgm1f(j);
    return x.integrator()->operator()(
        [&lgmi, &lgmj, rzzij](const Real t) { return lgmi->alpha(t) * lgmj->alpha(t) * rzzij; }, t0, t0 + dt);
}

Real ir_fx_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    const auto lgm0 = x.irlgm1f(0);
    const auto lgmi = x.irlgm1f(i);
    const auto lgmj1 = x.irlgm1f(j + 1);
    const auto fxj = x.fxbs(j);
    Real rzz0i = x.correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, i, 0, 0);
    Real rzzj1i = x.correlation(CrossAssetModel::AssetType::IR, j + 1, CrossAssetModel::AssetType::IR, i, 0, 0);
    Real rzxij = x.correlation(CrossAssetModel::AssetType::IR, i, CrossAssetModel::AssetType::FX, j, 0, 0);

    Real H0 = lgm0->H(t0 + dt);
    Real Hj1 = lgmj1->H(t0 + dt);

    return x.integrator()->operator()(
        [&lgm0, &lgmi, &lgmj1, &fxj, rzz0i, rzzj1i, rzxij, H0, Hj1](const Real t) {
            Real a0 = lgm0->alpha(t);
            Real ai = lgmi->alpha(t);
            Real aj1 = lgmj1->alpha(t);
            Real H0t = lgm0->H(t);
            Real Hj1t = lgmj1->H(t);
            Real sj = fxj->sigma(t);
            return H0 * a0 * ai * rzz0i - H0t * a0 * ai * rzz0i - Hj1 * aj1 * ai * rzzj1i + Hj1t * aj1 * ai * rzzj1i +
                   ai * sj * rzxij;
        },
        t0, t0 + dt);
}

Real fx_fx_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    const auto lgm0 = x.irlgm1f(0);
    const auto lgmi1 = x.irlgm1f(i + 1);
    const auto lgmj1 = x.irlgm1f(j + 1);
    const auto fxi = x.fxbs(i);
    const auto fxj = x.fxbs(j);
    Real rzz0i1 = x.correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, i + 1, 0, 0);
    Real rzz0j1 = x.correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, j + 1, 0, 0);
    Real rzzi1j1 = x.correlation(CrossAssetModel::AssetType::IR, i + 1, CrossAssetModel::AssetType::IR, j + 1, 0, 0);
    Real rzx0i = x.correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX, i, 0, 0);
    Real rzx0j = x.correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX, j, 0, 0);
    Real rzxi1j = x.correlation(CrossAssetModel::AssetType::IR, i + 1, CrossAssetModel::AssetType::FX, j, 0, 0);
    Real rzxj1i = x.correlation(CrossAssetModel::AssetType::IR, j + 1, CrossAssetModel::AssetType::FX, i, 0, 0);
    Real rxxij = x.correlation(CrossAssetModel::AssetType::FX, i, CrossAssetModel::AssetType::FX, j, 0, 0);
    Real H0 = lgm0->H(t0 + dt);
    Real Hi1 = lgmi1->H(t0 + dt);
    Real Hj1 = lgmj1->H(t0 + dt);

    return
        // row 1 (pt 1 of 2)
        H0 * H0 * (lgm0->zeta(t0 + dt) - lgm0->zeta(t0)) +
        x.integrator()->operator()(
            [&lgm0, &lgmi1, &lgmj1, &fxi, &fxj, rzz0i1, rzz0j1, rzzi1j1, rzx0i, rzx0j, rzxi1j, rzxj1i, rxxij, H0, Hi1,
             Hj1](const Real t) {
                Real a0 = lgm0->alpha(t);
                Real ai1 = lgmi1->alpha(t);
                Real aj1 = lgmj1->alpha(t);
                Real H0t = lgm0->H(t);
                Real Hi1t = lgmi1->H(t);
                Real Hj1t = lgmj1->H(t);
                Real si = fxi->sigma(t);
                Real sj = fxj->sigma(t);
                return
                    // row 1 (pt 2 of 2)
                    -2.0 * H0 * H0t * a0 * a0 + H0t * H0t * a0 * a0 -
                    // row 2
                    H0 * Hj1 * a0 * aj1 * rzz0j1 + Hj1 * H0t * a0 * aj1 * rzz0j1 + H0 * Hj1t * aj1 * a0 * rzz0j1 -
                    H0t * Hj1t * a0 * aj1 * rzz0j1 -
                    // row 3
                    H0 * Hi1 * a0 * ai1 * rzz0i1 + Hi1 * H0t * a0 * ai1 * rzz0i1 + H0 * Hi1t * ai1 * a0 * rzz0i1 -
                    H0t * Hi1t * a0 * ai1 * rzz0i1 +
                    // row 4
                    H0 * a0 * sj * rzx0j - H0t * a0 * sj * rzx0j +
                    // row 5
                    H0 * a0 * si * rzx0i - H0t * a0 * si * rzx0i -
                    // row 6
                    Hi1 * ai1 * sj * rzxi1j + Hi1t * ai1 * sj * rzxi1j -
                    // row 7
                    Hj1 * aj1 * si * rzxj1i + Hj1t * aj1 * si * rzxj1i +
                    // row 8
                    Hi1 * Hj1 * ai1 * aj1 * rzzi1j1 - Hj1 * Hi1t * ai1 * aj1 * rzzi1j1 -
                    Hi1 * Hj1t * aj1 * ai1 * rzzi1j1 + Hi1t * Hj1t * ai1 * aj1 * rzzi1j1 +
                    // row 9
                    si * sj * rxxij;
            },
            t0, t0 + dt);
}

Real infz_infz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    return integral(x, P(ryy(i, j), ay(i), ay(j)), t0, t0 + dt);
}

Real infz_infy_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Real res = 0.0;

    // Assumption that INF is either JY or DK. j-th inflation model's y component depends on model type.
    if (x.modelType(CrossAssetModel::AssetType::INF, j) == CrossAssetModel::ModelType::DK) {
        res = integral(x, P(ryy(i, j), ay(i), Hy(j), ay(j)), t0, t0 + dt);
    } else {
        // i_j - index of j-th inflation component's currency.
        Size i_j = x.ccyIndex(x.infjy(j)->currency());
        // H_{i_j}^{z}(t_0 + dt)
        Real Hi_j = Hz(i_j).eval(x, t0 + dt);
        // H_{j}^{y}(t_0 + dt)
        Real Hj = Hy(j).eval(x, t0 + dt);

        res = integral(x, P(rzy(i_j, i), az(i_j), ay(i), LC(Hi_j, -1.0, Hz(i_j))), t0, t0 + dt);
        res -= integral(x, P(ryy(i, j), ay(i), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res += integral(x, P(ryy(i, j, 0, 1), ay(i), sy(j)), t0, t0 + dt);
    }

    return res;
}

Real infy_infy_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Real res = 0.0;

    // Assumption that INF is either JY or DK. Four possibilities.
    auto mti = x.modelType(CrossAssetModel::AssetType::INF, i);
    auto mtj = x.modelType(CrossAssetModel::AssetType::INF, j);
    if (mti == CrossAssetModel::ModelType::DK && mtj == CrossAssetModel::ModelType::DK) {
        res = integral(x, P(ryy(i, j), Hy(i), ay(i), Hy(j), ay(j)), t0, t0 + dt);
    } else if (mti == CrossAssetModel::ModelType::JY && mtj == CrossAssetModel::ModelType::DK) {

        // i_i - index of i-th inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        // H_{i_i}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        // H_{i}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);

        // 3 terms in the covariance.
        res = integral(x, P(rzy(i_i, j), Hy(j), ay(j), az(i_i), LC(Hi_i, -1.0, Hz(i_i))), t0, t0 + dt);
        res -= integral(x, P(ryy(i, j, 0, 0), Hy(j), ay(j), ay(i), LC(Hi, -1.0, Hy(i))), t0, t0 + dt);
        res += integral(x, P(ryy(i, j, 1, 0), Hy(j), ay(j), sy(i)), t0, t0 + dt);

    } else if (mti == CrossAssetModel::ModelType::DK && mtj == CrossAssetModel::ModelType::JY) {

        // i_j - index of j-th inflation component's currency.
        Size i_j = x.ccyIndex(x.infjy(j)->currency());
        // H_{i_j}^{z}(t_0 + dt)
        Real Hi_j = Hz(i_j).eval(x, t0 + dt);
        // H_{j}^{y}(t_0 + dt)
        Real Hj = Hy(j).eval(x, t0 + dt);

        // 3 terms in the covariance.
        res = integral(x, P(rzy(i_j, i), Hy(i), ay(i), az(i_j), LC(Hi_j, -1.0, Hz(i_j))), t0, t0 + dt);
        res -= integral(x, P(ryy(i, j, 0, 0), Hy(i), ay(i), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res += integral(x, P(ryy(i, j, 0, 1), Hy(i), ay(i), sy(j)), t0, t0 + dt);

    } else {

        // index of each inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        Size i_j = x.ccyIndex(x.infjy(j)->currency());
        // H_{i_\cdot}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        Real Hi_j = Hz(i_j).eval(x, t0 + dt);
        // H_{\cdot}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);
        Real Hj = Hy(j).eval(x, t0 + dt);

        res = integral(x, P(rzz(i_i, i_j), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), az(i_j), LC(Hi_j, -1.0, Hz(i_j))), t0,
                       t0 + dt);
        res -=
            integral(x, P(rzy(i_i, j, 0), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res += integral(x, P(rzy(i_i, j, 1), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), sy(j)), t0, t0 + dt);
        res -=
            integral(x, P(rzy(i_j, i, 0), ay(i), LC(Hi, -1.0, Hy(i)), az(i_j), LC(Hi_j, -1.0, Hz(i_j))), t0, t0 + dt);
        res += integral(x, P(ryy(i, j), ay(i), LC(Hi, -1.0, Hy(i)), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res -= integral(x, P(ryy(i, j, 0, 1), ay(i), LC(Hi, -1.0, Hy(i)), sy(j)), t0, t0 + dt);
        res += integral(x, P(rzy(i_j, i, 1), sy(i), az(i_j), LC(Hi_j, -1.0, Hz(i_j))), t0, t0 + dt);
        res -= integral(x, P(ryy(i, j, 1, 0), sy(i), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res += integral(x, P(ryy(i, j, 1, 1), sy(i), sy(j)), t0, t0 + dt);
    }

    return res;
}

Real ir_infz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    return integral(x, P(rzy(i, j), az(i), ay(j)), t0, t0 + dt);
}

Real ir_infy_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    // Assumption that INF is either JY or DK.
    if (x.modelType(CrossAssetModel::AssetType::INF, j) == CrossAssetModel::ModelType::DK) {
        return integral(x, P(rzy(i, j), az(i), Hy(j), ay(j)), t0, t0 + dt);
    } else {
        // i_j - index of j-th inflation component's currency.
        Size i_j = x.ccyIndex(x.infjy(j)->currency());
        // H_{i_j}^{z}(t_0 + dt)
        Real Hi_j = Hz(i_j).eval(x, t0 + dt);
        // H_{j}^{y}(t_0 + dt)
        Real Hj = Hy(j).eval(x, t0 + dt);

        Real res = integral(x, P(rzz(i, i_j), az(i), az(i_j), LC(Hi_j, -1.0, Hz(i_j))), t0, t0 + dt);
        res -= integral(x, P(rzy(i, j, 0), az(i), ay(j), LC(Hj, -1.0, Hy(j))), t0, t0 + dt);
        res += integral(x, P(rzy(i, j, 1), az(i), sy(j)), t0, t0 + dt);

        return res;
    }
}

Real fx_infz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzy(0, j), Hz(0), az(0), ay(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzy(0, j), az(0), ay(j)), t0, t0 + dt) +
               integral(x, P(rzy(i + 1, j), Hz(i + 1), az(i + 1), ay(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzy(i + 1, j), az(i + 1), ay(j)), t0, t0 + dt) +
               integral(x, P(rxy(i, j), sx(i), ay(j)), t0, t0 + dt);
    return res;
}

Real fx_infy_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Real res = 0.0;
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);

    if (x.modelType(CrossAssetModel::AssetType::INF, j) == CrossAssetModel::ModelType::DK) {
        res = -integral(x, P(rzy(0, j), Hz(0), az(0), Hy(j), ay(j)), t0, t0 + dt) +
              H0 * integral(x, P(rzy(0, j), az(0), Hy(j), ay(j)), t0, t0 + dt) +
              integral(x, P(rzy(i + 1, j), Hz(i + 1), az(i + 1), Hy(j), ay(j)), t0, t0 + dt) -
              Hi * integral(x, P(rzy(i + 1, j), az(i + 1), Hy(j), ay(j)), t0, t0 + dt) +
              integral(x, P(rxy(i, j), sx(i), Hy(j), ay(j)), t0, t0 + dt);
    } else {

        // i_j - index of j-th inflation component's currency.
        Size i_j = x.ccyIndex(x.infjy(j)->currency());
        // H_{i_j}^{z}(t_0 + dt)
        Real Hi_j = Hz(i_j).eval(x, t0 + dt);
        // H_{j}^{y}(t_0 + dt)
        Real Hj = Hy(j).eval(x, t0 + dt);

        res = integral(x, P(rzz(i_j, 0), az(i_j), LC(Hi_j, -1.0, Hz(i_j)), az(0), LC(H0, -1.0, Hz(0))), t0, t0 + dt);
        res -= integral(x, P(rzz(i_j, i + 1), az(i_j), LC(Hi_j, -1.0, Hz(i_j)), az(i + 1), LC(Hi, -1.0, Hz(i + 1))), t0,
                        t0 + dt);
        res += integral(x, P(rzx(i_j, i), az(i_j), LC(Hi_j, -1.0, Hz(i_j)), sx(i)), t0, t0 + dt);
        res -= integral(x, P(rzy(0, j, 0), ay(j), LC(Hj, -1.0, Hy(j)), az(0), LC(H0, -1.0, Hz(0))), t0, t0 + dt);
        res += integral(x, P(rzy(i + 1, j, 0), ay(j), LC(Hj, -1.0, Hy(j)), az(i + 1), LC(Hi, -1.0, Hz(i + 1))), t0,
                        t0 + dt);
        res -= integral(x, P(rxy(i, j), ay(j), LC(Hj, -1.0, Hy(j)), sx(i)), t0, t0 + dt);
        res += integral(x, P(rzy(0, j, 1), sy(j), az(0), LC(H0, -1.0, Hz(0))), t0, t0 + dt);
        res -= integral(x, P(rzy(i + 1, j, 1), sy(j), az(i + 1), LC(Hi, -1.0, Hz(i + 1))), t0, t0 + dt);
        res += integral(x, P(rxy(i, j, 1), sx(i), sy(j)), t0, t0 + dt);
    }

    return res;
}

Real crz_crz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), al(i), al(j)), t0, t0 + dt);
    return res;
}

Real crz_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), al(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real cry_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rll(i, j), Hl(i), al(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real ir_crz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzl(i, j), az(i), al(j)), t0, t0 + dt);
    return res;
}

Real ir_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(rzl(i, j), az(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real fx_crz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzl(0, j), Hz(0), az(0), al(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzl(0, j), az(0), al(j)), t0, t0 + dt) +
               integral(x, P(rzl(i + 1, j), Hz(i + 1), az(i + 1), al(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzl(i + 1, j), az(i + 1), al(j)), t0, t0 + dt) +
               integral(x, P(rxl(i, j), sx(i), al(j)), t0, t0 + dt);
    return res;
}

Real fx_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real H0 = Hz(0).eval(x, t0 + dt);
    Real Hi = Hz(i + 1).eval(x, t0 + dt);
    Real res = -integral(x, P(rzl(0, j), Hz(0), az(0), Hl(j), al(j)), t0, t0 + dt) +
               H0 * integral(x, P(rzl(0, j), az(0), Hl(j), al(j)), t0, t0 + dt) +
               integral(x, P(rzl(i + 1, j), Hz(i + 1), az(i + 1), Hl(j), al(j)), t0, t0 + dt) -
               Hi * integral(x, P(rzl(i + 1, j), az(i + 1), Hl(j), al(j)), t0, t0 + dt) +
               integral(x, P(rxl(i, j), sx(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real infz_crz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), ay(i), al(j)), t0, t0 + dt);
    return res;
}

Real infz_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(ryl(i, j), ay(i), Hl(j), al(j)), t0, t0 + dt);
    return res;
}

Real infy_crz_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Real res = 0.0;
    if (x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::DK) {
        res = integral(x, P(ryl(i, j), Hy(i), ay(i), al(j)), t0, t0 + dt);
    } else {

        // i_i - index of i-th inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        // H_{i_i}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        // H_{i}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);

        res = integral(x, P(rzl(i_i, j), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), al(j)), t0, t0 + dt);
        res -= integral(x, P(ryl(i, j, 0), ay(i), LC(Hi, -1.0, Hy(i)), al(j)), t0, t0 + dt);
        res += integral(x, P(ryl(i, j, 1), sy(i), al(j)), t0, t0 + dt);
    }

    return res;
}

Real infy_cry_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Real res = 0.0;
    if (x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::DK) {
        res = integral(x, P(ryl(i, j), Hy(i), ay(i), Hl(j), al(j)), t0, t0 + dt);
    } else {

        // i_i - index of i-th inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        // H_{i_i}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        // H_{i}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);

        res = integral(x, P(rzl(i_i, j), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), Hl(j), al(j)), t0, t0 + dt);
        res -= integral(x, P(ryl(i, j, 0), ay(i), LC(Hi, -1.0, Hy(i)), Hl(j), al(j)), t0, t0 + dt);
        res += integral(x, P(ryl(i, j, 1), sy(i), Hl(j), al(j)), t0, t0 + dt);
    }

    return res;
}

Real ir_eq_covariance(const CrossAssetModel& x, const Size j, const Size k, const Time t0, const Time dt) {
    Size i = x.ccyIndex(x.eqbs(k)->currency()); // the equity underlying currency
    Real Hi_b = Hz(i).eval(x, t0 + dt);
    Real res = Hi_b * integral(x, P(rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res -= integral(x, P(Hz(i), rzz(i, j), az(i), az(j)), t0, t0 + dt);
    res += integral(x, P(rzs(j, k), az(j), ss(k)), t0, t0 + dt);
    return res;
}

Real fx_eq_covariance(const CrossAssetModel& x, const Size j, const Size k, const Time t0, const Time dt) {
    Size i = x.ccyIndex(x.eqbs(k)->currency()); // the equity underlying currency
    const Size& j_lgm = j + 1;                  // indexing of the FX currency for extracting the LGM terms
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

Real infz_eq_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x.ccyIndex(x.eqbs(j)->currency());
    Real Hk = Hz(k).eval(x, t0 + dt);
    Real res = Hk * integral(x, P(rzy(k, i), az(k), ay(i)), t0, t0 + dt) -
               integral(x, P(rzy(k, i), Hz(k), az(k), ay(i)), t0, t0 + dt) +
               integral(x, P(rys(i, j), ay(i), ss(j)), t0, t0 + dt);
    return res;
}

Real infy_eq_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {

    Size k = x.ccyIndex(x.eqbs(j)->currency());
    Real Hk = Hz(k).eval(x, t0 + dt);

    Real res = 0.0;
    if (x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::DK) {
        res = Hk * integral(x, P(rzy(k, i), az(k), Hy(i), ay(i)), t0, t0 + dt) -
              integral(x, P(rzy(k, i), Hz(k), az(k), Hy(i), ay(i)), t0, t0 + dt) +
              integral(x, P(rys(i, j), Hy(i), ay(i), ss(j)), t0, t0 + dt);
    } else {

        // i_i - index of i-th inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        // H_{i_i}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        // H_{i}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);

        res = integral(x, P(rzz(i_i, k), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), az(k), LC(Hk, -1.0, Hz(k))), t0, t0 + dt);
        res += integral(x, P(rzs(i_i, j), az(i_i), LC(Hi_i, -1.0, Hz(i_i)), ss(j)), t0, t0 + dt);
        res -= integral(x, P(rzy(k, i, 0), ay(i), LC(Hi, -1.0, Hy(i)), az(k), LC(Hk, -1.0, Hz(k))), t0, t0 + dt);
        res -= integral(x, P(rys(i, j, 0), ay(i), LC(Hi, -1.0, Hy(i)), ss(j)), t0, t0 + dt);
        res += integral(x, P(rzy(k, i, 1), sy(i), az(k), LC(Hk, -1.0, Hz(k))), t0, t0 + dt);
        res += integral(x, P(rys(i, j, 1), sy(i), ss(j)), t0, t0 + dt);
    }

    return res;
}

Real crz_eq_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x.ccyIndex(x.eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzl(k, i), az(k), al(i)), t0, t0 + dt) -
               integral(x, P(rzl(k, i), Hz(k), az(k), al(i)), t0, t0 + dt) +
               integral(x, P(rls(i, j), al(i), ss(j)), t0, t0 + dt);
    return res;
}

Real cry_eq_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x.ccyIndex(x.eqbs(j)->currency());
    Real Hk_b = Hz(k).eval(x, t0 + dt);
    Real res = Hk_b * integral(x, P(rzl(k, i), az(k), Hl(i), al(i)), t0, t0 + dt) -
               integral(x, P(rzl(k, i), Hz(k), az(k), Hl(i), al(i)), t0, t0 + dt) +
               integral(x, P(rls(i, j), Hl(i), al(i), ss(j)), t0, t0 + dt);
    return res;
}

Real eq_eq_covariance(const CrossAssetModel& x, const Size k, const Size l, const Time t0, const Time dt) {
    Size i = x.ccyIndex(x.eqbs(k)->currency()); // ccy underlying equity k
    Size j = x.ccyIndex(x.eqbs(l)->currency()); // ccy underlying equity l
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

Real aux_aux_covariance(const CrossAssetModel& x, const Time t0, const Time dt) {
    Real res = integral(x, P(az(0), az(0), Hz(0), Hz(0)), t0, t0 + dt);
    return res;
}

Real aux_ir_covariance(const CrossAssetModel& x, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(az(0), Hz(0), az(j), rzz(0, j)), t0, t0 + dt);
    return res;
}

Real aux_fx_covariance(const CrossAssetModel& x, const Size j, const Time t0, const Time dt) {
    Real res = Hz(0).eval(x, t0 + dt) * integral(x, P(az(0), az(0), Hz(0)), t0, t0 + dt) -
               integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t0 + dt) -
               Hz(j + 1).eval(x, t0 + dt) * integral(x, P(az(j + 1), az(0), Hz(0), rzz(j + 1, 0)), t0, t0 + dt) +
               integral(x, P(Hz(j + 1), az(j + 1), az(0), Hz(0), rzz(j + 1, 0)), t0, t0 + dt) +
               integral(x, P(az(0), Hz(0), sx(j), rzx(0, j)), t0, t0 + dt);
    return res;
}

Real com_com_covariance(const CrossAssetModel& x, const Size k, const Size l, const Time t0, const Time dt) {
    Real res = integral(x, P(rcc(k, l), comDiffusionIntegrand(t0 + dt, k), comDiffusionIntegrand(t0 + dt, l)), t0, t0 + dt);
    return res;
}

Real ir_com_covariance(const CrossAssetModel& model, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(model, P(rzc(i,j), az(i), comDiffusionIntegrand(t0 + dt, j)), t0, t0 + dt);
    return res;
}

Real fx_com_covariance(const CrossAssetModel& x, const Size j, const Size k, const Time t0, const Time dt) {
    Real Hj_b = Hz(j + 1).eval(x, t0 + dt);
    Real H0_b = Hz(0).eval(x, t0 + dt);
    Real res = H0_b * integral(x, P(rzc(0, k), az(0), comDiffusionIntegrand(t0 + dt,k)), t0, t0 + dt);
    res -= integral(x, P(Hz(0), rzc(0, k), az(0), comDiffusionIntegrand(t0 + dt,k)), t0, t0 + dt);
    res -= Hj_b * integral(x, P(rzc(j + 1, k), az(j + 1), comDiffusionIntegrand(t0 + dt,k)), t0, t0 + dt);
    res += integral(x, P(Hz(j + 1), rzc(j + 1, k), az(j + 1), comDiffusionIntegrand(t0 + dt,k)), t0, t0 + dt);
    res += integral(x, P(rxc(j, k), sx(j), comDiffusionIntegrand(t0 + dt,k)), t0, t0 + dt);
    return res; 
}

Real infz_com_covariance(const CrossAssetModel& model, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(model, P(ryc(i, j), ay(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    return res;
}

Real infy_com_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = 0.0;
    if (x.modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::DK) {
        res = integral(x, P(ryc(i, j), Hy(i), ay(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    } else {
        // i_i - index of i-th inflation component's currency.
        Size i_i = x.ccyIndex(x.infjy(i)->currency());
        // H_{i_i}^{z}(t_0 + dt)
        Real Hi_i = Hz(i_i).eval(x, t0 + dt);
        // H_{i}^{y}(t_0 + dt)
        Real Hi = Hy(i).eval(x, t0 + dt);
        res += Hi_i*integral(x, P(rzc(i_i, i), az(i_i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
        res -= integral(x, P(rzc(i_i, i), Hz(i_i), az(i_i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
        res -= Hi*integral(x, P(ryc(i, j, 0), ay(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
        res += integral(x, P(ryc(i, j, 0), Hy(i), ay(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
        res += integral(x, P(ryc(i, j, 1), sy(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    }
    return res;
}

Real cry_com_covariance(const CrossAssetModel& model, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(model, P(rlc(i, j), Hl(i), al(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    return res;
}

Real crz_com_covariance(const CrossAssetModel& model, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(model, P(rlc(i, j), al(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    return res;
}

Real eq_com_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Size k = x.ccyIndex(x.comModel(j)->currency());
    Real Hk = Hz(k).eval(x, t0 + dt);
    Real res = Hk*integral(x, P(rzc(k, j),  az(k), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    res -= integral(x, P(rzc(k, j), Hz(k), az(k), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    res += integral(x, P(rsc(i, j), ss(i), comDiffusionIntegrand(t0 + dt,j)), t0, t0 + dt);
    return res;
}

Real ir_crstate_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, P(az(i), rzcrs(i, j)), t0, t0 + dt);
    return res;
}

Real fx_crstate_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = Hz(0).eval(x, t0 + dt) * integral(x, P(az(0), rzcrs(0, j)), t0, t0 + dt) -
               integral(x, P(Hz(0), az(0), rzcrs(0, j)), t0, t0 + dt) -
               Hz(i + 1).eval(x, t0 + dt) * integral(x, P(az(i + 1), rzcrs(i + 1, j)), t0, t0 + dt) +
               integral(x, P(Hz(i + 1), az(i + 1), rzcrs(i + 1, j)), t0, t0 + dt) +
               integral(x, P(sx(i), rxcrs(i, j)), t0, t0 + dt);
    return res;
}

Real crstate_crstate_covariance(const CrossAssetModel& x, const Size i, const Size j, const Time t0, const Time dt) {
    Real res = integral(x, rccrs(i, j), t0, t0 + dt);
    return res;
}

} // namespace CrossAssetAnalytics
} // namespace QuantExt
