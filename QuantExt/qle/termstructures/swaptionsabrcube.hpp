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

#include <qle/termstructures/parametricvolatilitysmilesection.hpp>
#include <qle/termstructures/sabrparametricvolatility.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/optional.hpp>
namespace QuantExt {
using namespace QuantLib;

class SwaptionSabrCube : public SwaptionVolatilityCube {
public:
    SwaptionSabrCube(const Handle<SwaptionVolatilityStructure>& atmVolStructure,
                     const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors,
                     const std::vector<Period>& atmOptionTenors, const std::vector<Period>& atmSwapLengths,
                     const std::vector<Spread>& strikeSpreads,
                     const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                     const QuantLib::ext::shared_ptr<SwapIndex>& swapIndexBase,
                     const QuantLib::ext::shared_ptr<SwapIndex>& shortSwapIndexBase,
                     const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                     const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType = QuantLib::ext::nullopt,
                     const std::map<std::pair<Period, Period>,
                                    std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
                         initialModelParameters = {},
                     const std::vector<Real>& outputShift = {}, const std::vector<Real>& modelShift = {},
                     const QuantLib::Size maxCalibrationAttempts = 10,
                     const QuantLib::Real exitEarlyErrorThreshold = 0.005,
                     const QuantLib::Real maxAcceptableError = 0.05);
    void performCalculations() const override;
    QuantLib::ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;

    QuantLib::ext::shared_ptr<ParametricVolatility> parametricVolatility() const;

    VolatilityType volatilityType() const override;

private:
    Real shiftImpl(Time optionTime, Time swapLength) const override;

    mutable std::map<std::pair<Real, Real>, QuantLib::ext::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable QuantLib::ext::shared_ptr<ParametricVolatility> parametricVolatility_;
    mutable Interpolation outputShiftInt_;
    mutable std::vector<Real> outputShiftX_, outputShiftY_;
    std::vector<Period> atmOptionTenors_, atmSwapTenors_;
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType_;
    std::map<std::pair<QuantLib::Period, QuantLib::Period>,
             std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>
        initialModelParameters_;
    std::vector<Real> outputShift_;
    std::vector<Real> modelShift_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
};

} // namespace QuantExt

#endif
