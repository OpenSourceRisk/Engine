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

namespace {
Real resolveModelLognormalShift(const ParametricVolatility::MarketSmile& marketSmile,
                                const std::map<Real, Real>& modelShifts, const char* context);
std::vector<Real> collectModelLognormalShifts(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                                              const std::map<Real, Real>& modelShifts, const char* context);
std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>
flattenModelParameters(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                       const std::map<std::pair<Real, Real>,
                                      std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
                           modelParameters,
                       const char* context);
}

SviParametricVolatility::SviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : SviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                              std::move(modelParameters), modelShifts, maxCalibrationAttempts,
                              exitEarlyErrorThreshold, maxAcceptableError, enforceNoArbitrage, false) {
    init();
}

SviParametricVolatility::SviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage, bool)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), enforceNoArbitrage_(enforceNoArbitrage) {}

ParametricVolatility::MarketQuoteType SviParametricVolatility::preferredOutputQuoteType() const {
    return SviModelTraits::preferredOutputQuoteType(modelVariant_);
}

std::vector<Real> SviParametricVolatility::getGuess(const std::vector<std::pair<Real, ParameterCalibration>>& params,
                                                    const std::vector<Real>& randomSeq, const Real forward,
                                                    const Real lognormalShift) const {
    return SviModelTraits::getGuess(modelVariant_, params, randomSeq, forward, lognormalShift);
}

std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>
SviParametricVolatility::defaultModelParameters() const {
    return SviModelTraits::defaultParameters(modelVariant_);
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize(ModelVariant modelVariant) {
    return SviModelTraits::expectedParametersSize(modelVariant);
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize() const {
    return expectedModelParametersSize(modelVariant_);
}

Constraint SviParametricVolatility::getCalibrationConstraint(
    std::vector<std::pair<Real, ParameterCalibration>> params, bool arbitrageFree) const {
    return SviModelTraits::calibrationConstraint(modelVariant_, params, arbitrageFree);
}

void SviParametricVolatility::sanitiseSviParams(std::vector<Matrix>& m) {
    SviModelTraits::sanitiseParams(modelVariant_, m);
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

    Real modelLognormalShift = resolveModelLognormalShift(
        marketSmile, modelShifts_, "SviParametricVolatility::calibrateModelParameters()");

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

using ParameterSpec = std::pair<Real, ParametricVolatility::ParameterCalibration>;
using ModelKey = std::pair<Real, Real>;
using ParameterSpecMap = std::map<std::pair<Real, Real>, std::vector<ParameterSpec>>;
using RealParameterMap = std::map<ModelKey, std::vector<Real>>;

ModelKey modelKey(const ParametricVolatility::MarketSmile& marketSmile) {
    return std::make_pair(marketSmile.timeToExpiry, marketSmile.underlyingLength);
}

Real resolveModelLognormalShift(const ParametricVolatility::MarketSmile& marketSmile,
                                const std::map<Real, Real>& modelShifts, const char* context) {
    if (modelShifts.empty()) {
        return marketSmile.lognormalShift;
    }
    auto it = modelShifts.find(marketSmile.underlyingLength);
    QL_REQUIRE(it != modelShifts.end(), context << ": model shifts are specified but underlying length "
                                                 << marketSmile.underlyingLength
                                                 << " is missing in this specification.");
    return it->second;
}

std::vector<Real> collectModelLognormalShifts(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                                              const std::map<Real, Real>& modelShifts, const char* context) {
    std::vector<Real> result;
    result.reserve(marketSmiles.size());
    for (const auto& marketSmile : marketSmiles) {
        result.push_back(resolveModelLognormalShift(marketSmile, modelShifts, context));
    }
    return result;
}

std::vector<ParameterSpec> flattenModelParameters(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                                                  const ParameterSpecMap& modelParameters, const char* context) {
    std::vector<ParameterSpec> result;
    for (const auto& marketSmile : marketSmiles) {
        auto key = std::make_pair(marketSmile.timeToExpiry, marketSmile.underlyingLength);
        auto param = modelParameters.find(key);
        QL_REQUIRE(param != modelParameters.end(), context << ": no model parameter given for ("
                                                           << marketSmile.timeToExpiry << ", "
                                                           << marketSmile.underlyingLength
                                                           << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be covered by the given model parameters.");
        result.insert(result.end(), param->second.begin(), param->second.end());
    }
    return result;
}
Array rebuildParameterArray(const Array& p, const Array& fixed, const std::vector<bool>& isFreeParams, Size size) {
    // Expand the optimizer's free-parameter vector back to the full Corbetta parameter tuple.
    Array fullParams(size);
    Size freeParamIndex = 0;
    for (Size i = 0; i < size; ++i) {
        if (isFreeParams[i]) {
            fullParams[i] = p[freeParamIndex];
            ++freeParamIndex;
        } else {
            fullParams[i] = fixed[i];
        }
    }
    return fullParams;
}

struct CorbettaNaturalParams {
    Real theta;
    Real psi;
    Real rhoPsi;
};

CorbettaNaturalParams corbettaNaturalParams(const Real kStar, const Real thetaStar, const Real rho, const Real psi) {
    return {thetaStar - rho * psi * kStar, psi, rho * psi};
}

CorbettaNaturalParams corbettaNaturalParams(const std::vector<Real>& params) {
    QL_REQUIRE(params.size() >= 4, "internal: expected at least 4 Corbetta parameters, got " << params.size());
    return corbettaNaturalParams(params[0], params[1], params[2], params[3]);
}

Real corbettaPsiUpperBound(const Real kStar, const Real thetaStar, const Real rho) {
    const Real onePlusAbsRho = 1.0 + std::abs(rho);
    Real psiPlus = 4.0 * rho * rho * kStar * kStar / (onePlusAbsRho * onePlusAbsRho);
    psiPlus += 4.0 * thetaStar / onePlusAbsRho;
    psiPlus = -2.0 * rho * kStar / onePlusAbsRho + std::sqrt(std::max(0.0, psiPlus));
    return std::min(psiPlus, 4.0 / onePlusAbsRho);
}

Real corbettaPsiLowerBound(const Real psiPrev, const Real rhoPsiPrev, const Real rho) {
    const Real bound1 = (psiPrev - rhoPsiPrev) / (1.0 - rho);
    const Real bound2 = (psiPrev + rhoPsiPrev) / (1.0 + rho);
    return std::max(std::max(bound1, bound2), psiPrev);
}

const std::vector<Real>& requirePreviousCorbettaParams(const QuantLib::ext::optional<ModelKey>& prevSliceKey,
                                                       const RealParameterMap& calibratedModelParams,
                                                       const char* context) {
    QL_REQUIRE(prevSliceKey, context << ": previous slice key not set.");
    auto prevSliceParams = calibratedModelParams.find(*prevSliceKey);
    QL_REQUIRE(prevSliceParams != calibratedModelParams.end(),
               context << ": previous slice parameters not found for key ("
                       << prevSliceKey->first << ", " << prevSliceKey->second
                       << "). This should not happen as we only set prevSliceKey_ if we have a valid calibration for that slice.");
    return prevSliceParams->second;
}

void applyCorbettaPsiLowerBound(std::vector<ParameterSpec>& params,
                                const QuantLib::ext::optional<ModelKey>& prevSliceKey,
                                const RealParameterMap& calibratedModelParams, const Real rho,
                                const char* context) {
    if (params[3].second != ParametricVolatility::ParameterCalibration::Calibrated || !prevSliceKey) {
        return;
    }
    const auto& prevParams = requirePreviousCorbettaParams(prevSliceKey, calibratedModelParams, context);
    const auto prevNatural = corbettaNaturalParams(prevParams);
    params[3].first = corbettaPsiLowerBound(prevNatural.psi, prevNatural.rhoPsi, rho) + 1e-6;
}

void corbettaParameterBounds(const std::vector<bool>& isFreeParams, Array& lowerBound, Array& upperBound) {
    // Corbetta keeps k* unbounded and imposes simple box constraints on theta*, rho and psi.
    for (Size j = 0, i = 0; i < isFreeParams.size(); ++i) {
        if (!isFreeParams[i]) {
            continue;
        }
        switch (i) {
        case 0: // kStar
            lowerBound[j] = -QL_MAX_REAL;
            upperBound[j] = QL_MAX_REAL;
            break;
        case 1: // thetaStar
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

class CorbettaButterflyConstraint : public Constraint {
public:
    CorbettaButterflyConstraint(const Array& fixed, const std::vector<bool>& isFreeParams)
        : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams))) {}

private:
    class Impl final : public Constraint::Impl {
    public:
        Impl(const Array& fixed, const std::vector<bool>& isFreeParams)
            : fixed_(fixed), isFreeParams_(isFreeParams) {}

        bool test(const Array& p) const override {
            Array q = rebuildParameterArray(p, fixed_, isFreeParams_, 4);
            return q[3] > 0.0 && q[3] < corbettaPsiUpperBound(q[0], q[1], q[2]);
        }

    private:
        Array fixed_;
        std::vector<bool> isFreeParams_;
    };
};

class CorbettaCalendarSpreadConstraint : public Constraint {
public:
    CorbettaCalendarSpreadConstraint(const Array& fixed, const std::vector<bool>& isFreeParams,
                                     const std::tuple<Real, Real, Real>& prevSlice)
        : Constraint(ext::shared_ptr<Constraint::Impl>(new Impl(fixed, isFreeParams, prevSlice))) {}

private:
    class Impl final : public Constraint::Impl {
    public:
        Impl(const Array& fixed, const std::vector<bool>& isFreeParams,
             const std::tuple<Real, Real, Real>& prevSlice)
            : fixed_(fixed), isFreeParams_(isFreeParams), prevSlice_(prevSlice) {}

        bool test(const Array& p) const override {
            Array q = rebuildParameterArray(p, fixed_, isFreeParams_, 4);
            Real rho = q[2];
            auto [thetaPrev, psiPrev, rhoPsiPrev] = prevSlice_;
            Real theta = q[1] - rho * q[3] * q[0];
            if (theta <= thetaPrev) {
                return false;
            }
            if (q[3] <= psiPrev) {
                return false;
            }
            return q[3] >= corbettaPsiLowerBound(psiPrev, rhoPsiPrev, rho);
        }

    private:
        Array fixed_;
        std::vector<bool> isFreeParams_;
        std::tuple<Real, Real, Real> prevSlice_;
    };
};

QuantLib::ext::optional<std::tuple<Real, Real, Real>>
findPreviousCorbettaNaturalParams(const QuantLib::ext::optional<ModelKey>& prevSliceKey,
                                  const RealParameterMap& calibratedModelParams) {
    if (!prevSliceKey) {
        return QuantLib::ext::nullopt;
    }
    auto it = calibratedModelParams.find(*prevSliceKey);
    if (it == calibratedModelParams.end()) {
        return QuantLib::ext::nullopt;
    }
    const auto prevNaturalParams = corbettaNaturalParams(it->second);
    return std::make_tuple(prevNaturalParams.theta, prevNaturalParams.psi, prevNaturalParams.rhoPsi);
}

Constraint corbettaConstraint(const Array& lowerBound, const Array& upperBound, const Array& fixedValues,
                              const std::vector<bool>& isFreeParams,
                              const QuantLib::ext::optional<std::tuple<Real, Real, Real>>& prevNatural) {
    Constraint butterflyConstraint = CompositeConstraint(NonhomogeneousBoundaryConstraint(lowerBound, upperBound),
                                                         CorbettaButterflyConstraint(fixedValues, isFreeParams));
    if (!prevNatural) {
        return butterflyConstraint;
    }
    return CompositeConstraint(butterflyConstraint,
                               CorbettaCalendarSpreadConstraint(fixedValues, isFreeParams, *prevNatural));
}

void storeCalibrationOutcome(RealParameterMap& calibratedModelParams, RealParameterMap& calibratedSviParams,
                             std::map<ModelKey, Real>& calibrationErrors,
                             std::map<ModelKey, Real>& lognormalShifts,
                             std::map<ModelKey, Size>& noOfAttempts, const ModelKey& key,
                             const std::vector<Real>& modelParams, const std::vector<Real>& sviParams,
                             const Real error, const Real shift, const Size attempts,
                             const Real maxAcceptableError) {
    if (error < maxAcceptableError) {
        calibratedModelParams[key] = modelParams;
        calibratedSviParams[key] = sviParams;
    }
    calibrationErrors[key] = error;
    lognormalShifts[key] = shift;
    noOfAttempts[key] = attempts;
}

void initialiseCorbettaAtmParameters(const ParametricVolatility& volatility, std::vector<ParameterSpec>& params,
                                     const ParametricVolatility::MarketSmile& marketSmile,
                                     const Real modelLognormalShift,
                                     const ParametricVolatility::MarketQuoteType inputMarketQuoteType) {
    bool implyStrike = params[0].second == ParametricVolatility::ParameterCalibration::Implied ||
                       params[0].second == ParametricVolatility::ParameterCalibration::Calibrated;
    bool implyTheta = params[1].second == ParametricVolatility::ParameterCalibration::Implied ||
                      params[1].second == ParametricVolatility::ParameterCalibration::Calibrated;
    QL_REQUIRE(!(implyStrike && !implyTheta) && !(!implyStrike && implyTheta),
               "SsviParametricVolatilityRobust::calibrate(): for CorbettaEtAl2019Essvi model, strike and theta must be both implied or both fixed.");
    if (!(implyStrike && implyTheta)) {
        return;
    }

    Real kStar = Null<Real>();
    Size bestIdx = Null<Size>();
    Real minDist = QL_MAX_REAL;
    for (Size j = 0; j < marketSmile.strikes.size(); ++j) {
        Real k = std::log((std::max(marketSmile.strikes[j], 1E-6) + modelLognormalShift) /
                          (marketSmile.forward + modelLognormalShift));
        if (std::abs(k) < minDist) {
            minDist = std::abs(k);
            kStar = k;
            bestIdx = j;
        }
    }

    params[0].first = kStar;
    params[0].second = ParametricVolatility::ParameterCalibration::Fixed;

    Real volStar = volatility.convert(
        marketSmile.marketQuotes[bestIdx], inputMarketQuoteType, marketSmile.lognormalShift,
        marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                        : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[bestIdx]),
        marketSmile.timeToExpiry, marketSmile.strikes[bestIdx], marketSmile.forward,
        ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility, modelLognormalShift,
        QuantLib::ext::nullopt);
    params[1].first = volStar * volStar * marketSmile.timeToExpiry;
    params[1].second = ParametricVolatility::ParameterCalibration::Fixed;
}

std::vector<Real> extractSsviGlobalSliceParameters(const std::vector<Real>& params, const Size sliceIndex,
                                                   const Size numberOfSlices) {
    std::vector<Real> localParams = {params[sliceIndex]};
    for (Size j = numberOfSlices; j < params.size(); ++j) {
        localParams.push_back(params[j]);
    }
    return localParams;
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

    Real sumSqVol = 0.0, sumSqPrice = 0.0, sumSqTv = 0.0;
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
        // max total variance = max(vol^2 * T) = refVol^2 * T  (max vol gives max Tv for fixed T)
        Real refTv    = refVol * refVol * T;

        if (refVol <= 0.0 || refPrice <= 0.0)
            continue;

        Real sliceSqVol = 0.0, sliceSqPrice = 0.0, sliceSqTv = 0.0;
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
                Real rTv    = (modVol * modVol * T - mktVol[k] * mktVol[k] * T) / refTv;

                sliceSqVol   += rVol   * rVol;
                sliceSqPrice += rPrice * rPrice;
                sliceSqTv    += rTv    * rTv;
                ++nSlice;
            } catch (...) {
            }
        }
        if (nSlice == 0)
            continue;

        Real n = static_cast<Real>(nSlice);
        volRmseShiftedLognormal_(row, col) = std::sqrt(sliceSqVol   / n);
        volRmsePrice_(row, col)            = std::sqrt(sliceSqPrice / n);
        volRmseTotalVariance_(row, col)    = std::sqrt(sliceSqTv    / n);

        sumSqVol   += sliceSqVol;
        sumSqPrice += sliceSqPrice;
        sumSqTv    += sliceSqTv;
        nTotal     += nSlice;
    }

    if (nTotal > 0) {
        Real n = static_cast<Real>(nTotal);
        globalVolRmseShiftedLognormal_ = std::sqrt(sumSqVol   / n);
        globalVolRmsePrice_            = std::sqrt(sumSqPrice / n);
        globalVolRmseTotalVariance_    = std::sqrt(sumSqTv    / n);
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
    return SviModelTraits::toRawSvi(timeToExpiry, params, modelVariant);
}

std::vector<Real>
SviParametricVolatility::convertFromRawSvi(const Real timeToExpiry, const std::vector<Real>& params, ModelVariant modelVariant) {
    return SviModelTraits::fromRawSvi(timeToExpiry, params, modelVariant);
}


std::tuple<Real, Real, Real>
SviParametricVolatility::convertToNaturalSvi(const Real timeToExpiry, const std::vector<Real>& params, ModelVariant modelVariant) {
    return SviModelTraits::toNaturalSvi(timeToExpiry, params, modelVariant);
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
    : SsviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                               std::move(modelParameters), modelShifts, maxCalibrationAttempts,
                               exitEarlyErrorThreshold, maxAcceptableError, enforceNoArbitrage, false) {
    QL_REQUIRE(modelVariant == ModelVariant::Gatheral2012SsviHeston ||
               modelVariant == ModelVariant::Gatheral2012SsviPowerLaw,
               "SsviParametricVolatility::SsviParametricVolatility(): only SSVI model variants are allowed.");
    init();
}

SsviParametricVolatility::SsviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, const bool enforceNoArbitrage, bool)
    : SviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                              modelParameters, modelShifts, maxCalibrationAttempts, exitEarlyErrorThreshold,
                              maxAcceptableError, enforceNoArbitrage, false) {}

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
        std::vector<Real> lognormalShifts = collectModelLognormalShifts(
            marketSmiles, modelShifts_, "SsviParametricVolatility::calibrateModelParametersGlobal()");
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

        Real modelLognormalShift = resolveModelLognormalShift(
            marketSmile, modelShifts_, "SsviParametricVolatility::calibrateModelParametersGlobal()");

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

    Real modelLognormalShift = resolveModelLognormalShift(
        marketSmile, modelShifts_, "SsviParametricVolatility::calibrateModelParameters()");

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

    std::vector<Real> modelLognormalShifts =
        collectModelLognormalShifts(marketSmiles_, modelShifts_, "SsviParametricVolatility::calibrate()");

    switch (modelVariant_) {
        case ModelVariant::Gatheral2012SsviHeston:
        case ModelVariant::Gatheral2012SsviPowerLaw: {
            // The local SSVI variants solve one global problem with one theta per slice and shared shape parameters.
            initialiseSsviAtmGuesses(modelParams, modelLognormalShifts);

            std::vector<std::pair<Real, ParameterCalibration>> flatParams =
                flattenModelParameters(marketSmiles_, modelParams, "SsviParametricVolatility::calibrate()");

            try {
                auto [params, error, shift, noOfAttempts] = calibrateModelParametersGlobal(marketSmiles_, flatParams);
                Size i = 0;
                for (auto const& s : marketSmiles_) {
                    std::vector<Real> localParams =
                        extractSsviGlobalSliceParameters(params, i, marketSmiles_.size());
                    storeSsviCalibrationResult(modelKey(s), localParams, error, shift[i], noOfAttempts);
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

void SsviParametricVolatility::initialiseSsviAtmGuesses(
    ParameterSpecMap& modelParams, const std::vector<Real>& modelLognormalShifts) const {
    for (Size i = 0; i < marketSmiles_.size(); ++i) {
        auto& params = modelParams[modelKey(marketSmiles_[i])];
        if (params[0].second == ParameterCalibration::Implied) {
            Real atmQuote = getAtmQuote(marketSmiles_[i], modelLognormalShifts[i],
                                        MarketQuoteType::ShiftedLognormalVolatility);
            params[0].first = atmQuote * atmQuote * marketSmiles_[i].timeToExpiry;
            params[0].second = ParameterCalibration::Fixed;
        }
    }
}

void SsviParametricVolatility::storeSsviCalibrationResult(const ModelKey& key, const std::vector<Real>& params,
                                                          const Real error, const Real shift,
                                                          const Size noOfAttempts) {
    storeCalibrationOutcome(calibratedModelParams_, calibratedSviParams_, calibrationErrors_, lognormalShifts_,
                            noOfAttempts_, key, params, params, error, shift, noOfAttempts,
                            maxAcceptableError_);
}

SsviParametricVolatilityRobust::SsviParametricVolatilityRobust(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError, bool enforceNoArbitrage)
    : SsviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                               std::move(modelParameters), modelShifts, maxCalibrationAttempts,
                               exitEarlyErrorThreshold, maxAcceptableError, enforceNoArbitrage, false) {
    QL_REQUIRE(modelVariant == ModelVariant::CorbettaEtAl2019Essvi,
               "SsviParametricVolatilityRobust::SsviParametricVolatilityRobust(): only robust SSVI model variants are allowed.");
    init();
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
        corbettaParameterBounds(isFreeParams, lowerBound, upperBound);
        if (arbitrageFree) {
            return corbettaConstraint(lowerBound, upperBound, fixedValues, isFreeParams,
                                      findPreviousCorbettaNaturalParams(prevSliceKey(), calibratedModelParams_));
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

    std::vector<Real> modelLognormalShifts = collectModelLognormalShifts(
        marketSmiles_, modelShifts_, "SsviParametricVolatilityRobust::calibrate()");

    switch (modelVariant_) {
        case ModelVariant::CorbettaEtAl2019Essvi: {
            // Clear previous slice key before forward calibration
            prevSliceKey_ = QuantLib::ext::nullopt;

            initialiseCorbettaAtmGuesses(modelParams, modelLognormalShifts);
            calibrateCorbettaSlices(modelParams);
            break;   
        }
        default:
             QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): model variant ("
                    << static_cast<int>(modelVariant_) << ") not handled.");
    }
    
}

void SsviParametricVolatilityRobust::storeCorbettaCalibrationResult(const ModelKey& key,
                                                                    const std::vector<Real>& params,
                                                                    const Real error, const Real shift,
                                                                    const Size noOfAttempts) {
    auto [rho, theta, phi] = SviParametricVolatility::convertToNaturalSvi(key.first, params, modelVariant_);
    storeCalibrationOutcome(calibratedModelParams_, calibratedSviParams_, calibrationErrors_, lognormalShifts_,
                            noOfAttempts_, key, params, {rho, theta, phi}, error, shift, noOfAttempts,
                            maxAcceptableError_);
}

void SsviParametricVolatilityRobust::calibrateCorbettaFixedRhoSlice(
    const MarketSmile& marketSmile, std::vector<ParameterSpec>& params, const ModelKey& key) {
    applyCorbettaPsiLowerBound(params, prevSliceKey_, calibratedModelParams_, params[2].first,
                               "SsviParametricVolatilityRobust::calibrate()");
    auto [resultParams, error, shift, noOfAttempts] = calibrateModelParameters(marketSmile, params);
    storeCorbettaCalibrationResult(key, resultParams, error, shift, noOfAttempts);
}

void SsviParametricVolatilityRobust::calibrateCorbettaRhoSlice(
    const MarketSmile& marketSmile, std::vector<ParameterSpec>& params, const ModelKey& key) {
    const Real candidateCount = 10;
    std::vector<Real> rhoCandidates(candidateCount);
    Real rhoLower = -0.99;
    Real rhoUpper = 0.99;
    Real rhoStep = (rhoUpper - rhoLower) / (candidateCount + 1);
    Real bestSliceError = QL_MAX_REAL;

    // Sweep rho on a shrinking grid and keep the best calibration seen across all passes.
    while (rhoStep > 0.0001) {
        for (Size j = 0; j < candidateCount; ++j) {
            rhoCandidates[j] = rhoLower + (j + 1) * rhoStep;
        }
        Real bestPassError = QL_MAX_REAL;
        Real bestPassRho = rhoCandidates[0];

        for (auto rho : rhoCandidates) {
            params[2].first = rho;
            params[2].second = ParameterCalibration::Fixed;
            applyCorbettaPsiLowerBound(params, prevSliceKey_, calibratedModelParams_, rho,
                                       "SsviParametricVolatilityRobust::calibrate()");

            auto [resultParams, error, shift, noOfAttempts] = calibrateModelParameters(marketSmile, params);
            if (error < bestPassError) {
                bestPassError = error;
                bestPassRho = rho;
            }
            if (error < bestSliceError) {
                bestSliceError = error;
                storeCorbettaCalibrationResult(key, resultParams, error, shift, noOfAttempts);
            }
            if (error < exitEarlyErrorThreshold_) {
                break;
            }
        }

        if (bestPassError < exitEarlyErrorThreshold_) {
            break;
        }

        rhoLower = std::max(-0.99, bestPassRho - rhoStep);
        rhoUpper = std::min(0.99, bestPassRho + rhoStep);
        rhoStep = (rhoUpper - rhoLower) / (candidateCount + 1);
    }
}

void SsviParametricVolatilityRobust::initialiseCorbettaAtmGuesses(
    ParameterSpecMap& modelParams, const std::vector<Real>& modelLognormalShifts) const {
    for (Size i = 0; i < marketSmiles_.size(); ++i) {
        auto& params = modelParams[modelKey(marketSmiles_[i])];
        initialiseCorbettaAtmParameters(*this, params, marketSmiles_[i], modelLognormalShifts[i],
                                        inputMarketQuoteType_);
    }
}

void SsviParametricVolatilityRobust::calibrateCorbettaSlices(ParameterSpecMap& modelParams) {
    for (const auto& marketSmile : marketSmiles_) {
        const auto key = modelKey(marketSmile);
        auto sliceParams = modelParams.find(key);
        QL_REQUIRE(sliceParams != modelParams.end(),
                   "SsviParametricVolatilityRobust::performCalculations(): no model parameter given for ("
                       << marketSmile.timeToExpiry << ", " << marketSmile.underlyingLength
                       << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                          "covered by the given model parameters.");
        try {
            if (sliceParams->second[2].second == ParameterCalibration::Calibrated) {
                calibrateCorbettaRhoSlice(marketSmile, sliceParams->second, key);
            } else {
                calibrateCorbettaFixedRhoSlice(marketSmile, sliceParams->second, key);
            }
        } catch (const std::exception& e) {
            // all calibration failed -> do not populate params
        }

        // The previous successful slice is needed for the next calendar-spread constraint.
        if (calibratedSviParams_.find(key) != calibratedSviParams_.end()) {
            prevSliceKey_ = key;
        }
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
            Real kStar = firstParam->second[0];
            Real thetaStar = firstParam->second[1];
            rho = firstParam->second[2];
            Real psi = lambda * firstParam->second[3];
            theta = lambda * (thetaStar - rho * psi * kStar);
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
            auto kStarN = lastParam->second[0];
            auto kStarNm1 = secondLastParam->second[0];
            auto thetaStarN = lastParam->second[1];
            auto thetaStarNm1 = secondLastParam->second[1];
            Real rhoN = lastParam->second[2];
            Real rhoNm1 = secondLastParam->second[2];
            auto psiN = lastParam->second[3];
            auto psiNm1 = secondLastParam->second[3];
            auto tN = timeToExpiries_.back();
            auto tNm1 = *it1;
            auto thetaN = thetaStarN - rhoN * psiN * kStarN;
            auto thetaNm1 = thetaStarNm1 - rhoNm1 * psiNm1 * kStarNm1;
            rho = rhoN;
            phi = psiN / thetaN;
            Real lambda  = (thetaN - thetaNm1) / (tN - tNm1);
            theta = thetaN + lambda * (timeToExpiry - tN);
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
    : SsviParametricVolatility(modelVariant, marketSmiles, marketModelType, inputMarketQuoteType, discountCurve,
                               std::move(modelParameters), modelShifts, maxCalibrationAttempts,
                               exitEarlyErrorThreshold, maxAcceptableError, enforceNoArbitrage, false) {
    QL_REQUIRE(modelVariant == ModelVariant::HendriksMartini2017EssviFirstPowerLaw ||
               modelVariant == ModelVariant::HendriksMartini2017EssviSecondPowerLaw ||
               modelVariant == ModelVariant::Mingone2022EssviGJ || modelVariant == ModelVariant::Mingone2022EssviMM,
               "SsviParametricVolatilityGlobal::SsviParametricVolatilityGlobal(): only global SSVI model variants are allowed.");
    init();
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

            Real modelLognormalShift = resolveModelLognormalShift(
                s, modelShifts_, "SsviParametricVolatilityGlobal::setDefaultParameters()");

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

    std::vector<Real> modelLognormalShifts = collectModelLognormalShifts(
        marketSmiles_, modelShifts_, "SsviParametricVolatilityGlobal::calibrate()");

    switch (modelVariant_) {
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw: {
            QL_FAIL("SsviParametricVolatilityGlobal::calibrate(): HendriksMartini2017EssviFirstPowerLaw and HendriksMartini2017EssviSecondPowerLaw model variants are not implemented yet.");
            break;
        }
        case ModelVariant::Mingone2022EssviGJ:
        case ModelVariant::Mingone2022EssviMM: {
            // The global Mingone variants calibrate the full surface in one shot and then unpack one slice at a time.
            std::vector<std::pair<Real, ParameterCalibration>> flatParams =
                flattenModelParameters(marketSmiles_, modelParams, "SsviParametricVolatilityGlobal::calibrate()");

            try {
                auto [params, error, shift, noOfAttempts] = calibrateModelParametersGlobal(marketSmiles_, flatParams);
                storeGlobalCalibrationResults(params, shift, error, noOfAttempts);
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

void SsviParametricVolatilityGlobal::storeGlobalCalibrationResults(const std::vector<Real>& params,
                                                                   const std::vector<Real>& shifts,
                                                                   const Real error,
                                                                   const Size noOfAttempts) {
    const auto paramSize = expectedModelParametersSize();
    Size i = 0;
    auto [rho, theta, phi] = convertToNaturalSvi(params, modelVariant_);
    for (const auto& marketSmile : marketSmiles_) {
        const auto key = modelKey(marketSmile);
        std::vector<Real> localParams(params.begin() + i * paramSize, params.begin() + (i + 1) * paramSize);
        storeCalibrationOutcome(calibratedModelParams_, calibratedSviParams_, calibrationErrors_, lognormalShifts_,
                                noOfAttempts_, key, localParams, {rho[i], theta[i], phi[i]}, error, shifts[i],
                                noOfAttempts, maxAcceptableError_);
        ++i;
    }
}

std::tuple<std::vector<Real>, std::vector<Real>, std::vector<Real>>
SsviParametricVolatilityGlobal::convertToNaturalSvi(const std::vector<Real>& params, ModelVariant modelVariant) {
    return SviModelTraits::toNaturalSviGlobal(params, modelVariant);
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
            auto thetaN = lastParam->second[1];
            auto thetaNm1 = secondLastParam->second[1];
            auto tN = timeToExpiries_.back();
            auto tNm1 = *it1;
            Real lambda  = (thetaN - thetaNm1) / (tN - tNm1);
            rho = lastParam->second[0];
            theta = thetaN + lambda * (timeToExpiry - tN);
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
