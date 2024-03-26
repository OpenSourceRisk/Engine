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
    virtual Real minStrike() const override { return -QL_MAX_REAL; }
    virtual Real maxStrike() const override { return QL_MAX_REAL; }
    virtual Real atmLevel() const override { return 0.0; }

private:
    virtual Volatility volatilityImpl(Rate strike) const override { return 0.0; }
};

class SwaptionSabrCube : public SwaptionVolatilityCube {
public:
    SwaptionSabrCube(const Handle<SwaptionVolatilityStructure>& atmVolStructure,
                     const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors,
                     const std::vector<Spread>& strikeSpreads,
                     const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                     const boost::shared_ptr<SwapIndex>& swapIndexBase,
                     const boost::shared_ptr<SwapIndex>& shortSwapIndexBase,
                     QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                     boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none);
    void performCalculations() const override;
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;

private:
    mutable std::map<std::pair<Real, Real>, boost::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable boost::shared_ptr<ParametricVolatility> parametricVolatility_;
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    boost::optional<QuantLib::VolatilityType> outputVolatilityType_;
};

} // namespace QuantExt

#endif
