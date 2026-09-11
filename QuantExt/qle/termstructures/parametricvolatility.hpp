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
    enum class ParameterCalibration { Fixed, Calibrated, Implied };

    struct MarketSmile {
        QuantLib::Real timeToExpiry;
        // not mandatory, used e.g. for swaptions, but not cap / floors
        QuantLib::Real underlyingLength = Null<Real>();
        QuantLib::Real forward;
        QuantLib::Real lognormalShift = 0.0;
        // if empty, otm strikes are used
        std::vector<QuantLib::Option::Type> optionTypes;
        std::vector<QuantLib::Real> strikes;
        std::vector<QuantLib::Real> marketQuotes;
        // If provided, use this to read off the ATM volatility for the calibration.
        QuantLib::ext::optional<QuantLib::Real> fwdForAtmVol = QuantLib::ext::nullopt;
    };

    /*! Allow for a residual correction to be applied to the model output smile where for each slice the residuals are:
        \f[
            \epsilon_i = \epsilon(K_i) = \sigma_{\text{market}}(K_i) - \sigma_{\text{model}}(K_i)
        \f]
    */
    struct ResidualCorrection {
        enum class Dimension {
            // Use (K_i, \epsilon_i)
            AbsoluteStrike,
            // Use (K_i - F, \epsilon_i)
            StrikeMinusForward,
            // Use (K_i / F, \epsilon_i)
            StrikeOverForward
            // Potentially more over time ...
        };
        Dimension dimension = Dimension::AbsoluteStrike;
        // Over time, we may want to configure elements here e.g. interpolation type, extrapolation type, etc.
    };

    virtual ~ParametricVolatility() {}
    ParametricVolatility(const std::vector<MarketSmile>& marketSmiles,
        MarketModelType marketModelType,
        MarketQuoteType inputMarketQuoteType,
        QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        QuantLib::ext::optional<ResidualCorrection> residualCorrection = QuantLib::ext::nullopt);

    // if outputOptionType is none, otm strike is used (and call for atm)
    Real convert(const Real inputQuote, const MarketQuoteType inputMarketQuoteType,
                 const QuantLib::Real inputLognormalShift,
                 const QuantLib::ext::optional<QuantLib::Option::Type> inputOptionType, const QuantLib::Real timeToExpiry,
                 const QuantLib::Real strike, const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
                 const QuantLib::Real outputLognormalShift,
                 const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const;

    /* - if outputOptionType is none, otm strike is used
       - the outputMarketQuoteType must always be given and can be different from input market quote type
       - if outputLognormalShift is null, the model lognormal shift is used (only for
         outputMarketQuoteType == ShiftedLognormalVolatility) */
    virtual QuantLib::Real
    evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
             const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
             const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
             const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const = 0;

protected:
    std::vector<MarketSmile> marketSmiles_;
    MarketModelType marketModelType_;
    MarketQuoteType inputMarketQuoteType_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::ext::optional<ResidualCorrection> residualCorrection_;
};

/* strict weak ordering on MarketPoint by lexicographic comparison, i.e.
   (timeToExpiry1, underlyingLength1) < (timeToExpiry2, underlyingLength2)*/
bool operator<(const ParametricVolatility::MarketSmile& s, const ParametricVolatility::MarketSmile& t);

QuantExt::ParametricVolatility::ParameterCalibration parseParametricSmileParameterCalibration(const std::string& s);
std::ostream& operator<<(std::ostream& os, QuantExt::ParametricVolatility::ParameterCalibration type);

using PVRCDimension = QuantExt::ParametricVolatility::ResidualCorrection::Dimension;
PVRCDimension parseParametricVolResidualCorrectionDimension(const std::string& s);
std::ostream& operator<<(std::ostream& os, PVRCDimension dimension);

} // namespace QuantExt
