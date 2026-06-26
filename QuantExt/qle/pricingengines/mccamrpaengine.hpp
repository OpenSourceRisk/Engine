/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

/*! \file mccamrpaengine.hpp
    \brief MC CAM engine for rpa
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <qle/instruments/riskparticipationagreement.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

class McCamRpaEngine : public McMultiLegBaseEngine, public CurrencySwap::engine {
public:
    McCamCurrencySwapEngine(
        const Handle<CrossAssetModel>& model, const Handle<DefaultProbabilityTermStructure>& creditCurve,
        const Handle<Quote>& recoveryRate, const Size maxGapDays, const Size maxDiscretisationPoints,
        const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const McRegressionModel::RegressorModel regressorModel = McRegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const McRegressionModel::VarGroupMode regressionVarGroupMode = McRegressionModel::VarGroupMode::Global);

    void calculate() const override;
    const Handle<CrossAssetModel>& model() const { return model_; }

private:
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    Handle<Quote> recoveryRate_;
    Size maxGapDays_;
    Size maxDiscretisationPoints_;
};

} // namespace QuantExt
