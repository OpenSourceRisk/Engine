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
#include <ql/patterns/lazyobject.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/exactbachelierimpliedvolatility.hpp>

namespace QuantExt {

class ParametricVolatility : public QuantLib::LazyObject {
public:
    enum class MarketModelType { Black76 };
    enum class MarketQuoteType { Price, NormalVolatility, ShiftedLognormalVolatility };

    struct MarketPoint {
        QuantLib::Real timeToExpiry;
        QuantLib::Real strike;
        QuantLib::Handle<QuantLib::Quote> forward;
        QuantLib::Real underlyingLength;
        QuantLib::Handle<QuantLib::Quote> marketQuote;
        QuantLib::Real lognormalShift;
        QuantLib::Option::Type optionType;
    };

    ParametricVolatility(const std::vector<MarketPoint> marketPoints, const MarketModelType marketModelType,
                         const MarketQuoteType inputMarketQuoteType,
                         const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve);

    QuantLib::Real convert(const MarketPoint& p, const MarketQuoteType inputMarketQuoteType,
                           const MarketQuoteType outputMarketQuoteType,
                           const QuantLib::Real outputLognormalShift) const;

    virtual QuantLib::Real evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real strike,
                                    const QuantLib::Real underlyingLength, const MarketQuoteType outputMarketQuoteType,
                                    const QuantLib::Real outputLognormalShift) const = 0;

protected:
    std::vector<MarketPoint> marketPoints_;
    MarketModelType marketModelType_;
    MarketQuoteType inputMarketQuoteType_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

class SabrParametricVolatility final : public ParametricVolatility {
public:
    enum class ModelVariant {
        Hagan2002Lognormal,
        Hagan2002Normal,
        Hagan2002NormalZeroBeta,
        Antonov2015FreeBoundaryNormal
    };

    //! modelParameters are given by (tte, underlyingLen) as a vector of parameter value and whether this value is fixed
    SabrParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketPoint> marketPoints,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters);

    QuantLib::Real evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real strike,
                            const QuantLib::Real underlyingLength, const MarketQuoteType outputMarketQuoteType,
                            const QuantLib::Real outputLognormalShift) const override;

private:
    void performCalculations() const override;
    ModelVariant modelVariant_;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters_;
};

} // namespace QuantExt
