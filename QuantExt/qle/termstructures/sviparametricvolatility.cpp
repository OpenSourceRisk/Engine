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
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool deferCalculate)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError) {
    if (!deferCalculate)
        calculate();
}

ParametricVolatility::MarketQuoteType SviParametricVolatility::preferredOutputQuoteType() const {
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2004SviNatural:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2004SviJw:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Gatheral2012SsviHeston:
        return MarketQuoteType::Price;
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return MarketQuoteType::Price;
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
                {0.0, ParameterCalibration::Calibrated}, // will be set to ATM total implied variances
                {0.5, ParameterCalibration::Calibrated}};
    default:
        QL_FAIL("SviParametricVolatility::defaultModelParameters(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize() const {
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw: // a, b, rho, m, sigma
    case ModelVariant::Gatheral2004SviNatural: // delta, miu, rho, omega, zeta
    case ModelVariant::Gatheral2004SviJw:
        return 5; // a, b, rho, m, sigma
    case ModelVariant::Gatheral2012SsviHeston:
        return 3; // theta, rho, lambda
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return 4; // theta, rho, eta, gamma
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        return 7; // theta, eta, lambda, p_0, p_m, theta_max, a
    case ModelVariant::CorbettaEtAl2019Essvi:
        return 4; // theta_star, k_star, rho, phi
    case ModelVariant::Mingone2022EssviGJ:
    case ModelVariant::Mingone2022EssviMM:
        return 3; // rho, a, c
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
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
                // Set bounds based on parameter index
                switch(i) {
                    case 1: // rho
                        lowerBound[j] = -1 + 1e-6;
                        upperBound[j] = 1 - 1e-6;
                        break;
                    case 0: // theta
                    case 2: // lambda
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
            // Constraint for no butterfly arbitrage
            class HestonConstraint : public Constraint {
                private:
                    class Impl final : public Constraint::Impl {
                    public:
                        Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
                        : fixed_(fixed), isFreeParams_(isFreeParams) {}
                        bool test(const Array& p) const override {
                            Array q(3);
                            Size j = 0;
                            for (Size i = 0; i < 3; ++i) {
                                if (isFreeParams_[i]) {
                                    q[i] = p[j];
                                    ++j;
                                } else {
                                    q[i] = fixed_[i];
                                }
                            }
                            return q[2] >= (1.0 + std::abs(q[1])) / 4.0;
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
                // Set bounds based on parameter index
                switch(i) {
                    case 1: // rho
                        lowerBound[j] = -1 + 1e-6;
                        upperBound[j] = 1 - 1e-6;
                        break;
                    case 0: // theta
                    case 2: // eta
                        lowerBound[j] = 1e-6;
                        upperBound[j] = QL_MAX_REAL;
                        break;
                    case 3: // gamma
                        lowerBound[j] = 1e-6;
                        upperBound[j] = 1 - 1e-6;
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
            // Constraint for no butterfly arbitrage
            class PowerLawConstraint : public Constraint {
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

                            const Real cond3Bound = std::pow(4 / (q[2] * (1 + std::abs(q[1]))), 1 / (1 - q[3]));
                            if (q[0] > cond3Bound)
                                return false;

                            if (q[3] < 0.5) {
                                return q[0] <= std::pow(4 / (q[2] * q[2] * (1 + std::abs(q[1]))), 1 / (1 - 2 * q[3]));
                            } else if (q[3] > 0.5) {
                                return q[0] >= std::pow(4 / (q[2] * q[2] * (1 + std::abs(q[1]))), 1 / (1 - 2 * q[3]));
                            } else {
                                return q[2] * q[2] * (1 + std::abs(q[1])) <= 4;
                            }
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
    // case ModelVariant::CorbettaEtAl2019Essvi:
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
    Constraint constraint = getCalibrationConstraint(params, false);

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
        case ModelVariant::Gatheral2012SsviPowerLaw: {
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
            return {rho, theta, lambda};
        }
        case ModelVariant::Gatheral2012SsviPowerLaw: {
            Real theta = m;
            Real gamma = 0.5; // assume gamma = 0.5 for simplicity
            // Reverse the relation: sigma = eta * theta^(-gamma)
            // Therefore: eta = sigma * theta^gamma
            Real eta = sigma * std::pow(theta, gamma);
            return {rho, theta, eta, gamma};
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
    default:
        QL_FAIL("SsviParametricVolatility::convertToNaturalSvi(): model variant ("
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
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError)
    : SviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                             modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                             maxAcceptableError, true) {  // deferCalculate = true
        QL_REQUIRE(modelVariant == ModelVariant::Gatheral2012SsviHeston ||
                   modelVariant == ModelVariant::Gatheral2012SsviPowerLaw,
               "SsviParametricVolatility::SsviParametricVolatility(): only SSVI model variants are allowed.");
        // Now call calculate() so virtual dispatch works correctly
        calculate();
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

std::tuple<std::vector<Real>, Real, Real, QuantLib::Size>
SsviParametricVolatility::calibrateModelParameters(
    const MarketSmile& marketSmile, const std::vector<std::pair<Real, ParameterCalibration>>& params) const {

    // get initial guess for the parameters

    Real firstTimeToExpiry = std::numeric_limits<Real>::max();
    for (auto const& smile : marketSmiles_) {
        firstTimeToExpiry = std::min(firstTimeToExpiry, smile.timeToExpiry);
    }
    Real previousTimeToExpiry = QuantLib::Null<Real>();
    for (auto const& smile : marketSmiles_) {
        if (smile.timeToExpiry >= marketSmile.timeToExpiry)
            continue;
        if (previousTimeToExpiry == QuantLib::Null<Real>() ||
            smile.timeToExpiry > previousTimeToExpiry) {
            previousTimeToExpiry = smile.timeToExpiry;
        }
    }

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

    std::tuple<std::vector<Real>, Real, Real, Size> resultForInitialCalibration;
    if (firstTimeToExpiry == marketSmile.timeToExpiry) {
        std::vector<std::pair<Real, ParameterCalibration>> modifiedParams = params;
        if (params[0].second == ParametricVolatility::ParameterCalibration::Implied) {
            modifiedParams[0].first = atmVol * atmVol * marketSmile.timeToExpiry;
            modifiedParams[0].second = ParameterCalibration::Fixed;
        }
        resultForInitialCalibration = SviParametricVolatility::calibrateModelParameters(marketSmile, modifiedParams);
    } else {
        // fix rho after initial calibration at first expiry
        std::vector<std::pair<Real, ParameterCalibration>> modifiedParams = params;
        if (params[0].second == ParametricVolatility::ParameterCalibration::Implied) {
            modifiedParams[0].first = atmVol * atmVol * marketSmile.timeToExpiry;
            modifiedParams[0].second = ParameterCalibration::Fixed;
        }
        modifiedParams[1].first = calibratedSviParams_.at(
            std::make_pair(firstTimeToExpiry, marketSmile.underlyingLength))[1];
        modifiedParams[1].second = ParameterCalibration::Fixed;
        resultForInitialCalibration = SviParametricVolatility::calibrateModelParameters(marketSmile, modifiedParams);
    }
    std::vector<std::pair<Real, ParameterCalibration>> modifiedParams = params;
    auto calibratedParams = std::get<0>(resultForInitialCalibration);
    for (Size i = 0; i < modifiedParams.size(); ++i) {
        modifiedParams[i].first = calibratedParams[i];
    }
    modifiedParams[1].second = ParameterCalibration::Fixed; // fix rho for current slice

    // The reset will be similar to SviParametricVolatility::calibrateModelParameters, but with penalty for
    // calendar spread arbitrage

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : modifiedParams)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    // if there are no free parameters, we pass back fixed parameters as the result

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : modifiedParams) {
            resultParams.push_back(p.first);
        }
        if (params[0].second == ParametricVolatility::ParameterCalibration::Implied) {
            resultParams[0] = atmVol * atmVol * marketSmile.timeToExpiry;
        }
        return std::make_tuple(resultParams, 0.0, modelLognormalShift, 0);
    }

    if (modifiedParams[0].second == ParametricVolatility::ParameterCalibration::Implied) {
        modifiedParams[0].first = atmVol * atmVol * marketSmile.timeToExpiry;
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

    t.params_ = modifiedParams;

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
    if (previousTimeToExpiry != Null<Real>())
        t.paramsPreviousSlice_ = calibratedSviParams_.at(std::make_pair(previousTimeToExpiry, marketSmile.underlyingLength));

    // perform the calibration (this step might throw if all minimizations go wrong)

    // Define box constraints for each free parameter
    Constraint constraint = getCalibrationConstraint(modifiedParams, true);

    LevenbergMarquardt lm;
    EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);

    std::vector<Real> bestResult(modifiedParams.size());
    Real bestError = QL_MAX_REAL;

    HaltonRsg haltonRsg(noFreeParams, 42);

    Array guess(noFreeParams);

    Size attempt;
    for (attempt = 0; attempt < maxCalibrationAttempts_; ++attempt) {

        if (attempt == 0) {
            // first attempt uses given initial model parameters
            for (Size i = 0, j = 0; i < t.params_.size(); ++i) {
                if (modifiedParams[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = t.params_[i].first;
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = getGuess(modifiedParams, haltonRsg.nextSequence().value, t.forward_, t.lognormalShift_);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (modifiedParams[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
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
                if (modifiedParams[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
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

SsviParametricVolatilityGlobal::SsviParametricVolatilityGlobal(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError)
    : SviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                             modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                             maxAcceptableError, true) {  // deferCalculate = true
        QL_REQUIRE(modelVariant == ModelVariant::Mingone2022EssviGJ || modelVariant == ModelVariant::Mingone2022EssviMM,
                   "SsviParametricVolatilityGlobal::SsviParametricVolatilityGlobal(): only global SSVI model variants are allowed.");
        // Now call calculate() so virtual dispatch works correctly
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
                    // Apply inverse logit transformation for c parameters (i%3==2 for i>=3, or i==2)
                    if (i == 2 || (i > 2 && i % 3 == 2)) {
                        // Transform from unbounded x back to c ∈ (0,1): c = 1/(1 + exp(-x))
                        params[i] = 1.0 / (1.0 + std::exp(-x[j]));
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
    }

    t.evalSvi_ = [this](const std::vector<Real>& params,
                        const std::vector<Real>& forward,
                        const std::vector<Real>& timeToExpiry,
                        const std::vector<Real>& lognormalShift,
                        const std::vector<std::vector<Real>>& strikes,
                        const std::vector<std::vector<QuantLib::Option::Type>>& outputOptionTypes,
                        const std::vector<Real>& outputLognormalShift) {

        auto [rho, theta, psi] = convertToNaturalSvi(params, modelVariant_);
        Size n = forward.size();

        std::vector<Real> svi;
        for (Size i = 0; i < n; ++i) {
            for (Size j = 0; j < strikes[i].size(); ++j) {
                Real k = std::log((std::max(strikes[i][j], 1E-6) + lognormalShift[i]) / (forward[i] + lognormalShift[i]));
                Real totalVariance = psi[i] * k + theta[i] * rho[i];
                totalVariance *= totalVariance;
                totalVariance = std::sqrt(totalVariance + theta[i] * theta[i] * (1 - rho[i] * rho[i]));
                totalVariance = 0.5 * (theta[i] + rho[i] * psi[i] * k + totalVariance);
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
    Constraint constraint = getCalibrationConstraint(params, true);

    LevenbergMarquardt lm(1e-8, 1e-8, 1e-6, false, 100000, false);
    EndCriteria endCriteria(100000, 1000, 1E-6, 1E-6, 1E-6);
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
                    if (i == 2 || (i > 2 && i % 3 == 2)) {
                        // Transform c to unbounded: x = log(c/(1-c))
                        Real c = std::max(1e-6, std::min(1.0 - 1e-6, t.params_[i].first));
                        guess[j++] = std::log(c / (1.0 - c));
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
                    if (i == 2 || (i > 2 && i % 3 == 2)) {
                        // Transform c to unbounded: x = log(c/(1-c))
                        Real c = std::max(1e-6, std::min(1.0 - 1e-6, g[i]));
                        guess[j++] = std::log(c / (1.0 - c));
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
                    // Apply inverse logit for c parameters
                    if (i == 2 || (i > 2 && i % 3 == 2)) {
                        bestResult[i] = 1.0 / (1.0 + std::exp(-problem.currentValue()[j]));
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

    // default parameters for a's should be implied from atm vols
    for (auto const& s : marketSmiles_) {

        if (modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)][1].first == 0.0) {

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
            modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)][1].first = atmQuote * atmQuote * s.timeToExpiry;
        }
    }

}

void SsviParametricVolatilityGlobal::calibrate() {
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
        auto [rho, theta, psi] = convertToNaturalSvi(params, modelVariant_);
        for (auto const& s : marketSmiles_) {
            auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
            if (error < maxAcceptableError_) {
                calibratedModelParams_[key] = std::vector<Real>(
                    params.begin() + i * paramSize, params.begin() + (i + 1) * paramSize);
                calibratedSviParams_[key] = {rho[i], theta[i], psi[i]};
            }
            calibrationErrors_[key] = error;
            lognormalShifts_[key] = shift[i];
            noOfAttempts_[key] = noOfAttempts;
            ++i;
        }
    } catch (const std::exception& e) {
        // all calibration failed -> do not populate params, but interpolate them below
    }
}

std::tuple<std::vector<Real>, std::vector<Real>, std::vector<Real>>
SsviParametricVolatilityGlobal::convertToNaturalSvi(const std::vector<Real>& params, ModelVariant modelVariant) {

    QL_REQUIRE(modelVariant == ModelVariant::Mingone2022EssviGJ || modelVariant == ModelVariant::Mingone2022EssviMM,
               "SsviParametricVolatilityGlobal::convertToNaturalSvi only supports Mingone2022EssviGJ and Mingone2022EssviMM model variants.");
    QL_REQUIRE(params.size() % 3 == 0, "SsviParametricVolatilityGlobal::convertToNaturalSvi: wrong number of parameters.");

    Size n = params.size() / 3;

    std::vector<Real> rho(n);
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

    std::vector<Real> theta(n);
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
    }

    return std::make_tuple(rho, theta, psi);
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
        Real rho, theta, psi;
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
            psi = lambda * firstParam->second[2];
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
            psi = lastParam->second[2];
        } else {
            rho = sviParametersInterpolations_[0](timeToExpiry, uLength);
            theta = sviParametersInterpolations_[1](timeToExpiry, uLength);
            psi = sviParametersInterpolations_[2](timeToExpiry, uLength);
        }
        params = { 0.0, 0.0, rho, theta, psi / theta };
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
