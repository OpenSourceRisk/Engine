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
#include <qle/models/normalsabr.hpp>
#include <qle/termstructures/sabrparametricvolatility.hpp>
#include <qle/math/flatextrapolation.hpp>

#include <ql/experimental/math/laplaceinterpolation.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/termstructures/volatility/sabr.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

using namespace QuantLib;
using Dimension = ParametricVolatility::ResidualCorrection::Dimension;
using std::pair;
using std::string;
using std::vector;

SabrParametricVolatility::SabrParametricVolatility(
    const ModelVariant modelVariant,
    const std::vector<MarketSmile>& marketSmiles,
    const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType,
    const Handle<YieldTermStructure> discountCurve,
    const ParamInfo& modelParameters,
    const std::map<QuantLib::Real, QuantLib::Real>& modelShifts,
    const Size maxCalibrationAttempts,
    const Real exitEarlyErrorThreshold,
    const Real maxAcceptableError,
    ext::optional<ResidualCorrection> residualCorrection)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve, residualCorrection),
      modelVariant_(modelVariant), modelParameters_(modelParameters), modelShifts_(modelShifts),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError) {
    calculate();
}

ParametricVolatility::MarketQuoteType SabrParametricVolatility::preferredOutputQuoteType() const {
    switch (modelVariant_) {
    case ModelVariant::Hagan2002Lognormal:
        return MarketQuoteType::ShiftedLognormalVolatility;
    case ModelVariant::Hagan2002Normal:
        return MarketQuoteType::NormalVolatility;
    case ModelVariant::Hagan2002NormalZeroBeta:
        return MarketQuoteType::NormalVolatility;
    case ModelVariant::Antonov2015FreeBoundaryNormal:
        return MarketQuoteType::Price;
    case ModelVariant::KienitzLawsonSwaynePde:
        return MarketQuoteType::Price;
    case ModelVariant::FlochKennedy:
        return MarketQuoteType::ShiftedLognormalVolatility;
    default:
        QL_FAIL("SabrParametricVolatility::preferredOutputQuoteType(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

std::vector<Real> SabrParametricVolatility::getGuess(const SliceParamInfo& params,
    const std::vector<Real>& randomSeq, const Real forward, const Real lognormalShift) const {

    std::vector<Real> result(4);
    for (Size i = 0, j = 0; i < 4; ++i) {
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

SabrParametricVolatility::SliceParamInfo SabrParametricVolatility::defaultModelParameters() const {
    switch (modelVariant_) {
    case ModelVariant::Hagan2002Lognormal:
        return {{0.0050, ParameterCalibration::Implied},
                {0.8, ParameterCalibration::Calibrated},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    case ModelVariant::Hagan2002Normal:
        return {{0.0050, ParameterCalibration::Implied},
                {0.8, ParameterCalibration::Calibrated},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    case ModelVariant::Hagan2002NormalZeroBeta:
        return {{0.0050, ParameterCalibration::Implied},
                {0.0, ParameterCalibration::Fixed},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    case ModelVariant::Antonov2015FreeBoundaryNormal:
        return {{0.0050, ParameterCalibration::Implied},
                {0.0, ParameterCalibration::Fixed},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    case ModelVariant::KienitzLawsonSwaynePde:
        return {{0.0050, ParameterCalibration::Implied},
                {0.8, ParameterCalibration::Calibrated},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    case ModelVariant::FlochKennedy:
        return {{0.0050, ParameterCalibration::Implied},
                {0.8, ParameterCalibration::Calibrated},
                {0.30, ParameterCalibration::Calibrated},
                {0.0, ParameterCalibration::Calibrated}};
    default:
        QL_FAIL("SabrParametricVolatility::defaultModelParameters(): model variant (" << static_cast<int>(modelVariant_)
                                                                                      << ") not handled.");
    }
}

std::vector<Real> SabrParametricVolatility::direct(const std::vector<Real>& x, const Real forward,
                                                   const Real lognormalShift) const {
    std::vector<Real> y(4);
    y[1] = std::max(eps1, std::min(1.0 - eps1, std::exp(-(x[1] * x[1]))));
    Real fbeta = std::pow(std::max(forward + lognormalShift, eps1), y[1]);
    y[0] = std::max(eps1, std::exp(-(x[0] * x[0])) / fbeta * max_nvol_equiv);
    y[2] = std::max(eps1, std::exp(-(x[2] * x[2])) * max_nu);
    y[3] = std::fabs(x[3]) < 2.5 * M_PI ? eps2 * std::sin(x[3]) : eps2 * (x[3] > 0.0 ? 1.0 : (-1.0));
    return y;
}

std::vector<Real> SabrParametricVolatility::inverse(const std::vector<Real>& y, const Real forward,
                                                    const Real lognormalShift) const {
    std::vector<Real> x(4);
    x[1] = std::sqrt(-std::log(std::min(1.0 - eps1, std::max(eps1, y[1]))));
    Real fbeta = std::pow(std::max(forward + lognormalShift, eps1), y[1]);
    x[0] = std::sqrt(-std::log(std::min(1.0 - eps1, std::max(eps1, y[0] * fbeta / max_nvol_equiv))));
    x[2] = std::sqrt(-std::log(std::min(1.0 - eps1, std::max(eps1, y[2] / max_nu))));
    x[3] = std::asin(std::max(-eps2, std::min(eps2, y[3])));
    return x;
}

std::vector<Real> SabrParametricVolatility::evaluateSabr(const std::vector<Real>& params, const Real forward,
                                                         const Real timeToExpiry, const Real lognormalShift,
                                                         const std::vector<Real>& strikes) const {
    std::vector<Real> result;
    switch (modelVariant_) {
    case ModelVariant::Hagan2002Lognormal: {
        result.resize(strikes.size());
        for (Size i = 0; i < strikes.size(); ++i) {
            try {
                if (strikes[i] < -lognormalShift || QuantLib::close_enough(strikes[i], 0.0))
                    result[i] = 0.0;
                else
                    result[i] = unsafeSabrLogNormalVolatility(strikes[i] + lognormalShift, forward + lognormalShift,
                                                              timeToExpiry, params[0], params[1], params[2], params[3]);
            } catch (...) {
                result[i] = 0.0;
            }
        }
        break;
    }
    case ModelVariant::Hagan2002Normal: {
        result.resize(strikes.size());
        for (Size i = 0; i < strikes.size(); ++i) {
            try {
                if (strikes[i] < -lognormalShift || QuantLib::close_enough(strikes[i], 0.0))
                    result[i] = 0.0;
                else
                    result[i] = unsafeSabrNormalVolatility(strikes[i] + lognormalShift, forward + lognormalShift,
                                                           timeToExpiry, params[0], params[1], params[2], params[3]);
            } catch (...) {
                result[i] = 0.0;
            }
        }
        break;
    }
    case ModelVariant::Hagan2002NormalZeroBeta: {
        result.resize(strikes.size());
        for (Size i = 0; i < strikes.size(); ++i) {
            try {
                result[i] =
                    QuantExt::normalSabrVolatility(strikes[i], forward, timeToExpiry, params[0], params[2], params[3]);
            } catch (...) {
                result[i] = 0.0;
            }
        }
        break;
    }
    case ModelVariant::Antonov2015FreeBoundaryNormal: {
        result.resize(strikes.size());
        for (Size i = 0; i < strikes.size(); ++i) {
            try {
                result[i] = QuantExt::normalFreeBoundarySabrPrice(strikes[i], forward, timeToExpiry, params[0],
                                                                  params[2], params[3]) *
                            (discountCurve_.empty() ? 1.0 : discountCurve_->discount(timeToExpiry));
                if (strikes[i] < forward)
                    result[i] = result[i] - forward + strikes[i];
            } catch (...) {
                result[i] = 0.0;
            }
        }
        break;
    }
    case ModelVariant::KienitzLawsonSwaynePde: {
        try {
            KienitzLawsonSwayneSabrPdeDensity pde(params[0], params[1], params[2], params[3], forward, timeToExpiry,
                                                  lognormalShift, 50,
                                                  std::max<Size>(5, std::lround(24.0 * timeToExpiry + 0.5)), 5.0);
            result = pde.callPrices(strikes);
            for (Size i = 0; i < strikes.size(); ++i) {
                if (strikes[i] < forward)
                    result[i] = result[i] - forward + strikes[i];
                result[i] *= discountCurve_.empty() ? 1.0 : discountCurve_->discount(timeToExpiry);
            }
        } catch (...) {
            result = std::vector<Real>(strikes.size(), 0.0);
        }
        break;
    }
    case ModelVariant::FlochKennedy: {
        result.resize(strikes.size());
        for (Size i = 0; i < strikes.size(); ++i) {
            try {
                if (strikes[i] < -lognormalShift || QuantLib::close_enough(strikes[i], 0.0))
                    result[i] = 0.0;
                else
                    result[i] = sabrFlochKennedyVolatility(strikes[i] + lognormalShift, forward + lognormalShift,
                                                           timeToExpiry, params[0], params[1], params[2], params[3]);
            } catch (...) {
                result[i] = 0.0;
            }
        }
        break;
    }
    default:
        QL_FAIL("SabrParametricVolatility::preferredOutputQuoteType(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }

    // ensure we have a number, not inf or nan

    for (auto& v : result)
        if (!std::isfinite(v))
            v = 0.0;

    return result;
}

std::tuple<std::vector<Real>, Real, Real, Size> SabrParametricVolatility::calibrateModelParameters(
    const MarketSmile& marketSmile, const SliceParamInfo& params, const vector<Real>& convertedMarketQuotes) const {

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : params)
        if (p.second == ParameterCalibration::Calibrated)
            ++noFreeParams;

    // determine the shift for the model (if applicable)
    Real modelLognormalShift = getLognormalShift(marketSmile);

    // get atm vol from market smile, converted to the preferred model vol type
    Interpolation m = LinearFlat().interpolate(marketSmile.strikes.begin(),
        marketSmile.strikes.end(), convertedMarketQuotes.begin());
    m.enableExtrapolation();
    Real atmVol = m(marketSmile.forward);

    // if there are no free parameters, we pass back fixed parameters (maybe implied alpha) as the result

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : params) {
            resultParams.push_back(p.first);
        }
        if (params[0].second == ParametricVolatility::ParameterCalibration::Implied) {
            resultParams = implyAlpha(resultParams, marketSmile.forward, marketSmile.timeToExpiry,
                                      modelLognormalShift, atmVol);
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
        Real atmVol_;
        Real refQuote_;
        std::function<std::vector<Real>(const std::vector<Real>&, const Real, const Real, const Real,
                                        const std::vector<Real>&)>
            evalSabr_;
        std::function<std::vector<Real>(const std::vector<Real>& params, const Real forward, const Real tte,
                                        const Real shift, const Real atmVol)>
            implyAlpha_;
        std::vector<std::pair<Real, ParameterCalibration>> params_;
        std::vector<Real> invParams_;
        std::function<std::vector<Real>(const std::vector<Real>&)> direct_;
        std::function<std::vector<Real>(const std::vector<Real>&)> inverse_;

        std::vector<Real> evalSabr(const Array& x) const {
            std::vector<Real> params(4);
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    params[i] = invParams_[i];
                else
                    params[i] = x[j++];
            }
            params = direct_(params);
            return evalSabr_(params_[0].second == ParametricVolatility::ParameterCalibration::Implied
                                 ? implyAlpha_(params, forward_, timeToExpiry_, lognormalShift_, atmVol_)
                                 : params,
                             forward_, timeToExpiry_, lognormalShift_, strikes_);
        }

        Array values(const Array& x) const override {
            Array result(strikes_.size());
            auto sabr = evalSabr(x);
            for (Size i = 0; i < strikes_.size(); ++i) {
                result[i] = (marketQuotes_[i] - sabr[i]) / refQuote_;
            }
            return result;
        }
    };

    /* build the target function and populate the members:
       strikes, marketQuotes  : the latter are converted to the preferred output quote type of the SABR
       evalSabr               : the function to produce SABR values for a given vector of strikes */

    TargetFunction t;

    t.forward_ = marketSmile.forward;
    t.timeToExpiry_ = marketSmile.timeToExpiry;
    t.lognormalShift_ = modelLognormalShift;

    t.evalSabr_ = [this](const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                         const Real lognormalShift, const std::vector<Real>& strikes) {
        return evaluateSabr(params, forward, timeToExpiry, lognormalShift, strikes);
    };

    t.implyAlpha_ = [this](const std::vector<Real>& params, const Real forward,
                           const Real tte, const Real shift,
                           const Real atmVol) { return implyAlpha(params, forward, tte, shift, atmVol); };

    t.params_ = params;
    for (Size i = 0; i < params.size(); ++i)
        t.invParams_.push_back(params[i].first);
    t.invParams_ = inverse(t.invParams_, marketSmile.forward, marketSmile.lognormalShift);

    t.inverse_ = [this, f = t.forward_, s = t.lognormalShift_](const std::vector<Real>& y) { return inverse(y, f, s); };
    t.direct_ = [this, f = t.forward_, s = t.lognormalShift_](const std::vector<Real>& x) { return direct(x, f, s); };

    t.atmVol_ = atmVol;
    t.strikes_ = marketSmile.strikes;
    t.marketQuotes_ = convertedMarketQuotes;
    // we use relative errors w.r.t. the max market quote, because far otm quotes are close to zero
    t.refQuote_ = *std::max_element(t.marketQuotes_.begin(), t.marketQuotes_.end());

    // perform the calibration (this step might throw if all minimizations go wrong)

    NoConstraint noConstraint;
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
            for (Size i = 0, j = 0; i < t.invParams_.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = t.invParams_[i];
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = inverse(getGuess(params, haltonRsg.nextSequence().value, t.forward_, t.lognormalShift_),
                             t.forward_, t.lognormalShift_);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (params[i].second == ParametricVolatility::ParameterCalibration::Calibrated) {
                    guess[j++] = g[i];
                }
            }
        }

        Problem problem(t, noConstraint, guess);
        try {
            lm.minimize(problem, endCriteria);
        } catch (const std::exception&) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (params[i].second != ParametricVolatility::ParameterCalibration::Calibrated)
                    bestResult[i] = t.invParams_[i];
                else
                    bestResult[i] = problem.currentValue()[j++];
            }
            bestResult = direct(bestResult, t.forward_, t.lognormalShift_);
            if (params[0].second == ParametricVolatility::ParameterCalibration::Implied)
                bestResult =
                    implyAlpha(bestResult, marketSmile.forward, marketSmile.timeToExpiry, modelLognormalShift, atmVol);
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
    result.calibrationResult = evaluateSabr(bestResult, t.forward_, t.timeToExpiry_, t.lognormalShift_, t.strikes_);
    result.error = bestError;
    result.accepted = bestError < maxAcceptableError_;
    calibrationResults_.push_back(result);

    // check if if have at least one valid calibration
    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");

    // return the best calibration result
    return std::make_tuple(bestResult, bestError, t.lognormalShift_, ++attempt);
}

std::vector<Real> SabrParametricVolatility::implyAlpha(const std::vector<Real>& params, const Real forward,
                                                       const Real tte, const Real shift, const Real atmVol) const {
    QL_REQUIRE(atmVol != Null<Real>(), "SabrParametricVolatility::implyAlpha(): no atm vol given to imply alpha.");

    QL_REQUIRE(params.size() == 4,
               "SabrParametricVolatility::implyAlpha(), params have wrong size (" << params.size() << "), expected 4");

    auto result = params;

    switch (modelVariant_) {
    case ModelVariant::Hagan2002NormalZeroBeta:
        result[0] = normalSabrAlphaFromAtmVol(forward, tte, atmVol, params[2], params[3]);
        break;
    case ModelVariant::Hagan2002Lognormal:
    case ModelVariant::Hagan2002Normal:
    case ModelVariant::Antonov2015FreeBoundaryNormal:
    case ModelVariant::KienitzLawsonSwaynePde:
    case ModelVariant::FlochKennedy: {
        auto target = [this, &params, forward, tte, shift, atmVol](const Real alpha) {
            return evaluateSabr({alpha, params[1], params[2], params[3]}, forward, tte, shift, {forward})[0] - atmVol;
        };
        Brent brent;
        brent.setLowerBound(0.0);
        try {
            result[0] = brent.solve(target, 1E-6, params[0], 1E-5);
        } catch (const std::exception& e) {
            QL_FAIL("SabrParametricVolatility::implyAlpha() failed: " << e.what());
        }
    }
    }

    return result;
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
               "Error during laplaceInterpolation() in SabrParametricVolatility ("
                   << boost::join(errorText, ",")
                   << "), this might be related to the numerical parameters relTol, maxIterMult. Contact dev.");
}
} // namespace

void SabrParametricVolatility::calculate() {

    // if no model parameters are given, we provide the default ones

    if (modelParameters_.empty()) {
        for (auto const& s : marketSmiles_) {
            modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)] = defaultModelParameters();
        }
    }

    // Replace any Null<Real>() initial value with the model's hard-coded default for that parameter index.
    // Null<Real>() means "<InitialValue> was omitted" in XML.
    auto const sabrDefaults = defaultModelParameters();
    for (auto& [key, params] : modelParameters_) {
        for (Size i = 0; i < params.size() && i < sabrDefaults.size(); ++i) {
            if (params[i].first == Null<Real>())
                params[i].first = sabrDefaults[i].first;
        }
    }

    // check validity of model parameters

    for (auto const& [k, v] : modelParameters_) {
        QL_REQUIRE(v.size() == 4, "SabrParametricVolatility::calculate(): model parameter at ("
                                      << k.first << ", " << k.second
                                      << ") is not valid, expected 4 parameters, but got " << v.size());
        for (Size n = 1; n < 4; ++n) {
            QL_REQUIRE(v[n].second != ParameterCalibration::Implied,
                       "SabrParametricVolatility::calculate(): only alpha (parameter #0) can be of type 'Implied'. Got "
                       "parameter #"
                           << n << " of type 'Implied");
        }
    }

    // clear stored data

    calibratedSabrParams_.clear();
    lognormalShifts_.clear();
    calibrationErrors_.clear();

    // Populate the market volatilities converted to the requested output type.
    populateConvertedMarketQuotes();

    // for each market smile calibrate the SABR variant

    for (auto const& s : marketSmiles_) {
        auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
        auto param = modelParameters_.find(key);
        QL_REQUIRE(param != modelParameters_.end(), "SabrParametricVolatility: no model parameter given for ("
            << s.timeToExpiry << ", " << s.underlyingLength << "). All (timeToExpiry, underlyingLength) pairs "
            "that are given as market points must be covered by the given model parameters.");

        // Conversion may have failed for this smile. If so, we cannot calibrate the model. Will interpolate below.
        auto itQuotes = convertedMarketQuotes_.find(key);
        if (itQuotes == convertedMarketQuotes_.end())
            continue;

        try {
            auto [params, error, shift, noOfAttempts] = calibrateModelParameters(s, param->second, itQuotes->second);
            if (error < maxAcceptableError_)
                calibratedSabrParams_[key] = params;
            calibrationErrors_[key] = error;
            lognormalShifts_[key] = shift;
            noOfAttempts_[key] = noOfAttempts;
        } catch (const std::exception&) {
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

    // Record if underlying length is relevant. Will be used below to determine how / if we apply residual correction.
    bool haveUndLengths = !(underlyingLengths_.size() == 1 && underlyingLengths_[0] == Null<Real>());

    // build a matrix of calibrated SABR parameters, possibly with null values

    Size m = underlyingLengths_.size();
    Size n = timeToExpiries_.size();

    alpha_ = Matrix(m, n, Null<Real>());
    beta_ = Matrix(m, n, Null<Real>());
    nu_ = Matrix(m, n, Null<Real>());
    rho_ = Matrix(m, n, Null<Real>());
    lognormalShift_ = Matrix(m, n, Null<Real>());
    calibrationError_ = Matrix(m, n, Null<Real>());
    isInterpolated_ = Matrix(m, n, 1.0);
    numberOfCalibrationAttempts_ = Matrix(m, n, 0.0);

    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j < n; ++j) {
            auto key = std::make_pair(timeToExpiries_[j], underlyingLengths_[i]);
            if (auto p = calibratedSabrParams_.find(key); p != calibratedSabrParams_.end()) {
                alpha_(i, j) = p->second[0];
                beta_(i, j) = p->second[1];
                nu_(i, j) = p->second[2];
                rho_(i, j) = p->second[3];
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

    laplaceInterpolationWithErrorHandling(alpha_, timeToExpiries_, underlyingLengths_);
    laplaceInterpolationWithErrorHandling(beta_, timeToExpiries_, underlyingLengths_);
    laplaceInterpolationWithErrorHandling(nu_, timeToExpiries_, underlyingLengths_);
    laplaceInterpolationWithErrorHandling(rho_, timeToExpiries_, underlyingLengths_);

    // sanitize values produced by the the interpolation that are not allowed

    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j < n; ++j) {
            alpha_(i, j) = std::max(alpha_(i, j), 0.0);
            beta_(i, j) = std::max(std::min(beta_(i, j), 1.0), 0.0);
            nu_(i, j) = std::max(nu_(i, j), 0.0);
            rho_(i, j) = std::max(std::min(rho_(i, j), 1.0), -1.0);
        }
    }

    // workaround because BilinearInterpolation below requires at least two points in each dimension

    timeToExpiriesForInterpolation_ = timeToExpiries_;
    underlyingLengthsForInterpolation_ = underlyingLengths_;

    if (m == 1 || n == 1) {

        auto mNew = m == 1 ? m + 1 : m;
        auto nNew = n == 1 ? n + 1 : n;

        auto alphaTmp = alpha_;
        auto betaTmp = beta_;
        auto nuTmp = nu_;
        auto rhoTmp = rho_;
        auto lognormalShiftTmp = lognormalShift_;

        alpha_ = Matrix(mNew, nNew, Null<Real>());
        beta_ = Matrix(mNew, nNew, Null<Real>());
        nu_ = Matrix(mNew, nNew, Null<Real>());
        rho_ = Matrix(mNew, nNew, Null<Real>());
        lognormalShift_ = Matrix(mNew, nNew, Null<Real>());

        for (Size i = 0; i < mNew; ++i) {
            for (Size j = 0; j < nNew; ++j) {
                Size iOld = std::min(i, m - 1);
                Size jOld = std::min(j, n - 1);
                alpha_(i, j) = alphaTmp(iOld, jOld);
                beta_(i, j) = betaTmp(iOld, jOld);
                nu_(i, j) = nuTmp(iOld, jOld);
                rho_(i, j) = rhoTmp(iOld, jOld);
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

    alphaInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), alpha_));
    betaInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), beta_));
    nuInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), nu_));
    rhoInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), rho_));
    lognormalShiftInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), lognormalShift_));

    alphaInterpolation_.enableExtrapolation();
    betaInterpolation_.enableExtrapolation();
    nuInterpolation_.enableExtrapolation();
    rhoInterpolation_.enableExtrapolation();
    lognormalShiftInterpolation_.enableExtrapolation();

    // Residual interpolations.
    if (residualCorrection_) {
        QL_REQUIRE(!haveUndLengths, "SabrParametricVolatility: residual correction not yet supported for "
            "volatility structures with underlying lengths.");
        if (residualSmiles_.empty())
            buildResidualSmiles();
        else
            updateResidualSmiles();
    }
}

Real SabrParametricVolatility::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                        const Real forward, const MarketQuoteType outputMarketQuoteType,
                                        const Real outputLognormalShift,
                                        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType) const {

    Real alpha = alphaInterpolation_(timeToExpiry, underlyingLength);
    Real beta = betaInterpolation_(timeToExpiry, underlyingLength);
    Real nu = nuInterpolation_(timeToExpiry, underlyingLength);
    Real rho = rhoInterpolation_(timeToExpiry, underlyingLength);
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);

    Real result = evaluateSabr({alpha, beta, nu, rho}, forward, timeToExpiry, lognormalShift, {strike}).front();
    result += residualCorrection(timeToExpiry, underlyingLength, strike, forward);
    return convert(result, preferredOutputQuoteType(), lognormalShift, QuantLib::ext::nullopt, timeToExpiry, strike,
        forward, outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
        outputOptionType);
}

QuantLib::ext::shared_ptr<SabrParametricVolatility> SabrParametricVolatility::clone(
    const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
    const std::vector<ParameterCalibration>& calibrationTypes) const {

    auto modelParameters = modelParameters_;

    if (calibrationTypes.size() > 0) {
        // If calibration types are provided, we need to adjust the model parameters accordingly
        for (Size i = 0; i < timeToExpiries_.size(); ++i) {
            const auto& tte = timeToExpiries_[i];
            for (Size j = 0; j < underlyingLengths_.size(); ++j) {
                const auto& ul = underlyingLengths_[j];
                auto key = std::make_pair(tte, ul);
                QL_REQUIRE(calibrationTypes.size() == modelParameters[key].size(),
                           "SabrParametricVolatility::clone(): number of calibration types ("
                               << calibrationTypes.size() << ") does not match number of model parameters ("
                               << modelParameters[key].size() << ") for ("
                               << tte << ", " << ul << ").");
                for (Size k = 0; k < modelParameters[key].size(); ++k) {
                    modelParameters[key][k].second = calibrationTypes[k];
                }
            }
        }
    }

    return QuantLib::ext::make_shared<SabrParametricVolatility>(
        modelVariant_, marketSmiles, marketModelType_, inputMarketQuoteType_, discountCurve_, modelParameters,
        modelShifts_, maxCalibrationAttempts_, exitEarlyErrorThreshold_, maxAcceptableError_);
}

Real SabrParametricVolatility::getLognormalShift(const ParametricVolatility::MarketSmile& marketSmile) const {
    if (modelShifts_.empty())
        return marketSmile.lognormalShift;

    auto it = modelShifts_.find(marketSmile.underlyingLength);
    QL_REQUIRE(it != modelShifts_.end(), "SabrParametricVolatility: model shifts are specified but underlying length "
        << marketSmile.underlyingLength << " is missing in this specification.");
    return it->second;
}

void SabrParametricVolatility::populateConvertedMarketQuotes() const {
    for (const auto& marketSmile : marketSmiles_) {
        auto tteUndKey = std::make_pair(marketSmile.timeToExpiry, marketSmile.underlyingLength);
        Real modelLognormalShift = getLognormalShift(marketSmile);
        auto poqt = preferredOutputQuoteType();
        ext::optional<Option::Type> optType;
        Size nStrikes = marketSmile.strikes.size();

        // If the conversion fails for any volatility in the smile, we move to the next smile.
        vector<Real> vols;
        vols.reserve(nStrikes);
        try
        {
            for (Size i = 0; i < nStrikes; ++i) {
                if (!marketSmile.optionTypes.empty())
                    optType = marketSmile.optionTypes[i];
                vols.push_back(convert(marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
                    optType, marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward, poqt,
                    modelLognormalShift));
            }
        } catch (const std::exception&) {
            continue;
        }
        convertedMarketQuotes_.emplace(tteUndKey, vols);
    }
}

namespace {

// Small helper to convert strike to the correct coordinate system for residual correction below.
Real strikeCoordinate(Real strike, Real forward, Dimension dimension) {
    switch (dimension) {
    case Dimension::AbsoluteStrike:
        return strike;
    case Dimension::StrikeMinusForward:
        return strike - forward;
    case Dimension::StrikeOverForward:
        QL_REQUIRE(forward > 0.0, "SabrParametricVolatility: expect positive forward with StrikeOverForward "
            "dimension when calculating strike coordinates");
        return strike / forward;
    }
    QL_FAIL("SabrParametricVolatility: unsupported strike dimension when calculating strike coordinates");
}

// Small helper to calculate taper coordinates for residual correction below.
// The 15% taper fraction is to ensure that the residuals are tapered to zero outside the market strikes.
// It may need to be reviewed or made configurable.
pair<Real, Real> boundaryCoordinates(const vector<Real>& marketStrikes, Real forward, Dimension dimension,
    Real taperFraction = 0.15)
{
    QL_REQUIRE(!marketStrikes.empty(), "SabrParametricVolatility: empty market strike vector when calculating "
        "boundary strike coordinates");
    QL_REQUIRE(taperFraction > 0.0, "SabrParametricVolatility: taper fraction must be positive when calculating "
        "boundary strike coordinates");
    Real left = strikeCoordinate(marketStrikes.front(), forward, dimension);
    Real right = strikeCoordinate(marketStrikes.back(), forward, dimension);
    QL_REQUIRE(right > left, "SabrParametricVolatility: invalid or degenerate strike coordinate range");
    Real width = taperFraction * (right - left);
    return { left - width, right + width };
}

}

void SabrParametricVolatility::buildResidualSmiles() const {

    auto dimension = residualCorrection_->dimension;

    for (const auto& marketSmile : marketSmiles_) {
        auto tteUndKey = std::make_pair(marketSmile.timeToExpiry, marketSmile.underlyingLength);

        // Key string for error messages below.
        string keyStr = "tte = " + std::to_string(marketSmile.timeToExpiry);
        if (marketSmile.underlyingLength != Null<Real>())
            keyStr = "(" + keyStr + ", und_length = " + std::to_string(marketSmile.underlyingLength) + ")";

        // Build the strike coordinates according to the dimension specified in the residual correction.
        // We will taper the residuals to zero at the edges of the smile => extra 2 points here and in the residuls.
        vector<Real> strikeCoords;
        strikeCoords.reserve(marketSmile.strikes.size() + 2);
        auto [leftCoord, rightCoord] = boundaryCoordinates(marketSmile.strikes, marketSmile.forward, dimension);
        strikeCoords.push_back(leftCoord);
        for (Real strike : marketSmile.strikes)
            strikeCoords.push_back(strikeCoordinate(strike, marketSmile.forward, dimension));
        strikeCoords.push_back(rightCoord);

        // Calculate the residuals.
        auto residuals = calculateResiduals(marketSmile, tteUndKey, keyStr);

        // Build the residual smile and store it.
        residualSmiles_.emplace(tteUndKey, std::make_unique<ResidualSmile>(
            std::move(strikeCoords), std::move(residuals)));
    }
}

void SabrParametricVolatility::updateResidualSmiles() const {

    for (const auto& marketSmile : marketSmiles_) {
        auto tteUndKey = std::make_pair(marketSmile.timeToExpiry, marketSmile.underlyingLength);

        // Key string for error messages below.
        string keyStr = "tte = " + std::to_string(marketSmile.timeToExpiry);
        if (marketSmile.underlyingLength != Null<Real>())
            keyStr = "(" + keyStr + ", und_length = " + std::to_string(marketSmile.underlyingLength) + ")";

        // Make sure there is an existing residual smile to update.
        auto itResidualSmile = residualSmiles_.find(tteUndKey);
        QL_REQUIRE(itResidualSmile != residualSmiles_.end(),
            "SabrParametricVolatility: no residual smile found for " << keyStr << ".");

        // Calculate the residuals.
        auto residuals = calculateResiduals(marketSmile, tteUndKey, keyStr);

        // Update the residual smile.
        itResidualSmile->second->updateResiduals(residuals);
    }
}

Real SabrParametricVolatility::residualCorrection(Real timeToExpiry, Real underlyingLength,
    Real strike, Real forward) const
{
    QL_REQUIRE(timeToExpiry >= 0.0, "SabrParametricVolatility: residual correction requested at a "
        "negative time to expiry (" << timeToExpiry << ")");

    // If no residual correction, return 0.
    if (residualSmiles_.empty())
        return 0.0;

    // We should not get to here if underlyingLength != Null<Real>(), because residual correction is not yet supported
    // for structures with underlying lengths. However, we check it anyway. If / when we want to support residual
    // correction for structures with underlying lengths, we should separate out the residual correction logic into 
    // two separate methods, one where underlyingLength == Null<Real>() (1-D) and one where 
    // underlyingLength != Null<Real>() (2-D).
    QL_REQUIRE(underlyingLength == Null<Real>(), "SabrParametricVolatility: residual correction not yet supported "
        "for volatility structures with underlying lengths.");

    // If time to expiry is zero, return 0.0.
    if (close(timeToExpiry, 0.0))
        return 0.0;

    // Linear interpolation of error term in the time to expiry dimension, with flat extrapolation beyond last expiry.
    Real result;
    TteUndKey searchKey{ timeToExpiry, Null<Real>() };
    // right is first element in residualSmiles_ with time to expiry >= searchKey.timeToExpiry
    auto itRight = residualSmiles_.lower_bound(searchKey);
    if (itRight == residualSmiles_.begin()) {
        // timeToExpiry in [0, t_1] where t_1 is the first time to expiry in residualSmiles_.
        // Linearly interpolate the residuals between (0, 0) to (t_1, eps_1) for the value at timeToExpiry.
        Real rightEps = evaluateResidual(*itRight->second, strike, forward);
        result = (timeToExpiry / itRight->first.first) * rightEps;
    } else if (itRight == residualSmiles_.end()) {
        // timeToExpiry beyond the last expiry in residualSmiles_.
        // Flat extrapolation using the last residual smile.
        auto last = std::prev(residualSmiles_.end());
        result = evaluateResidual(*last->second, strike, forward);
    } else {
        // timeToExpiry in [t_i, t_{i+1}] where t_i and t_{i+1} are the time to expiries of the two residual smiles
        // surrounding timeToExpiry. Linearly interpolate the residuals between (t_i, eps_i) and (t_{i+1}, eps_{i+1})
        // for the value at timeToExpiry.
        auto itLeft = std::prev(itRight);
        Real t1 = itLeft->first.first;
        Real t2 = itRight->first.first;
        Real e1 = evaluateResidual(*itLeft->second, strike, forward);
        Real e2 = evaluateResidual(*itRight->second, strike, forward);
        result = e1 + (e2 - e1) * (timeToExpiry - t1) / (t2 - t1);
    }
    return result;
}

vector<Real> SabrParametricVolatility::calculateResiduals(const MarketSmile& marketSmile,
    const TteUndKey& tteUndKey, const string& keyStr) const
{
    // Calculate the SABR model volatilities for the market strikes, using the calibrated parameters.
    const auto& params = calibratedSabrParams_.at(tteUndKey);
    Real lognormalShift = getLognormalShift(marketSmile);
    auto modelVols = evaluateSabr(params, marketSmile.forward, marketSmile.timeToExpiry,
        lognormalShift, marketSmile.strikes);

    // Market volatilities. The conversion of market quotes to the preferred output quote type may have failed for 
    // this smile. In this case, we set the residuals to zero and return. Should probably improve this but there is 
    // likely an issue with the market data in this case.
    auto itQuotes = convertedMarketQuotes_.find(tteUndKey);
    if (itQuotes == convertedMarketQuotes_.end())
        return vector<Real>(marketSmile.strikes.size() + 2, 0.0);
    const auto& marketVols = itQuotes->second;

    // Sanity checks.
    QL_REQUIRE(modelVols.size() == marketSmile.strikes.size(), "SabrParametricVolatility: model vol size (" <<
        modelVols.size() << ") and strikes size (" << marketSmile.strikes.size() << ") do not match for " <<
        keyStr << ".");
    QL_REQUIRE(modelVols.size() == marketVols.size(), "SabrParametricVolatility: model vol size (" <<
        modelVols.size() << ") and market vol size (" << marketVols.size() << ") do not match for " <<
        keyStr << ".");

    // Build the residuals.
    vector<Real> residuals;
    residuals.reserve(marketSmile.strikes.size() + 2);
    residuals.push_back(0.0);
    for (Size i = 0; i < marketSmile.strikes.size(); ++i)
        residuals.push_back(marketVols[i] - modelVols[i]);
    residuals.push_back(0.0);

    return residuals;
}

Real SabrParametricVolatility::evaluateResidual(const ResidualSmile& residualSmile, Real strike, Real forward) const {
    Real strikeCoord = strikeCoordinate(strike, forward, residualCorrection_->dimension);
    return residualSmile.interpolation_(strikeCoord);
}

SabrParametricVolatility::ResidualSmile::ResidualSmile(vector<Real> strikeCoordinates, vector<Real> residuals)
    : strikeCoordinates_(std::move(strikeCoordinates)), residuals_(std::move(residuals)) {
    build();
}

void SabrParametricVolatility::ResidualSmile::build() {
    auto interp = ext::make_shared<QuantLib::FritschButlandCubic>(strikeCoordinates_.begin(),
        strikeCoordinates_.end(), residuals_.begin());
    interpolation_ = FlatExtrapolation(interp);
    interpolation_.enableExtrapolation();
}

void SabrParametricVolatility::ResidualSmile::updateResiduals(const vector<Real>& newResiduals) {
    QL_REQUIRE(newResiduals.size() == residuals_.size(), "SabrParametricVolatility: unexpected residual size change");
    std::copy(newResiduals.begin(), newResiduals.end(), residuals_.begin());
    interpolation_.update();
}

} // namespace QuantExt
