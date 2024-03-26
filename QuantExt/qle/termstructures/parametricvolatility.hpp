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

/*! \file parametricvolatility.hpp
    \brief cross-asset, generic volatility structure
*/

#pragma once

#include <ql/handle.hpp>
#include <ql/option.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

using QuantLib::Null;
using QuantLib::Real;

class ParametricVolatility {
public:
    enum class MarketModelType { Black76 };
    enum class MarketQuoteType { Price, NormalVolatility, ShiftedLognormalVolatility };

    struct MarketSmile {
        QuantLib::Real timeToExpiry;
        // not mandatory, used e.g. for swaptions, but not cap / floors
        QuantLib::Real underlyingLength = Null<Real>();
        QuantLib::Real forward;
        // this is also used as output lognormal shift for lnvol - type model variants
        QuantLib::Real lognormalShift = 0.0;
        // if empty, otm strikes are used
        std::vector<QuantLib::Option::Type> optionTypes;
        std::vector<QuantLib::Real> strikes;
        std::vector<QuantLib::Real> marketQuotes;
    };

    virtual ~ParametricVolatility() {}
    ParametricVolatility(const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
                         const MarketQuoteType inputMarketQuoteType,
                         const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve);

    // if outputOptionType is none, otm strike is used (and call for atm)
    Real convert(const Real inputQuote, const MarketQuoteType inputMarketQuoteType,
                 const QuantLib::Real inputLognormalShift, const boost::optional<QuantLib::Option::Type> inputOptionType,
                 const QuantLib::Real timeToExpiry, const QuantLib::Real strike, const QuantLib::Real forward,
                 const MarketQuoteType outputMarketQuoteType, const QuantLib::Real outputLognormalShift,
                 const boost::optional<QuantLib::Option::Type> outputOptionType = boost::none) const;

    /* - if outputOptionType is none, otm strike is used
       - the outputMarketQuoteType must always be given and can be different from input market quote type
       - if outputLognormalShift is null, the input data = model's lognormal shift is used (only for
         outputMarketQuoteType = ShiftedLognormalVolatility) */
    virtual QuantLib::Real
    evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
             const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
             const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
             const boost::optional<QuantLib::Option::Type> outputOptionType = boost::none) const = 0;

protected:
    std::vector<MarketSmile> marketSmiles_;
    MarketModelType marketModelType_;
    MarketQuoteType inputMarketQuoteType_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

/* strict weak ordering on MarketPoint by lexicographic comparison, i.e.
   (timeToExpiry1, underlyingLength1) < (timeToExpiry2, underlyingLength2)*/
bool operator<(const ParametricVolatility::MarketSmile& s, const ParametricVolatility::MarketSmile& t);

} // namespace QuantExt
