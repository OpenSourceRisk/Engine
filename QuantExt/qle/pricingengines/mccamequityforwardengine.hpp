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

/*! \file mccamequityforwardengine.hpp
    \brief MC CAM engine for Equity forward instrument
*/

#pragma once

#include <qle/indexes/equityindex.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/instruments/equityforward.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>

#include <qle/instruments/equityforward.hpp>

namespace QuantExt {

class McCamEquityForwardEngine : public McMultiLegBaseEngine, public EquityForward::engine {
public:
    McCamEquityForwardEngine(const Handle<EquityIndex2>& equityIndex, const Handle<CrossAssetModel>& model,
                             const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
                             const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed,
                             const Size pricingSeed, const Size polynomOrder,
                             const LsmBasisSystem::PolynomialType polynomType,
                             const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                             const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                             const std::vector<Date>& simulationDates = std::vector<Date>(),
                             const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
                             const std::vector<Size>& externalModelIndices = std::vector<Size>(),
                             const bool minimalObsDate = true,
                             const RegressorModel regressorModel = RegressorModel::Simple,
                             const Real regressionVarianceCutoff = Null<Real>(),
                             const bool recalibrateOnStickyCloseOutDates = false,
                             const bool reevaluateExerciseInStickyRun = false,
                             const Size cfOnCpnMaxSimTimes = 1,
                             const Period& cfOnCpnAddSimTimesCutoff = Period(),
                             const Size regressionMaxSimTimesIr = 0,
                             const Size regressionMaxSimTimesFx = 0,
                             const Size regressionMaxSimTimesEq = 0,
                             const VarGroupMode regressionVarGroupMode = VarGroupMode::Global);

    const Handle<CrossAssetModel>& model() const { return model_; }

private:
    void calculate() const override;

    Handle<EquityIndex2> equityIndex_;
};

} // namespace QuantExt
