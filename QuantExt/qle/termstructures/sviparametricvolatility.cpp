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

#include <qle/termstructures/sviparametricvolatility.hpp>
#include <qle/math/flatextrapolation.hpp>

#include <ql/experimental/math/laplaceinterpolation.hpp>
#include <ql/experimental/volatility/sviinterpolation.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/optimization/constraint.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/math/solvers1d/brent.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

using namespace QuantLib;

SviParametricVolatility::SviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), enforceNoArbitrage_(enforceNoArbitrage) {
    calculate();
}

SviParametricVolatility::SviParametricVolatility(
    DeferredInit,
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), enforceNoArbitrage_(enforceNoArbitrage) {}

ParametricVolatility::MarketQuoteType SviParametricVolatility::preferredOutputQuoteType() const {
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2004SviNatural:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2004SviJw:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2012SsviHeston:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::CorbettaEtAl2019Essvi:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Mingone2022EssviGJ:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Mingone2022EssviMM:
        return MarketQuoteType::ShiftedLognormalVolatility;
    default:
        QL_FAIL("SviParametricVolatility::preferredOutputQuoteType(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
    return MarketQuoteType::ShiftedLognormalVolatility;
}

std::vector<Real> SviParametricVolatility::getGuess(const std::vector<std::pair<Real, ParameterCalibration>>& params,
                                                    const std::vector<Real>& randomSeq, const Real forward,
                                                    const Real lognormalShift) const {
    std::vector<Real> result(params.size(), 0.0);

    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw:
    case ModelVariant::Gatheral2004SviNatural:
    case ModelVariant::Gatheral2004SviJw:
    case ModelVariant::Gatheral2012SsviHeston:
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        for (Size i = 0, j = 0; i < params.size(); ++i) {
            if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                result[i] = params[i].first;
            } else {
                switch (i) {
                case 0: {
                    Real fbeta = std::pow(forward + lognormalShift, params[1].first);
                    result[0] = (eps1 + randomSeq[j] * 0.01) / fbeta;
                    break;
                }
                case 1: {
                    result[1] = eps1 + randomSeq[j] * eps2;
                    break;
                }
                case 2: {
                    result[2] = eps1 + randomSeq[j] * max_nu;
                    break;
                }
                case 3: {
                    result[3] = (randomSeq[j] * 2.0 - 1.0) * eps2;
                    break;
                }
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
                case 0: {
                    result[0] = (randomSeq[j] - 0.5) * 10.0; // k_star in [-5, 5]
                    break;
                }
                case 1: {
                    result[1] = (randomSeq[j]) * 3.0; // theta_star in [0, 3]
                    break;
                }
                case 2: {
                    result[2] = (randomSeq[j] - 0.5) * 2.0; // rho in [-1, 1]
                    break;
                }
                case 3: {
                    result[3] = (randomSeq[j]) * 5.0; // psi in [0, 5]
                    break;
                }
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
                case 0: {
                    result[j] = eps1 + randomSeq[j] * 2.0 - 1.0; // rho in [-1, 1]
                    break;
                }
                case 1: {
                    result[j] = eps1 + randomSeq[j] * 0.1; // 0 < a < 0.1
                    break;
                }
                case 2: {
                    result[j] = eps1 + randomSeq[j]; // c > 0
                    break;
                }
                default:
                    break;
                }
                ++j;
            }
        }
        break;
    }
    default:
        QL_FAIL("SviParametricVolatility::getGuess(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
    return result;
}

std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>
SviParametricVolatility::defaultModelParameters() const {
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw:
        return {{0.005, ParameterCalibration::Calibrated},
                {0.8, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.002, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2004SviNatural:
        return {{0.1, ParameterCalibration::Calibrated},
                {0.1, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.2, ParameterCalibration::Calibrated},
                {0.5, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2004SviJw:
        return {{0.01, ParameterCalibration::Calibrated},
                {0.1, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.1, ParameterCalibration::Calibrated},
                {0.01, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2012SsviHeston:
        return {{0.02, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {1.0, ParameterCalibration::Calibrated}};
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return {{0.02, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated},
                {0.5, ParameterCalibration::Calibrated},
                {0.5, ParameterCalibration::Calibrated}};
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        return {{0.0, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}, // will be set to ATM total implied variances
                {0.5, ParameterCalibration::Calibrated}};
    default:
        QL_FAIL("SviParametricVolatility::defaultModelParameters(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize(ModelVariant modelVariant) {
    switch (modelVariant) {
    case ModelVariant::Gatheral2004SviRaw: // a, b, rho, m, sigma
    case ModelVariant::Gatheral2004SviNatural: // delta, miu, rho, omega, zeta
    case ModelVariant::Gatheral2004SviJw:
        return 5; // a, b, rho, m, sigma
    case ModelVariant::Gatheral2012SsviHeston:
        return 3; // theta, rho, lambda
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return 4; // theta, rho, eta, gamma
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        return 7; // theta, eta, lambda, p_0, p_m, theta_max, a
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        return 7; // theta, eta, lambda, p_0, p_m, theta_max, a
    case ModelVariant::CorbettaEtAl2019Essvi:
        return 4; // k_star, theta_star, rho, psi
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        return 3; // rho, a, c
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant) << ") not handled.");
    }
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize() const {
    return expectedModelParametersSize(modelVariant_);
}

Constraint SviParametricVolatility::getCalibrationConstraint(
    std::vector<std::pair<Real, ParameterCalibration>> params, bool arbitrageFree) const {
    Size noFreeParams = 0;
    Array fixedValues(params.size());
    std::vector<bool> isFreeParams(params.size(), false);
    for (Size i = 0; i < params.size(); ++i) {
        auto const& p = params[i];
        if (p.second == ParameterCalibration::Calibrated) {
            isFreeParams[i] = true;
            ++noFreeParams;
        } else {
            fixedValues[i] = params[i].first;
        }
    }

    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw: {
        QL_REQUIRE(!arbitrageFree, "Arbitrage-free constraint for Gatheral2004SviRaw model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                // Set bounds based on parameter index
                switch(i) {
                    case 1: // b
                        lowerBound[j] = 0.0;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    case 2: // rho
                        lowerBound[j] = -1.0 + 1e-6;
                        upperBound[j] = 1.0 - 1e-6;
                        break;
                    case 4: // sigma
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    default:
                        lowerBound[j] = -QL_MAX_REAL;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2004SviNatural: {
        QL_REQUIRE(!arbitrageFree, "Arbitrage-free constraint for Gatheral2004SviNatural model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                // Set bounds based on parameter index
                switch(i) {
                    case 2: // rho
                        lowerBound[j] = -1.0 + 1e-6;
                        upperBound[j] = 1.0 - 1e-6;
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
                        upperBound[j] = QL_MAX_REAL;
                        break;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2004SviJw: {
        QL_REQUIRE(!arbitrageFree, "Arbitrage-free constraint for Gatheral2004SviJw model is not implemented.");
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                // Set bounds based on parameter index
                switch(i) {
                    case 0: // v
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    case 4: // v_tilta
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    default:
                        lowerBound[j] = -QL_MAX_REAL;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                }
                ++j;
            }
        }
        // Constraint for explicit transformation between SVI-JW and SVI-Raw parameters
        class JwConstraint : public Constraint {
            private:
                class Impl final : public Constraint::Impl {
                public:
                    Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
                    : fixed_(fixed), isFreeParams_(isFreeParams) {}
                    bool test(const Array& p) const override {
                        Array q(5);
                        Size j = 0;
                        for (Size i = 0; i < 5; ++i) {
                            if (isFreeParams_[i]) {
                                q[i] = p[j];
                                ++j;
                            } else {
                                q[i] = fixed_[i];
                            }
                        }
                        return -q[2] <= 2.0 * q[1] && q[1] <= 0.5 * q[3];
                    }
                private:
                    Array fixed_;
                    std::vector<bool> isFreeParams_;
                };
            public:
                JwConstraint(const Array& fixed, const std::vector<bool>& isFreeParams)
                : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams))) {}
        };
        return CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                   JwConstraint(fixedValues, isFreeParams));
        // return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2012SsviHeston: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                if (i == params.size() - 1) { // lambda
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else if (i == params.size() - 2) { // rho
                    lowerBound[j] = -1 + 1e-6;
                    upperBound[j] = 1 - 1e-6;
                } else { // theta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                }
                ++j;
            }
        }
        if (arbitrageFree) {
            // Constraint for no butterfly arbitrage
            class HestonConstraint : public Constraint {
                private:
                    class Impl final : public Constraint::Impl {
                    public:
                        Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
                        : fixed_(fixed), isFreeParams_(isFreeParams) {}
                        bool test(const Array& p) const override {
                            Array q(isFreeParams_.size());
                            Size j = 0;
                            for (Size i = 0; i < isFreeParams_.size(); ++i) {
                                if (isFreeParams_[i]) {
                                    q[i] = p[j];
                                    ++j;
                                } else {
                                    q[i] = fixed_[i];
                                }
                            }
                            const Real rho = q[isFreeParams_.size() - 2];
                            const Real lambda = q[isFreeParams_.size() - 1];
                            return lambda >= (1.0 + std::abs(rho)) / 4.0;
                        }
                    private:
                        Array fixed_;
                        std::vector<bool> isFreeParams_;
                    };
                public:
                    HestonConstraint(const Array& fixed, const std::vector<bool>& isFreeParams)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams))) {}
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
                    upperBound[j] = 1 - 1e-6;
                } else if (i == params.size() - 2) { // eta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else if (i == params.size() - 3) { // rho
                    lowerBound[j] = -1 + 1e-6;
                    upperBound[j] = 1 - 1e-6;
                } else { // theta
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                }
                ++j;
            }
        }
        if (arbitrageFree) {
            // Constraint for no butterfly arbitrage
            class PowerLawConstraint : public Constraint {
                private:
                    class Impl final : public Constraint::Impl {
                    public:
                        Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
                        : fixed_(fixed), isFreeParams_(isFreeParams) {}
                        bool test(const Array& p) const override {
                            Array q(isFreeParams_.size());
                            Size j = 0;
                            for (Size i = 0; i < isFreeParams_.size(); ++i) {
                                if (isFreeParams_[i]) {
                                    q[i] = p[j];
                                    ++j;
                                } else {
                                    q[i] = fixed_[i];
                                }
                            }
                            // return true;

                            const Real rho = q[isFreeParams_.size() - 3];
                            const Real eta = q[isFreeParams_.size() - 2];
                            const Real gamma = q[isFreeParams_.size() - 1];

                            const Real cond3Bound = std::pow(4 / (eta * (1 + std::abs(rho))), 1 / (1 - gamma));
                            const Real cond4Bound = std::pow(4 / (q[2] * q[2] * (1 + std::abs(q[1]))), 1 / (1 - 2 * q[3]));

                            for (Size i = 0; i < isFreeParams_.size() - 3; ++i) {
                                if (q[i] > cond3Bound)
                                    return false;
                                if (gamma < 0.5 && q[i] > cond4Bound) {
                                    return false;
                                } else if (gamma > 0.5 && q[i] < cond4Bound) {
                                    return false;
                                }
                            }
                            if (close_enough(gamma, 0.5)) {
                                return eta * eta * (1 + std::abs(rho)) <= 4;
                            }
                            return true;
                        }
                    private:
                        Array fixed_;
                        std::vector<bool> isFreeParams_;
                    };
                public:
                    PowerLawConstraint(const Array& fixed, const std::vector<bool>& isFreeParams)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams))) {}
            };
            return CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                    PowerLawConstraint(fixedValues, isFreeParams));
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    // case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    // case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                // Set bounds based on parameter index
                if (i % 3 == 0) { // rho
                    lowerBound[j] = -1 + 1e-6;
                    upperBound[j] = 1 - 1e-6;
                } else if (i % 3 == 1) { // a
                    lowerBound[j] = 1e-6;
                    upperBound[j] = QL_MAX_REAL;
                } else if (i % 3 == 2) { // c - using logit transform, no bounds needed
                    lowerBound[j] = -QL_MAX_REAL;
                    upperBound[j] = QL_MAX_REAL;
                }
                ++j;
            }
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

void SviParametricVolatility::sanitiseSviParams(std::vector<Matrix>& m) {
    switch(modelVariant_) {
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {
            for (Size k = 0; k < m.size(); ++k) {
                Matrix& mat = m[k];
                for (Size i = 0; i < mat.rows(); ++i) {
                    for (Size j = 0; j < mat.columns(); ++j) {
                        if (k == 0) { // rho
                            mat(i, j) = std::max(std::min(mat(i, j), 1.0 - 1e-6), -1.0 + 1e-6);
                        } else if (k == 1) { // theta
                            mat(i, j) = std::max(mat(i, j), 1e-6);
                        } else if (k == 2) { // psi
                            mat(i, j) = std::max(mat(i, j), 1e-6);
                        }
                    }
                }
            }
            break;
        }
        // add sanitization for other model variants if needed
        default:
            break;
    }
}

std::vector<Real> SviParametricVolatility::evaluateSvi(const std::vector<Real>& params, const Real forward,
                                                       const Real timeToExpiry, const Real lognormalShift,
                                                       const std::vector<Real>& strikes,
                                                       const MarketQuoteType outputMarketQuoteType,
                                                       const std::vector<QuantLib::Option::Type>& outputOptionTypes,
                                                       const Real outputLognormalShift) const {
    std::vector<Real> result(strikes.size());
    Real a, b, rho, m, sigma;
    std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, params, modelVariant_);

    for (Size i = 0; i < strikes.size(); ++i) {
        try {
            Real k = std::log((std::max(strikes[i], 1E-6) + lognormalShift) / (forward + lognormalShift));
            Real totalVariance = detail::sviTotalVariance(a, b, sigma, rho, m, k);
            result[i] = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
        } catch (...) {
            result[i] = 0.0;
        }
    }

    // ensure we have a number, not inf or nan

    for (auto& v : result)
        if (!std::isfinite(v))
            v = 0.0;

    return result;
}

Real SviParametricVolatility::getAtmQuote(const MarketSmile& marketSmile,
                                          Real modelLognormalShift,
                                          QuantLib::ext::optional<MarketQuoteType> outputMarketQuoteType) const {

    // get atm vol from market smile, converted to the preferred model vol type

    std::vector<Real> x, y;
    for (Size i = 0; i < marketSmile.strikes.size(); ++i) {
        x.push_back(marketSmile.strikes[i]);
        y.push_back(convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                            : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                            outputMarketQuoteType ? *outputMarketQuoteType : preferredOutputQuoteType(),
                            modelLognormalShift, QuantLib::ext::nullopt));
    }

    Interpolation m = LinearFlat().interpolate(x.begin(), x.end(), y.begin());
    m.enableExtrapolation();
    return m(marketSmile.forward);
}

std::tuple<std::vector<Real>, Real, Real, Size> SviParametricVolatility::calibrateModelParameters(
    const MarketSmile& marketSmile, const std::vector<std::pair<Real, ParameterCalibration>>& params) const {

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : params)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    // determine the shift for the model (if applicable)

    Real modelLognormalShift;
    if (modelShifts_.empty()) {
        modelLognormalShift = marketSmile.lognormalShift;
    } else {
        auto it = modelShifts_.find(marketSmile.underlyingLength);
        QL_REQUIRE(
            it != modelShifts_.end(),
            "SviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                << marketSmile.underlyingLength << " is missing in this specification.");
        modelLognormalShift = it->second;
    }

    // get atm vol from market smile, converted to the preferred model vol type
    Real atmQuote = getAtmQuote(marketSmile, modelLognormalShift);

    // if there are no free parameters, we pass back fixed parameters as the result

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : params) {
            resultParams.push_back(p.first);
        }
        return std::make_tuple(resultParams, 0.0, modelLognormalShift, 0);
    }

    // if we have less data points than free parameters -> exit early

    QL_REQUIRE(noFreeParams <= marketSmile.strikes.size(), "internal: less data points than free parameters");

    // // define the target function

    struct TargetFunction : public QuantLib::CostFunction {
        Real forward_;
        Real timeToExpiry_;
        Real lognormalShift_;
        std::vector<Real> strikes_;
        std::vector<Real> marketQuotes_;
        std::vector<QuantLib::Option::Type> optionTypes_;
        Real atmQuote_;
        Real refQuote_;
        std::function<std::vector<Real>(const std::vector<Real>&, const Real, const Real, const Real,
                                        const std::vector<Real>&, const std::vector<QuantLib::Option::Type>&, const Real)>
            evalSvi_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;

        std::vector<Real> evalSvi(const Array& x) const {
            std::vector<Real> params(params_.size());
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    params[i] = params_[i].first;
                else
                    params[i] = x[j++];
            }
            return evalSvi_(params, forward_, timeToExpiry_, lognormalShift_, strikes_, optionTypes_, lognormalShift_);
        }

        Array values(const Array& x) const override {
            Array result(strikes_.size());
            auto svi = evalSvi(x);
            for (Size i = 0; i < strikes_.size(); ++i) {
                result[i] = abs(marketQuotes_[i] - svi[i]) / refQuote_;
            }
            return result;
        }
    };

    /* build the target function and populate the members:
       strikes, marketQuotes  : the latter are converted to the preferred output quote type of the SABR
       evalSvi               : the function to produce SVI values for a given vector of strikes */

    TargetFunction t;

    t.forward_ = marketSmile.forward;
    t.timeToExpiry_ = marketSmile.timeToExpiry;
    t.lognormalShift_ = modelLognormalShift;

    t.evalSvi_ = [this](const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                        const Real lognormalShift, const std::vector<Real>& strikes,
                        const std::vector<QuantLib::Option::Type>& outputOptionTypes, const Real outputLognormalShift) {
        return evaluateSvi(params, forward, timeToExpiry, lognormalShift, strikes,
                           preferredOutputQuoteType(), outputOptionTypes, outputLognormalShift);
    };

    t.params_ = params;

    t.atmQuote_ = atmQuote;
    t.strikes_  = marketSmile.strikes;
    t.optionTypes_ = marketSmile.optionTypes;
    for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
        t.marketQuotes_.push_back(convert(
            marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward, preferredOutputQuoteType(),
            t.lognormalShift_, QuantLib::ext::nullopt));
    }
    // we use relative errors w.r.t. the max market quote, because far otm quotes are close to zero
    t.refQuote_ = *std::max_element(t.marketQuotes_.begin(), t.marketQuotes_.end());

    // perform the calibration (this step might throw if all minimizations go wrong)

    // Define box constraints for each free parameter
    Constraint constraint = getCalibrationConstraint(params, enforceNoArbitrage_);

    LevenbergMarquardt lm;
    EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);

    std::vector<Real> bestResult(params.size());
    Real bestError = QL_MAX_REAL;

    HaltonRsg haltonRsg(noFreeParams, 42);

    Array guess(noFreeParams);

    Size attempt;
    for (attempt = 0; attempt < maxCalibrationAttempts_; ++attempt) {

        if (attempt == 0) {
            // first attempt uses given initial model parameters
            for (Size i = 0, j = 0; i < t.params_.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = t.params_[i].first;
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = getGuess(params, haltonRsg.nextSequence().value, t.forward_, t.lognormalShift_);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = g[i];
                }
            }
        }

        Problem problem(t, constraint, guess);
        try {
            lm.minimize(problem, endCriteria);
        } catch (const std::exception& e) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    bestResult[i] = t.params_[i].first;
                else
                    bestResult[i] = problem.currentValue()[j++];
            }
        }

        if (bestError < exitEarlyErrorThreshold_)
            break;
    }

    // store the calibration results
    CalibrationResult result;
    result.timeToExpiry = t.timeToExpiry_;
    result.underlyingLength = marketSmile.underlyingLength;
    result.forward = t.forward_;
    result.strikes = t.strikes_;
    result.marketInput = marketSmile.marketQuotes;
    result.calibrationTarget = t.marketQuotes_;
    result.calibrationResult = evaluateSvi(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_,
                                           t.strikes_, preferredOutputQuoteType(), t.optionTypes_, t.lognormalShift_);
    result.error = bestError;
    result.accepted = bestError < maxAcceptableError_;
    calibrationResults_.push_back(result);

    // check if if have at least one valid calibration
    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");

    // return the best calibration result
    return std::make_tuple(bestResult, bestError, t.lognormalShift_, ++attempt);
}

namespace {
void laplaceInterpolationWithErrorHandling(Matrix& m, const std::vector<Real>& x, const std::vector<Real>& y) {
    std::vector<std::pair<double, Size>> tolerances = {{1E-6, 100}, {1E-5, 100}, {1E-4, 100}, {1E-4, 1000}};
    std::vector<std::string> errorText;
    bool success = false;
    for (auto const& [acc, iter] : tolerances) {
        try {
            laplaceInterpolation(m, x, y, acc, iter);
            success = true;
            break;
        } catch (const std::exception& e) {
            errorText.push_back(e.what());
        }
    }
    QL_REQUIRE(success,
               "Error during laplaceInterpolation() in SviParametricVolatility ("
                   << boost::join(errorText, ",")
                   << "), this might be related to the numerical parameters relTol, maxIterMult. Contact dev.");
}
} // namespace

void SviParametricVolatility::setDefaultParameters() {

    // if no model parameters are given, we provide the default ones

    if (modelParameters_.empty()) {
        for (auto const& s : marketSmiles_) {
            modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)] = defaultModelParameters();
        }
    }

}

void SviParametricVolatility::calibrate() {

    // for each market smile calibrate the SVI variant

    for (auto const& s : marketSmiles_) {
        auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
        auto param = modelParameters_.find(key);
        QL_REQUIRE(param != modelParameters_.end(),
                   "SviParametricVolatility::performCalculations(): no model parameter given for ("
                       << s.timeToExpiry << ", " << s.underlyingLength
                       << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                          "covered by the given model parameters.");
        try {
            auto [params, error, shift, noOfAttempts] = calibrateModelParameters(s, param->second);
            if (error < maxAcceptableError_)
                calibratedSviParams_[key] = params;
            calibrationErrors_[key] = error;
            lognormalShifts_[key] = shift;
            noOfAttempts_[key] = noOfAttempts;
        } catch (const std::exception& e) {
            // all calibration failed -> do not populate params, but interpolate them below
        }
    }
}

void SviParametricVolatility::calculate() {

    // validate market smiles
    for (auto const& s : marketSmiles_) {
        QL_REQUIRE(s.timeToExpiry > 0.0,
                   "SviParametricVolatility::calculate(): non-positive timeToExpiry ("
                       << s.timeToExpiry << ") given for ("
                       << "underlyingLength=" << s.underlyingLength << ").");
        QL_REQUIRE(s.underlyingLength == Null<Real>() || s.underlyingLength > 0.0,
                   "SviParametricVolatility::calculate(): non-positive underlyingLength ("
                       << s.underlyingLength << ") given for ("
                       << "timeToExpiry=" << s.timeToExpiry << ").");
        QL_REQUIRE(s.forward > 0.0,
                   "SviParametricVolatility::calculate(): non-positive forward ("
                       << s.forward << ") given for ("
                       << "timeToExpiry=" << s.timeToExpiry << ", underlyingLength=" << s.underlyingLength << ").");
        QL_REQUIRE(s.lognormalShift != Null<Real>(),
                   "SviParametricVolatility::calculate(): null lognormalShift given for ("
                       << "timeToExpiry=" << s.timeToExpiry << ", underlyingLength=" << s.underlyingLength << ").");
        QL_REQUIRE(s.optionTypes.empty() || s.optionTypes.size() == s.strikes.size(),
                   "SviParametricVolatility::calculate(): number of option types ("
                       << s.optionTypes.size() << ") does not match number of strikes (" << s.strikes.size()
                       << ") for ("
                       << "timeToExpiry=" << s.timeToExpiry << ", underlyingLength=" << s.underlyingLength << ").");
        QL_REQUIRE(s.strikes.size() == s.marketQuotes.size(),
                   "SviParametricVolatility::calculate(): number of strikes ("
                       << s.strikes.size() << ") does not match number of market quotes (" << s.marketQuotes.size()
                       << ") for ("
                       << "timeToExpiry=" << s.timeToExpiry << ", underlyingLength=" << s.underlyingLength << ").");
        QL_REQUIRE(s.strikes.size() >= 2,
                   "SviParametricVolatility::calculate(): less than two strikes ("
                       << s.strikes.size() << ") given for ("
                       << "timeToExpiry=" << s.timeToExpiry << ", underlyingLength=" << s.underlyingLength
                       << "), cannot calibrate SVI model.");
        for (auto const& k : s.strikes) {
            QL_REQUIRE(k >= 0.0,
                       "SviParametricVolatility::calculate(): negative strike (" << k << ") given for ("
                           << "timeToExpiry=" << s.timeToExpiry
                           << ", underlyingLength=" << s.underlyingLength
                           << ").");
        }
        for (auto const& q : s.marketQuotes) {
            QL_REQUIRE(q != Null<Real>(),
                       "SviParametricVolatility::calculate(): null market quote given for ("
                           << "timeToExpiry=" << s.timeToExpiry
                           << ", underlyingLength=" << s.underlyingLength
                           << ").");
        }
    }

    setDefaultParameters();

    // validate model parameters

    Size paramSize = expectedModelParametersSize();
    for (auto const& [k, v] : modelParameters_) {
        QL_REQUIRE(v.size() == expectedModelParametersSize(),
                   "SviParametricVolatility::performCalculations(): wrong number of model parameters ("
                       << v.size() << ") given for ("
                       << "timeToExpiry=" << k.first << ", underlyingLength=" << k.second << "), expected "
                       << expectedModelParametersSize() << " for model variant "
                       << static_cast<int>(modelVariant_) << ".");
    }

    // clear stored data

    calibratedSviParams_.clear();
    lognormalShifts_.clear();
    calibrationErrors_.clear();
    sviParametersMatrices_.clear();
    sviParametersInterpolations_.clear();

    calibrate();

    // build the timeToExpiry, underlyingLength vectors

    std::set<Real> tmpTimeToExpiries, tmpUnderlyingLengths;

    for (auto const& s : marketSmiles_) {
        tmpTimeToExpiries.insert(s.timeToExpiry);
        tmpUnderlyingLengths.insert(s.underlyingLength);
    }

    timeToExpiries_ = std::vector<Real>(tmpTimeToExpiries.begin(), tmpTimeToExpiries.end());
    underlyingLengths_ = std::vector<Real>(tmpUnderlyingLengths.begin(), tmpUnderlyingLengths.end());

    // build a matrix of calibrated SABR parameters, possibly with null values

    Size m = underlyingLengths_.size();
    Size n = timeToExpiries_.size();

    sviParametersMatrices_.resize(paramSize);
    for (Size i = 0; i < paramSize; ++i) {
        sviParametersMatrices_[i] = Matrix(m, n, Null<Real>());
    }

    lognormalShift_ = Matrix(m, n, Null<Real>());
    calibrationError_ = Matrix(m, n, Null<Real>());
    isInterpolated_ = Matrix(m, n, 1.0);
    numberOfCalibrationAttempts_ = Matrix(m, n, 0.0);

    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j < n; ++j) {
            auto key = std::make_pair(timeToExpiries_[j], underlyingLengths_[i]);
            if (auto p = calibratedSviParams_.find(key); p != calibratedSviParams_.end()) {
                for (Size k = 0; k < paramSize; ++k) {
                    sviParametersMatrices_[k](i, j) = p->second[k];
                }
                isInterpolated_(i, j) = 0.0;
            }
            if (auto s = lognormalShifts_.find(key); s != lognormalShifts_.end()) {
                lognormalShift_(i, j) = s->second;
            }
            if (auto e = calibrationErrors_.find(key); e != calibrationErrors_.end()) {
                calibrationError_(i, j) = e->second;
            }
            if (auto e = noOfAttempts_.find(key); e != noOfAttempts_.end()) {
                numberOfCalibrationAttempts_(i, j) = static_cast<Real>(e->second);
            }
        }
    }

    // interpolate the null values

    for (Size k = 0; k < paramSize; ++k) {
        laplaceInterpolationWithErrorHandling(sviParametersMatrices_[k],
                                              timeToExpiries_, underlyingLengths_);
    }

    // sanitise values produced by the the interpolation that are not allowed

    sanitiseSviParams(sviParametersMatrices_);

    // workaround because BilinearInterpolation below requires at least two points in each dimension

    timeToExpiriesForInterpolation_ = timeToExpiries_;
    underlyingLengthsForInterpolation_ = underlyingLengths_;

    if (m == 1 || n == 1) {

        auto mNew = m == 1 ? m + 1 : m;
        auto nNew = n == 1 ? n + 1 : n;

        auto sviParametersMatricesTmp = sviParametersMatrices_;
        auto lognormalShiftTmp = lognormalShift_;

        for (Size i = 0; i < paramSize; ++i) {
            sviParametersMatrices_[i] = Matrix(mNew, nNew, Null<Real>());
        }
        lognormalShift_ = Matrix(mNew, nNew, Null<Real>());

        for (Size i = 0; i < mNew; ++i) {
            for (Size j = 0; j < nNew; ++j) {
                Size iOld = std::min(i, m - 1);
                Size jOld = std::min(j, n - 1);
                for (Size k = 0; k < paramSize; ++k) {
                    sviParametersMatrices_[k](i, j) = sviParametersMatricesTmp[k](iOld, jOld);
                }
                lognormalShift_(i, j) = lognormalShiftTmp(iOld, jOld);
            }
        }

        if (m == 1) {
            // do not use null value for interpolation grid
            if (underlyingLengthsForInterpolation_[0] == Null<Real>())
                underlyingLengthsForInterpolation_[0] = 1.0;
            underlyingLengthsForInterpolation_.push_back(underlyingLengthsForInterpolation_.back() + 1.0);
        }
        if (n == 1)
            timeToExpiriesForInterpolation_.push_back(timeToExpiriesForInterpolation_.back() + 1.0);

        m = mNew;
        n = nNew;
    }

    // set up the parameter interpolations

    sviParametersInterpolations_.resize(paramSize);
    for (Size i = 0; i < paramSize; ++i) {
        sviParametersInterpolations_[i] = FlatExtrapolator2D(
            QuantLib::ext::make_shared<BilinearInterpolation>(
                timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
                underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(),
                sviParametersMatrices_[i]
        ));
        sviParametersInterpolations_[i].enableExtrapolation();
    }
    lognormalShiftInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), lognormalShift_));
    lognormalShiftInterpolation_.enableExtrapolation();

    computeVolRmseMetrics();
}

void SviParametricVolatility::computeVolRmseMetrics() {
    Size nRows = underlyingLengths_.size();
    Size nCols = timeToExpiries_.size();
    volRmseShiftedLognormal_ = Matrix(nRows, nCols, Null<Real>());
    volRmsePrice_            = Matrix(nRows, nCols, Null<Real>());
    volRmseTotalVariance_    = Matrix(nRows, nCols, Null<Real>());
    globalVolRmseShiftedLognormal_ = Null<Real>();
    globalVolRmsePrice_            = Null<Real>();
    globalVolRmseTotalVariance_    = Null<Real>();

    Real sumSqVol = 0.0, sumSqPrice = 0.0, sumSqTV = 0.0;
    Size nTotal = 0;

    for (auto const& smile : marketSmiles_) {
        if (smile.strikes.empty())
            continue;

        auto iIt = std::find(underlyingLengths_.begin(), underlyingLengths_.end(), smile.underlyingLength);
        auto jIt = std::find(timeToExpiries_.begin(), timeToExpiries_.end(), smile.timeToExpiry);
        if (iIt == underlyingLengths_.end() || jIt == timeToExpiries_.end())
            continue;
        Size row = std::distance(underlyingLengths_.begin(), iIt);
        Size col = std::distance(timeToExpiries_.begin(), jIt);

        auto shiftIt = lognormalShifts_.find(std::make_pair(smile.timeToExpiry, smile.underlyingLength));
        Real modelShift = shiftIt != lognormalShifts_.end() ? shiftIt->second : smile.lognormalShift;
        Real T = smile.timeToExpiry;

        // Convert market quotes to both ShiftedLognormal vol and Price in one pass.
        // Total variance = vol^2 * T is derived from the vol without an extra evaluate() call.
        std::vector<Real> mktVol, mktPrice;
        mktVol.reserve(smile.strikes.size());
        mktPrice.reserve(smile.strikes.size());
        try {
            for (Size k = 0; k < smile.strikes.size(); ++k) {
                auto optType = smile.optionTypes.empty() ? ext::nullopt
                                                         : ext::optional<Option::Type>(smile.optionTypes[k]);
                mktVol.push_back(convert(smile.marketQuotes[k], inputMarketQuoteType_, smile.lognormalShift,
                                         optType, T, smile.strikes[k], smile.forward,
                                         MarketQuoteType::ShiftedLognormalVolatility, modelShift, ext::nullopt));
                // Use nullopt for option type so we get OTM prices (put for K<F, call for K>=F),
                // matching the convention used internally by evaluate(..., Price, ...).
                mktPrice.push_back(convert(smile.marketQuotes[k], inputMarketQuoteType_, smile.lognormalShift,
                                           ext::nullopt, T, smile.strikes[k], smile.forward,
                                           MarketQuoteType::Price, modelShift, ext::nullopt));
            }
        } catch (...) {
            continue;
        }

        Real refVol   = *std::max_element(mktVol.begin(),   mktVol.end());
        Real refPrice = *std::max_element(mktPrice.begin(), mktPrice.end());
        // max total variance = max(vol^2 * T) = refVol^2 * T  (max vol gives max TV for fixed T)
        Real refTV    = refVol * refVol * T;

        if (refVol <= 0.0 || refPrice <= 0.0)
            continue;

        Real sliceSqVol = 0.0, sliceSqPrice = 0.0, sliceSqTV = 0.0;
        Size nSlice = 0;
        for (Size k = 0; k < smile.strikes.size(); ++k) {
            auto optType = smile.optionTypes.empty() ? ext::nullopt
                                                     : ext::optional<Option::Type>(smile.optionTypes[k]);
            try {
                Real modVol = evaluate(smile.timeToExpiry, smile.underlyingLength, smile.strikes[k], smile.forward,
                                       MarketQuoteType::ShiftedLognormalVolatility, modelShift, optType);
                Real modPrice = evaluate(smile.timeToExpiry, smile.underlyingLength, smile.strikes[k], smile.forward,
                                         MarketQuoteType::Price, modelShift, optType);

                Real rVol   = (modVol   - mktVol[k])   / refVol;
                Real rPrice = (modPrice - mktPrice[k])  / refPrice;
                Real rTV    = (modVol * modVol * T - mktVol[k] * mktVol[k] * T) / refTV;

                sliceSqVol   += rVol   * rVol;
                sliceSqPrice += rPrice * rPrice;
                sliceSqTV    += rTV    * rTV;
                ++nSlice;
            } catch (...) {
            }
        }
        if (nSlice == 0)
            continue;

        Real n = static_cast<Real>(nSlice);
        volRmseShiftedLognormal_(row, col) = std::sqrt(sliceSqVol   / n);
        volRmsePrice_(row, col)            = std::sqrt(sliceSqPrice / n);
        volRmseTotalVariance_(row, col)    = std::sqrt(sliceSqTV    / n);

        sumSqVol   += sliceSqVol;
        sumSqPrice += sliceSqPrice;
        sumSqTV    += sliceSqTV;
        nTotal     += nSlice;
    }

    if (nTotal > 0) {
        Real n = static_cast<Real>(nTotal);
        globalVolRmseShiftedLognormal_ = std::sqrt(sumSqVol   / n);
        globalVolRmsePrice_            = std::sqrt(sumSqPrice / n);
        globalVolRmseTotalVariance_    = std::sqrt(sumSqTV    / n);
    }
}

std::tuple<Real, Real, Real, Real, Real>
SviParametricVolatility::convertToRawSvi(const Real timeToExpiry, const Real underlyingLength) const {
    std::vector<Real> params;
    params.resize(expectedModelParametersSize());
    for (Size i = 0; i < params.size(); ++i) {
        params[i] = sviParametersInterpolations_[i](timeToExpiry, underlyingLength);
    }
    return convertToRawSvi(timeToExpiry, params, modelVariant_);
}

std::tuple<Real, Real, Real, Real, Real>
SviParametricVolatility::convertToRawSvi(const Real timeToExpiry, const std::vector<Real>& params, ModelVariant modelVariant) {

    Real a, b, rho, m, sigma;
    switch (modelVariant) {
        case ModelVariant::Gatheral2004SviRaw: {
            a = params[0];
            b = params[1];
            rho = params[2];
            m = params[3];
            sigma = params[4];
            break;
        }
        case ModelVariant::Gatheral2004SviNatural: {
            Real delta = params[0];
            Real miu = params[1];
            rho = params[2];
            Real omega = params[3];
            Real zeta = params[4];
            a = delta + 0.5 * omega * (1.0 - rho * rho);
            b = zeta * omega / 2.0;
            m = miu - rho / zeta;
            sigma = std::sqrt(1.0 - rho * rho) / zeta;
            break;
        }
        case ModelVariant::Gatheral2004SviJw: {
            Real v = params[0];
            Real phi = params[1];
            Real p = params[2];
            Real c = params[3];
            Real v_tilda = params[4];
            Real w = v * timeToExpiry;

            b = 0.5 * std::sqrt(w) * (c + p);
            rho = 1.0 - p * std::sqrt(w) / b;
            Real beta = rho - (2.0 * phi * std::sqrt(w)) / b;

            // Ensure beta is within valid range (-1, 1) for numerical stability
            beta = std::max(-1.0 + 1E-6, std::min(1.0 - 1E-6, beta));
            Real alpha = std::sqrt(1.0 / (beta * beta) - 1.0) * beta >= 0 ? 1.0 : -1.0;

            Real tmp = std::sqrt(1 + alpha * alpha) * alpha >= 0 ? 1.0 : -1.0;
            tmp -= alpha * std::sqrt(1.0 - rho * rho);
            m = (v - v_tilda) * timeToExpiry / (b * (-rho + tmp));

            if (!close_enough(m, 0.0)) {
                sigma = alpha * m;
                a = v_tilda * timeToExpiry - b * sigma * std::sqrt(1.0 - rho * rho);
            } else {
                // Solve the circular dependency between a and sigma
                // sigma = (v * timeToExpiry - a) / b;
                // a = v_tilda * timeToExpiry - b * sigma * std::sqrt(1.0 - rho * rho);
                // => a = v_tilda * timeToExpiry - (v * timeToExpiry - a) * std::sqrt(1.0 - rho * rho);
                // => a = (v_tilda * timeToExpiry - v * timeToExpiry * std::sqrt(1.0 - rho * rho)) / (1.0 - std::sqrt(1.0 - rho * rho));
                const Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
                const Real denom = b * (1.0 - sqrtOneMinusRho2);
                if (!close_enough(denom, 0.0)) {
                    a = v_tilda * timeToExpiry;
                    sigma = (v * timeToExpiry - a) / b;
                } else {
                    sigma = (v - v_tilda) * timeToExpiry / denom;
                    a = v_tilda * timeToExpiry - b * sigma * sqrtOneMinusRho2;
                }
            }
            break;
        }
        case ModelVariant::Gatheral2012SsviHeston:
        case ModelVariant::Gatheral2012SsviPowerLaw:
        case ModelVariant::CorbettaEtAl2019Essvi: {
            Real theta, phi;
            std::tie(rho, theta, phi) = convertToNaturalSvi(timeToExpiry, params, modelVariant);
            std::vector<Real> sviNaturalParams = {0.0, 0.0, rho, theta, phi};
            std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, sviNaturalParams, ModelVariant::Gatheral2004SviNatural);
            break;
        }
        default: {
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not handled.");
        }
    }
    return std::make_tuple(a, b, rho, m, sigma);

}

std::vector<Real>
SviParametricVolatility::convertFromRawSvi(const Real timeToExpiry, const std::vector<Real>& params, ModelVariant modelVariant) {
    // params are in raw SVI form: a, b, rho, m, sigma
    //                             0, 1,   2, 3,     4
    Real a = params[0];
    Real b = params[1];
    Real rho = params[2];
    Real m = params[3];
    Real sigma = params[4];
    QL_REQUIRE(params.size() == 5,
               "SviParametricVolatility::convertToRawSvi(): expected 5 parameters in raw SVI form, got "
                   << params.size() << ".");
    QL_REQUIRE(std::abs(params[2]) < 1.0,
               "SviParametricVolatility::convertToRawSvi(): rho parameter ("
                   << params[2] << ") must be in (-1, 1).");
    QL_REQUIRE(params[4] > 0,
               "SviParametricVolatility::convertToRawSvi(): sigma parameter ("
                   << params[4] << ") must be positive.");
    switch (modelVariant) {
        case ModelVariant::Gatheral2004SviRaw: {
            return {params[0], params[1], params[2], params[3], params[4]};
            break;
        }
        case ModelVariant::Gatheral2004SviNatural: {
            Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
            Real miu = m + rho * sigma / sqrtOneMinusRho2;
            Real omega = 2.0 * b * sigma / sqrtOneMinusRho2;
            Real zeta = sqrtOneMinusRho2 / sigma;
            Real delta = a - 0.5 * omega * sqrtOneMinusRho2 * sqrtOneMinusRho2;
            return {delta, miu, rho, omega, zeta};
        }
        case ModelVariant::Gatheral2004SviJw: {
            Real sqrtM2PlusSigma2 = std::sqrt(m * m + sigma * sigma);
            Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
            Real v = -rho * m + sqrtM2PlusSigma2;
            v = a + b * v;
            v /= timeToExpiry;
            Real oneOverSqrtWt = 1.0 / std::sqrt(v * timeToExpiry);
            Real phi = -m / sqrtM2PlusSigma2 + rho;
            phi *= 0.5 * b * oneOverSqrtWt * phi;
            Real p = oneOverSqrtWt * b * (1.0 - rho);
            Real c = oneOverSqrtWt * b * (1.0 + rho);
            Real v_tilda = a + b * sigma * sqrtOneMinusRho2;
            v_tilda /= timeToExpiry;
            return {v, phi, p, c, v_tilda};
        }
        case ModelVariant::Gatheral2012SsviHeston: {
            auto naturalParams = convertFromRawSvi(timeToExpiry, params, ModelVariant::Gatheral2004SviNatural);
            Real rho = naturalParams[2];
            Real m = naturalParams[3];
            Real sigma = naturalParams[4];
            Real theta = m;
            // Reverse the relation: sigma = (1 - (1 - exp(-lambda*theta))/(lambda*theta))/(lambda*theta)
            // This is a transcendental equation, so we solve numerically
            // Starting approximation: assume small lambda*theta, then sigma ~ lambda*theta/2
            Real x = theta * sigma;  // x = lambda * theta (initial guess: x ~ 2*sigma*theta)
            Real lambda;
            if (x < 1e-6) {
                lambda = 2.0 * sigma;  // Linear approximation for small values
            } else {
                // Newton-Raphson iteration to solve: sigma*(lambda*theta)^2 = lambda*theta - 1 + exp(-lambda*theta)
                Real tol = 1e-10;
                Size maxIter = 50;
                x = std::max(2.0 * sigma * theta, 1e-6);  // Better initial guess
                for (Size iter = 0; iter < maxIter; ++iter) {
                    Real expMx = std::exp(-x);
                    Real f = x - 1.0 + expMx - sigma * x * x;  // f(x) = 0
                    Real df = 1.0 - expMx - 2.0 * sigma * x;   // f'(x)
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
            Real gamma = 0.5; // assume gamma = 0.5 for simplicity
            // Reverse the relation: sigma = eta * theta^(-gamma)
            // Therefore: eta = sigma * theta^gamma
            Real eta = sigma * std::pow(theta, gamma);
            return {theta, rho, eta, gamma};
        }
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        case ModelVariant::CorbettaEtAl2019Essvi:
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM:
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not implemented.");
            break;
        default: {
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not handled.");
        }
    }
}


std::tuple<Real, Real, Real>
SviParametricVolatility::convertToNaturalSvi(const Real timeToExpiry, const std::vector<Real>& params, ModelVariant modelVariant) {
    Real rho, theta, phi;
    switch (modelVariant) {
    case ModelVariant::Gatheral2012SsviHeston: {
        theta = params[0];
        rho = params[1];
        Real lambda = params[2];
        phi = 1.0 - std::exp(-lambda * theta);
        phi = 1.0 - phi / (lambda * theta);
        phi = phi / (lambda * theta);
        break;
    }
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        theta = params[0];
        rho = params[1];
        Real eta = params[2];
        Real gamma = params[3];
        phi = eta * std::pow(theta, -gamma);
        break;
    }
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Real k_star = params[0];
        Real theta_star = params[1];
        rho = params[2];
        Real psi = params[3];
        theta = theta_star - rho * psi * k_star;
        phi = psi / theta;
        break;
    }
    default:
        QL_FAIL("SviParametricVolatility::convertToNaturalSvi(): model variant ("
                << static_cast<int>(modelVariant) << ") not handled.");
    }
    return std::make_tuple(rho, theta, phi);
}

Real SviParametricVolatility::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                       const Real forward, const MarketQuoteType outputMarketQuoteType,
                                       const Real outputLognormalShift,
                                       const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType) const {
    Real totalVariance;
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);
    Real k = std::log((std::max(strike, 1E-6) + lognormalShift) / (forward + lognormalShift));
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw:
    case ModelVariant::Gatheral2004SviNatural:
    case ModelVariant::Gatheral2004SviJw:
    case ModelVariant::Gatheral2012SsviHeston:
    case ModelVariant::Gatheral2012SsviPowerLaw:
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Real a, b, rho, m, sigma;
        std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, underlyingLength);
        totalVariance = detail::sviTotalVariance(a, b, sigma,
                                                 rho, m, k);
        break;
    }
    default:
        QL_FAIL("SviParametricVolatility::evaluate(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }

    Real result = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
    return convert(result, preferredOutputQuoteType(), lognormalShift, QuantLib::ext::nullopt, timeToExpiry, strike, forward,
                   outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
                   outputOptionType);

}

SsviParametricVolatility::SsviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, const bool enforceNoArbitrage)
    : SviParametricVolatility(DeferredInit{}, modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                              modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                              maxAcceptableError, enforceNoArbitrage) {
    QL_REQUIRE(modelVariant == ModelVariant::Gatheral2012SsviHeston ||
               modelVariant == ModelVariant::Gatheral2012SsviPowerLaw,
               "SsviParametricVolatility::SsviParametricVolatility(): only SSVI model variants are allowed.");
    calculate();
}

SsviParametricVolatility::SsviParametricVolatility(
    DeferredInit,
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, const bool enforceNoArbitrage)
    : SviParametricVolatility(DeferredInit{}, modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                              modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                              maxAcceptableError, enforceNoArbitrage) {
}

std::vector<Real> SsviParametricVolatility::evaluateSvi(const std::vector<Real>& params, const Real forward,
                                                        const Real timeToExpiry, const Real lognormalShift,
                                                        const std::vector<Real>& strikes,
                                                        const MarketQuoteType outputMarketQuoteType,
                                                        const std::vector<QuantLib::Option::Type>& outputOptionTypes,
                                                        const Real outputLognormalShift) const {
    std::vector<Real> result(strikes.size());
    Real a, b, rho, m, sigma;
    std::tie(a, b, rho, m, sigma) = SviParametricVolatility::convertToRawSvi(timeToExpiry, params, modelVariant_);

    for (Size i = 0; i < strikes.size(); ++i) {
        try {
            Real k = std::log((std::max(strikes[i], 1E-6) + lognormalShift) / (forward + lognormalShift));
            Real totalVariance = detail::sviTotalVariance(a, b, sigma, rho, m, k);
            Real sigma = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
            result[i] = convert(sigma, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift,
                                outputOptionTypes.empty() ? QuantLib::ext::nullopt
                                                    : QuantLib::ext::optional<Option::Type>(outputOptionTypes[i]),
                                timeToExpiry, strikes[i], forward,
                                outputMarketQuoteType, outputLognormalShift, QuantLib::ext::nullopt);
        } catch (...) {
            result[i] = 0.0;
        }
    }

    // ensure we have a number, not inf or nan

    for (auto& v : result)
        if (!std::isfinite(v))
            v = 0.0;

    return result;
}

Real SsviParametricVolatility::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                        const Real forward, const MarketQuoteType outputMarketQuoteType,
                                        const Real outputLognormalShift,
                                        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType) const {
    Real totalVariance;
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);
    Real k = std::log((std::max(strike, 1E-6) + lognormalShift) / (forward + lognormalShift));
    switch (modelVariant_) {
    case ModelVariant::Gatheral2012SsviHeston:
    case ModelVariant::Gatheral2012SsviPowerLaw: {
        Real a, b, rho, m, sigma;
        std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, underlyingLength);
        totalVariance = detail::sviTotalVariance(a, b, sigma, rho, m, k);
        break;
    }
    default:
        QL_FAIL("SsviParametricVolatility::evaluate(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }

    Real result = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
    return convert(result, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift, QuantLib::ext::nullopt, timeToExpiry, strike, forward,
                   outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
                   outputOptionType);

}

std::tuple<std::vector<Real>, Real, std::vector<Real>, QuantLib::Size>
SsviParametricVolatility::calibrateModelParametersGlobal(
    const std::vector<MarketSmile>& marketSmiles, const std::vector<std::pair<Real, ParameterCalibration>>& params) const {
        
    // theta is time dependent
    // rho and other parameters are constant across maturities

    std::vector<std::pair<Real, ParameterCalibration>> ssviParams;
    Size paramSize = expectedModelParametersSize();
    Size nMaturities = params.size() / paramSize;
    Size nConstantParameters = paramSize - 1; // all except theta
    ssviParams.resize(nMaturities + nConstantParameters);
    for (Size i = 0; i < nMaturities; ++i) {
        ssviParams[i] = params[i * paramSize];
    }
    for (Size i = 0; i < nConstantParameters; ++i) {
        ssviParams[nMaturities + i] = params[i + 1];
    }

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : ssviParams)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : ssviParams) {
            resultParams.push_back(p.first);
        }
        std::vector<Real> lognormalShifts;
        for (const auto& marketSmile : marketSmiles) {
            if (modelShifts_.empty()) {
                lognormalShifts.push_back(marketSmile.lognormalShift);
            } else {
                auto it = modelShifts_.find(marketSmile.underlyingLength);
                QL_REQUIRE(it != modelShifts_.end(),
                           "SsviParametricVolatility::calibrateModelParametersGlobal(): model shifts are specified but underlying length "
                               << marketSmile.underlyingLength << " is missing in this specification.");
                lognormalShifts.push_back(it->second);
            }
        }
        return std::make_tuple(resultParams, 0.0, lognormalShifts, QuantLib::Size(0));
    }


    struct TargetFunction : public QuantLib::CostFunction {
        std::vector<Real> forward_;
        std::vector<Real> timeToExpiry_;
        std::vector<Real> lognormalShift_;
        std::vector<std::vector<Real>> strikes_;
        std::vector<std::vector<QuantLib::Option::Type>> optionTypes_;
        std::vector<Real> marketQuotes_;
        std::vector<Real> weight_;
        ModelVariant modelVariant_;
        std::vector<std::function<Real(Real)>> paramTransform_; // whether to apply logit transform to the parameter
        std::vector<std::function<Real(Real)>> paramInverseTransform_; // whether to apply logit transform to the parameter
        std::function<std::vector<Real>(const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<std::vector<Real>>&,
                                        const std::vector<std::vector<QuantLib::Option::Type>>&,
                                        const std::vector<Real>&)>
            evalSvi_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;

        std::vector<Real> evalSvi(const Array& x) const {
            std::vector<Real> params(params_.size());
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                    params[i] = params_[i].first;
                } else {
                    // Apply inverse logit transformation for parameters that require it
                    if (paramInverseTransform_[i]) {
                        params[i] = paramInverseTransform_[i](x[j]);
                    } else {
                        params[i] = x[j];
                    }
                    ++j;
                }
            }
            return evalSvi_(params, forward_, timeToExpiry_, lognormalShift_, strikes_,
                            optionTypes_, lognormalShift_);
        }

        Array values(const Array& x) const override {
            auto svi = evalSvi(x);
            Array result(svi.size());
            std::vector<Real> resultVec(svi.size());
            for (Size i = 0; i < svi.size(); ++i) {
                result[i] = (svi[i] - marketQuotes_[i]) * weight_[i];
                resultVec[i] = result[i];
            }

            return result;
        }

        Real value(const Array& x) const override {
            Array v = values(x);
            std::transform(v.begin(), v.end(), v.begin(), [](Real x) -> Real { return x*x; });
            return std::accumulate(v.begin(), v.end(), Real(0.0)) / Real(2.0);
        }
    };

    TargetFunction t;

    for (auto marketSmile : marketSmiles) {
        // determine the shift for the model (if applicable)

        Real modelLognormalShift;
        if (modelShifts_.empty()) {
            modelLognormalShift = marketSmile.lognormalShift;
        } else {
            auto it = modelShifts_.find(marketSmile.underlyingLength);
            QL_REQUIRE(
                it != modelShifts_.end(),
                "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                    << marketSmile.underlyingLength << " is missing in this specification.");
            modelLognormalShift = it->second;
        }

        t.forward_.push_back(marketSmile.forward);
        t.timeToExpiry_.push_back(marketSmile.timeToExpiry);
        t.lognormalShift_.push_back(modelLognormalShift);
        t.strikes_.push_back(marketSmile.strikes);
        t.optionTypes_.push_back(marketSmile.optionTypes);
        for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
            Real convertedQuote = convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                                          marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                                          : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                                          marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                                          preferredOutputQuoteType(), modelLognormalShift, QuantLib::ext::nullopt);
            t.marketQuotes_.push_back(convertedQuote);
        }
        switch (modelVariant_) {
        case ModelVariant::Gatheral2012SsviHeston:
        case ModelVariant::Gatheral2012SsviPowerLaw: {
            for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
                t.weight_.push_back(1.0);
            }
            break;
        }
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {
            for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
                Real k = std::log((std::max(marketSmile.strikes[i], 1E-6) + modelLognormalShift) /
                                  (marketSmile.forward + modelLognormalShift));
                Real vol = convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                                   marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                                   : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                                   marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                                   MarketQuoteType::ShiftedLognormalVolatility, modelLognormalShift, QuantLib::ext::nullopt);
                Real volSqrtT = vol * std::sqrt(marketSmile.timeToExpiry);
                Real d1 = volSqrtT * 0.5 - k / volSqrtT;
                Real vega = discountCurve_->discount(marketSmile.timeToExpiry) * marketSmile.forward *
                            (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * d1 * d1) * std::sqrt(marketSmile.timeToExpiry);
                t.weight_.push_back(1.0 / (vega + 1E-12)); // avoid division by zero
            }
            t.paramTransform_.push_back(nullptr); // rho parameter
            t.paramTransform_.push_back(nullptr); // a parameter
            t.paramTransform_.push_back([](Real x) { return std::log(x / (1.0 - x)); });  // c parameter
            t.paramInverseTransform_.push_back(nullptr); // rho parameter
            t.paramInverseTransform_.push_back(nullptr); // a parameter
            t.paramInverseTransform_.push_back([](Real x) { return 1.0 / (1.0 + std::exp(-x)); });  // c parameter
            break;
        }
        default:
            QL_FAIL("SsviParametricVolatilityGlobal::calibrateModelParametersGlobal(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
        }
    }

    for (Size i = 0; i < t.forward_.size(); ++i) {
        t.paramTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
    }
    if (modelVariant_ == ModelVariant::Gatheral2012SsviHeston) {
        t.paramTransform_.push_back(nullptr);
        t.paramTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
    } else if (modelVariant_ == ModelVariant::Gatheral2012SsviPowerLaw) {
        t.paramTransform_.push_back(nullptr);
        t.paramTransform_.push_back(nullptr);
        t.paramTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
        t.paramInverseTransform_.push_back(nullptr);
    }

    t.evalSvi_ = [this](const std::vector<Real>& params,
                        const std::vector<Real>& forward,
                        const std::vector<Real>& timeToExpiry,
                        const std::vector<Real>& lognormalShift,
                        const std::vector<std::vector<Real>>& strikes,
                        const std::vector<std::vector<QuantLib::Option::Type>>& outputOptionTypes,
                        const std::vector<Real>& outputLognormalShift) {

        Size n = forward.size();

        std::vector<Real> svi;
        for (Size i = 0; i < n; ++i) {
            std::vector<Real> localParams = {params[i]};
            for (Size j = n; j < params.size(); ++j) {
                localParams.push_back(params[j]);
            }
            auto [rho, theta, phi] = SviParametricVolatility::convertToNaturalSvi(timeToExpiry[i], localParams, modelVariant_);
            for (Size j = 0; j < strikes[i].size(); ++j) {
                Real k = std::log((std::max(strikes[i][j], 1E-6) + lognormalShift[i]) / (forward[i] + lognormalShift[i]));
                Real totalVariance = phi * k + rho;
                totalVariance *= totalVariance;
                totalVariance += 1 - rho * rho;
                totalVariance = 1 + rho * phi * k + std::sqrt(totalVariance);
                totalVariance *= theta / 2.0;
                Real sigma = std::sqrt(std::max(0.0, totalVariance / timeToExpiry[i]));
                // convert to output quote type
                svi.push_back(convert(sigma, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift[i],
                                      outputOptionTypes[i].empty() ? QuantLib::ext::nullopt
                                                                   : QuantLib::ext::optional<Option::Type>(outputOptionTypes[i][j]),
                                      timeToExpiry[i], strikes[i][j], forward[i],
                                      preferredOutputQuoteType(), outputLognormalShift[i], QuantLib::ext::nullopt));
            }
        }
        return svi;
    };

    t.params_ = ssviParams;
    t.modelVariant_ = modelVariant_;

    // Define box constraints for each free parameter
    Constraint constraint = getCalibrationConstraint(ssviParams, enforceNoArbitrage_);

    LevenbergMarquardt lm;
    EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);
    std::vector<Real> bestResult(ssviParams.size());
    EndCriteria::Type bestResultEc = EndCriteria::None;
    Real bestError = QL_MAX_REAL;

    HaltonRsg haltonRsg(noFreeParams, 42);

    Array guess(noFreeParams);

    Size attempt;
    // for (attempt = 0; attempt < maxCalibrationAttempts_; ++attempt) {
    for (attempt = 0; attempt < 1; ++attempt) {

        if (attempt == 0) {
            // first attempt uses given initial model parameters
            for (Size i = 0, j = 0; i < t.params_.size(); ++i) {
                if (t.params_[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    // Apply logit transformation for c parameters to unbounded space
                    if (t.paramTransform_[i]) {
                        guess[j++] = t.paramTransform_[i](t.params_[i].first);
                    } else {
                        guess[j++] = t.params_[i].first;
                    }
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = getGuess(params, haltonRsg.nextSequence().value, t.forward_[0], t.lognormalShift_[0]);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    // Apply logit transformation for c parameters to unbounded space
                    if (t.paramTransform_[i]) {
                        guess[j++] = t.paramTransform_[i](g[i]);
                    } else {
                        guess[j++] = g[i];
                    }
                }
            }
        }

        Problem problem(t, constraint, guess);
        EndCriteria::Type ec;
        try {
            ec = lm.minimize(problem, endCriteria);
        } catch (const std::exception& e) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (ssviParams[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                    bestResult[i] = t.params_[i].first;
                } else {
                    if (t.paramInverseTransform_[i]) {
                        // Apply inverse logit transformation to get back to original space
                        bestResult[i] = t.paramInverseTransform_[i](problem.currentValue()[j]);
                    } else {
                        bestResult[i] = problem.currentValue()[j];
                    }
                    ++j;
                }
            }
            bestResultEc = ec;
        }

        if (bestError < exitEarlyErrorThreshold_)
            break;
    }

    // store the calibration results
    Size i = 0;
    auto calibrationResult = t.evalSvi_(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_,
                                        t.strikes_, t.optionTypes_, t.lognormalShift_);
    for (const auto& marketSmile : marketSmiles) {
        CalibrationResult result;
        result.timeToExpiry = marketSmile.timeToExpiry;
        result.underlyingLength = marketSmile.underlyingLength;
        result.forward = marketSmile.forward;
        result.strikes.insert(result.strikes.end(), marketSmile.strikes.begin(), marketSmile.strikes.end());
        result.marketInput.insert(result.marketInput.end(), marketSmile.marketQuotes.begin(), marketSmile.marketQuotes.end());
        result.calibrationTarget = std::vector<Real>(t.marketQuotes_.begin() + i, t.marketQuotes_.begin() + i + marketSmile.strikes.size());
        result.calibrationResult = std::vector<Real>(calibrationResult.begin() + i, calibrationResult.begin() + i + marketSmile.strikes.size());

        result.error = bestError;
        result.accepted = bestError < maxAcceptableError_;
        calibrationResults_.push_back(result);
        i+=marketSmile.strikes.size();
    }

    // check if if have at least one valid calibration
    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");
    QL_REQUIRE(bestResultEc != EndCriteria::None, "internal: all calibrations failed");

    // return the best calibration result
    return std::make_tuple(bestResult, bestError, t.lognormalShift_, ++attempt);

}

std::tuple<std::vector<Real>, Real, Real, QuantLib::Size>
SsviParametricVolatility::calibrateModelParameters(
    const MarketSmile& marketSmile, const std::vector<std::pair<Real, ParameterCalibration>>& params) const {


    // determine the shift for the model (if applicable)

    Real modelLognormalShift;
    if (modelShifts_.empty()) {
        modelLognormalShift = marketSmile.lognormalShift;
    } else {
        auto it = modelShifts_.find(marketSmile.underlyingLength);
        QL_REQUIRE(
            it != modelShifts_.end(),
            "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                << marketSmile.underlyingLength << " is missing in this specification.");
        modelLognormalShift = it->second;
    }

    // get atm vol from market smile, converted to the preferred model vol type

    std::vector<Real> x, y;
    for (Size i = 0; i < marketSmile.strikes.size(); ++i) {
        x.push_back(marketSmile.strikes[i]);
        y.push_back(convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                            : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                            MarketQuoteType::ShiftedLognormalVolatility, modelLognormalShift, QuantLib::ext::nullopt));
    }

    Interpolation m = LinearFlat().interpolate(x.begin(), x.end(), y.begin());
    m.enableExtrapolation();
    Real atmVol = m(marketSmile.forward);


    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : params)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    // if there are no free parameters, we pass back fixed parameters as the result

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : params) {
            resultParams.push_back(p.first);
        }
        if (params[0].second == ParametricVolatility::ParameterCalibration::Implied) {
            resultParams[0] = atmVol * atmVol * marketSmile.timeToExpiry;
        }
        return std::make_tuple(resultParams, 0.0, modelLognormalShift, 0);
    }


    // if we have less data points than free parameters -> exit early

    QL_REQUIRE(noFreeParams <= marketSmile.strikes.size(), "internal: less data points than free parameters");

    // define the target function

    struct TargetFunction : public QuantLib::CostFunction {
        Real forward_;
        Real timeToExpiry_;
        Real lognormalShift_;
        std::vector<Real> strikes_;
        std::vector<Real> marketQuotes_;
        std::vector<QuantLib::Option::Type> optionTypes_;
        ModelVariant modelVariant_;
        Real atmVol_;
        Real refQuote_;
        std::function<std::vector<Real>(const std::vector<Real>&, const Real, const Real, const Real,
                                        const std::vector<Real>&, const std::vector<QuantLib::Option::Type>&, const Real)>
            evalSvi_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;
        std::vector<Real> paramsPreviousSlice_;

        std::vector<Real> evalSvi(const Array& x) const {
            std::vector<Real> params(params_.size());
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    params[i] = params_[i].first;
                else
                    params[i] = x[j++];
            }
            return evalSvi_(params, forward_, timeToExpiry_, lognormalShift_, strikes_, optionTypes_, lognormalShift_);
        }

        Array values(const Array& x) const override {
            Array result(strikes_.size());
            auto svi = evalSvi(x);
            for (Size i = 0; i < strikes_.size(); ++i) {
                result[i] = abs(marketQuotes_[i] - svi[i]) / refQuote_;
            }
            return result;
        }
    };

    /* build the target function and populate the members:
       strikes, marketQuotes  : the latter are converted to the preferred output quote type of the SABR
       evalSvi               : the function to produce SVI values for a given vector of strikes */

    TargetFunction t;

    t.forward_ = marketSmile.forward;
    t.timeToExpiry_ = marketSmile.timeToExpiry;
    t.lognormalShift_ = modelLognormalShift;

    t.evalSvi_ = [this](const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                        const Real lognormalShift, const std::vector<Real>& strikes,
                        const std::vector<QuantLib::Option::Type>& outputOptionTypes, const Real outputLognormalShift) {
        return evaluateSvi(params, forward, timeToExpiry, lognormalShift, strikes,
                           preferredOutputQuoteType(), outputOptionTypes, outputLognormalShift);
    };

    t.params_ = params;

    t.atmVol_ = atmVol;
    t.strikes_  = marketSmile.strikes;
    t.optionTypes_ = marketSmile.optionTypes;
    for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
        t.marketQuotes_.push_back(convert(
            marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward, preferredOutputQuoteType(),
            t.lognormalShift_, QuantLib::ext::nullopt));
    }
    // we use relative errors w.r.t. the max market quote, because far otm quotes are close to zero
    t.refQuote_ = *std::max_element(t.marketQuotes_.begin(), t.marketQuotes_.end());

    t.modelVariant_ = modelVariant_;
    t.paramsPreviousSlice_ = {};

    // perform the calibration (this step might throw if all minimizations go wrong)

    // Define box constraints for each free parameter
    Constraint constraint = getCalibrationConstraint(params, enforceNoArbitrage_);

    LevenbergMarquardt lm;
    EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);

    std::vector<Real> bestResult(params.size());
    Real bestError = QL_MAX_REAL;

    HaltonRsg haltonRsg(noFreeParams, 42);

    Array guess(noFreeParams);

    Size attempt;
    for (attempt = 0; attempt < maxCalibrationAttempts_; ++attempt) {

        if (attempt == 0) {
            // first attempt uses given initial model parameters
            for (Size i = 0, j = 0; i < t.params_.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = t.params_[i].first;
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = getGuess(params, haltonRsg.nextSequence().value, t.forward_, t.lognormalShift_);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = g[i];
                }
            }
        }

        Problem problem(t, constraint, guess);
        try {
            lm.minimize(problem, endCriteria);
        } catch (const std::exception& e) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    bestResult[i] = t.params_[i].first;
                else
                    bestResult[i] = problem.currentValue()[j++];
            }
        }

        if (bestError < exitEarlyErrorThreshold_)
            break;
    }

    // store the calibration results
    CalibrationResult result;
    result.timeToExpiry = t.timeToExpiry_;
    result.underlyingLength = marketSmile.underlyingLength;
    result.forward = t.forward_;
    result.strikes = t.strikes_;
    result.marketInput = marketSmile.marketQuotes;
    result.calibrationTarget = t.marketQuotes_;
    result.calibrationResult = evaluateSvi(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_, t.strikes_,
                                          preferredOutputQuoteType(), t.optionTypes_, t.lognormalShift_);
    result.error = bestError;
    result.accepted = bestError < maxAcceptableError_;
    calibrationResults_.push_back(result);

    // check if if have at least one valid calibration
    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");

    // return the best calibration result
    return std::make_tuple(bestResult, bestError, t.lognormalShift_, ++attempt);
}

std::tuple<Real, Real, Real>
SsviParametricVolatility::convertToNaturalSvi(const Real timeToExpiry, const Real underlyingLength) const {
    std::vector<Real> params;
    params.resize(expectedModelParametersSize());
    for (Size i = 0; i < params.size(); ++i) {
        params[i] = sviParametersInterpolations_[i](timeToExpiry, underlyingLength);
    }
    return SviParametricVolatility::convertToNaturalSvi(timeToExpiry, params, modelVariant_);
}

void SsviParametricVolatility::calibrate() {

    auto modelParams = modelParameters_;

    std::vector<Real> modelLognormalShifts(marketSmiles_.size());
    for (Size i = 0; i < marketSmiles_.size(); ++i) {
        auto marketSmile = marketSmiles_[i];
        // determine the shift for the model (if applicable)

        Real modelLognormalShift;
        if (modelShifts_.empty()) {
            modelLognormalShift = marketSmile.lognormalShift;
        } else {
            auto it = modelShifts_.find(marketSmile.underlyingLength);
            QL_REQUIRE(
                it != modelShifts_.end(),
                "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                    << marketSmile.underlyingLength << " is missing in this specification.");
            modelLognormalShift = it->second;
        }
        modelLognormalShifts[i] = modelLognormalShift;
    }

    switch (modelVariant_) {
        case ModelVariant::Gatheral2012SsviHeston:
        case ModelVariant::Gatheral2012SsviPowerLaw: {
            for (Size i = 0; i < marketSmiles_.size(); ++i) {
                auto marketSmile = marketSmiles_[i];
                auto& params = modelParams[std::make_pair(marketSmiles_[i].timeToExpiry, marketSmiles_[i].underlyingLength)];
                if (params[0].second == ParameterCalibration::Implied) {
                    Real atmQuote = getAtmQuote(marketSmile, modelLognormalShifts[i], MarketQuoteType::ShiftedLognormalVolatility);
                    auto theta = atmQuote * atmQuote * marketSmile.timeToExpiry;
                    params[0].first = theta;
                    params[0].second = ParameterCalibration::Fixed;
                }
            }

            // flatten model parameters
            std::vector<std::pair<Real, ParameterCalibration>> flatParams;
            for (auto const& s : marketSmiles_) {
                auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
                auto param = modelParams.find(key);
                QL_REQUIRE(param != modelParams.end(),
                            "SviParametricVolatility::performCalculations(): no model parameter given for ("
                                << s.timeToExpiry << ", " << s.underlyingLength
                                << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                                    "covered by the given model parameters.");
                flatParams.insert(flatParams.end(), param->second.begin(), param->second.end());
            }

            try {
                auto [params, error, shift, noOfAttempts] = calibrateModelParametersGlobal(marketSmiles_, flatParams);
                Size i = 0;
                for (auto const& s : marketSmiles_) {
                    std::vector<Real> localParams = {params[i]};
                    for (Size j = marketSmiles_.size(); j < params.size(); ++j) {
                        localParams.push_back(params[j]);
                    }
                    auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
                    if (error < maxAcceptableError_) {
                        calibratedModelParams_[key] = localParams;
                        calibratedSviParams_[key] = localParams;
                    }
                    calibrationErrors_[key] = error;
                    lognormalShifts_[key] = shift[i];
                    noOfAttempts_[key] = noOfAttempts;
                    ++i;
                }
            } catch (const std::exception& e) {
                // all calibration failed -> do not populate params
            }

            break;
        }
        default:
             QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
    }
    
}

SsviParametricVolatilityRobust::SsviParametricVolatilityRobust(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : SsviParametricVolatility(DeferredInit{}, modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                               modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                               maxAcceptableError, enforceNoArbitrage) {
    QL_REQUIRE(modelVariant == ModelVariant::CorbettaEtAl2019Essvi,
               "SsviParametricVolatilityRobust::SsviParametricVolatilityRobust(): only robust SSVI model variants are allowed.");
    calculate();
}

Constraint SsviParametricVolatilityRobust::getCalibrationConstraint(
    std::vector<std::pair<Real, ParameterCalibration>> params, bool arbitrageFree) const {
    Size noFreeParams = 0;
    Array fixedValues(params.size());
    std::vector<bool> isFreeParams(params.size(), false);
    for (Size i = 0; i < params.size(); ++i) {
        auto const& p = params[i];
        if (p.second == ParameterCalibration::Calibrated) {
            isFreeParams[i] = true;
            ++noFreeParams;
        } else {
            fixedValues[i] = params[i].first;
        }
    }

    switch (modelVariant_) {
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Array lowerBound(noFreeParams), upperBound(noFreeParams);
        for (Size j = 0, i = 0; i < params.size(); ++i) {
            if (isFreeParams[i]) {
                // Set bounds based on parameter index
                switch(i) {
                    case 0: // k_star
                        lowerBound[j] = -QL_MAX_REAL;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    case 1: // theta_star
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    case 2: // rho
                        lowerBound[j] = -1.0 + 1e-6;
                        upperBound[j] = 1.0 - 1e-6;
                        break;
                    case 3: // psi
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    default:
                        lowerBound[j] = -QL_MAX_REAL;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                }
                ++j;
            }
        }
        if (arbitrageFree) {
            // Constraint for no butterfly arbitrage:
            //   0 < psi < min(psi+, 4/(1+|rho|))
            //   where psi+ = -2*rho*k*/(1+|rho|) + sqrt(4*rho^2*k*^2/(1+|rho|)^2 + 4*theta*/(1+|rho|))
            class ButterflyConstraint : public Constraint {
                private:
                    class Impl final : public Constraint::Impl {
                    public:
                        Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
                        : fixed_(fixed), isFreeParams_(isFreeParams) {}
                        bool test(const Array& p) const override {
                            Array q(4);
                            Size j = 0;
                            for (Size i = 0; i < 4; ++i) {
                                if (isFreeParams_[i]) {
                                    q[i] = p[j];
                                    ++j;
                                } else {
                                    q[i] = fixed_[i];
                                }
                            }
                            // q[0] = k_star, q[1] = theta_star, q[2] = rho, q[3] = psi
                            Real rho = q[2];
                            Real onePlusAbsRho = 1.0 + std::abs(rho);
                            Real psiPlus = 4.0 * rho * rho * q[0] * q[0] / (onePlusAbsRho * onePlusAbsRho);
                            psiPlus += 4.0 * q[1] / onePlusAbsRho;
                            psiPlus = -2.0 * rho * q[0] / onePlusAbsRho + std::sqrt(std::max(0.0, psiPlus));
                            Real upperBound = std::min(psiPlus, 4.0 / onePlusAbsRho);
                            return q[3] > 0.0 && q[3] < upperBound;
                        }
                    private:
                        Array fixed_;
                        std::vector<bool> isFreeParams_;
                    };
                public:
                    ButterflyConstraint(const Array& fixed, const std::vector<bool>& isFreeParams)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams))) {}
            };

            // Constraint for no calendar spread arbitrage (when prevSliceNaturalParams_ is set):
            //   theta > theta_prev  i.e. theta* - rho*psi*k* > theta_prev
            //   psi >= psi_prev
            //   psi >= psi-(rho) := max((psi_prev - rho_prev*psi_prev)/(1-rho),
            //                           (psi_prev + rho_prev*psi_prev)/(1+rho))
            class CalendarSpreadConstraint : public Constraint {
                private:
                    class Impl final : public Constraint::Impl {
                    public:
                        Impl(const Array& fixed, const std::vector<bool>& isFreeParams,
                             std::tuple<Real, Real, Real> prevSlice)
                        : fixed_(fixed), isFreeParams_(isFreeParams), prevSlice_(prevSlice) {}
                        bool test(const Array& p) const override {
                            Array q(4);
                            Size j = 0;
                            for (Size i = 0; i < 4; ++i) {
                                if (isFreeParams_[i]) {
                                    q[i] = p[j];
                                    ++j;
                                } else {
                                    q[i] = fixed_[i];
                                }
                            }
                            // q[0] = k_star, q[1] = theta_star, q[2] = rho, q[3] = psi
                            Real rho = q[2];
                            auto [thetaPrev, psiPrev, rhoPsiPrev] = prevSlice_;
                            // theta = theta* - rho*psi*k* must exceed theta_prev
                            Real theta = q[1] - rho * q[3] * q[0];
                            if (theta <= thetaPrev)
                                return false;
                            if (q[3] <= psiPrev)
                                return false;
                            Real bound1 = (psiPrev - rhoPsiPrev) / (1.0 - rho);
                            Real bound2 = (psiPrev + rhoPsiPrev) / (1.0 + rho);
                            return q[3] >= std::max(bound1, bound2);
                        }
                    private:
                        Array fixed_;
                        std::vector<bool> isFreeParams_;
                        std::tuple<Real, Real, Real> prevSlice_;
                    };
                public:
                    CalendarSpreadConstraint(const Array& fixed, const std::vector<bool>& isFreeParams,
                                             std::tuple<Real, Real, Real> prevSlice)
                    : Constraint(ext::shared_ptr<Constraint::Impl>(
                          new Impl(fixed, isFreeParams, prevSlice))) {}
            };

            Constraint butterfly = CompositeConstraint(
                NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                ButterflyConstraint(fixedValues, isFreeParams));
            auto prevKey = prevSliceKey();
            if (prevKey) {
                auto it = calibratedModelParams_.find(*prevKey);
                if (it != calibratedModelParams_.end()) {
                    Real rho_prev = it->second[2];
                    Real psi_prev = it->second[3];
                    Real k_star_prev = it->second[0];
                    Real theta_star_prev = it->second[1];
                    Real theta_prev = theta_star_prev - rho_prev * psi_prev * k_star_prev;
                    auto prevNatural = std::make_tuple(theta_prev, psi_prev, rho_prev * psi_prev);
                    return CompositeConstraint(
                        butterfly,
                        CalendarSpreadConstraint(fixedValues, isFreeParams, prevNatural));
                }
            }
            return butterfly;
        }
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

void SsviParametricVolatilityRobust::calibrate() {

    auto modelParams = modelParameters_;

    std::vector<Real> modelLognormalShifts(marketSmiles_.size());
    for (Size i = 0; i < marketSmiles_.size(); ++i) {
        auto marketSmile = marketSmiles_[i];
        // determine the shift for the model (if applicable)

        Real modelLognormalShift;
        if (modelShifts_.empty()) {
            modelLognormalShift = marketSmile.lognormalShift;
        } else {
            auto it = modelShifts_.find(marketSmile.underlyingLength);
            QL_REQUIRE(
                it != modelShifts_.end(),
                "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                    << marketSmile.underlyingLength << " is missing in this specification.");
            modelLognormalShift = it->second;
        }
        modelLognormalShifts[i] = modelLognormalShift;
    }

    switch (modelVariant_) {
        case ModelVariant::CorbettaEtAl2019Essvi: {
            // Clear previous slice key before forward calibration
            prevSliceKey_ = QuantLib::ext::nullopt;

            for (Size i = 0; i < marketSmiles_.size(); ++i) {
                auto& params = modelParams[std::make_pair(marketSmiles_[i].timeToExpiry, marketSmiles_[i].underlyingLength)];
                Real k_star = Null<Real>();
                Real theta_star = Null<Real>();
                Size bestIdx = Null<Size>();
                bool implyStrike = params[0].second == ParameterCalibration::Implied || params[0].second == ParameterCalibration::Calibrated;
                bool implyTheta = params[1].second == ParameterCalibration::Implied || params[1].second == ParameterCalibration::Calibrated;
                QL_REQUIRE(!(implyStrike && !implyTheta) && !(!implyStrike && implyTheta),
                        "SsviParametricVolatilityRobust::calibrate(): for CorbettaEtAl2019Essvi model, strike and theta must be both implied or both fixed.");
                auto marketSmile = marketSmiles_[i];
                if (implyStrike && implyTheta) {

                    // Find the strike closest to the forward (ATM) in log-moneyness space
                    Real minDist = QL_MAX_REAL;
                    for (Size j = 0; j < marketSmile.strikes.size(); ++j) {
                        Real k = std::log((std::max(marketSmile.strikes[j], 1E-6) + modelLognormalShifts[i]) /
                                            (marketSmile.forward + modelLognormalShifts[i]));
                        if (std::abs(k) < minDist) {
                            minDist = std::abs(k);
                            k_star = k;
                            bestIdx = j;
                        }
                    }
                    params[0].first = k_star;
                    params[0].second = ParameterCalibration::Fixed;

                    // Total variance at k_star: theta_star = sigma_imp(k_star)^2 * T
                    Real vol_star = convert(marketSmile.marketQuotes[bestIdx], inputMarketQuoteType_, marketSmile.lognormalShift,
                                            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                                            : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[bestIdx]),
                                            marketSmile.timeToExpiry, marketSmile.strikes[bestIdx], marketSmile.forward,
                                            MarketQuoteType::ShiftedLognormalVolatility, modelLognormalShifts[i], QuantLib::ext::nullopt);
                    theta_star = vol_star * vol_star * marketSmile.timeToExpiry;
                    params[1].first = theta_star;
                    params[1].second = ParameterCalibration::Fixed;
                }
            }

            // for each market smile calibrate the SSVI variant

            for (auto const& s : marketSmiles_) {

                auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
                auto sliceParams = modelParams.find(key);
                QL_REQUIRE(sliceParams != modelParams.end(),
                           "SsviParametricVolatilityRobust::performCalculations(): no model parameter given for ("
                            << s.timeToExpiry << ", " << s.underlyingLength
                            << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                                "covered by the given model parameters.");
                try {
                    if (sliceParams->second[2].second == ParameterCalibration::Calibrated) {
                        // calibrate rho
                        Real n = 10;
                        std::vector<Real> rho_candidates(n);
                        Real start = -0.99;
                        Real end = 0.99;
                        Real step = (end - start) / (n + 1);
                        // find the rho that gives the best fit, reduce the range iteratively until step is less than 0.0001
                        // start with -0.99 to 0.99, and narraw down the range around the best candidate until we have a step of 0.0001
                        Real globalBestError = QL_MAX_REAL;
                        while (step > 0.0001) {
                            for (Size j = 0; j < n; ++j) {
                                rho_candidates[j] = start + (j+1) * step;
                            }
                            Real bestError = QL_MAX_REAL;
                            Real bestRho = rho_candidates[0];
                            for (auto rho : rho_candidates) {
                                sliceParams->second[2].first = rho;
                                sliceParams->second[2].second = ParameterCalibration::Fixed;
                                if (sliceParams->second[3].second == ParameterCalibration::Calibrated) {
                                    if (prevSliceKey_) {
                                        auto prevSliceParams = calibratedModelParams_.find(*prevSliceKey_);
                                        QL_REQUIRE(prevSliceParams != calibratedModelParams_.end(),
                                                "SsviParametricVolatilityRobust::calibrate(): previous slice parameters not found for key ("
                                                    << prevSliceKey_->first << ", " << prevSliceKey_->second << "). This should not happen as we only set prevSliceKey_ if we have a valid calibration for that slice.");
                                        Real bound1 = (prevSliceParams->second[3] - prevSliceParams->second[3] * prevSliceParams->second[2]) / (1.0 - rho);
                                        Real bound2 = (prevSliceParams->second[3] + prevSliceParams->second[3] * prevSliceParams->second[2]) / (1.0 + rho);
                                        sliceParams->second[3].first = std::max(std::max(bound1, bound2), prevSliceParams->second[3]) + 1e-6;
                                    }
                                }
                                auto [params, error, shift, noOfAttempts] = calibrateModelParameters(s, sliceParams->second);
                                auto [dummy, theta, phi] = SviParametricVolatility::convertToNaturalSvi(s.timeToExpiry, params, modelVariant_);
                                if (error < bestError) {
                                    bestError = error;
                                    bestRho = rho;
                                }
                                if (error < globalBestError) {
                                    globalBestError = error;
                                    if (error < maxAcceptableError_) {
                                        calibratedModelParams_[key] = params;
                                        calibratedSviParams_[key] = {rho, theta, phi};
                                    }
                                    calibrationErrors_[key] = error;
                                    lognormalShifts_[key] = shift;
                                    noOfAttempts_[key] = noOfAttempts; 
                                }
                                if (error < exitEarlyErrorThreshold_)
                                    break;
                            }
                            if (bestError < exitEarlyErrorThreshold_)
                                break;
                            // narrow down the range around the best candidate
                            start = std::max(-0.99, bestRho - step);
                            end = std::min(0.99, bestRho + step);
                            step = (end - start) / (n + 1);
                        }
                    } else {
                        if (sliceParams->second[3].second == ParameterCalibration::Calibrated) {
                            if (prevSliceKey_) {
                                auto prevSliceParams = calibratedModelParams_.find(*prevSliceKey_);
                                QL_REQUIRE(prevSliceParams != calibratedModelParams_.end(),
                                        "SsviParametricVolatilityGlobal::calibrate(): previous slice parameters not found for key ("
                                            << prevSliceKey_->first << ", " << prevSliceKey_->second << "). This should not happen as we only set prevSliceKey_ if we have a valid calibration for that slice.");
                                Real rho = prevSliceParams->second[2];
                                Real bound1 = (prevSliceParams->second[3] - prevSliceParams->second[3] * prevSliceParams->second[2]) / (1.0 - rho);
                                Real bound2 = (prevSliceParams->second[3] + prevSliceParams->second[3] * prevSliceParams->second[2]) / (1.0 + rho);
                                sliceParams->second[3].first = std::max(std::max(bound1, bound2), prevSliceParams->second[3]) + 1e-6;
                            }
                        }
                        auto [params, error, shift, noOfAttempts] = calibrateModelParameters(s, sliceParams->second);
                        auto [rho, theta, phi] = SviParametricVolatility::convertToNaturalSvi(s.timeToExpiry, params, modelVariant_);
                        if (error < maxAcceptableError_) {
                            calibratedModelParams_[key] = params;
                            calibratedSviParams_[key] = {rho, theta, phi};
                        }
                        calibrationErrors_[key] = error;
                        lognormalShifts_[key] = shift;
                        noOfAttempts_[key] = noOfAttempts;
                    }
                } catch (const std::exception& e) {
                    // all calibration failed -> do not populate params
                }

                // Store key for calendar spread constraint on next slice
                if (calibratedSviParams_.find(key) != calibratedSviParams_.end()) {
                    prevSliceKey_ = key;
                }
            }
            break;   
        }
        default:
             QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
    }
    
}

Real SsviParametricVolatilityRobust::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                              const Real forward, const MarketQuoteType outputMarketQuoteType,
                                              const Real outputLognormalShift,
                                              const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType) const {
    QL_REQUIRE(timeToExpiry >= 0.0, "SsviParametricVolatilityRobust::evaluate(): negative time to expiry ("
                                        << timeToExpiry << ") not allowed.");
    QL_REQUIRE(underlyingLength >= 0.0, "SsviParametricVolatilityRobust::evaluate(): negative underlying length ("
                                        << underlyingLength << ") not allowed.");
    QL_REQUIRE(strike >= 0.0, "SsviParametricVolatilityRobust::evaluate(): negative strike ("
                                        << strike << ") not allowed.");
    QL_REQUIRE(forward > 0.0, "SsviParametricVolatilityRobust::evaluate(): non positive forward ("
                                        << forward << ") not allowed.");
    QL_REQUIRE(!calibratedSviParams_.empty(),
               "SsviParametricVolatilityRobust::evaluate(): no calibrated SVI parameters available for evaluation.");

    // we don't interpolate the svi parameters in underlying length, but take the last available slice
    Real uLength;
    if (underlyingLength == Null<Real>()) {
        uLength = underlyingLength;
    } else {
        auto it = std::upper_bound(underlyingLengths_.begin(), underlyingLengths_.end(), underlyingLength);
        uLength = it == underlyingLengths_.end() ? underlyingLengths_.back() : *it;
    }

    std::vector<Real> params;
    switch (modelVariant_) {
    case ModelVariant::CorbettaEtAl2019Essvi: {
        Real rho, theta, phi;
        auto firstParam = calibratedModelParams_.find(std::make_pair(timeToExpiries_.front(), uLength));
        auto lastParam = calibratedModelParams_.find(std::make_pair(timeToExpiries_.back(), uLength));
        QL_REQUIRE(firstParam != calibratedModelParams_.end(),
            "SsviParametricVolatilityRobust::evaluate(): no calibrated model parameters found for ("
                    << timeToExpiries_.front() << ", " << uLength << ").");
        QL_REQUIRE(lastParam != calibratedModelParams_.end(),
            "SsviParametricVolatilityRobust::evaluate(): no calibrated model parameters found for ("
                    << timeToExpiries_.back() << ", " << uLength << ").");
        auto lambda = timeToExpiry / timeToExpiries_.front();

        if (lambda < 1.0) {
            Real k_star = firstParam->second[0];
            Real theta_star = firstParam->second[1];
            rho = firstParam->second[2];
            Real psi = lambda * firstParam->second[3];
            theta = lambda * (theta_star - rho * psi * k_star);
            phi = psi / theta;
        } else if (timeToExpiry > timeToExpiries_.back()) {
            QL_REQUIRE(timeToExpiries_.size() >= 2,
                       "SsviParametricVolatilityRobust::evaluate(): cannot extrapolate beyond last time to expiry "
                       "with only one calibrated slice.");
            auto it1 = timeToExpiries_.end() - 2;
            auto secondLastParam = calibratedModelParams_.find(std::make_pair(*it1, uLength));
            QL_REQUIRE(secondLastParam != calibratedModelParams_.end(),
                       "SsviParametricVolatilityRobust::evaluate(): no calibrated model parameters found for ("
                       << *it1 << ", " << uLength << ").");
            auto k_star_N = lastParam->second[0];
            auto k_star_Nm1 = secondLastParam->second[0];
            auto theta_star_N = lastParam->second[1];
            auto theta_star_Nm1 = secondLastParam->second[1];
            Real rho_N = lastParam->second[2];
            Real rho_Nm1 = secondLastParam->second[2];
            auto psi_N = lastParam->second[3];
            auto psi_Nm1 = secondLastParam->second[3];
            auto T_N = timeToExpiries_.back();
            auto T_Nm1 = *it1;
            auto theta_N = theta_star_N - rho_N * psi_N * k_star_N;
            auto theta_Nm1 = theta_star_Nm1 - rho_Nm1 * psi_Nm1 * k_star_Nm1;
            rho = rho_N;
            phi = psi_N / theta_N;
            Real lambda  = (theta_N - theta_Nm1) / (T_N - T_Nm1);
            theta = theta_N + lambda * (timeToExpiry - T_N);
        } else {
            rho = sviParametersInterpolations_[0](timeToExpiry, uLength);
            theta = sviParametersInterpolations_[1](timeToExpiry, uLength);
            phi = sviParametersInterpolations_[2](timeToExpiry, uLength);
        }
        params = { 0.0, 0.0, rho, theta, phi };
        break;
    }
    default:
        QL_FAIL("SsviParametricVolatilityGlobal::evaluate(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }

    Real totalVariance;
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);
    Real k = std::log((std::max(strike, 1E-6) + lognormalShift) / (forward + lognormalShift));
    Real a, b, rho, m, sigma;
    std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, params, ModelVariant::Gatheral2004SviNatural);
    totalVariance = detail::sviTotalVariance(a, b, sigma, rho, m, k);

    Real result = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
    return convert(result, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift, QuantLib::ext::nullopt, timeToExpiry, strike, forward,
                   outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
                   outputOptionType);

}

SsviParametricVolatilityGlobal::SsviParametricVolatilityGlobal(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : SsviParametricVolatility(DeferredInit{}, modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                               modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                               maxAcceptableError, enforceNoArbitrage) {
    QL_REQUIRE(modelVariant == ModelVariant::HendriksMartini2017EssviFirstPowerLaw ||
               modelVariant == ModelVariant::HendriksMartini2017EssviSecondPowerLaw ||
               modelVariant == ModelVariant::Mingone2022EssviGJ || modelVariant == ModelVariant::Mingone2022EssviMM,
               "SsviParametricVolatilityGlobal::SsviParametricVolatilityGlobal(): only global SSVI model variants are allowed.");
    calculate();
}

std::tuple<std::vector<Real>, Real, std::vector<Real>, QuantLib::Size>
SsviParametricVolatilityGlobal::calibrateModelParametersGlobal(
    const std::vector<MarketSmile>& marketSmiles,
    const std::vector<std::pair<Real, ParameterCalibration>>& params) const {

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : params)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    struct TargetFunction : public QuantLib::CostFunction {
        std::vector<Real> forward_;
        std::vector<Real> timeToExpiry_;
        std::vector<Real> lognormalShift_;
        std::vector<std::vector<Real>> strikes_;
        std::vector<std::vector<QuantLib::Option::Type>> optionTypes_;
        std::vector<Real> marketQuotes_;
        std::vector<Real> weight_;
        ModelVariant modelVariant_;
        std::vector<std::function<Real(Real)>> paramTransform_; // whether to apply logit transform to the parameter
        std::vector<std::function<Real(Real)>> paramInverseTransform_; // whether to apply logit transform to the parameter
        std::function<std::vector<Real>(const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<Real>&,
                                        const std::vector<std::vector<Real>>&,
                                        const std::vector<std::vector<QuantLib::Option::Type>>&,
                                        const std::vector<Real>&)>
            evalSvi_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;

        std::vector<Real> evalSvi(const Array& x) const {
            std::vector<Real> params(params_.size());
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                    params[i] = params_[i].first;
                } else {
                    // Apply inverse logit transformation for parameters that require it
                    if (paramInverseTransform_[i]) {
                        params[i] = paramInverseTransform_[i](x[j]);
                    } else {
                        params[i] = x[j];
                    }
                    ++j;
                }
            }
            return evalSvi_(params, forward_, timeToExpiry_, lognormalShift_, strikes_,
                            optionTypes_, lognormalShift_);
        }

        Array values(const Array& x) const override {
            auto svi = evalSvi(x);
            Array result(svi.size());
            std::vector<Real> resultVec(svi.size());
            for (Size i = 0; i < svi.size(); ++i) {
                result[i] = (svi[i] - marketQuotes_[i]) * weight_[i];
                resultVec[i] = result[i];
            }

            return result;
        }

        Real value(const Array& x) const override {
            Array v = values(x);
            std::transform(v.begin(), v.end(), v.begin(), [](Real x) -> Real { return x*x; });
            return std::accumulate(v.begin(), v.end(), Real(0.0)) / Real(2.0);
        }
    };

    TargetFunction t;

    for (auto marketSmile : marketSmiles) {
        // determine the shift for the model (if applicable)

        Real modelLognormalShift;
        if (modelShifts_.empty()) {
            modelLognormalShift = marketSmile.lognormalShift;
        } else {
            auto it = modelShifts_.find(marketSmile.underlyingLength);
            QL_REQUIRE(
                it != modelShifts_.end(),
                "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                    << marketSmile.underlyingLength << " is missing in this specification.");
            modelLognormalShift = it->second;
        }

        t.forward_.push_back(marketSmile.forward);
        t.timeToExpiry_.push_back(marketSmile.timeToExpiry);
        t.lognormalShift_.push_back(modelLognormalShift);
        t.strikes_.push_back(marketSmile.strikes);
        t.optionTypes_.push_back(marketSmile.optionTypes);
        for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
            Real convertedQuote = convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                                          marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                                          : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                                          marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                                          preferredOutputQuoteType(), modelLognormalShift, QuantLib::ext::nullopt);
            t.marketQuotes_.push_back(convertedQuote);
        }
        switch (modelVariant_) {
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {
            for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
                Real k = std::log((std::max(marketSmile.strikes[i], 1E-6) + modelLognormalShift) /
                                  (marketSmile.forward + modelLognormalShift));
                Real vol = convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                                   marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                                   : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                                   marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                                   MarketQuoteType::ShiftedLognormalVolatility, modelLognormalShift, QuantLib::ext::nullopt);
                Real volSqrtT = vol * std::sqrt(marketSmile.timeToExpiry);
                Real d1 = volSqrtT * 0.5 - k / volSqrtT;
                Real vega = discountCurve_->discount(marketSmile.timeToExpiry) * marketSmile.forward *
                            (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * d1 * d1) * std::sqrt(marketSmile.timeToExpiry);
                t.weight_.push_back(1.0 / (vega + 1E-12)); // avoid division by zero
            }
            t.paramTransform_.push_back(nullptr); // rho parameter
            t.paramTransform_.push_back(nullptr); // a parameter
            t.paramTransform_.push_back([](Real x) { return std::log(x / (1.0 - x)); });  // c parameter
            t.paramInverseTransform_.push_back(nullptr); // rho parameter
            t.paramInverseTransform_.push_back(nullptr); // a parameter
            t.paramInverseTransform_.push_back([](Real x) { return 1.0 / (1.0 + std::exp(-x)); });  // c parameter
            break;
        }
        default:
            QL_FAIL("SsviParametricVolatilityGlobal::calibrateModelParametersGlobal(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
        }
    }

    t.evalSvi_ = [this](const std::vector<Real>& params,
                        const std::vector<Real>& forward,
                        const std::vector<Real>& timeToExpiry,
                        const std::vector<Real>& lognormalShift,
                        const std::vector<std::vector<Real>>& strikes,
                        const std::vector<std::vector<QuantLib::Option::Type>>& outputOptionTypes,
                        const std::vector<Real>& outputLognormalShift) {

        auto [rho, theta, phi] = convertToNaturalSvi(params, modelVariant_);
        Size n = forward.size();

        std::vector<Real> svi;
        for (Size i = 0; i < n; ++i) {
            Real psi  = phi[i] * theta[i];
            for (Size j = 0; j < strikes[i].size(); ++j) {
                Real k = std::log((std::max(strikes[i][j], 1E-6) + lognormalShift[i]) / (forward[i] + lognormalShift[i]));
                Real totalVariance = psi * k + theta[i] * rho[i];
                totalVariance *= totalVariance;
                totalVariance = std::sqrt(totalVariance + theta[i] * theta[i] * (1 - rho[i] * rho[i]));
                totalVariance = 0.5 * (theta[i] + rho[i] * psi * k + totalVariance);
                Real sigma = std::sqrt(std::max(0.0, totalVariance / timeToExpiry[i]));
                // convert to output quote type
                svi.push_back(convert(sigma, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift[i],
                                      outputOptionTypes[i].empty() ? QuantLib::ext::nullopt
                                                                   : QuantLib::ext::optional<Option::Type>(outputOptionTypes[i][j]),
                                      timeToExpiry[i], strikes[i][j], forward[i],
                                      preferredOutputQuoteType(), outputLognormalShift[i], QuantLib::ext::nullopt));
            }
        }
        return svi;
    };

    t.params_ = params;
    t.modelVariant_ = modelVariant_;

    // Define box constraints for each free parameter
    Constraint constraint = getCalibrationConstraint(params, enforceNoArbitrage_);

    LevenbergMarquardt lm;
    EndCriteria endCriteria(2000, 200, 1E-8, 1E-8, 1E-8);
    std::vector<Real> bestResult(params.size());
    EndCriteria::Type bestResultEc = EndCriteria::None;
    Real bestError = QL_MAX_REAL;

    HaltonRsg haltonRsg(noFreeParams, 42);

    Array guess(noFreeParams);

    Size attempt;
    for (attempt = 0; attempt < maxCalibrationAttempts_; ++attempt) {

        if (attempt == 0) {
            // first attempt uses given initial model parameters
            for (Size i = 0, j = 0; i < t.params_.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    // Apply logit transformation for c parameters to unbounded space
                    if (t.paramTransform_[i]) {
                        guess[j++] = t.paramTransform_[i](t.params_[i].first);
                    } else {
                        guess[j++] = t.params_[i].first;
                    }
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = getGuess(params, haltonRsg.nextSequence().value, t.forward_[0], t.lognormalShift_[0]);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    // Apply logit transformation for c parameters to unbounded space
                    if (t.paramTransform_[i]) {
                        guess[j++] = t.paramTransform_[i](g[i]);
                    } else {
                        guess[j++] = g[i];
                    }
                }
            }
        }

        Problem problem(t, constraint, guess);
        EndCriteria::Type ec;
        try {
            ec = lm.minimize(problem, endCriteria);
        } catch (const std::exception& e) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated) {
                    bestResult[i] = t.params_[i].first;
                } else {
                    if (t.paramInverseTransform_[i]) {
                        // Apply inverse logit transformation to get back to original space
                        bestResult[i] = t.paramInverseTransform_[i](problem.currentValue()[j]);
                    } else {
                        bestResult[i] = problem.currentValue()[j];
                    }
                    ++j;
                }
            }
            bestResultEc = ec;
        }

        if (bestError < exitEarlyErrorThreshold_)
            break;
    }

    // store the calibration results
    Size i = 0;
    auto calibrationResult = t.evalSvi_(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_,
                                        t.strikes_, t.optionTypes_, t.lognormalShift_);
    for (const auto& marketSmile : marketSmiles) {
        CalibrationResult result;
        result.timeToExpiry = marketSmile.timeToExpiry;
        result.underlyingLength = marketSmile.underlyingLength;
        result.forward = marketSmile.forward;
        result.strikes.insert(result.strikes.end(), marketSmile.strikes.begin(), marketSmile.strikes.end());
        result.marketInput.insert(result.marketInput.end(), marketSmile.marketQuotes.begin(), marketSmile.marketQuotes.end());
        result.calibrationTarget = std::vector<Real>(t.marketQuotes_.begin() + i, t.marketQuotes_.begin() + i + marketSmile.strikes.size());
        result.calibrationResult = std::vector<Real>(calibrationResult.begin() + i, calibrationResult.begin() + i + marketSmile.strikes.size());

        result.error = bestError;
        result.accepted = bestError < maxAcceptableError_;
        calibrationResults_.push_back(result);
        i+=marketSmile.strikes.size();
    }

    // check if if have at least one valid calibration
    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");
    QL_REQUIRE(bestResultEc != EndCriteria::None, "internal: all calibrations failed");

    // return the best calibration result
    return std::make_tuple(bestResult, bestError, t.lognormalShift_, ++attempt);

}

void SsviParametricVolatilityGlobal::setDefaultParameters() {

    SviParametricVolatility::setDefaultParameters();

    switch (modelVariant_) {
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {

        // Set default parameters for a's implied from atm vols. The parameter at index [1] is:
        //   - slice 0: theta_0 (the ATM total variance level)
        //   - slice i>0: a_i (the INCREMENT, so that theta[i] = theta[i-1]*p + a_i)
        // With the default rho=0 the p-factor equals 1, so a_i = ATM_var[i] - ATM_var[i-1].
        Real prevAtmVar = 0.0;
        Size sliceIndex = 0;
        for (auto const& s : marketSmiles_) {

            // determine the shift for the model (if applicable)

            Real modelLognormalShift;
            if (modelShifts_.empty()) {
                modelLognormalShift = s.lognormalShift;
            } else {
                auto it = modelShifts_.find(s.underlyingLength);
                QL_REQUIRE(
                    it != modelShifts_.end(),
                    "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                        << s.underlyingLength << " is missing in this specification.");
                modelLognormalShift = it->second;
            }

            // get atm vol from market smile, converted to the preferred model vol type
            Real atmQuote = getAtmQuote(s, modelLognormalShift, MarketQuoteType::ShiftedLognormalVolatility);
            Real atmVar = atmQuote * atmQuote * s.timeToExpiry;

            if (modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)][1].first == 0.0) {
                if (sliceIndex == 0) {
                    // theta_0: set to the ATM total variance level for the first slice
                    modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)][1].first = atmVar;
                } else {
                    // a_i: set to the increment so that theta[i] starts at ATM_var[i]
                    // (assuming p=1 with default rho=0: theta[i] = theta[i-1] + a_i)
                    modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)][1].first =
                        std::max(1e-6, atmVar - prevAtmVar);
                }
            }

            prevAtmVar = atmVar;
            ++sliceIndex;
        }
        break;
    }
    default:
    break;
    }
}

void SsviParametricVolatilityGlobal::calibrate() {

    auto modelParams = modelParameters_;

    std::vector<Real> modelLognormalShifts(marketSmiles_.size());
    for (Size i = 0; i < marketSmiles_.size(); ++i) {
        auto marketSmile = marketSmiles_[i];
        // determine the shift for the model (if applicable)

        Real modelLognormalShift;
        if (modelShifts_.empty()) {
            modelLognormalShift = marketSmile.lognormalShift;
        } else {
            auto it = modelShifts_.find(marketSmile.underlyingLength);
            QL_REQUIRE(
                it != modelShifts_.end(),
                "SsviParametricVolatility::calibrateModelParameters(): model shifts are specified but underlying length "
                    << marketSmile.underlyingLength << " is missing in this specification.");
            modelLognormalShift = it->second;
        }
        modelLognormalShifts[i] = modelLognormalShift;
    }

    switch (modelVariant_) {
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw: {
            QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): HendriksMartini2017EssviFirstPowerLaw and HendriksMartini2017EssviSecondPowerLaw model variants are not implemented yet.");
            break;
        }
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {
            // flatten model parameters
            std::vector<std::pair<Real, ParameterCalibration>> flatParams;
            for (auto const& s : marketSmiles_) {
                auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
                auto param = modelParameters_.find(key);
                QL_REQUIRE(param != modelParameters_.end(),
                            "SviParametricVolatility::performCalculations(): no model parameter given for ("
                                << s.timeToExpiry << ", " << s.underlyingLength
                                << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                                    "covered by the given model parameters.");
                flatParams.insert(flatParams.end(), param->second.begin(), param->second.end());
            }

            auto paramSize = expectedModelParametersSize();

            try {
                auto [params, error, shift, noOfAttempts] = calibrateModelParametersGlobal(marketSmiles_, flatParams);
                Size i = 0;
                auto [rho, theta, phi] = convertToNaturalSvi(params, modelVariant_);
                for (auto const& s : marketSmiles_) {
                    auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
                    if (error < maxAcceptableError_) {
                        calibratedModelParams_[key] = std::vector<Real>(
                            params.begin() + i * paramSize, params.begin() + (i + 1) * paramSize);
                        calibratedSviParams_[key] = {rho[i], theta[i], phi[i]};
                    }
                    calibrationErrors_[key] = error;
                    lognormalShifts_[key] = shift[i];
                    noOfAttempts_[key] = noOfAttempts;
                    ++i;
                }
            } catch (const std::exception& e) {
                // all calibration failed -> do not populate params
            }
            break;
        }
        default:
             QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
    }
    
}

std::tuple<std::vector<Real>, std::vector<Real>, std::vector<Real>>
SsviParametricVolatilityGlobal::convertToNaturalSvi(const std::vector<Real>& params, ModelVariant modelVariant) {

    QL_REQUIRE(modelVariant == ModelVariant::HendriksMartini2017EssviFirstPowerLaw ||
               modelVariant == ModelVariant::HendriksMartini2017EssviSecondPowerLaw ||
               modelVariant == ModelVariant::Mingone2022EssviGJ || modelVariant == ModelVariant::Mingone2022EssviMM,
               "SsviParametricVolatilityGlobal::convertToNaturalSvi(): only global SSVI model variants are allowed.");
    QL_REQUIRE(params.size() % expectedModelParametersSize(modelVariant) == 0, "SsviParametricVolatilityGlobal::convertToNaturalSvi: wrong number of parameters.");
    Size n = params.size() / expectedModelParametersSize(modelVariant);
    std::vector<Real> rho(n);
    std::vector<Real> theta(n);
    std::vector<Real> phi(n);
    switch (modelVariant) {
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw: {
            for (Size i = 0; i < n; ++i) {
                theta[i] = params[7 * i];
                Real eta = params[7 * i + 1];
                Real lambda = params[7 * i + 2];
                Real rho_0 = params[7 * i + 3];
                Real rho_m = params[7 * i + 4];
                Real theta_max = params[7 * i + 5];
                Real a = params[7 * i + 6];
                rho[i] = rho_0 + (rho_m - rho_0) * std::pow(theta[i] / theta_max, a);
                phi[i] = eta * std::pow(theta[i], -lambda);
            }
            break;
        }
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw: {
            for (Size i = 0; i < n; ++i) {
                theta[i] = params[7 * i];
                Real eta = params[7 * i + 1];
                Real lambda = params[7 * i + 2];
                Real rho_0 = params[7 * i + 3];
                Real rho_m = params[7 * i + 4];
                Real theta_max = params[7 * i + 5];
                Real a = params[7 * i + 6];
                rho[i] = rho_0 + (rho_m - rho_0) * std::pow(theta[i] / theta_max, a);
                phi[i] = eta * std::pow(theta[i], -lambda) * std::pow(1.0 + theta[i], lambda - 1.0);
            }
            break;
        }
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {

            for (Size i = 0; i < n; ++i) {
                rho[i] = params[3 * i];
            }

            std::vector<Real> p(n-1);
            for(Size i = 0; i < n-1; ++i) {
                auto rho_i = rho[i];
                auto rho_i1 = rho[i+1];
                p[i] = std::max((1 + rho_i) / (1 + rho_i1),
                                (1 - rho_i) / (1 - rho_i1));
            }

            theta[0] = params[1];
            for(Size i = 1; i < n; ++i) {
                auto a_i = params[3*i+1];
                theta[i] = theta[i-1] * p[i-1] + a_i;
            }

            std::vector<Real> f(n);
            for(Size i = 0; i < n; ++i) {
                const Real absRho = std::abs(rho[i]);

                switch (modelVariant) {
                    case ModelVariant::Mingone2022EssviGJ: {
                        Real fGJ = 4.0 * theta[i] / (1.0 + absRho);
                        f[i] = std::min(4.0 / (1.0 + absRho), std::sqrt(std::max(0.0, fGJ)));
                        break;
                    }
                    case ModelVariant::Mingone2022EssviMM: {
                        const Real sqrtOneMinusRho2 = std::sqrt(std::max(0.0, 1.0 - absRho * absRho));
                        const Real sqrtOneMinusRho2Safe = std::max(sqrtOneMinusRho2, 1e-12);
                        // Lower bound l2(|rho|) from MM; we nudge it to stay inside the valid domain.
                        const Real l2 = 1.0 / std::tan(std::acos(-absRho) / 3.0);
                        const Real lMin = l2 + 1e-6;
                        // Cap the search range; the MM infimum is typically attained well before this.
                        const Real lMax = std::max(10.0, lMin * 100.0);
                        // Coarse grid keeps runtime predictable; Brent only refines when we see a sign change.
                        const Size gridSize = 80;
                        Real fMm = QL_MAX_REAL;

                        auto candidateAt = [&](const Real l) {
                            // Compose the MM bound from N, N', N'' as in the paper.
                            const Real N = sqrtOneMinusRho2 + absRho * l + std::sqrt(l * l + 1.0);
                            const Real Np = absRho + l / std::sqrt(l * l + 1.0);
                            const Real Npp = 1.0 / std::pow(l * l + 1.0, 1.5);
                            const Real g = 0.25 * Np;
                            const Real h = 1.0 - (l - absRho / sqrtOneMinusRho2Safe) * Np / (2.0 * N);
                            const Real g2 = Npp - (Np * Np) / (2.0 * N);
                            const Real denom = theta[i] * sqrtOneMinusRho2Safe * g * g - g2;
                            if (denom <= 0.0)
                                return QL_MAX_REAL;
                            // MM bound candidate for a given l.
                            return 4.0 * theta[i] * sqrtOneMinusRho2Safe * h * h / denom;
                        };

                        auto derivAt = [&](const Real l) {
                            const Real h = 1e-6 * std::max(1.0, l);
                            const Real fPlus = candidateAt(l + h);
                            const Real fMinus = candidateAt(std::max(lMin, l - h));
                            if (!std::isfinite(fPlus) || !std::isfinite(fMinus))
                                return QL_MAX_REAL;
                            // Central difference derivative for root finding.
                            return (fPlus - fMinus) / (2.0 * h);
                        };

                        // Start from the left edge and then sweep log-space to find a bracket for Brent.
                        Real bestF = candidateAt(lMin);
                        Real prevL = lMin;
                        Real prevD = derivAt(prevL);
                        for (Size k = 1; k <= gridSize; ++k) {
                            const Real t = static_cast<Real>(k) / static_cast<Real>(gridSize);
                            // Log-spaced grid over l in (l2, lMax) to probe the infimum.
                            const Real l = lMin * std::exp(std::log(lMax / lMin) * t);
                            const Real fVal = candidateAt(l);
                            if (fVal < bestF) {
                                bestF = fVal;
                            }
                            const Real dVal = derivAt(l);
                            if (std::isfinite(prevD) && std::isfinite(dVal) && prevD * dVal < 0.0) {
                                try {
                                    // Bracketed root solve for a stationary point.
                                    Brent solver;
                                    solver.setMaxEvaluations(100);
                                    const Real guess = 0.5 * (prevL + l);
                                    const Real lStar = solver.solve(derivAt, 1e-8, guess, prevL, l);
                                    const Real fStar = candidateAt(lStar);
                                    if (fStar < bestF) {
                                        bestF = fStar;
                                    }
                                } catch (...) {
                                }
                            }
                            prevL = l;
                            prevD = dVal;
                        }

                        fMm = std::min(fMm, bestF);
                        f[i] = std::min(4.0 / (1.0 + absRho), std::sqrt(std::max(0.0, fMm)));
                        break;
                    }
                    default:
                        QL_FAIL("SsviParametricVolatilityGlobal::convertToNaturalSvi: unsupported model variant.");
                }
            }

            std::vector<Real> C(n);
            C[0] = f[0];
            Real denom = 1.0;
            for(Size i = 1; i < n; ++i) {
                Real num = f[i];
                denom *= p[i-1];
                C[0] = std::min(C[0], num / denom);
            }

            std::vector<Real> A(n);
            A[0] = 0.0;
            std::vector<Real> psi(n);
            psi[0] = params[2] * (C[0] - A[0]) + A[0];
            phi[0] = psi[0] / theta[0];

            for (Size i = 1; i < n; ++i) {
                A[i] = psi[i-1] * p[i-1];
                C[i] = psi[i-1] / theta[i-1] * theta[i];
                C[i] = std::min(C[i], f[i]);
                denom = 1.0;
                for (Size j = i+1; j < n; ++j) {
                    Real num = f[j];
                    denom *= p[j-1];
                    C[i] = std::min(C[i], num / denom);
                }
                psi[i] = params[3*i+2] * (C[i] - A[i]) + A[i];
                phi[i] = psi[i] / theta[i];
            }
            return std::make_tuple(rho, theta, phi);
        }
        default:
            QL_FAIL("SsviParametricVolatilityGlobal::convertToNaturalSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not handled.");
    }
    return std::make_tuple(rho, theta, phi);
}

Real SsviParametricVolatilityGlobal::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                              const Real forward, const MarketQuoteType outputMarketQuoteType,
                                              const Real outputLognormalShift,
                                              const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType) const {
    QL_REQUIRE(timeToExpiry >= 0.0, "SsviParametricVolatilityGlobal::evaluate(): negative time to expiry ("
                                        << timeToExpiry << ") not allowed.");
    QL_REQUIRE(underlyingLength >= 0.0, "SsviParametricVolatilityGlobal::evaluate(): negative underlying length ("
                                        << underlyingLength << ") not allowed.");
    QL_REQUIRE(strike >= 0.0, "SsviParametricVolatilityGlobal::evaluate(): negative strike ("
                                        << strike << ") not allowed.");
    QL_REQUIRE(forward > 0.0, "SsviParametricVolatilityGlobal::evaluate(): non positive forward ("
                                        << forward << ") not allowed.");
    QL_REQUIRE(!calibratedSviParams_.empty(),
               "SsviParametricVolatilityGlobal::evaluate(): no calibrated SVI parameters available for evaluation.");

    // we don't interpolate the svi parameters in underlying length, but take the last available slice
    Real uLength;
    if (underlyingLength == Null<Real>()) {
        uLength = underlyingLength;
    } else {
        auto it = std::upper_bound(underlyingLengths_.begin(), underlyingLengths_.end(), underlyingLength);
        uLength = it == underlyingLengths_.end() ? underlyingLengths_.back() : *it;
    }

    std::vector<Real> params;
    switch (modelVariant_) {
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM: {
        Real rho, theta, phi;
        auto firstParam = calibratedSviParams_.find(std::make_pair(timeToExpiries_.front(), uLength));
        auto lastParam = calibratedSviParams_.find(std::make_pair(timeToExpiries_.back(), uLength));
        QL_REQUIRE(firstParam != calibratedSviParams_.end(),
                "SsviParametricVolatilityGlobal::evaluate(): no calibrated SVI parameters found for ("
                    << timeToExpiries_.front() << ", " << uLength << ").");
        QL_REQUIRE(lastParam != calibratedSviParams_.end(),
                "SsviParametricVolatilityGlobal::evaluate(): no calibrated SVI parameters found for ("
                    << timeToExpiries_.back() << ", " << uLength << ").");
        auto lambda = timeToExpiry / timeToExpiries_.front();

        if (lambda < 1.0) {
            rho = firstParam->second[0];
            theta = lambda * firstParam->second[1];
            phi = lambda * firstParam->second[2];
        } else if (timeToExpiry > timeToExpiries_.back()) {
            QL_REQUIRE(timeToExpiries_.size() >= 2,
                       "SsviParametricVolatilityGlobal::evaluate(): cannot extrapolate beyond last time to expiry "
                       "with only one calibrated slice.");
            auto it1 = timeToExpiries_.end() - 2;
            auto secondLastParam = calibratedSviParams_.find(std::make_pair(*it1, uLength));
            QL_REQUIRE(secondLastParam != calibratedSviParams_.end(),
                       "SsviParametricVolatilityGlobal::evaluate(): no calibrated SVI parameters found for ("
                       << *it1 << ", " << uLength << ").");
            auto theta_N = lastParam->second[1];
            auto theta_Nm1 = secondLastParam->second[1];
            auto T_N = timeToExpiries_.back();
            auto T_Nm1 = *it1;
            Real lambda  = (theta_N - theta_Nm1) / (T_N - T_Nm1);
            rho = lastParam->second[0];
            theta = theta_N + lambda * (timeToExpiry - T_N);
            phi = lastParam->second[2];
        } else {
            rho = sviParametersInterpolations_[0](timeToExpiry, uLength);
            theta = sviParametersInterpolations_[1](timeToExpiry, uLength);
            phi = sviParametersInterpolations_[2](timeToExpiry, uLength);
        }
        params = { 0.0, 0.0, rho, theta, phi };
        break;
    }
    default:
        QL_FAIL("SsviParametricVolatilityGlobal::evaluate(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }

    Real totalVariance;
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);
    Real k = std::log((std::max(strike, 1E-6) + lognormalShift) / (forward + lognormalShift));
    Real a, b, rho, m, sigma;
    std::tie(a, b, rho, m, sigma) = convertToRawSvi(timeToExpiry, params, ModelVariant::Gatheral2004SviNatural);
    totalVariance = detail::sviTotalVariance(a, b, sigma, rho, m, k);

    Real result = std::sqrt(std::max(0.0, totalVariance / timeToExpiry));
    return convert(result, MarketQuoteType::ShiftedLognormalVolatility, lognormalShift, QuantLib::ext::nullopt, timeToExpiry, strike, forward,
                   outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
                   outputOptionType);

}

} // namespace QuantExt
