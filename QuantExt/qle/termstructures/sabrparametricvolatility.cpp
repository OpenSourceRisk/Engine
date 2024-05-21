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

#include <ql/experimental/math/laplaceinterpolation.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/termstructures/volatility/sabr.hpp>

namespace QuantExt {

using namespace QuantLib;

SabrParametricVolatility::SabrParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters,
    const Size maxCalibrationAttempts, const Real exitEarlyErrorThreshold, const Real maxAcceptableError)
    : ParametricVolatility(marketSmiles, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)),
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

std::vector<Real> SabrParametricVolatility::getGuess(const std::vector<std::pair<Real, bool>>& params,
                                                     const std::vector<Real>& randomSeq, const Real forward,
                                                     const Real lognormalShift) const {
    std::vector<Real> result(4);
    for (Size i = 0, j = 0; i < 4; ++i) {
        if (params[i].second) {
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
                result[2] = eps1 + randomSeq[j] * 5.0;
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

std::vector<std::pair<Real, bool>> SabrParametricVolatility::defaultModelParameters() const {
    switch (modelVariant_) {
    case ModelVariant::Hagan2002Lognormal:
        return {{0.0050, false}, {0.8, false}, {0.30, false}, {0.0, false}};
    case ModelVariant::Hagan2002Normal:
        return {{0.0050, false}, {0.8, false}, {0.30, false}, {0.0, false}};
    case ModelVariant::Hagan2002NormalZeroBeta:
        return {{0.0050, false}, {0.0, true}, {0.30, false}, {0.0, false}};
    case ModelVariant::Antonov2015FreeBoundaryNormal:
        return {{0.0050, false}, {0.0, true}, {0.30, false}, {0.0, false}};
    case ModelVariant::KienitzLawsonSwaynePde:
        return {{0.0050, false}, {0.8, false}, {0.30, false}, {0.0, false}};
    case ModelVariant::FlochKennedy:
        return {{0.0050, false}, {0.8, false}, {0.30, false}, {0.0, false}};
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

std::tuple<std::vector<Real>, Real, Size>
SabrParametricVolatility::calibrateModelParameters(const MarketSmile& marketSmile,
                                                   const std::vector<std::pair<Real, bool>>& params) const {

    // determine the number of free parameters

    Size noFreeParams = 0;
    for (auto const& p : params)
        if (!p.second)
            ++noFreeParams;

    // if there are no free parameters, we just pass back the fixed parameters as the result

    if (noFreeParams == 0) {
        std::vector<Real> resultParams;
        for (auto const& p : params)
            resultParams.push_back(p.first);
        return std::make_tuple(resultParams, 0.0, 0);
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
        Real refQuote_;
        std::function<std::vector<Real>(const std::vector<Real>&, const Real, const Real, const Real,
                                        const std::vector<Real>&)>
            evalSabr_;
        std::vector<std::pair<Real, bool>> params_;
        std::vector<Real> invParams_;
        std::function<std::vector<Real>(const std::vector<Real>&)> direct_;
        std::function<std::vector<Real>(const std::vector<Real>&)> inverse_;

        std::vector<Real> evalSabr(const Array& x) const {
            std::vector<Real> params(4);
            for (Size i = 0, j = 0; i < params_.size(); ++i) {
                if (params_[i].second)
                    params[i] = invParams_[i];
                else
                    params[i] = x[j++];
            }
            params = direct_(params);
            return evalSabr_(params, forward_, timeToExpiry_, lognormalShift_, strikes_);
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
    t.lognormalShift_ = marketSmile.lognormalShift;

    t.evalSabr_ = [this](const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                         const Real lognormalShift, const std::vector<Real>& strikes) {
        return evaluateSabr(params, forward, timeToExpiry, lognormalShift, strikes);
    };

    t.params_ = params;
    for (Size i = 0; i < params.size(); ++i)
        t.invParams_.push_back(params[i].first);
    t.invParams_ = inverse(t.invParams_, marketSmile.forward, marketSmile.lognormalShift);

    t.inverse_ = [this, &marketSmile](const std::vector<Real>& y) {
        return inverse(y, marketSmile.forward, marketSmile.lognormalShift);
    };
    t.direct_ = [this, &marketSmile](const std::vector<Real>& x) {
        return direct(x, marketSmile.forward, marketSmile.lognormalShift);
    };

    t.strikes_ = marketSmile.strikes;
    for (Size i = 0; i < marketSmile.marketQuotes.size(); ++i) {
        t.marketQuotes_.push_back(convert(
            marketSmile.marketQuotes[i], inputMarketQuoteType_, marketSmile.lognormalShift,
            marketSmile.optionTypes.empty() ? boost::none : boost::optional<Option::Type>(marketSmile.optionTypes[i]),
            marketSmile.timeToExpiry, marketSmile.strikes[i], marketSmile.forward, preferredOutputQuoteType(),
            marketSmile.lognormalShift, boost::none));
    }
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
                if (!params[i].second) {
                    guess[j++] = t.invParams_[i];
                }
            }
        } else {
            // subsequent attempts use randomized guess
            auto g = inverse(getGuess(params, haltonRsg.nextSequence().value, t.forward_, t.lognormalShift_),
                             t.forward_, t.lognormalShift_);
            for (Size i = 0, j = 0; i < g.size(); ++i) {
                if (!params[i].second) {
                    guess[j++] = g[i];
                }
            }
        }

        Problem problem(t, noConstraint, guess);
        try {
            lm.minimize(problem, endCriteria);
        } catch (...) {
            continue;
        }

        Real thisError = problem.functionValue();
        if (thisError < bestError) {
            bestError = thisError;
            for (Size i = 0, j = 0; i < bestResult.size(); ++i) {
                if (params[i].second)
                    bestResult[i] = t.invParams_[i];
                else
                    bestResult[i] = problem.currentValue()[j++];
            }
            bestResult = direct(bestResult, t.forward_, t.lognormalShift_);
        }

        if (bestError < exitEarlyErrorThreshold_)
            break;
    }

    QL_REQUIRE(bestError < QL_MAX_REAL, "internal: all calibrations failed");

    return std::make_tuple(bestResult, bestError, ++attempt);
}

namespace {
void laplaceInterpolationWithErrorHandling(Matrix& m, const std::vector<Real>& x, const std::vector<Real>& y) {
    try {
        laplaceInterpolation(m, x, y, 1E-6, 100);
    } catch (const std::exception& e) {
        QL_FAIL("Error during laplaceInterpolation() in SabrParametricVolatility: "
                << e.what() << ", this might be related to the numerical parameters relTol, maxIterMult. Contact dev.");
    }
}
} // namespace

void SabrParametricVolatility::calculate() {

    // if no model parameters are given, we provide the default ones

    if (modelParameters_.empty()) {
        for (auto const& s : marketSmiles_) {
            modelParameters_[std::make_pair(s.timeToExpiry, s.underlyingLength)] = defaultModelParameters();
        }
    }

    // clear stored data

    calibratedSabrParams_.clear();
    lognormalShifts_.clear();
    calibrationErrors_.clear();

    // for each market smile calibrate the SABR variant

    for (auto const& s : marketSmiles_) {
        auto key = std::make_pair(s.timeToExpiry, s.underlyingLength);
        auto param = modelParameters_.find(key);
        QL_REQUIRE(param != modelParameters_.end(),
                   "SabrParametricVolatility::performCalculations(): no model parameter given for ("
                       << s.timeToExpiry << ", " << s.underlyingLength
                       << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                          "covered by the given model parameters.");
        try {
            auto [params, error, noOfAttempts] = calibrateModelParameters(s, param->second);
            if (error < maxAcceptableError_)
                calibratedSabrParams_[key] = params;
            calibrationErrors_[key] = error;
            noOfAttempts_[key] = noOfAttempts;
        } catch (const std::exception& e) {
            // all calibration failed -> do not populate params, but interpolate them below
        }
        lognormalShifts_[key] = s.lognormalShift;
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
            beta_(i, j) = std::max(beta_(i, j), 0.0);
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

    alphaInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), alpha_));
    betaInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), beta_));
    nuInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), nu_));
    rhoInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), rho_));
    lognormalShiftInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        timeToExpiriesForInterpolation_.begin(), timeToExpiriesForInterpolation_.end(),
        underlyingLengthsForInterpolation_.begin(), underlyingLengthsForInterpolation_.end(), lognormalShift_));

    alphaInterpolation_.enableExtrapolation();
    betaInterpolation_.enableExtrapolation();
    nuInterpolation_.enableExtrapolation();
    rhoInterpolation_.enableExtrapolation();
    lognormalShiftInterpolation_.enableExtrapolation();
}

Real SabrParametricVolatility::evaluate(const Real timeToExpiry, const Real underlyingLength, const Real strike,
                                        const Real forward, const MarketQuoteType outputMarketQuoteType,
                                        const Real outputLognormalShift,
                                        const boost::optional<QuantLib::Option::Type> outputOptionType) const {

    Real alpha = alphaInterpolation_(timeToExpiry, underlyingLength);
    Real beta = betaInterpolation_(timeToExpiry, underlyingLength);
    Real nu = nuInterpolation_(timeToExpiry, underlyingLength);
    Real rho = rhoInterpolation_(timeToExpiry, underlyingLength);
    Real lognormalShift = lognormalShiftInterpolation_(timeToExpiry, underlyingLength);

    Real result = evaluateSabr({alpha, beta, nu, rho}, forward, timeToExpiry, lognormalShift, {strike}).front();
    return convert(result, preferredOutputQuoteType(), lognormalShift, boost::none, timeToExpiry, strike, forward,
                   outputMarketQuoteType, outputLognormalShift == Null<Real>() ? lognormalShift : outputLognormalShift,
                   outputOptionType);
}

} // namespace QuantExt
