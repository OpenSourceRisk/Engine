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

#include <qle/pricingengines/mccamequityforwardengine.hpp>

#include <qle/cashflows/equitycashflow.hpp>

namespace QuantExt {

using namespace QuantLib;

McCamEquityForwardEngine::McCamEquityForwardEngine(
    const Handle<EquityIndex2>& equityIndex, const Handle<CrossAssetModel>& model,
    const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator, const Size calibrationSamples,
    const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    const SobolRsg::DirectionIntegers directionIntegers, const std::vector<Date>& simulationDates,
    const std::vector<Date>& stickyCloseOutDates, const std::vector<Size>& externalModelIndices,
    const bool minimalObsDate, const RegressorModel regressorModel, const Real regressionVarianceCutoff,
    const bool recalibrateOnStickyCloseOutDates, const bool reevaluateExerciseInStickyRun,
    const Size cfOnCpnMaxSimTimes, const Period& cfOnCpnAddSimTimesCutoff,
    const Size regressionMaxSimTimesIr, const Size regressionMaxSimTimesFx, const Size regressionMaxSimTimesEq,
    const VarGroupMode regressionVarGroupMode)
    : McMultiLegBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                           calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, {},
                           simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate, regressorModel,
                           regressionVarianceCutoff, recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun,
                           cfOnCpnMaxSimTimes, cfOnCpnAddSimTimesCutoff,
                           regressionMaxSimTimesIr, regressionMaxSimTimesFx, regressionMaxSimTimesEq,
                           regressionVarGroupMode),
      equityIndex_(equityIndex) {}

void McCamEquityForwardEngine::calculate() const {
    Leg eqLeg{QuantLib::ext::make_shared<EquityCashFlow>(arguments_.payDate, arguments_.quantity,
                                                         arguments_.maturityDate, *equityIndex_)};
    Leg strikeLeg{
        QuantLib::ext::make_shared<SimpleCashFlow>(arguments_.quantity * arguments_.strike, arguments_.payDate)};

    leg_ = {eqLeg, strikeLeg};
    currency_ = {model_->ir(0)->currency(), model_->ir(0)->currency()};
    payer_ = {arguments_.longShort != Position::Long, arguments_.longShort == Position::Long};

    McMultiLegBaseEngine::calculate();

    results_.value = resultValue_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
}

} // namespace QuantExt
