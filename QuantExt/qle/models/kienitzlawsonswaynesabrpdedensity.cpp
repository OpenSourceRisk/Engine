/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/models/kienitzlawsonswaynesabrpdedensity.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

KienitzLawsonSwayneSabrPdeDensity::KienitzLawsonSwayneSabrPdeDensity(const Real alpha, const Real beta, const Real nu,
                                                                     const Real rho, const Real forward,
                                                                     const Real expiryTime, const Real displacement,
                                                                     const Size zSteps, const Size tSteps,
                                                                     const Real nStdDev)
    : alpha_(alpha), beta_(beta), nu_(nu), rho_(rho), forward_(forward), expiryTime_(expiryTime),
      displacement_(displacement), zSteps_(zSteps), tSteps_(tSteps), nStdDev_(nStdDev) {

    QL_REQUIRE(alpha > 0.0, "KienitzLawsonSwayneSabrPdeDensity: alpha (" << alpha << ") must be positive");
    QL_REQUIRE(beta >= 0.0 && beta < 1.0, "KienitzLawsonSwayneSabrPdeDensity: beta (" << beta << ") must be in [0,1)");
    QL_REQUIRE(nu > 0.0, "KienitzLawsonSwayneSabrPdeDensity: nu (" << nu << ") must be positive");
    QL_REQUIRE(rho > -1.0 && rho < 1.0, "KienitzLawsonSwayneSabrPdeDensity: rho (" << rho << ") must be in (-1,1)");
    QL_REQUIRE(expiryTime > 0.0,
               "KienitzLawsonSwayneSabrPdeDensity: expiryTime (" << expiryTime << ") must be positve");
    QL_REQUIRE(zSteps > 1, "KienitzLawsonSwayneSabrPdeDensity: zSteps (" << zSteps << ") must be >1");
    QL_REQUIRE(tSteps > 0, "KienitzLawsonSwayneSabrPdeDensity: tSteps (" << tSteps << ") must be positive");
    QL_REQUIRE(nStdDev > 0.0, "KienitzLawsonSwayneSabrPdeDensity: nStdDev (" << nStdDev << ") must be positive");

    calculate();
}

Real KienitzLawsonSwayneSabrPdeDensity::yf(const Real f) const {
    Real betam = 1.0 - beta_;
    return (std::pow(f + displacement_, betam) - std::pow(forward_ + displacement_, betam)) / betam;
}

Real KienitzLawsonSwayneSabrPdeDensity::fy(const Real y) const {
    Real betam = 1.0 - beta_;
    Real par = std::pow(forward_ + displacement_, betam) + betam * y;
    if (beta_ > 0) {
        par = std::max(par, 0.0);
        return std::pow(par, 1.0 / betam) - displacement_;
    } else {
        return par - displacement_;
    }
}

Real KienitzLawsonSwayneSabrPdeDensity::yz(const Real z) const {
    return alpha_ / nu_ * (std::sinh(nu_ * z) + rho_ * (std::cosh(nu_ * z) - 1.0));
}

Real KienitzLawsonSwayneSabrPdeDensity::zy(const Real y) const {
    Real tmp = rho_ + nu_ * y / alpha_;
    return -1.0 / nu_ *
           std::log((std::sqrt(1.0 - rho_ * rho_ + tmp * tmp) - rho_ - nu_ * y / alpha_) / (1.0 - rho_)); // ???
}

void KienitzLawsonSwayneSabrPdeDensity::tridag(const Array& a, const Array& b, const Array& c, const Array& R, Array& u,
                                               const Size n, bool firstLastRZero) {
    Array gam(n);
    if (QuantLib::close_enough(b[0], 0.0)) {
        std::fill(u.begin(), u.end(), 0.0); // ???
        return;
    }
    Real bet = b[0];
    u[0] = firstLastRZero ? 0.0 : (R[0] / bet);
    for (Size j = 1; j < n; ++j) {
        gam[j] = c[j - 1] / bet;
        bet = b[j] - a[j] * gam[j];
        if (QuantLib::close_enough(bet, 0.0)) {
            QL_FAIL("KienitzLawsonSwayneSabrPdeDensity: tridag failed");
        }
        if (j < n - 1 || !firstLastRZero)
            u[j] = (R[j] - a[j] * u[j - 1]) / bet;
        else
            u[j] = (-a[j] * u[j - 1]) / bet;
    }
    for (Size j = n - 1; j >= 1; --j) {
        u[j - 1] -= gam[j] * u[j];
    }
} // tridag

void KienitzLawsonSwayneSabrPdeDensity::solveTimeStep_LS(const Array& fm, const Array& cm, const Array& Em,
                                                         const Real dt, const Array& PP_in, const Real PL_in,
                                                         const Real PR_in, const Size n, Array& PP_out, Real& PL_out,
                                                         Real& PR_out) {

    Array bb(n), aa(n), cc(n);

    Real frac = dt / (2.0 * hh_);

    for (Size i = 1; i < n - 1; ++i) {
        aa[i] = -frac * cm[i - 1] * Em[i - 1] / (fm[i] - fm[i - 1]); // lower diagonal
        bb[i] = 1.0 + frac * (cm[i] * Em[i] * (1.0 / (fm[i + 1] - fm[i]) + 1.0 / (fm[i] - fm[i - 1]))); // diagonal
        cc[i] = -frac * cm[i + 1] * Em[i + 1] / (fm[i + 1] - fm[i]); // upper diagonal
    }

    bb[0] = cm[0] / (fm[1] - fm[0]) * Em[0];
    bb[n - 1] = cm[n - 1] / (fm[n - 1] - fm[n - 2]) * Em[n - 1];
    aa[0] = 0.0; // need to init this ?
    aa[n - 1] = cm[n - 2] / (fm[n - 1] - fm[n - 2]) * Em[n - 2];
    cc[0] = cm[1] / (fm[1] - fm[0]) * Em[1];
    cc[n - 1] = 0.0; // need to init this ?

    tridag(aa, bb, cc, PP_in, PP_out, n, true);

    PL_out = PL_in + dt * cm[1] / (fm[1] - fm[0]) * Em[1] * PP_out[1];
    PR_out = PR_in + dt * cm[n - 2] / (fm[n - 1] - fm[n - 2]) * Em[n - 2] * PP_out[n - 2];
} // solveTimeStep_LS

void KienitzLawsonSwayneSabrPdeDensity::PDE_method(const Array& fm, const Array& Ccm, const Array& Gamma_Vec,
                                                   const Real dt, const Size Nj) {

    Array Emdt1_vec(Nj), Emdt2_vec(Nj), PP1(Nj), PP2(Nj), Em(Nj, 1.0), EmO(Nj);
    constexpr Real b = 1.0 - M_SQRT_2;
    Real dt1 = dt * b;
    Real dt2 = dt * (1.0 - 2.0 * b);
    for (Size i = 1; i < Nj - 1; ++i) {
        Emdt1_vec[i] = std::exp(rho_ * nu_ * alpha_ * Gamma_Vec[i] * dt1);
        Emdt2_vec[i] = std::exp(rho_ * nu_ * alpha_ * Gamma_Vec[i] * dt2);
    }
    Emdt1_vec[0] = Emdt1_vec[1];
    Emdt1_vec[Nj - 1] = Emdt1_vec[Nj - 2];
    Emdt2_vec[0] = Emdt2_vec[1];
    Emdt2_vec[Nj - 1] = Emdt2_vec[Nj - 2];
    pL_ = pR_ = 0.0;

    // we need two time steps

    Real PL1, PR1, PL2, PR2;
    for (Size it = 0; it < tSteps_; ++it) {
        for (Size j = 1; j < Nj - 1; ++j) {
            Em[j] *= Emdt1_vec[j];
        }
        solveTimeStep_LS(fm, Ccm, Em, dt1, SABR_Prob_Vec_, pL_, pR_, Nj, PP1, PL1, PR1);
        for (Size j = 1; j < Nj - 1; ++j) {
            Em[j] *= Emdt1_vec[j];
        }
        solveTimeStep_LS(fm, Ccm, Em, dt1, PP1, PL1, PR1, Nj, PP2, PL2, PR2);
        for (Size j = 1; j < Nj - 1; ++j) {
            SABR_Prob_Vec_[j] = (M_SQRT2 + 1.0) * PP2[j] - M_SQRT2 * PP1[j];
            Em[j] *= Emdt2_vec[j];
        }
        SABR_Prob_Vec_[0] = -SABR_Prob_Vec_[1];
        SABR_Prob_Vec_[Nj - 1] = -SABR_Prob_Vec_[Nj - 2];
        pL_ = (M_SQRT2 + 1.0) * PL2 - M_SQRT2 * PL1;
        pR_ = (M_SQRT2 + 1.0) * PR2 - M_SQRT2 * PR1;
    }

} // PDE_method

void KienitzLawsonSwayneSabrPdeDensity::calculate() {

    Real betam = 1.0 - beta_;
    zMin_ = -nStdDev_ * std::sqrt(expiryTime_);
    zMax_ = -zMin_;

    if (beta_ > 0.0) {
        zMin_ = std::max(zMin_, zy(yf(-displacement_)));
    }

    Size j = zSteps_ - 2;
    Real h0 = (zMax_ - zMin_) / static_cast<Real>(j);
    Size j0 = static_cast<int>(-zMin_ / h0 + 0.5);
    hh_ = -zMin_ / (static_cast<Real>(j0) - 0.5);

    Array zm(zSteps_), ym(zSteps_), fm(zSteps_), CCm(zSteps_), Gamma_vec(zSteps_);

    for (Size i = 0; i < zSteps_; ++i) {
        zm[i] = static_cast<Real>(i) * hh_ + zMin_ - 0.5 * hh_;
        ym[i] = yz(zm[i]);
        fm[i] = fy(ym[i]);
    }

    zMax_ = static_cast<Real>(zSteps_ - 1) * hh_ + zMin_;
    Real ymax = yz(zMax_);
    Real ymin = yz(zMin_);
    Real fmax = fy(ymax);
    Real fmin = fy(ymin);

    fm[0] = 2 * fmin - fm[1];
    fm[zSteps_ - 1] = 2 * fmax - fm[zSteps_ - 2];

    for (Size i = 1; i < zSteps_ - 1; ++i) {
        CCm[i] = std::sqrt(alpha_ * alpha_ + 2.0 * rho_ * alpha_ * nu_ * ym[i] + nu_ * nu_ * ym[i] * ym[i]) *
                 std::pow(fm[i] + displacement_, beta_);
        if (i != j0)
            Gamma_vec[i] = (std::pow(fm[i] + displacement_, beta_) - std::pow(forward_ + displacement_, beta_)) /
                           (fm[i] - forward_);
    }
    CCm[0] = CCm[1];
    CCm[zSteps_ - 1] = CCm[zSteps_ - 2];
    Gamma_vec[0] = 0.0;           // need to init this ?
    Gamma_vec[zSteps_ - 1] = 0.0; // need to init this ?

    SABR_Prob_Vec_ = Array(zSteps_, 0.0);

    Gamma_vec[j0] = beta_ / std::pow(forward_ + displacement_, betam);
    SABR_Prob_Vec_[j0] = 1.0 / hh_;

    PDE_method(fm, CCm, Gamma_vec, expiryTime_ / static_cast<Real>(tSteps_), zSteps_);
    theta0_ = SABR_Prob_Vec_[j0];

    // cumulative probability
    SABR_Cum_Prob_Vec_ = Array(SABR_Prob_Vec_.size());
    SABR_Cum_Prob_Vec_[0] = pL();
    for (Size k = 1; k < SABR_Cum_Prob_Vec_.size() - 1; ++k)
        SABR_Cum_Prob_Vec_[k] = SABR_Cum_Prob_Vec_[k - 1] + SABR_Prob_Vec_[k] * hh_;
    SABR_Cum_Prob_Vec_.back() = 1.0;

} // calculate

Real KienitzLawsonSwayneSabrPdeDensity::inverseCumulative(const Real p) const {
    int idx = std::upper_bound(SABR_Cum_Prob_Vec_.begin(), SABR_Cum_Prob_Vec_.end(), std::max(std::min(p, 1.0), 0.0)) -
              SABR_Cum_Prob_Vec_.begin();
    if (idx == 0)
        return fy(yz(zMin()));
    else if (idx == SABR_Cum_Prob_Vec_.size())
        return fy(yz(zMax()));
    Real zl = zMin() + static_cast<Real>(idx - 1) * hh();
    Real zr = zMin() + static_cast<Real>(idx) * hh();
    Real cl = SABR_Cum_Prob_Vec_[idx - 1];
    Real cr = SABR_Cum_Prob_Vec_[idx];
    Real alpha = (cr - p) / (cr - cl);
    return fy(yz(alpha * zl + (1.0 - alpha) * zr));
}

std::vector<Real> KienitzLawsonSwayneSabrPdeDensity::callPrices(const std::vector<Real>& strikes) const {
    std::vector<Real> result;
    for (auto const& strike : strikes) {
        Real zstrike = zy(yf(strike));
        if (zstrike <= zMin())
            result.push_back(forward() - strike);
        else if (zstrike >= zMax())
            result.push_back(0.0);
        else {
            Real fmax = fy(yz(zMax()));
            Real p = (fmax - strike) * pR(); // rightmost value
            // unused ???
            // Real k0 = std::ceil((zstrike - zMin()) / hh());
            // Real ftilde = fy(yz(zMin() + k0 * hh));
            // Real term1 = ftilde - strike;
            // in between points
            Real ft1;
            Size k = zSteps() - 2;
            for (; k >= 1; --k) {
                Real zm1 = zMin() + (static_cast<Real>(k) - 0.5) * hh();
                ft1 = fy(yz(zm1));
                if (ft1 > strike)
                    p += (ft1 - strike) * sabrProb()[k] * hh();
                else
                    break;
            }
            // k=k+1 ???
            // now k is the value where the payoff is zero and at k+1 the payoff is positive
            // calculating the value for subgridscale
            Real zmK = zMin() + static_cast<Real>(k) * hh();       // last admissable value ft > strike
            Real zmKm1 = zMin() + static_cast<Real>(k - 1) * hh(); // first value with ft < strike
            Real ftK = fy(yz(zmK));
            Real ftKm1 = fy(yz(zmKm1));
            Real diff = ftK - ftKm1;
            Real b = (2.0 * ft1 - ftKm1 - ftK) / diff;
            Real subgridadjust = 0.5 * hh() * sabrProb()[k] * (ftK - strike) * (ftK - strike) / diff *
                                 (1.0 + b * (ftK + 2.0 * strike - 3.0 * ftKm1) / diff);
            p += subgridadjust;
            result.push_back(p);
        }
    }
    return result;
} // callPrices

std::vector<Real> KienitzLawsonSwayneSabrPdeDensity::putPrices(const std::vector<Real>& strikes) const {
    std::vector<Real> result;
    for (auto const& strike : strikes) {
        Real zstrike = zy(yf(strike));
        if (zstrike <= zMin())
            result.push_back(0.0);
        else if (zstrike >= zMax())
            result.push_back(strike - forward()); // wrong in VBA ???
        else {
            Real fmin = fy(yz(zMin()));
            Real p = (strike - fmin) * pL(); // leftmost value
            // in between points
            Real ft1;
            Size k = 1;
            for (; k <= zSteps() - 2; ++k) {
                Real zm1 = zMin() + (static_cast<Real>(k) - 0.5) * hh();
                ft1 = fy(yz(zm1));
                if (strike >= ft1)
                    p += (strike - ft1) * sabrProb()[k] * hh();
                else
                    break;
            }
            // ???
            if (k > 1)
                --k;
            Real zmK = zMin() + static_cast<Real>(k) * hh();       // last admissable value ft > strike
            Real zmKm1 = zMin() + static_cast<Real>(k - 1) * hh(); // first value with ft < strike
            Real ftK = fy(yz(zmK));
            Real ftKm1 = fy(yz(zmKm1));
            Real diff = ftK - ftKm1;
            Real b = (2.0 * ft1 - ftKm1 - ftK) / diff;
            Real subgridadjust = 0.5 * hh() * sabrProb()[k] * (strike - ftKm1) * (strike - ftKm1) / diff *
                                 (1.0 - b * (3.0 * ftK - 2.0 * strike - ftKm1) / diff);
            p += subgridadjust;
            result.push_back(p);
        }
    }
    return result;
} // putPrices

} // namespace QuantExt
