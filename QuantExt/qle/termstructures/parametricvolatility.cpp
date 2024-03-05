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
    if(p.timeToExpiry < q.timeToExpiry)
        return true;
    if(p.timeToExpiry > q.timeToExpiry)
        return false;
    if(p.strike < q.strike)
        return true;
    if(p.strike > q.strike)
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
    default:
        QL_FAIL("SabrParametricVolatility::preferredOutputQuoteType(): model variant ("
                << static_cast<int>(modelVariant_) << ") not handled.");
    }
}

std::vector<Real>
SabrParametricVolatility::calibrateModelParameters(const Real timeToExpiry, const Real underlyingLength,
                                                   const std::set<MarketPoint>& marketPoints,
                                                   const std::vector<std::pair<Real, bool>>& params) const {

    // check that all market points have the same lognormal shift

    Real smileLognormalShift = Null<Real>();
    bool smileLognormalShiftSet = false;
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

        // build set of (strike, market quote), the latter are converted to the preferred output quote type of the SABR
        // variant here as well

        std::set<std::pair<Real, Real>> marketQuotes;

        for (auto const& p : marketPoints) {
            marketQuotes.insert(std::make_pair(
                p.strike, convert(p, inputMarketQuoteType_, preferredOutputQuoteType(), smileLognormalShift)));
        }

        // perform the calibration
    }

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
    for (auto const & [ s, m ] : smiles) {
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
