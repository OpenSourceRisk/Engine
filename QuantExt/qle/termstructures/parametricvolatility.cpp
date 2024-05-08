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

#include <qle/models/exactbachelierimpliedvolatility.hpp>
#include <qle/termstructures/parametricvolatility.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

using namespace QuantLib;

ParametricVolatility::ParametricVolatility(const std::vector<MarketSmile> marketSmiles,
                                           const MarketModelType marketModelType,
                                           const MarketQuoteType inputMarketQuoteType,
                                           const Handle<YieldTermStructure> discountCurve)
    : marketSmiles_(std::move(marketSmiles)), marketModelType_(marketModelType),
      inputMarketQuoteType_(inputMarketQuoteType), discountCurve_(std::move(discountCurve)) {}

Real ParametricVolatility::convert(const Real inputQuote, const MarketQuoteType inputMarketQuoteType,
                                   const Real inputLognormalShift,
                                   const boost::optional<Option::Type> inputOptionTypeOpt, const Real timeToExpiry,
                                   const Real strike, const Real forward, const MarketQuoteType outputMarketQuoteType,
                                   const Real outputLognormalShift,
                                   const boost::optional<Option::Type> outputOptionTypeOpt) const {

    // determine the input and output option types

    Option::Type otmOptionType = strike < forward ? Option::Put : Option::Call;
    Option::Type inputOptionType = inputOptionTypeOpt ? *inputOptionTypeOpt : otmOptionType;
    Option::Type outputOptionType = outputOptionTypeOpt ? *outputOptionTypeOpt : otmOptionType;

    // check if there is nothing to convert

    if (inputMarketQuoteType == outputMarketQuoteType && close_enough(inputLognormalShift, outputLognormalShift) &&
        inputOptionType == outputOptionType) {
        return inputQuote;
    }

    // otherwise compute the forward premium

    Real forwardPremium;
    switch (marketModelType_) {
    case MarketModelType::Black76:
        if (inputMarketQuoteType == MarketQuoteType::Price) {
            forwardPremium = inputQuote / (discountCurve_.empty() ? 1.0 : discountCurve_->discount(timeToExpiry));
        } else if (inputMarketQuoteType == MarketQuoteType::NormalVolatility) {
            forwardPremium =
                bachelierBlackFormula(inputOptionType, strike, forward, inputQuote * std::sqrt(timeToExpiry));
        } else if (inputMarketQuoteType == MarketQuoteType::ShiftedLognormalVolatility) {
            if (strike < -inputLognormalShift)
                forwardPremium = inputOptionType == Option::Call ? forward - strike : 0.0;
            else {
                forwardPremium = blackFormula(inputOptionType, strike, forward, inputQuote * std::sqrt(timeToExpiry),
                                              1.0, inputLognormalShift);
            }
        } else {
            QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                    << static_cast<int>(inputMarketQuoteType) << ") not handled. Internal error, contact dev.");
        }
        break;
    default:
        QL_FAIL("ParametricVolatility::convert(): MarketModelType (" << static_cast<int>(marketModelType_)
                                                                     << ") not handled. Internal error, contact dev.");
    }

    // ... and compute the result from the forward premium

    switch (marketModelType_) {
    case MarketModelType::Black76: {
        if (outputMarketQuoteType == MarketQuoteType::Price) {
            return forwardPremium * (discountCurve_.empty() ? 1.0 : discountCurve_->discount(timeToExpiry));
        } else if (outputMarketQuoteType == MarketQuoteType::NormalVolatility) {
            return exactBachelierImpliedVolatility(outputOptionType, strike, forward, timeToExpiry, forwardPremium);
        } else if (outputMarketQuoteType == MarketQuoteType::ShiftedLognormalVolatility) {
            if (strike > -outputLognormalShift)
                return blackFormulaImpliedStdDev(outputOptionType, strike, forward, forwardPremium, 1.0,
                                                 outputLognormalShift);
            else
                return 0.0;
        } else {
            QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                    << static_cast<int>(outputMarketQuoteType) << ") not handled. Internal error, contact dev.");
        }
        break;
    }
    default:
        QL_FAIL("ParametricVolatility::convert(): MarketModelType (" << static_cast<int>(marketModelType_)
                                                                     << ") not handled. Internal error, contact dev.");
    }
}

bool operator<(const ParametricVolatility::MarketSmile& s, const ParametricVolatility::MarketSmile& t) {
    if (s.timeToExpiry < t.timeToExpiry)
        return true;
    if (s.timeToExpiry > t.timeToExpiry)
        return false;
    if (s.underlyingLength < t.underlyingLength)
        return true;
    if (s.underlyingLength > t.underlyingLength)
        return false;
    return false;
}

} // namespace QuantExt
