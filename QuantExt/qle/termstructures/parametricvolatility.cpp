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

SabrParametricVolatility::SabrParametricVolatility(
    const ModelVariant modelVariant, const std::vector<MarketPoint> marketPoints, const MarketModelType marketModelType,
    const MarketQuoteType inputMarketQuoteType, const Handle<YieldTermStructure> discountCurve,
    const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters)
    : ParametricVolatility(marketPoints, marketModelType, inputMarketQuoteType, discountCurve),
      modelVariant_(modelVariant), modelParameters_(std::move(modelParameters)) {}

void SabrParametricVolatility::performCalculations() const {

    // 1 collect the available (tte, underlyingLength) pairs

    // 2 for each (tte, underlyingLength) pair calibrate the SABR variant

    // 2.1 convert the market quote to the preferred output quote type of the SABR variant

    // 2.2 perform the calibration

    // 2.3 store the calibrated model parameters
}

Real SabrParametricVolatility::evaluate(const Real timeToExpiry, const Real strike, const Real underlyingLength,
                                        const MarketQuoteType outputMarketQuoteType,
                                        const Real outputLognormalShift) const {
    calculate();

    return 0.0;
}

} // namespace QuantExt
