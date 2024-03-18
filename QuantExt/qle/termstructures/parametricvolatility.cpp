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
      inputMarketQuoteType_(inputMarketQuoteType), discountCurve_(std::move(discountCurve)) {
    for (auto const& s : marketSmiles_) {
        registerWith(s.forward);
        for (auto const& p : s.marketQuotes) {
            registerWith(p);
        }
    }
    registerWith(discountCurve);
}

std::vector<Real> ParametricVolatility::convert(const MarketSmile& s, const MarketQuoteType outputMarketQuoteType,
                                                const Real outputLognormalShift,
                                                const std::vector<QuantLib::Option::Type>& outputOptionTypes) const {

    std::vector<Real> result(s.marketQuotes.size(), 0.0);

    for (Size i = 0; i < s.marketQuotes.size(); ++i) {

        // determine the input and output option types

        QuantLib::Option::Type otmOptionType =
            s.strikes[i] < s.forward->value() ? QuantLib::Option::Put : QuantLib::Option::Call;
        QuantLib::Option::Type inputOptionType = s.optionTypes.empty() ? otmOptionType : s.optionTypes[i];
        QuantLib::Option::Type outputOptionType = outputOptionTypes.empty() ? otmOptionType : outputOptionTypes[i];

        // check if there is nothing to convert

        if (inputMarketQuoteType_ == outputMarketQuoteType &&
            QuantLib::close_enough(s.lognormalShift, outputLognormalShift) && inputOptionType == outputOptionType) {
            result[i] = s.marketQuotes[i]->value();
            continue;
        }

        // otherwise compute the forward premium

        Real forwardPremium;
        switch (marketModelType_) {
        case MarketModelType::Black76:
            if (inputMarketQuoteType_ == MarketQuoteType::Price) {
                forwardPremium = s.marketQuotes[i]->value() / discountCurve_->discount(s.timeToExpiry);
            } else if (inputMarketQuoteType_ == MarketQuoteType::NormalVolatility) {
                forwardPremium = bachelierBlackFormula(inputOptionType, s.strikes[i], s.forward->value(),
                                                       s.marketQuotes[i]->value() * std::sqrt(s.timeToExpiry));
            } else if (inputMarketQuoteType_ == MarketQuoteType::ShiftedLognormalVolatility) {
                if (s.strikes[i] < -s.lognormalShift)
                    forwardPremium =
                        inputOptionType == QuantLib::Option::Call ? s.forward->value() - s.strikes[i] : 0.0;
                else
                    forwardPremium =
                        blackFormula(inputOptionType, s.strikes[i], s.forward->value(),
                                     s.marketQuotes[i]->value() * std::sqrt(s.timeToExpiry), s.lognormalShift);
            } else {
                QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                        << static_cast<int>(inputMarketQuoteType_) << ") not handled. Internal error, contact dev.");
            }
            break;
        default:
            QL_FAIL("ParametricVolatility::convert(): MarketModelType ("
                    << static_cast<int>(marketModelType_) << ") not handled. Internal error, contact dev.");
        }

        // ... and compute the result from the forward premium

        switch (marketModelType_) {
        case MarketModelType::Black76:
            if (outputMarketQuoteType == MarketQuoteType::Price) {
                result[i] = forwardPremium * discountCurve_->discount(s.timeToExpiry);
            } else if (outputMarketQuoteType == MarketQuoteType::NormalVolatility) {
                result[i] = exactBachelierImpliedVolatility(outputOptionType, s.strikes[i], s.forward->value(),
                                                            s.timeToExpiry, forwardPremium);
            } else if (outputMarketQuoteType == MarketQuoteType::ShiftedLognormalVolatility) {
                if (s.strikes[i] > -outputLognormalShift)
                    result[i] = blackFormulaImpliedStdDev(outputOptionType, s.strikes[i], s.forward->value(),
                                                          forwardPremium, 1.0, outputLognormalShift);
            } else {
                QL_FAIL("ParametricVolatility::convert(): MarketQuoteType ("
                        << static_cast<int>(outputMarketQuoteType) << ") not handled. Internal error, contact dev.");
            }
            break;
        default:
            QL_FAIL("ParametricVolatility::convert(): MarketModelType ("
                    << static_cast<int>(marketModelType_) << ") not handled. Internal error, contact dev.");
        }
    }

    return result;
};

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

