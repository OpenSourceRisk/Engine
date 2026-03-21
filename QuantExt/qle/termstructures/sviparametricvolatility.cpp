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
// #include <ql/math/comparison.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/optimization/constraint.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
// #include <ql/math/solvers1d/brent.hpp>
// #include <ql/termstructures/volatility/sabr.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

using namespace QuantLib;

SviParametricVolatility::SviParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts, const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold, const Real maxAcceptableError)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError) {
    calculate();
}

ParametricVolatility::MarketQuoteType SviParametricVolatility::preferredOutputQuoteType() const {
    return MarketQuoteType::ShiftedLognormalVolatility;
}

std::vector<Real> SviParametricVolatility::getGuess(const std::vector<std::pair<Real, ParameterCalibration>>& params,
                                                    const std::vector<Real>& randomSeq, const Real forward,
                                                    const Real lognormalShift) const {
    std::vector<Real> result(params.size(), 0.0);
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
    return result;
}

std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>
SviParametricVolatility::defaultModelParameters() const {
    return {{0.005, ParameterCalibration::Calibrated},
            {0.8, ParameterCalibration::Calibrated},
            {0.0, ParameterCalibration::Calibrated},
            {0.0, ParameterCalibration::Calibrated},
            {0.002, ParameterCalibration::Calibrated}};
}

QuantLib::Size SviParametricVolatility::expectedModelParametersSize() const {
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw: // a, b, rho, m, sigma
    case ModelVariant::Gatheral2004SviNatural: // delta, miu, rho, omega, zeta
    case ModelVariant::Gatheral2004SviJw:
        return 5; // a, b, rho, m, sigma
    case ModelVariant::Gatheral2012SsviHeston:
        return 3; // rho, theta, lambda
    case ModelVariant::Gatheral2012SsviPowerLaw:
        return 4; // rho, theta, eta, gamma
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        return 7; // theta, eta, lambda, p_0, p_m, theta_max, a
    case ModelVariant::CorbettaEtAl2019Essvi:
        return 4; // theta_star, k_star, rho, phi
    case ModelVariant::Mingone2022Essvi:
        return 5; // check
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

Constraint SviParametricVolatility::getCalibrationConstraint(std::vector<std::pair<Real, ParameterCalibration>> params) const {
    Size noFreeParams = 0;
    std::vector<bool> isFreeParams(params.size(), false);
    for (Size i = 0; i < params.size(); ++i) {
        auto const& p = params[i];
        if (p.second == ParameterCalibration::Calibrated) {
            isFreeParams[i] = true;
            ++noFreeParams;
        }
    }
    
    switch (modelVariant_) {
    case ModelVariant::Gatheral2004SviRaw: {
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
        return NonhomogeneousBoundaryConstraint(lowerBound, upperBound);
    }
    case ModelVariant::Gatheral2012SsviHeston:
        // return 3; // rho, theta, lambda
    case ModelVariant::Gatheral2012SsviPowerLaw:
        // return 4; // rho, theta, eta, gamma
    case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
    case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        // return 7; // theta, eta, lambda, p_0, p_m, theta_max, a
    case ModelVariant::CorbettaEtAl2019Essvi:
        // return 4; // theta_star, k_star, rho, phi
    case ModelVariant::Mingone2022Essvi:
        // return 5; // check
    default:
        QL_FAIL("SviParametricVolatility::expectedModelParametersSize(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

std::vector<Real> SviParametricVolatility::evaluateSvi(const std::vector<Real>& params, const Real forward,
                                                       const Real timeToExpiry, const Real lognormalShift,
                                                       const std::vector<Real>& strikes) const {
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

    std::vector<Real> x, y;
    for (Size i = 0; i < marketSmile.strikes.size(); ++i) {
        x.push_back(marketSmile.strikes[i]);
        y.push_back(convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                            marketSmile.optionTypes.empty() ? QuantLib::ext::nullopt
                                                            : QuantLib::ext::optional<Option::Type>(marketSmile.optionTypes[i]),
                            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward,
                            preferredOutputQuoteType(), modelLognormalShift, QuantLib::ext::nullopt));
    }

    Interpolation m = LinearFlat().interpolate(x.begin(), x.end(), y.begin());
    m.enableExtrapolation();
    Real atmVol = m(marketSmile.forward);

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
        Real atmVol_;
        Real refQuote_;
        std::function<std::vector<Real>(const std::vector<Real>&, const Real, const Real, const Real,
                                        const std::vector<Real>&)>
            evalSvi_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;

        std::vector<Real> evalSvi(const Array& x) const {
            std::vector<Real> params(x.size());
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    params[i] = params_[i].first;
                else
                    params[i] = x[j++];
            }
            return evalSvi_(params, forward_, timeToExpiry_, lognormalShift_, strikes_);
        }

        Array values(const Array& x) const override {
            Array result(strikes_.size());
            auto svi = evalSvi(x);
            for (Size i = 0; i < strikes_.size(); ++i) {
                result[i] = (marketQuotes_[i] - svi[i]) / refQuote_;
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
                         const Real lognormalShift, const std::vector<Real>& strikes) {
        return evaluateSvi(params, forward, timeToExpiry, lognormalShift, strikes);
    };

    t.params_ = params;

    t.atmVol_ = atmVol;
    t.strikes_  = marketSmile.strikes;
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
    Constraint constraint = getCalibrationConstraint(params);
    
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
    result.calibrationResult = evaluateSvi(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_, t.strikes_);
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

void SviParametricVolatility::calculate() {

    // if no model parameters are given, we provide the default ones

    if (modelParameters_.empty()) {
        for (auto const& s : marketSmiles_) {
            modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)] = defaultModelParameters();
        }
    }

    // check validity of model parameters

    for (auto const& [k, v] : modelParameters_) {
        QL_REQUIRE(v.size() == expectedModelParametersSize(),
                   "SviParametricVolatility::performCalculations(): wrong number of model parameters ("
                       << v.size() << ") given for ("
                       << "timeToExpiry=" << k.first << ", underlyingLength=" << k.second << "), expected "
                       << expectedModelParametersSize() << " for model variant "
                       << static_cast<int>(modelVariant_) << ".");
    }

    Size paramSize = expectedModelParametersSize();

    // clear stored data

    calibratedSviParams_.clear();
    lognormalShifts_.clear();
    calibrationErrors_.clear();
    sviParametersMatrices_.clear();
    sviParametersInterpolations_.clear();

    // // for each market smile calibrate the SVI variant

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

    // sanitize values produced by the the interpolation that are not allowed

    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j < n; ++j) {
            // b_(i, j) = std::max(b_(i, j), 0.0);
            // rho_(i, j) = std::max(std::min(rho_(i, j), 1.0 - 1e-6), -1.0 + 1e-6);
            // sigma_(i, j) = std::max(sigma_(i, j), 1e-6);
        }
    }

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

            // TODO: check below formulae for sigma, m, a

            // Ensure beta is within valid range (-1, 1) for numerical stability
            beta = std::max(-1.0 + 1E-6, std::min(1.0 - 1E-6, beta));

            Real val = (1.0 - rho * beta) / std::sqrt(1.0 - beta * beta) - std::sqrt(1.0 - rho * rho);

            if (std::abs(val) > 1E-10) {
                sigma = (v - v_tilda) * timeToExpiry / (b * val);
                m = sigma * beta / std::sqrt(1.0 - beta * beta);
                a = v_tilda * timeToExpiry - b * sigma * std::sqrt(1.0 - rho * rho);
            } else {
                // Fallback for v ~ v_tilda (implies phi ~ 0, beta ~ rho)
                // System is underdetermined for sigma. Assume a = 0 to close the system.
                sigma = w / (b * std::sqrt(1.0 - rho * rho));
                m = sigma * rho / std::sqrt(1.0 - rho * rho);
                a = 0.0;
            }
            break;
        }
        case ModelVariant::Gatheral2012SsviHeston: {
            a = b = 0.0;
            rho = params[0];
            Real theta = params[1];
            m =  theta;
            Real lambda = params[2];
            Real phi = 1.0 - std::exp(-lambda * theta);
            phi = 1.0 - phi / (lambda * theta);
            phi = phi / (lambda * theta);
            sigma = phi;
            break;
        }
        case ModelVariant::Gatheral2012SsviPowerLaw: {
            a = b = 0.0;
            rho = params[0];
            Real theta = params[1];
            m =  theta;
            Real eta = params[2];
            Real gamma = params[3];
            Real phi = eta * std::pow(theta, -gamma);
            sigma = phi;
            break;
        }
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw: {
            a = b = 0.0;
            Real theta = params[0];
            m = theta;
            Real eta = params[1];
            Real lambda = params[2];
            Real phi = eta * std::pow(theta, -lambda);
            sigma = phi;
            Real p_0 = params[3];
            Real p_m = params[4];
            Real theta_max = params[5];
            Real a = params[6];
            rho = p_0 + (p_m - p_0) * std::pow(theta / theta_max, a);
            break;
        }
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw: {
            a = b = 0.0;
            Real theta = params[0];
            m = theta;
            Real eta = params[1];
            Real lambda = params[2];
            Real phi = eta * std::pow(theta, -lambda);
            phi = phi * std::pow((1.0 + lambda), lambda - 1.0);
            sigma = phi;
            Real p_0 = params[3];
            Real p_m = params[4];
            Real theta_max = params[5];
            Real a = params[6];
            rho = p_0 + (p_m - p_0) * std::pow(theta / theta_max, a);
            break;
        }
        case ModelVariant::CorbettaEtAl2019Essvi: {
            a = b = 0.0;
            rho = params[0];
            Real theta_star = params[1];
            Real k_star = params[2];
            Real phi = params[3];
            Real theta = theta_star - rho * phi * k_star;
            m =  theta;
            sigma = phi;
            break;
        }
        case ModelVariant::Mingone2022Essvi:
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not implemented.");
            break;
        default: {
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not handled.");
        }
    }
    return std::make_tuple(a, b, rho, m, sigma);

}

std::tuple<Real, Real, Real, Real, Real>
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
            return std::make_tuple(params[0], params[1], params[2], params[3], params[4]);
            break;
        }
        case ModelVariant::Gatheral2004SviNatural: {
            Real sqrtOneMinusRho2 = std::sqrt(1.0 - rho * rho);
            Real miu = m + rho * sigma / sqrtOneMinusRho2;
            Real omega = 2.0 * b * sigma / sqrtOneMinusRho2;
            Real zeta = sqrtOneMinusRho2 / sigma;
            Real delta = a - 0.5 * omega * sqrtOneMinusRho2 * sqrtOneMinusRho2;
            return std::make_tuple(delta, miu, rho, omega, zeta);
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
            return std::make_tuple(v, phi, p, c, v_tilda);
        }
        case ModelVariant::Gatheral2012SsviHeston:
        case ModelVariant::Gatheral2012SsviPowerLaw:
        case ModelVariant::HendriksMartini2017EssviFirstPowerLaw:
        case ModelVariant::HendriksMartini2017EssviSecondPowerLaw:
        case ModelVariant::CorbettaEtAl2019Essvi:
        case ModelVariant::Mingone2022Essvi:
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not implemented.");
            break;
        default: {
            QL_FAIL("SviParametricVolatility::convertToRawSvi(): model variant ("
                    << static_cast<int>(modelVariant) << ") not handled.");
        }
    }
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
    case ModelVariant::CorbettaEtAl2019Essvi:
    case ModelVariant::Mingone2022Essvi: {
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

} // namespace QuantExt
