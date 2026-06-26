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

namespace ore::data {

class McCamRpaEngine : public QuantExt::McMultiLegBaseEngine, public QuantExt::RiskParticipationAgreement::engine {
public:
    McCamRpaEngine(const Handle<QuantExt::CrossAssetModel>& model, const std::vector<Currency>& currencies,
                   const Currency& npvCcy,
                   const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
                   const QuantLib::Handle<QuantLib::Quote>& recoveryRate, const Size maxGapDays,
                   const Size maxDiscretisationPoints, const QuantExt::SequenceType calibrationPathGenerator,
                   const QuantExt::SequenceType pricingPathGenerator, const QuantLib::Size calibrationSamples,
                   const QuantLib::Size pricingSamples, const QuantLib::Size calibrationSeed,
                   const QuantLib::Size pricingSeed, const QuantLib::Size polynomOrder,
                   const QuantLib::LsmBasisSystem::PolynomialType polynomType,
                   const QuantLib::SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                   const QuantLib::SobolRsg::DirectionIntegers directionIntegers = QuantLib::SobolRsg::JoeKuoD7,
                   const std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>>& discountCurves = {},
                   const std::vector<QuantLib::Date>& simulationDates = std::vector<QuantLib::Date>(),
                   const std::vector<QuantLib::Date>& stickyCloseOutDates = std::vector<QuantLib::Date>(),
                   const std::vector<QuantLib::Size>& externalModelIndices = std::vector<QuantLib::Size>(),
                   const bool minimalObsDate = true,
                   const QuantExt::McRegressionModel::RegressorModel regressorModel =
                       QuantExt::McRegressionModel::RegressorModel::Simple,
                   const QuantLib::Real regressionVarianceCutoff = Null<Real>(),
                   const bool recalibrateOnStickyCloseOutDates = false,
                   const bool reevaluateExerciseInStickyRun = false, const QuantLib::Size cfOnCpnMaxSimTimes = 1,
                   const QuantLib::Period& cfOnCpnAddSimTimesCutoff = QuantLib::Period(),
                   const QuantLib::Size regressionMaxSimTimesIr = 0, const QuantLib::Size regressionMaxSimTimesFx = 0,
                   const QuantLib::Size regressionMaxSimTimesEq = 0,
                   const QuantExt::McRegressionModel::VarGroupMode regressionVarGroupMode =
                       QuantExt::McRegressionModel::VarGroupMode::Global);

    void calculate() const override;
    const QuantLib::Handle<QuantExt::CrossAssetModel>& model() const { return model_; }

private:
    std::vector<Currency> currencies_;
    Currency npvCcy_;
    QuantLib::Size maxGapDays_;
    QuantLib::Size maxDiscretisationPoints_;
};

} // namespace ore::data
