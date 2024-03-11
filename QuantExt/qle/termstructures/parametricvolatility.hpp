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

    struct MarketSmile {
        QuantLib::Real timeToExpiry;
        // not mandatory, used e.g. for swaptions, but not cap / floors
        QuantLib::Real underlyingLength = Null<Real>();
        QuantLib::Handle<QuantLib::Quote> forward;
        // this is also used as output lognormal shift for lnvol - type model variants
        QuantLib::Real lognormalShift = 0.0;
        // if empty, otm strikes are used
        std::vector<QuantLib::Option::Type> optionTypes;
        std::vector<QuantLib::Real> strikes;
        std::vector<QuantLib::Handle<QuantLib::Quote>> marketQuotes;
    };

    ParametricVolatility(const std::vector<MarketSmile> marketSmiles, const MarketModelType marketModelType,
                         const MarketQuoteType inputMarketQuoteType,
                         const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve);

    // if outputOptionType is empty, otm strikes are used (call for atm)
    std::vector<QuantLib::Real> convert(const MarketSmile& p, const MarketQuoteType outputMarketQuoteType,
                                        const QuantLib::Real outputLognormalShift,
                                        const std::vector<QuantLib::Option::Type>& outputOptionTypes = {}) const;
    // if outputOptionType is none, otm strike is used
    virtual QuantLib::Real
    evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
             const MarketQuoteType outputMarketQuoteType, const QuantLib::Real outputLognormalShift,
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

class SabrParametricVolatility final : public ParametricVolatility {
public:
    enum class ModelVariant {
        Hagan2002Lognormal,
        Hagan2002Normal,
        Hagan2002NormalZeroBeta,
        Antonov2015FreeBoundaryNormal,
        KienitzLawsonSwaynePde,
        FlochKennedy
    };

    /*! modelParameters are given by (tte, underlyingLen) as a vector of parameter values and
        whether the values are fixed */
    SabrParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters);

    QuantLib::Real
    evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
             const MarketQuoteType outputMarketQuoteType, const QuantLib::Real outputLognormalShift,
             const boost::optional<QuantLib::Option::Type> outputOptionType = boost::none) const override;

private:
    static constexpr double eps1 = .0000001;
    static constexpr double eps2 = .9999;

    void performCalculations() const override;

    ParametricVolatility::MarketQuoteType preferredOutputQuoteType() const;
    std::vector<Real> direct(const std::vector<Real>& x, const Real forward, const Real lognormalShift) const;
    std::vector<Real> inverse(const std::vector<Real>& y, const Real forward, const Real lognormalShift) const;
    std::vector<Real> evaluateSabr(const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                                   const Real lognormalShift, const std::vector<Real>& strikes) const;
    std::vector<Real> calibrateModelParameters(const MarketSmile& marketSmile,
                                               const std::vector<std::pair<Real, bool>>& params) const;

    ModelVariant modelVariant_;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters_;

    mutable std::map<std::pair<Real, Real>, std::vector<Real>> calibratedSabrParams_;
};

} // namespace QuantExt
