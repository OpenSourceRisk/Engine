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

#include <qle/termstructures/parametricvolatility.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

using namespace QuantLib;

ParametricVolatility::ParametricVolatility(const std::vector<MarketPoint> marketPoints,
                                           const MarketModelType marketModelType,
                                           const MarketQuoteType inputMarketQuoteType,
                                           const Handle<YieldTermStructure> discountCurve)
    : marketPoints_(std::move(marketPoints)), marketModelType_(marketModelType),
      inputMarketQuoteType_(inputMarketQuoteType), discountCurve_(std::move(discountCurve)) {
    for (auto const& p : marketPoints_)
        registerWith(p.marketQuote);
    registerWith(discountCurve);
}

Real ParametricVolatility::convert(const MarketPoint& p, const MarketQuoteType inputMarketQuoteType,
                                   const MarketQuoteType outputMarketQuoteType, const Real outputLognormalShift) const {

    // check if there is nothing to convert

    if (inputMarketQuoteType == outputMarketQuoteType && QuantLib::close_enough(p.lognormalShift, outputLognormalShift))
        return p.marketQuote->value();

    // otherwise compute the forward premium

    Real forwardPremium;
    switch (marketModelType_) {
    case MarketModelType::Black76:
        if (inputMarketQuoteType_ == MarketQuoteType::Price) {
            forwardPremium = p.marketQuote->value() / discountCurve_->discount(p.timeToExpiry);
        } else if (inputMarketQuoteType_ == MarketQuoteType::NormalVolatility) {
            forwardPremium = bachelierBlackFormula(p.optionType, p.strike, p.forward->value(),
                                                   p.marketQuote->value() * std::sqrt(p.timeToExpiry));
        } else if (inputMarketQuoteType_ == MarketQuoteType::ShiftedLognormalVolatility) {
            forwardPremium = blackFormula(p.optionType, p.strike, p.forward->value(),
                                          p.marketQuote->value() * std::sqrt(p.timeToExpiry), p.lognormalShift);
        } else {
            QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                    << static_cast<int>(inputMarketQuoteType_) << ") not handled. Internal error, contact dev.");
        }
        break;
    default:
        QL_FAIL("ParametricVolatility::convert(): MarketModelType (" << static_cast<int>(marketModelType_)
                                                                     << ") not handled. Internal error, contact dev.");
    }

    // ... and compute the result from the forward premium

    switch (marketModelType_) {
    case MarketModelType::Black76:
        if (outputMarketQuoteType == MarketQuoteType::Price) {
            return forwardPremium * discountCurve_->discount(p.timeToExpiry);
        } else if (outputMarketQuoteType == MarketQuoteType::NormalVolatility) {
            return exactBachelierImpliedVolatility(p.optionType, p.strike, p.forward->value(), p.timeToExpiry,
                                                   forwardPremium);
        } else if (outputMarketQuoteType == MarketQuoteType::ShiftedLognormalVolatility) {
            return blackFormulaImpliedStdDev(p.optionType, p.strike, p.forward->value(), forwardPremium, 1.0,
                                             outputLognormalShift);
        } else {
            QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                    << static_cast<int>(outputMarketQuoteType) << ") not handled. Internal error, contact dev.");
        }
        break;
    default:
        QL_FAIL("ParametricVolatility::convert(): MarketModelType (" << static_cast<int>(marketModelType_)
                                                                     << ") not handled. Internal error, contact dev.");
    }
};

bool operator<(const ParametricVolatility::MarketPoint& p, const ParametricVolatility::MarketPoint& q) {
    if (p.timeToExpiry < q.timeToExpiry)
        return true;
    if (p.timeToExpiry > q.timeToExpiry)
        return false;
    if (p.strike < q.strike)
        return true;
    if (p.strike > q.strike)
        return false;
    return false;
}

SabrParametricVolatility::SabrParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketPoint> marketPoints, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters)
    : ParametricVolatility(marketPoints, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)) {}

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
    default:
        QL_FAIL("SabrParametricVolatility::preferredOutputQuoteType(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

std::vector<Real> SabrParametricVolatility::direct(const std::vector<Real>& x, const Real lognormalShift) const {
    std::vector<Real> y(4);
    // beta as in QuantLib::SABRSpecs
    y[1] = std::fabs(x[1]) < std::sqrt(-std::log(eps1)) ? std::exp(-(x[1] * x[1])) : eps1;
    // max 0.02 normal vol equivalent
    Real fbeta = std::pow(forward + lognormalShift, y[1]);
    y[0] =
        (std::fabs(x[0]) < std::sqrt(-std::log(fbeta * eps1 / 0.02)) ? 0.02 * std::exp(-(x[0] * x[0])) / fbeta : eps1);
    // nu max 5.0
    y[2] = (std::fabs(x[2]) < std::sqrt(-std::log(2.0 * eps1)) ? 2.0 * std::exp(-(x[2] * x[2])) : eps1);
    // rho as in QuantLib::SABRSpecs
    y[3] = std::fabs(x[3]) < 2.5 * M_PI ? eps2 * std::sin(x[3]) : eps2 * (x[3] > 0.0 ? 1.0 : (-1.0));
    return y;
}

std::vector<Real> SabrParametricVolatility::inverse(const std::vector<ReaL>& y, const Real lognormalShift) const {
    std::vector<Real> x(4);
    x[1] = std::sqrt(-std::log(y[1]));
    Real fbeta = std::pow(forward + lognormalShift, y[1]);
    x[0] = std::sqrt(-std::log(y[0] * fbeta / 0.02));
    x[2] = std::sqrt(-std::log(y[2] / 2.0));
    x[3] = std::asin(y[3] / eps2());
    return x;
}

std::vector<Real> SabrParametricVolatility::evaluateSabr(const std::vector<Real>& params,
                                                         const std::vector<Real>& strikes) const {
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
        return default : QL_FAIL("SabrParametricVolatility::preferredOutputQuoteType(): model variant ("
                                 << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

std::vector<Real>
SabrParametricVolatility::calibrateModelParameters(const Real timeToExpiry, const Real underlyingLength,
                                                   const std::set<MarketPoint>& marketPoints,
                                                   const std::vector<std::pair<Real, bool>>& params) const {

    // check that all market points have the same lognormal shift and forward

    Real smileLognormalShift = Null<Real>(), smileForward = Null<Real>();
    bool smileLognormalShiftSet = false, smileForwardSet = false;
    for (auto const& p : marketPoints) {
        if (!smileLognormalShiftSet) {
            smileLognormalShift = p.lognormalShift;
            smileLognormalShiftSet = true;
        } else {
            QL_REQUIRE(p.lognormalShift == smileLognormalShift,
                       "SabrParametricVolatility::calibrateModelParameters("
                           << timeToExpiry << "," << underlyingLength
                           << "): got different lognormal shifts for this smile which is not allowed: "
                           << p.lognormalShift << ", " << smileLognormalShift);
        }
        if (!smileForwardSet) {
            smileForward = p.lognormalShift;
            smileForwardSet = true;
        } else {
            QL_REQUIRE(p.lognormalShift == smileForward,
                       "SabrParametricVolatility::calibrateModelParameters("
                           << timeToExpiry << "," << underlyingLength
                           << "): got different lognormal shifts for this smile which is not allowed: "
                           << p.lognormalShift << ", " << smileForward);
        }
    }

    // check the number of free parameters vs. the number of given market points

    // define the target function

    struct TargetFunction : public QuantLib::CostFunction {
        std::vector<Real> strikes_;
        std::vector<Real> marketQuotes_;
        std::function<std::vector<Real>(const std::vector<Real>&)> evalSabr_;
        std::function<Real(const Size i, const Real y)> transformInverse_;

        Array values(const Array& x) const override {
            Array result(strikes_.size());
            std::vector<Real> params(4);
            for (Size i = 0, j = 0; i < params.size(); ++i) {
                if (params_[i].second)
                    params[i] = params_[i];
                else
                    params[i] = transformInverse_(i, x[j++]);
            }
            auto sabr = evalSabr_(params, strikes_);
            for (Size i = 0; i < strikes_.size(); ++i) {
                return (marketQuotes_[i] - sabr[i]) / marketQuotes_[i];
            }
        }
    };

    /* build the target function and populate the members:
       strikes, marketQuotes  : the latter are converted to the preferred output quote type of the SABR
       evalSabr               : the function to produce SABR values of the quote type for a given vector of strikes */

    TargetFunction t;

    t.evalSabr_ = [this](const std::vector<Real>& params, const std::vector<Real>& strikes) {
        return evaluateSabr(params, strikes, forward);
    };

    t.transformInverse_ = [this](const Size i, const Real y) { return transformInverse(i, y); }

    for (auto const& p : marketPoints) {
        t.strikes.push_back(p.strike);
        t.marketQuotes_.push_back(convert(p, inputMarketQuoteType_, preferredOutputQuoteType(), smileLognormalShift));
    }

    // perform the calibration and return the result

    return {};
}

void SabrParametricVolatility::performCalculations() const {

    // collect the available (tte, underlyingLength) pairs

    std::map<std::pair<Real, Real>, std::set<MarketPoint>> smiles;
    for (auto const& p : marketPoints_) {
        smiles[std::make_pair(p.timeToExpiry, p.underlyingLength)].insert(p);
    }

    // for each (tte, underlyingLength) pair calibrate the SABR variant

    calibratedSabrParams_.clear();
    for (auto const& [s, m] : smiles) {
        auto param = modelParameters_.find(s);
        QL_REQUIRE(param != modelParameters_.end(),
                   "SabrParametricVolatility::performCalculations(): no model parameter given for ("
                       << s.first << ", " << s.second
                       << "). All (timeToExpiry, underlyingLength) pairs that are given as market points must be "
                          "covered by the given model parameters.");
        calibratedSabrParams_[s] = calibrateModelParameters(s.first, s.second, m, param->second);
    }
}

Real SabrParametricVolatility::evaluate(const Real timeToExpiry, const Real strike, const Real underlyingLength,
                                        const MarketQuoteType outputMarketQuoteType,
                                        const Real outputLognormalShift) const {
    calculate();

    return 0.0;
}

} // namespace QuantExt
