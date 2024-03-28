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

/*! \file swaptionsabrcube.hpp
    \brief SABR Swaption volatility cube
    \ingroup termstructures
*/

#ifndef quantext_swaption_sabrcube_h
#define quantext_swaption_sabrcube_h

#include <qle/termstructures/sabrparametricvolatility.hpp>

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>

namespace QuantExt {
using namespace QuantLib;

class ParametricVolatilitySmileSection : public QuantLib::SmileSection {
public:
    ParametricVolatilitySmileSection(const Real optionTime, const Real swapLength, const Real atmLevel,
                                     const boost::shared_ptr<ParametricVolatility> parametricVolatility,
                                     const ParametricVolatility::MarketQuoteType outputMarketQuoteType);
    Real minStrike() const override { return -QL_MAX_REAL; }
    Real maxStrike() const override { return QL_MAX_REAL; }
    Real atmLevel() const override;

private:
    Volatility volatilityImpl(Rate strike) const override;
    Real optionTime_, swapLength_, atmLevel_;
    boost::shared_ptr<ParametricVolatility> parametricVolatility_;
    ParametricVolatility::MarketQuoteType outputMarketQuoteType_;
};

class SwaptionSabrCube : public SwaptionVolatilityCube {
public:
    SwaptionSabrCube(const Handle<SwaptionVolatilityStructure>& atmVolStructure,
                     const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors,
                     const std::vector<Spread>& strikeSpreads,
                     const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                     const boost::shared_ptr<SwapIndex>& swapIndexBase,
                     const boost::shared_ptr<SwapIndex>& shortSwapIndexBase,
                     const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                     const boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none,
                     const std::vector<std::pair<Real, bool>>& initialModelParameters = {},
                     const QuantLib::Size maxCalibrationAttempts = 10,
                     const QuantLib::Real exitEarlyErrorThreshold = 0.005,
                     const QuantLib::Real maxAcceptableError = 0.05);
    void performCalculations() const override;
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;

private:
    mutable std::map<std::pair<Real, Real>, boost::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable boost::shared_ptr<ParametricVolatility> parametricVolatility_;
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    boost::optional<QuantLib::VolatilityType> outputVolatilityType_;
    std::vector<std::pair<Real, bool>> initialModelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
};

} // namespace QuantExt

#endif
