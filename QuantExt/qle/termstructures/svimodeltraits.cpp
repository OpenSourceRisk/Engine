/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/termstructures/svimodeltraits.hpp>

#include <ql/math/optimization/constraint.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/utilities/null.hpp>

#include <algorithm>
#include <cmath>

namespace QuantExt {
namespace SviModelTraits {

using namespace QuantLib;

namespace {
// Small epsilon to keep calibration guesses away from boundary singularities,
// eps2 caps correlation |rho| < 1, max_nu bounds the vol-of-vol guess range.
constexpr double eps1    = .0000001;
constexpr double eps2    = .9999;
constexpr double max_nu  = 2.0;
} // anonymous namespace

// ============================================================
//  preferredOutputQuoteType
// ============================================================

MarketQuoteType preferredOutputQuoteType(ModelVariant mv) {
    // All SVI variants produce shifted-lognormal volatility natively.
    return MarketQuoteType::ShiftedLognormalVolatility;
}

// ============================================================
//  expectedParametersSize
// ============================================================

Size expectedParametersSize(ModelVariant mv) {
    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw:
    case ModelVariant::Gatheral2004SviNatural:
    case ModelVariant::Gatheral2004SviJw:
        return 5; // a/delta/v, b/miu/phi, rho, m/omega/p, sigma/zeta/c
    case ModelVariant::Gatheral2012SsviHeston:
        return 3; // theta, rho, lambda
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return 4; // theta, rho, eta, gamma
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        return 7; // theta, eta, lambda, rho0, rhoM, thetaMax, a
    case ModelVariant::CorbettaEtAl2019Essvi:
        return 4; // kStar, thetaStar, rho, psi
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        return 3; // rho, a, c
    default:
        QL_FAIL("SviModelTraits::expectedParametersSize(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
}

// ============================================================
//  defaultParameters
// ============================================================

std::vector<std::pair<Real, ParameterCalibration>> defaultParameters(ModelVariant mv) {
    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw:
        return {{0.005, ParameterCalibration::Calibrated},
                {0.8,   ParameterCalibration::Calibrated},
                {0.0,   ParameterCalibration::Calibrated},
                {0.0,   ParameterCalibration::Calibrated},
                {0.002, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2004SviNatural:
        return {{0.1, ParameterCalibration::Calibrated},
                {0.1, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.2, ParameterCalibration::Calibrated},
                {0.5, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2004SviJw:
        return {{0.01, ParameterCalibration::Calibrated},
                {0.1,  ParameterCalibration::Calibrated},
                {0.0,  ParameterCalibration::Calibrated},
                {0.1,  ParameterCalibration::Calibrated},
                {0.01, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2012SsviHeston:
        return {{0.02, ParameterCalibration::Implied},
                {0.0,  ParameterCalibration::Calibrated},
                {1.0,  ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return {{0.02, ParameterCalibration::Implied},
                {0.0,  ParameterCalibration::Calibrated},
                {0.5,  ParameterCalibration::Calibrated},
                {0.5,  ParameterCalibration::Calibrated}};
    case ModelVariant::CorbettaEtAl2019Essvi:
        // k_star, theta_star (index 1 set by initialiseCorbettaAtmGuesses), rho, psi
        return {{0.0,  ParameterCalibration::Implied},
                {0.0,  ParameterCalibration::Implied},
                {0.0,  ParameterCalibration::Calibrated},
                {0.01, ParameterCalibration::Calibrated}};
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        // a (index 1) left as Null<Real>(): setDefaultParameters() will replace it
        // with the market-implied ATM total variance unless the caller provides it.
        return {{0.0,          ParameterCalibration::Calibrated},
                {Null<Real>(), ParameterCalibration::Calibrated},
                {0.5,          ParameterCalibration::Calibrated}};
    default:
        QL_FAIL("SviModelTraits::defaultParameters(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
}

// ============================================================
//  getGuess
// ============================================================

std::vector<Real>
getGuess(ModelVariant mv,
         const std::vector<std::pair<Real, ParameterCalibration>>& params,
         const std::vector<Real>& randomSeq,
         Real forward, Real lognormalShift) {

    std::vector<Real> result(params.size(), 0.0);

    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw: {
        // Raw SVI params: a (unconstrained), b (>=0), rho in (-1,1), m (unconstrained), sigma (>0)
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: // a
                    result[0] = (2.0 * randomSeq[j] - 1.0) * 0.1;
                    break;
                case 1: // b >= 0
                    result[1] = eps1 + randomSeq[j] * 2.0;
                    break;
                case 2: // rho in (-1, 1)
                    result[2] = (2.0 * randomSeq[j] - 1.0) * eps2;
                    break;
                case 3: // m
                    result[3] = (2.0 * randomSeq[j] - 1.0) * 0.1;
                    break;
                case 4: // sigma > 0
                    result[4] = eps1 + randomSeq[j] * 0.5;
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::Gatheral2004SviNatural: {
        // Natural SVI params: delta (unconstrained), miu (unconstrained), rho in (-1,1), omega (>=0), zeta (>0)
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: // delta
                    result[0] = (2.0 * randomSeq[j] - 1.0) * 0.5;
                    break;
                case 1: // miu
                    result[1] = (2.0 * randomSeq[j] - 1.0) * 0.5;
                    break;
                case 2: // rho in (-1, 1)
                    result[2] = (2.0 * randomSeq[j] - 1.0) * eps2;
                    break;
                case 3: // omega >= 0
                    result[3] = eps1 + randomSeq[j];
                    break;
                case 4: // zeta > 0
                    result[4] = eps1 + randomSeq[j] * 2.0;
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::Gatheral2004SviJw: {
        // JW SVI params: v (>0), phi (unconstrained), p (unconstrained), c (unconstrained), vTilda (>0)
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: // v > 0
                    result[0] = eps1 + randomSeq[j] * 0.5;
                    break;
                case 1: // phi
                    result[1] = (2.0 * randomSeq[j] - 1.0) * 0.5;
                    break;
                case 2: // p
                    result[2] = (2.0 * randomSeq[j] - 1.0) * 2.0;
                    break;
                case 3: // c
                    result[3] = eps1 + randomSeq[j] * 2.0;
                    break;
                case 4: // vTilda > 0
                    result[4] = eps1 + randomSeq[j] * 0.5;
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::Gatheral2012SsviHeston: {
        // Heston SSVI params: theta (>0), rho in (-1,1), lambda (>0)
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: // theta > 0
                    result[0] = eps1 + randomSeq[j] * 0.5;
                    break;
                case 1: // rho in (-1, 1)
                    result[1] = (2.0 * randomSeq[j] - 1.0) * eps2;
                    break;
                case 2: // lambda > 0
                    result[2] = eps1 + randomSeq[j] * max_nu;
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        // PowerLaw SSVI params: theta (>0), rho in (-1,1), eta (>0), gamma in (0,1)
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: // theta > 0
                    result[0] = eps1 + randomSeq[j] * 0.5;
                    break;
                case 1: // rho in (-1, 1)
                    result[1] = (2.0 * randomSeq[j] - 1.0) * eps2;
                    break;
                case 2: // eta > 0
                    result[2] = eps1 + randomSeq[j] * max_nu;
                    break;
                case 3: // gamma in (0, 1)
                    result[3] = eps1 + randomSeq[j] * (1.0 - 2e-6);
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::CorbettaEtAl2019Essvi: {
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0:
                    result[0] = (randomSeq[j] - 0.5) * 10.0; // kStar in [-5, 5]
                    break;
                case 1:
                    result[1] = randomSeq[j] * 3.0; // thetaStar in [0, 3]
                    break;
                case 2:
                    result[2] = (randomSeq[j] - 0.5) * 2.0; // rho in [-1, 1]
                    break;
                case 3:
                    result[3] = randomSeq[j] * 5.0; // psi in [0, 5]
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i % 3) {
                case 0:
                    result[j] = eps1 + randomSeq[j] * 2.0 - 1.0; // rho in [-1, 1]
                    break;
                case 1:
                    result[j] = eps1 + randomSeq[j] * 0.1; // 0 < a < 0.1
                    break;
                case 2:
                    result[j] = eps1 + randomSeq[j]; // c > 0
                    break;
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    default:
        QL_FAIL("SviModelTraits::getGuess(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
    return result;
}

// ============================================================
//  calibrationConstraint
// ============================================================

Constraint
calibrationConstraint(ModelVariant mv,
                      const std::vector<std::pair<Real, ParameterCalibration>>& params,
                      bool arbitrageFree) {

    Size noFreeParams = 0;
    Array fixedValues(params.size());
    std::vector<bool> isFreeParams(params.size(), false);
    for (Size i = 0; i < params.size(); ++i) {
        if (params[i].second == ParameterCalibration::Calibrated) {
            isFreeParams[i] = true;
            ++noFreeParams;
        } else {
            fixedValues[i] = params[i].first;
        }
    }

    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw: {
        QL_REQUIRE(!arbitrageFree,
                   "Arbitrage-free constraint for Gatheral2004SviRaw model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                switch (i) {
                case 1: // b
                    lowerBound[j] = 0.0;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                case 2: // rho
                    lowerBound[j] = -1.0 + 1e-6;
                    upperBound[j] =  1.0 - 1e-6;
                    break;
                case 4: // sigma
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                default:
                    lowerBound[j] = -QL_MAX_REAL;
                    upperBound[j] =  QL_MAX_REAL;
                    break;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2004SviNatural: {
        QL_REQUIRE(!arbitrageFree,
                   "Arbitrage-free constraint for Gatheral2004SviNatural model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                switch (i) {
                case 2: // rho
                    lowerBound[j] = -1.0 + 1e-6;
                    upperBound[j] =  1.0 - 1e-6;
                    break;
                case 3: // omega
                    lowerBound[j] = 0.0;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                case 4: // zeta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                default:
                    lowerBound[j] = -QL_MAX_REAL;
                    upperBound[j] =  QL_MAX_REAL;
                    break;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2004SviJw: {
        QL_REQUIRE(!arbitrageFree,
                   "Arbitrage-free constraint for Gatheral2004SviJw model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                switch (i) {
                case 0: // v
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                case 4: // vTilda
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                    break;
                default:
                    lowerBound[j] = -QL_MAX_REAL;
                    upperBound[j] =  QL_MAX_REAL;
                    break;
                }
                ++j;
            }
        }
        class JwConstraint : public Constraint {
            class Impl final : public Constraint::Impl {
            public:
                Impl(const Array& fixed, const std::vector<bool>& isFree)
                    : fixed_(fixed), isFree_(isFree) {}
                bool test(const Array& p) const override {
                    Array q(5);
                    Size j = 0;
                    for (Size i = 0; i < 5; ++i)
                        q[i] = isFree_[i] ? p[j++] : fixed_[i];
                    return -q[2] <= 2.0 * q[1] && q[1] <= 0.5 * q[3];
                }
            private:
                Array fixed_;
                std::vector<bool> isFree_;
            };
        public:
            JwConstraint(const Array& fixed, const std::vector<bool>& isFree)
                : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFree))) {}
        };
        return CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                   JwConstraint(fixedValues, isFreeParams));
    }
    case ModelVariant::Gatheral2012SsviHeston: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                if (i == params.size() - 1) { // lambda
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else if (i == params.size() - 2) { // rho
                    lowerBound[j] = -1.0 + 1e-6;
                    upperBound[j] =  1.0 - 1e-6;
                } else { // theta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                }
                ++j;
            }
        }
        if (arbitrageFree) {
            class HestonConstraint : public Constraint {
                class Impl final : public Constraint::Impl {
                public:
                    Impl(const Array& fixed, const std::vector<bool>& isFree)
                        : fixed_(fixed), isFree_(isFree) {}
                    bool test(const Array& p) const override {
                        Array q(isFree_.size());
                        Size j = 0;
                        for (Size i = 0; i < isFree_.size(); ++i)
                            q[i] = isFree_[i] ? p[j++] : fixed_[i];
                        const Real rho    = q[isFree_.size() - 2];
                        const Real lambda = q[isFree_.size() - 1];
                        return lambda >= (1.0 + std::abs(rho)) / 4.0;
                    }
                private:
                    Array fixed_;
                    std::vector<bool> isFree_;
                };
            public:
                HestonConstraint(const Array& fixed, const std::vector<bool>& isFree)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFree))) {}
            };
            return CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                       HestonConstraint(fixedValues, isFreeParams));
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                if (i == params.size() - 1) { // gamma
                    lowerBound[j] = 1e-6;
                    upperBound[j] = 1.0 - 1e-6;
                } else if (i == params.size() - 2) { // eta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else if (i == params.size() - 3) { // rho
                    lowerBound[j] = -1.0 + 1e-6;
                    upperBound[j] =  1.0 - 1e-6;
                } else { // theta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                }
                ++j;
            }
        }
        if (arbitrageFree) {
            // Gatheral & Jacquier (2014) conditions 3 & 4 for the power-law SSVI:
            // each theta[i] must stay below bounds derived from eta, gamma, rho to
            // ensure no butterfly arbitrage across the surface.
            class PowerLawConstraint : public Constraint {
                class Impl final : public Constraint::Impl {
                public:
                    Impl(const Array& fixed, const std::vector<bool>& isFree)
                        : fixed_(fixed), isFree_(isFree) {}
                    bool test(const Array& p) const override {
                        Array q(isFree_.size());
                        Size j = 0;
                        for (Size i = 0; i < isFree_.size(); ++i)
                            q[i] = isFree_[i] ? p[j++] : fixed_[i];
                        const Real rho   = q[isFree_.size() - 3];
                        const Real eta   = q[isFree_.size() - 2];
                        const Real gamma = q[isFree_.size() - 1];
                        const Real cond3Bound =
                            std::pow(4.0 / (eta * (1.0 + std::abs(rho))), 1.0 / (1.0 - gamma));
                        const Real cond4Bound =
                            std::pow(4.0 / (q[2] * q[2] * (1.0 + std::abs(q[1]))), 1.0 / (1.0 - 2.0 * q[3]));
                        for (Size i = 0; i < isFree_.size() - 3; ++i) {
                            if (q[i] > cond3Bound)
                                return false;
                            if (gamma < 0.5 && q[i] > cond4Bound)
                                return false;
                            else if (gamma > 0.5 && q[i] < cond4Bound)
                                return false;
                        }
                        if (close_enough(gamma, 0.5))
                            return eta * eta * (1.0 + std::abs(rho)) <= 4.0;
                        return true;
                    }
                private:
                    Array fixed_;
                    std::vector<bool> isFree_;
                };
            public:
                PowerLawConstraint(const Array& fixed, const std::vector<bool>& isFree)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFree))) {}
            };
            return CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                       PowerLawConstraint(fixedValues, isFreeParams));
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                if (i % 3 == 0) { // rho
                    lowerBound[j] = -1.0 + 1e-6;
                    upperBound[j] =  1.0 - 1e-6;
                } else if (i % 3 == 1) { // a
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else { // c — logit transform used, no bound needed
                    lowerBound[j] = -QL_MAX_REAL;
                    upperBound[j] =  QL_MAX_REAL;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    default:
        QL_FAIL("SviModelTraits::calibrationConstraint(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
}

// ============================================================
//  sanitiseParams
// ============================================================

void sanitiseParams(ModelVariant mv, std::vector<Matrix>& m) {
    switch (mv) {
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        for (Size k = 0; k < m.size(); ++k) {
            Matrix& mat = m[k];
            for (Size i = 0; i < mat.rows(); ++i) {
                for (Size j = 0; j < mat.columns(); ++j) {
                    if (k == 0)      // psi*rho -- no individual bound (rho constraint enforced at use time)
                        ;
                    else if (k == 1) // theta
                        mat(i, j) = std::max(mat(i, j), 1e-6);
                    else if (k == 2) // psi
                        mat(i, j) = std::max(mat(i, j), 1e-6);
                }
            }
        }
        break;
    }
    default:
        break; // other variants need no clamping
    }
}

// ============================================================
//  toRawSvi
// ============================================================

std::tuple<Real, Real, Real, Real, Real>
toRawSvi(Real timeToExpiry, const std::vector<Real>& params, ModelVariant mv) {

    Real a, b, rho, m, sigma;

    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw: {
        a     = params[0];
        b     = params[1];
        rho   = params[2];
        m     = params[3];
        sigma = params[4];
        break;
    }
    case ModelVariant::Gatheral2004SviNatural: {
        Real delta = params[0];
        Real miu   = params[1];
        rho        = params[2];
        Real omega = params[3];
        Real zeta  = params[4];
        a     = delta + 0.5 * omega * (1.0 - rho * rho);
        b     = zeta * omega / 2.0;
        m     = miu - rho / zeta;
        sigma = std::sqrt(1.0 - rho * rho) / zeta;
        break;
    }
    case ModelVariant::Gatheral2004SviJw: {
        // Convert Jump-Wings (JW) params {v, phiJw, p, c, vTilda} to raw SVI {a, b, rho, m, sigma}.
        // See Gatheral & Jacquier (2014), "Arbitrage-free SVI volatility surfaces", Section 3.3.
        // Three branches handle the m ~ 0 and denom ~ 0 edge cases for numerical stability.
        Real v       = params[0];
        Real phiJw   = params[1];
        Real p       = params[2];
        Real c       = params[3];
        Real vTilda  = params[4];
        Real w       = v * timeToExpiry;

        b   = 0.5 * std::sqrt(w) * (c + p);
        rho = 1.0 - p * std::sqrt(w) / b;
        Real beta = rho - (2.0 * phiJw * std::sqrt(w)) / b;

        // Ensure beta is within valid range (-1, 1) for numerical stability
        beta = std::max(-1.0 + 1E-6, std::min(1.0 - 1E-6, beta));
        Real alpha = std::sqrt(1.0 / (beta * beta) - 1.0) * beta >= 0 ? 1.0 : -1.0;

        Real tmp = std::sqrt(1 + alpha * alpha) * alpha >= 0 ? 1.0 : -1.0;
        tmp -= alpha * std::sqrt(1.0 - rho * rho);
        m = (v - vTilda) * timeToExpiry / (b * (-rho + tmp));

        if (!close_enough(m, 0.0)) {
            sigma = alpha * m;
            a     = vTilda * timeToExpiry - b * sigma * std::sqrt(1.0 - rho * rho);
        } else {
            const Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
            const Real denom = b * (1.0 - sqrtOneMinusRho2);
            if (!close_enough(denom, 0.0)) {
                a     = vTilda * timeToExpiry;
                sigma = (v * timeToExpiry - a) / b;
            } else {
                sigma = (v - vTilda) * timeToExpiry / denom;
                a     = vTilda * timeToExpiry - b * sigma * sqrtOneMinusRho2;
            }
        }
        break;
    }
    case ModelVariant::Gatheral2012SsviHeston:
    case ModelVariant::Gatheral2012SsviPowerLaw:
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Real theta, phi;
        std::tie(rho, theta, phi) = toNaturalSvi(timeToExpiry, params, mv);
        std::vector<Real> sviNaturalParams = {0.0, 0.0, rho, theta, phi};
        std::tie(a, b, rho, m, sigma) =
            toRawSvi(timeToExpiry, sviNaturalParams, ModelVariant::Gatheral2004SviNatural);
        break;
    }
    default:
        QL_FAIL("SviModelTraits::toRawSvi(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
    return std::make_tuple(a, b, rho, m, sigma);
}

// ============================================================
//  fromRawSvi
// ============================================================

std::vector<Real>
fromRawSvi(Real timeToExpiry, const std::vector<Real>& params, ModelVariant mv) {

    QL_REQUIRE(params.size() == 5,
               "SviModelTraits::fromRawSvi(): expected 5 parameters in raw SVI form, got "
                   << params.size() << ".");
    QL_REQUIRE(std::abs(params[2]) < 1.0,
               "SviModelTraits::fromRawSvi(): rho parameter ("
                   << params[2] << ") must be in (-1, 1).");
    QL_REQUIRE(params[4] > 0,
               "SviModelTraits::fromRawSvi(): sigma parameter ("
                   << params[4] << ") must be positive.");

    Real a     = params[0];
    Real b     = params[1];
    Real rho   = params[2];
    Real m     = params[3];
    Real sigma = params[4];

    switch (mv) {
    case ModelVariant::Gatheral2004SviRaw:
        return {a, b, rho, m, sigma};
    case ModelVariant::Gatheral2004SviNatural: {
        Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
        Real miu   = m + rho * sigma / sqrtOneMinusRho2;
        Real omega = 2.0 * b * sigma / sqrtOneMinusRho2;
        Real zeta  = sqrtOneMinusRho2 / sigma;
        Real delta = a - 0.5 * omega * sqrtOneMinusRho2 * sqrtOneMinusRho2;
        return {delta, miu, rho, omega, zeta};
    }
    case ModelVariant::Gatheral2004SviJw: {
        Real sqrtM2PlusSigma2  = std::sqrt(m * m + sigma * sigma);
        Real sqrtOneMinusRho2  = std::sqrt(1.0 - rho * rho);
        Real v = -rho * m + sqrtM2PlusSigma2;
        v = a + b * v;
        v /= timeToExpiry;
        Real oneOverSqrtWt = 1.0 / std::sqrt(v * timeToExpiry);
        Real phi = -m / sqrtM2PlusSigma2 + rho;
        phi *= 0.5 * b * oneOverSqrtWt * phi;
        Real p = oneOverSqrtWt * b * (1.0 - rho);
        Real c = oneOverSqrtWt * b * (1.0 + rho);
        Real vTilda = a + b * sigma * sqrtOneMinusRho2;
        vTilda /= timeToExpiry;
        return {v, phi, p, c, vTilda};
    }
    case ModelVariant::Gatheral2012SsviHeston: {
        auto naturalParams = fromRawSvi(timeToExpiry, params, ModelVariant::Gatheral2004SviNatural);
        Real rho = naturalParams[2];
        Real m = naturalParams[3];
        Real sigma = naturalParams[4];
        Real theta = m;
        Real x = theta * sigma;
        Real lambda;
        if (x < 1e-6) {
            lambda = 2.0 * sigma;
        } else {
            Real tol = 1e-10;
            Size maxIter = 50;
            x = std::max(2.0 * sigma * theta, 1e-6);
            for (Size iter = 0; iter < maxIter; ++iter) {
                Real expMx = std::exp(-x);
                Real f = x - 1.0 + expMx - sigma * x * x;
                Real df = 1.0 - expMx - 2.0 * sigma * x;
                Real dx = f / df;
                x -= dx;
                if (std::abs(dx) < tol * std::abs(x))
                    break;
            }
            lambda = x / theta;
        }
        return {theta, rho, lambda};
    }
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        Real theta = m;
        Real gamma = 0.5; // assumed fixed
        Real eta   = sigma * std::pow(theta, gamma);
        return {theta, rho, eta, gamma};
    }
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
    case ModelVariant::CorbettaEtAl2019Essvi:
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        QL_FAIL("SviModelTraits::fromRawSvi(): model variant ("
                << static_cast<int>(mv) << ") is not invertible from raw SVI.");
    default:
        QL_FAIL("SviModelTraits::fromRawSvi(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
}

// ============================================================
//  toNaturalSvi  (single-slice)
// ============================================================

std::tuple<Real, Real, Real>
toNaturalSvi(Real timeToExpiry, const std::vector<Real>& params, ModelVariant mv) {

    Real rho, theta, phi;

    switch (mv) {
    case ModelVariant::Gatheral2012SsviHeston: {
        theta       = params[0];
        rho         = params[1];
        Real lambda = params[2];
        phi = 1.0 - std::exp(-lambda * theta);
        phi = 1.0 - phi / (lambda * theta);
        phi = phi / (lambda * theta);
        break;
    }
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        theta      = params[0];
        rho        = params[1];
        Real eta   = params[2];
        Real gamma = params[3];
        phi = eta * std::pow(theta, -gamma);
        break;
    }
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Real kStar     = params[0];
        Real thetaStar = params[1];
        rho            = params[2];
        Real psi       = params[3];
        theta = thetaStar - rho * psi * kStar;
        phi   = psi / theta;
        break;
    }
    default:
        QL_FAIL("SviModelTraits::toNaturalSvi(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
    return std::make_tuple(rho, theta, phi);
}

// ============================================================
//  toNaturalSviGlobal  (multi-slice)
// ============================================================

std::tuple<std::vector<Real>, std::vector<Real>, std::vector<Real>>
toNaturalSviGlobal(const std::vector<Real>& params, ModelVariant mv) {

    QL_REQUIRE(mv == ModelVariant::HendriksMartini2017EssviFirstPowerLaw  ||
               mv == ModelVariant::HendriksMartini2017EssviSecondPowerLaw ||
               mv == ModelVariant::Mingone2022EssviGJ                     ||
               mv == ModelVariant::Mingone2022EssviMM,
               "SviModelTraits::toNaturalSviGlobal(): only global SSVI variants are allowed.");
    QL_REQUIRE(params.size() % expectedParametersSize(mv) == 0,
               "SviModelTraits::toNaturalSviGlobal(): wrong number of parameters.");

    Size n = params.size() / expectedParametersSize(mv);
    std::vector<Real> rho(n), theta(n), phi(n);

    switch (mv) {
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw: {
        for (Size i = 0; i < n; ++i) {
            theta[i]        = params[7 * i];
            Real eta        = params[7 * i + 1];
            Real lambda     = params[7 * i + 2];
            Real rho0       = params[7 * i + 3];
            Real rhoM       = params[7 * i + 4];
            Real thetaMax   = params[7 * i + 5];
            Real a          = params[7 * i + 6];
            rho[i] = rho0 + (rhoM - rho0) * std::pow(theta[i] / thetaMax, a);
            phi[i] = eta * std::pow(theta[i], -lambda);
        }
        break;
    }
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw: {
        for (Size i = 0; i < n; ++i) {
            theta[i]        = params[7 * i];
            Real eta        = params[7 * i + 1];
            Real lambda     = params[7 * i + 2];
            Real rho0       = params[7 * i + 3];
            Real rhoM       = params[7 * i + 4];
            Real thetaMax   = params[7 * i + 5];
            Real a          = params[7 * i + 6];
            rho[i] = rho0 + (rhoM - rho0) * std::pow(theta[i] / thetaMax, a);
            phi[i] = eta * std::pow(theta[i], -lambda) * std::pow(1.0 + theta[i], lambda - 1.0);
        }
        break;
    }
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        for (Size i = 0; i < n; ++i)
            rho[i] = params[3 * i];

        // inter-slice ratios p_i
        std::vector<Real> p(n - 1);
        for (Size i = 0; i < n - 1; ++i) {
            p[i] = std::max((1.0 + rho[i])   / (1.0 + rho[i + 1]),
                            (1.0 - rho[i])   / (1.0 - rho[i + 1]));
        }

        // cumulative theta
        theta[0] = params[1];
        for (Size i = 1; i < n; ++i)
            theta[i] = theta[i - 1] * p[i - 1] + params[3 * i + 1];

        // per-slice upper bound on phi/theta (the f_i values)
        std::vector<Real> f(n);
        for (Size i = 0; i < n; ++i) {
            const Real absRho = std::abs(rho[i]);
            switch (mv) {
            case ModelVariant::Mingone2022EssviGJ: {
                Real fGJ = 4.0 * theta[i] / (1.0 + absRho);
                f[i] = std::min(4.0 / (1.0 + absRho), std::sqrt(std::max(0.0, fGJ)));
                break;
            }
            case ModelVariant::Mingone2022EssviMM: {
                // Compute per-slice upper bound f[i] on phi*theta using the Martini-Mingone (MM)
                // no-butterfly-arbitrage condition. Finds the minimum of a rational function of l
                // over [lMin, lMax] via 80-point log-spaced grid search + Brent refinement at sign changes.
                // N, Np, Npp are the function and its first two derivatives w.r.t. l.
                const Real sqrtOneMinusRho2     = std::sqrt(std::max(0.0, 1.0 - absRho * absRho));
                const Real sqrtOneMinusRho2Safe = std::max(sqrtOneMinusRho2, 1e-12);
                const Real l2   = 1.0 / std::tan(std::acos(-absRho) / 3.0);
                const Real lMin = l2 + 1e-6;
                const Real lMax = std::max(10.0, lMin * 100.0);

                auto candidateAt = [&](const Real l) {
                    const Real N   = sqrtOneMinusRho2 + absRho * l + std::sqrt(l * l + 1.0);
                    const Real Np  = absRho + l / std::sqrt(l * l + 1.0);
                    const Real Npp = 1.0 / std::pow(l * l + 1.0, 1.5);
                    const Real g   = 0.25 * Np;
                    const Real h   = 1.0 - (l - absRho / sqrtOneMinusRho2Safe) * Np / (2.0 * N);
                    const Real g2  = Npp - (Np * Np) / (2.0 * N);
                    const Real denom = theta[i] * sqrtOneMinusRho2Safe * g * g - g2;
                    if (denom <= 0.0)
                        return QL_MAX_REAL;
                    return 4.0 * theta[i] * sqrtOneMinusRho2Safe * h * h / denom;
                };

                auto derivAt = [&](const Real l) {
                    const Real h = 1e-6 * std::max(1.0, l);
                    const Real fP = candidateAt(l + h);
                    const Real fM = candidateAt(std::max(lMin, l - h));
                    if (!std::isfinite(fP) || !std::isfinite(fM))
                        return QL_MAX_REAL;
                    return (fP - fM) / (2.0 * h);
                };

                const Size gridSize = 80;
                Real bestF  = candidateAt(lMin);
                Real prevL  = lMin;
                Real prevD  = derivAt(prevL);

                for (Size k = 1; k <= gridSize; ++k) {
                    const Real t = static_cast<Real>(k) / static_cast<Real>(gridSize);
                    const Real l = lMin * std::exp(std::log(lMax / lMin) * t);
                    const Real fVal = candidateAt(l);
                    if (fVal < bestF)
                        bestF = fVal;
                    const Real dVal = derivAt(l);
                    if (std::isfinite(prevD) && std::isfinite(dVal) && prevD * dVal < 0.0) {
                        try {
                            Brent solver;
                            solver.setMaxEvaluations(100);
                            const Real lStar = solver.solve(derivAt, 1e-8, 0.5 * (prevL + l), prevL, l);
                            const Real fStar = candidateAt(lStar);
                            if (fStar < bestF)
                                bestF = fStar;
                        } catch (...) {}
                    }
                    prevL = l;
                    prevD = dVal;
                }

                f[i] = std::min(4.0 / (1.0 + absRho), std::sqrt(std::max(0.0, bestF)));
                break;
            }
            default:
                QL_FAIL("SviModelTraits::toNaturalSviGlobal(): unsupported model variant.");
            }
        }

        // C[i] = cumulative upper bound on phi·theta, A[i] = lower bound
        std::vector<Real> C(n), A(n), psi(n);
        C[0] = f[0];
        for (Size i = 1; i < n; ++i) {
            Real denom = 1.0;
            for (Size j = 1; j <= i; ++j)
                denom *= p[j - 1];
            C[0] = std::min(C[0], f[i] / denom);
        }

        A[0]   = 0.0;
        psi[0] = params[2] * (C[0] - A[0]) + A[0];
        phi[0] = psi[0] / theta[0];

        for (Size i = 1; i < n; ++i) {
            A[i] = psi[i - 1] * p[i - 1];
            C[i] = psi[i - 1] / theta[i - 1] * theta[i];
            C[i] = std::min(C[i], f[i]);
            Real denom2 = 1.0;
            for (Size j = i + 1; j < n; ++j) {
                denom2 *= p[j - 1];
                C[i] = std::min(C[i], f[j] / denom2);
            }
            psi[i] = params[3 * i + 2] * (C[i] - A[i]) + A[i];
            phi[i] = psi[i] / theta[i];
        }
        return std::make_tuple(rho, theta, phi);
    }
    default:
        QL_FAIL("SviModelTraits::toNaturalSviGlobal(): model variant ("
                << static_cast<int>(mv) << ") not handled.");
    }
    return std::make_tuple(rho, theta, phi);
}

} // namespace SviModelTraits
} // namespace QuantExt
