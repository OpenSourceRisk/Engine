/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file mccamfxoptionengine.hpp
    \brief MC CAM engine for FX Option instrument
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>

#include <ql/instruments/vanillaoption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>

namespace QuantExt {

class McCamFxOptionEngine : public McMultiLegBaseEngine, public VanillaOption::engine {
public:
    // this engine works as QuantLib::AnalyticsEuropeanEngine, so that it is compatible with the ORE FxOption trade
    // builder, i.e. for a call the foreign amount (1.0) is received, domestic (strike*1.0) is paid
    McCamFxOptionEngine(
        const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
        const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const bool regressionOnExerciseOnly = false);

    void calculate() const override;
    const Handle<CrossAssetModel>& model() const { return model_; }

private:
    const Currency domesticCcy_, foreignCcy_, npvCcy_;
};

} // namespace QuantExt
