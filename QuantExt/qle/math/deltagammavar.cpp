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

#include <qle/math/deltagammavar.hpp>
#include <qle/math/trace.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/solvers1d/brent.hpp>

namespace QuantExt {

namespace detail {
void check(const Real p) { QL_REQUIRE(p >= 0.0 && p <= 1.0, "p (" << p << ") must be in [0,1] in VaR calculation"); }

void check(const Matrix& omega, const Array& delta) {
    QL_REQUIRE(omega.rows() == omega.columns(),
               "omega (" << omega.rows() << "x" << omega.columns() << ") must be square in VaR calculation");
    QL_REQUIRE(delta.size() == omega.rows(), "delta vector size (" << delta.size() << ") must match omega ("
                                                                   << omega.rows() << "x" << omega.columns() << ")");
}

void check(const Matrix& omega, const Array& delta, const Matrix& gamma) {
    QL_REQUIRE(gamma.rows() == omega.rows() && gamma.columns() == omega.columns(),
               "gamma (" << gamma.rows() << "x" << gamma.columns() << ") must have same dimensions as omega ("
                         << omega.rows() << "x" << omega.columns() << ")");
}
} // namespace detail

namespace {
void moments(const Matrix& omega, const Array& delta, const Matrix& gamma, Real& num, Real& mu, Real& variance) {
    detail::check(omega, delta, gamma);

    // see Carol Alexander, Market Risk, Vol IV
    // Formulas IV5.30 and IV5.31 are buggy though:
    // IV.5.30 should have ... + 3 \delta' \Omega \Gamma \Omega \delta in the numerator
    // IV.5.31 should have ... + 12 \delta \Omega (\Gamma \Omega)^2 \delta + 3\sigma^4 in the numerator

    num = std::max(QuantExt::detail::absMax(delta), QuantExt::detail::absMax(gamma));
    if (close_enough(num, 0.0))
        return;

    Array tmpDelta = 1.0 / num * delta;
    Matrix tmpGamma = 1.0 / num * gamma;

    Real dOd = DotProduct(tmpDelta, omega * tmpDelta);
    Matrix go = tmpGamma * omega;
    Matrix go2 = go * go;
    Real trGo2 = Trace(go2);

    mu = 0.5 * Trace(go);
    variance = dOd + 0.5 * trGo2;
}

void moments(const Matrix& omega, const Array& delta, const Matrix& gamma, Real& num, Real& mu, Real& variance,
             Real& tau, Real& kappa) {
    detail::check(omega, delta, gamma);

    // see Carol Alexander, Market Risk, Vol IV
    // Formulas IV5.30 and IV5.31 are buggy though:
    // IV.5.30 should have ... + 3 \delta' \Omega \Gamma \Omega \delta in the numerator
    // IV.5.31 should have ... + 12 \delta \Omega (\Gamma \Omega)^2 \delta + 3\sigma^4 in the numerator

    num = std::max(QuantExt::detail::absMax(delta), QuantExt::detail::absMax(gamma));
    if (close_enough(num, 0.0))
        return;

    Array tmpDelta = 1.0 / num * delta;
    Matrix tmpGamma = 1.0 / num * gamma;

    Real dOd = DotProduct(tmpDelta, omega * tmpDelta);
    Matrix go = tmpGamma * omega;
    Matrix go2 = go * go;
    Real trGo2 = Trace(go2);

    mu = 0.5 * Trace(go);
    variance = dOd + 0.5 * trGo2;

    Matrix go3 = go2 * go;
    Matrix go4 = go2 * go2;
    Matrix ogo = omega * go;
    Matrix o_go2 = omega * go2;
    Real trGo3 = Trace(go3);
    Real trGo4 = Trace(go4);

    tau = (trGo3 + 3.0 * DotProduct(tmpDelta, ogo * tmpDelta)) / std::pow(variance, 1.5);
    kappa = (3.0 * trGo4 + 12.0 * DotProduct(tmpDelta, o_go2 * tmpDelta)) / (variance * variance);
}

std::pair<Real, Real> bracketRoot(const std::function<Real(Real)>& p, const Real h, const Real growth, const Real tol,
                                  const Size maxSteps, const Real leftBoundary, const Real rightBoundary) {
    // very simple bracketing algorithm, TODO can we do that smarter?
    // the start point is chosen as close to zero as possible
    Real start = std::min(std::max(leftBoundary + QL_EPSILON, 0.0), rightBoundary - QL_EPSILON);
    Real xl = start, xr = start;
    Real yl, yr;
    Real h0 = h;
    yl = yr = p(start);
    if (std::abs(yl) < tol)
        return std::make_pair(start, start);
    Size iter = 0;
    bool movingright = true, movingleft = true;
    while (iter++ < maxSteps && (movingleft || movingright)) {
        // left
        if (movingleft) {
            Real xlh = std::max(xl - h0, leftBoundary + QL_EPSILON);
            Real tmpl = p(xlh);
            if (std::abs(tmpl) < tol)
                return std::make_pair(xlh, xlh);
            if (yl * tmpl < 0)
                return std::make_pair(std::min(xlh, xl), xl);
            if (xl - h0 > leftBoundary) {
                xl -= h0;
                yl = tmpl;
            } else {
                movingleft = false;
            }
        }
        // right
        if (movingright) {
            Real xrh = std::min(xr + h0, rightBoundary - QL_EPSILON);
            Real tmpr = p(xrh);
            if (std::abs(tmpr) < tol)
                return std::make_pair(xrh, xrh);
            if (yr * tmpr < 0)
                return std::make_pair(xr, std::max(xrh, xr));
            if (xr + h0 < rightBoundary) {
                xr += h0;
                yr = tmpr;
            } else {
                movingright = false;
            }
        }
        // stepsize growth
        h0 *= 1.0 + growth;
    }

    // no solution
    return std::make_pair(Null<Real>(), Null<Real>());
}

Real F(const Array& lambda, const Array& delta, const Real x) {
    auto KPrimeMinusX = [&lambda, &delta, &x](const Real k) {
        Real sum = 0.0;
        for (Size j = 0; j < lambda.size(); ++j) {
            Real denom = (1.0 - 2.0 * lambda[j] * k);
            sum += lambda[j] / denom + delta[j] * delta[j] * k * (1.0 - k * lambda[j]) / (denom * denom);
        }
        return sum - x;
    };

    auto K = [&lambda, &delta](const Real k) {
        Real sum = 0.0;
        for (Size j = 0; j < lambda.size(); ++j) {
            Real tmp = (1.0 - 2.0 * lambda[j] * k);
            sum += -0.5 * std::log(tmp) + 0.5 * delta[j] * delta[j] * k * k / tmp;
        }
        return sum;
    };

    auto KPrime2 = [&lambda, &delta](const Real k) {
        Real sum = 0.0;
        for (Size j = 0; j < lambda.size(); ++j) {
            Real denom = (1.0 - 2.0 * lambda[j] * k);
            Real denom2 = denom * denom;
            sum += 2.0 * lambda[j] * lambda[j] / denom2 + delta[j] * delta[j] / (denom2 * denom);
        }
        return sum;
    };

    auto KPrime3 = [&lambda, &delta](const Real k) {
        Real sum = 0.0;
        for (Size j = 0; j < lambda.size(); ++j) {
            Real denom = (1.0 - 2.0 * lambda[j] * k);
            Real denom2 = denom * denom;
            Real l2 = lambda[j] * lambda[j];
            sum += 8.0 * lambda[j] * l2 / (denom2 * denom) + 6.0 * delta[j] * delta[j] * lambda[j] / (denom2 * denom2);
        }
        return sum;
    };

    auto KPrime4 = [&lambda, &delta](const Real k) {
        Real sum = 0.0;
        for (Size j = 0; j < lambda.size(); ++j) {
            Real denom = (1.0 - 2.0 * lambda[j] * k);
            Real denom2 = denom * denom;
            Real denom4 = denom2 * denom2;
            Real l2 = lambda[j] * lambda[j];
            sum += 48.0 * l2 * l2 / (denom2 * denom2) + 48.0 * delta[j] * delta[j] * l2 / (denom4 * denom);
        }
        return sum;
    };

    Real b1 = 0.0, b2 = 0.0, khat, yTol = 1E-7, xTol = 1E-10; // TODO check parameter

    // determine the interval (c1,c2) where K is valid
    QL_REQUIRE(!lambda.empty(), "lambda is empty");
    Real c1 = -QL_MAX_REAL, c2 = QL_MAX_REAL;
    Real minl = *std::min_element(lambda.begin(), lambda.end());
    Real maxl = *std::max_element(lambda.begin(), lambda.end());
    if (minl < 0.0 && !close_enough(minl, 0.0))
        c1 = 1.0 / (2.0 * minl);
    if (maxl > 0.0 && !close_enough(maxl, 0.0))
        c2 = 1.0 / (2.0 * maxl);

    std::vector<Real> singularities;
    for (Size i = 0; i < lambda.size(); ++i) {
        if (!close_enough(lambda[i], 0.0)) {
            singularities.push_back(1.0 / (2.0 * lambda[i]));
        }
    }

    // try to bracket saddlepoint, avoiding singularities
    singularities.push_back(-QL_MAX_REAL);
    singularities.push_back(QL_MAX_REAL);
    std::sort(singularities.begin(), singularities.end());
    Size index = std::upper_bound(singularities.begin(), singularities.end(), 0.0) - singularities.begin();
    int direction = 1;
    Size stepsize = 1, counter = 0, indextmp = index;
    while (counter < singularities.size() - 1) {
        if (indextmp > 0 && indextmp < singularities.size()) {
            // shrink the candidate interval to the admissaible region (c1,c2)
            Real a1 = std::max(std::min(singularities[indextmp - 1], c2), c1);
            Real a2 = std::max(std::min(singularities[indextmp], c2), c1);
            if (!close_enough(a1, a2)) {
                // TODO check parameters in bracketRoot
                std::tie(b1, b2) =
                    bracketRoot(KPrimeMinusX, std::min(1E-5, (a2 - a1) / 10.0), 1E-2, yTol, 100000, a1, a2);
                if (b1 != Null<Real>())
                    break;
            }
            ++counter;
        }
        indextmp = index + direction * stepsize;
        direction *= -1;
        if (direction > 0)
            ++stepsize;
        QL_REQUIRE(stepsize < singularities.size(), "deltaGammaVarSaddlepoint: unexpected stepsize");
    }

    QL_REQUIRE(b1 != Null<Real>(), "deltaGammaVarSaddlepoint: could not bracket root to find K'(k) = x");

    if (close_enough(b1, b2)) {
        khat = b1;
    } else {
        Brent b;
        khat = b.solve(KPrimeMinusX, xTol, (b1 + b2) / 2.0, b1, b2);
    }

    Real eta = khat * std::sqrt(KPrime2(khat));
    Real tmp = 2.0 * (khat * x - K(khat));
    Real xi = (khat >= 0.0 ? 1.0 : -1.0) * std::sqrt(tmp);

    QuantLib::CumulativeNormalDistribution cnd;
    QuantLib::NormalDistribution nd;

    if (std::abs(khat) > 1E-5) {
        Real res = cnd(xi) - nd(xi) * (1.0 / eta - 1.0 / xi);
        // handle nan, TODO what is reasonable here?
        if (std::isnan(res))
            res = 1.0;
        return res;
    } else {
        // see Daniels, TODO test this case (not yet covered by unit tests?)
        Real eta2 = eta * eta;
        Real eta3 = eta2 * eta;
        Real eta4 = eta2 * eta2;
        Real eta6 = eta3 * eta3;
        Real KP2 = KPrime2(khat);
        Real a2 = KPrime2(khat) / KP2;
        Real a3 = KPrime3(khat) / std::pow(KP2, 1.5);
        Real a4 = KPrime4(khat) / (KP2 * KP2);
        Real res = 1.0 - std::exp(K(khat) - khat * x + 0.5 * xi * xi) *
                             ((1.0 - cnd(eta)) * (1.0 - a3 * eta3 / 6.0 + a4 * eta4 / 24.0 + a2 * eta6 / 72.0) +
                              nd(eta) * (a3 * (eta2 - 1.0) / 6.0 - a4 * eta * (eta2 - 1.0) / 24.0 -
                                         eta3 * (eta4 - eta2 + 3.0) / 72.0));
        // handle nan, see above, TODO what is reasonable here?
        if (std::isnan(res)) {
            res = 1.0;
        }
        return res;
    }

} // F

} // namespace

Real deltaVar(const Matrix& omega, const Array& delta, const Real p, const CovarianceSalvage& sal) {
    detail::check(p);
    detail::check(omega, delta);
    Real num = detail::absMax(delta);
    if (close_enough(num, 0.0))
        return 0.0;
    Array tmpDelta = delta / num;
    return std::sqrt(DotProduct(tmpDelta, sal.salvage(omega).first * tmpDelta)) *
           QuantLib::InverseCumulativeNormal()(p) * num;
} // deltaVar

Real deltaGammaVarNormal(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p,
                         const CovarianceSalvage& sal) {
    detail::check(p);
    Real s = QuantLib::InverseCumulativeNormal()(p);
    Real num = 0.0, mu = 0.0, variance = 0.0;
    moments(sal.salvage(omega).first, delta, gamma, num, mu, variance);
    if (close_enough(num, 0.0) || close_enough(variance, 0.0))
        return 0.0;
    return (std::sqrt(variance) * s + mu) * num;

} // deltaGammaVarNormal

Real deltaGammaVarCornishFisher(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p,
                                const CovarianceSalvage& sal) {
    detail::check(p);
    Real s = QuantLib::InverseCumulativeNormal()(p);
    Real num = 0.0, mu = 0.0, variance = 0.0, tau = 0.0, kappa = 0.0;
    moments(sal.salvage(omega).first, delta, gamma, num, mu, variance, tau, kappa);
    if (close_enough(num, 0.0) || close_enough(variance, 0.0))
        return 0.0;

    Real xTilde =
        s + tau / 6.0 * (s * s - 1.0) + kappa / 24.0 * s * (s * s - 3.0) - tau * tau / 36.0 * s * (2.0 * s * s - 5.0);
    return (xTilde * std::sqrt(variance) + mu) * num;
} // deltaGammaVarCornishFisher

Real deltaGammaVarSaddlepoint(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p,
                              const CovarianceSalvage& sal) {

    /* References:

       Lugannani, R.and S.Rice (1980),
       Saddlepoint Approximations for the Distribution of the Sum of Independent Random Variables,
       Advances in Applied Probability, 12,475-490.

       Daniels, H. E. (1987), Tail Probability Approximations, International Statistical Review, 55, 37-48.
    */

    detail::check(p);
    detail::check(omega, delta, gamma);

    auto S = sal.salvage(omega);
    Matrix L = S.second;
    if (L.rows() == 0) {
        L = CholeskyDecomposition(omega, true);
    }

    Matrix hLGL = 0.5 * transpose(L) * gamma * L;
    SymmetricSchurDecomposition schur(hLGL);
    Array lambda = schur.eigenvalues();

    // we scale the problem to ensure numerical stability; for this we divide delta and gamma by a
    // factor such that the largest eigenvalue has absolute value 1.0

    // find the largest absolute eigenvalue
    Real scaling = 0.0;
    for (const auto l : lambda) {
        Real a = std::abs(l);
        if (!close_enough(a, 0.0))
            scaling = std::max(scaling, a);
    }

    // if all eigenvalues are zero we do not scale anything
    if (close_enough(scaling, 0.0))
        scaling = 1.0;

    // scaled delta and gamma
    Array tmpDelta = delta / scaling;
    Matrix tmpGamma = gamma / scaling;

    // adjusted eigenvalues
    for (auto& l : lambda) {
        l /= scaling;
    }

    // compute quantile
    Array deltaBar = transpose(schur.eigenvectors()) * transpose(L) * tmpDelta;

    // At this point, the delta-gamma PL can be written as scaling * ( deltaBar^T z + z^T GammaBar z ), where GammaBar
    // is a diagonal matrix with the lambdas on the diagonal and z a vector of independent standard normal variates.
    // We now compare the 2-norms of deltaBar and gammaBar and fall back on a simple delta VaR calculation, if the
    // norm of gammaBar is negligible compared to the norm of deltaBar; this prevents numerical instabilities in the
    // saddlepoint search, where the function k'(x)-x can become very steep around the saddlepoint in this situation.
    Real normDeltaBar = 0.0, normGammaBar = 0.0;
    for (Size i = 0; i < deltaBar.size(); ++i) {
        normDeltaBar += deltaBar[i] * deltaBar[i];
        normGammaBar += lambda[i] * lambda[i];
    }
    if (normGammaBar / normDeltaBar < 1E-10)
        return QuantExt::deltaVar(S.first, delta, p);

    // continue with the saddlepoint approach
    auto FMinusP = [&lambda, &deltaBar, &p](const Real x) { return F(lambda, deltaBar, x) - p; };
    Real quantile;
    try {
        // TODO check hardcoded tolerance, guess and step
        Brent b;
        quantile = b.solve(FMinusP, 1E-6, 0.0, 1.0);
    } catch (const std::exception& e) {
        QL_FAIL("deltaGammaVarSaddlepoint: could no solve for quantile p = " << p << ": " << e.what());
    }

    // undo scaling and return result
    return quantile * scaling;

} // deltaGammaVarSaddlepoint

} // namespace QuantExt
