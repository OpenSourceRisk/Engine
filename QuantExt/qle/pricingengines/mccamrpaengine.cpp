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

#include <qle/pricingengines/mccamrpaengine.hpp>

namespace QuantExt {

using namespace QuantLib;

McCamRpaEngine::McCamRpaEngine(
    const Handle<CrossAssetModel>& model, const Handle<DefaultProbabilityTermStructure>& crediCurve,
    const Handle<Quote>& recoveryRate, const Size maxGapDays, const Size maxDiscretisationPoints,
    const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator, const Size calibrationSamples,
    const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    const SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Date>& stickyCloseOutDates,
    const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const McRegressionModel::RegressorModel regressorModel, const Real regressionVarianceCutoff,
    const bool recalibrateOnStickyCloseOutDates, const bool reevaluateExerciseInStickyRun,
    const Size cfOnCpnMaxSimTimes, const Period& cfOnCpnAddSimTimesCutoff, const Size regressionMaxSimTimesIr,
    const Size regressionMaxSimTimesFx, const Size regressionMaxSimTimesEq,
    const McRegressionModel::VarGroupMode regressionVarGroupMode)
    : McMultiLegBaseEngine(
          model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
          pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, discountCurves, simulationDates,
          stickyCloseOutDates, externalModelIndices, minimalObsDate, regressorModel, regressionVarianceCutoff,
          recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun, cfOnCpnMaxSimTimes, cfOnCpnAddSimTimesCutoff,
          regressionMaxSimTimesIr, regressionMaxSimTimesFx, regressionMaxSimTimesEq, regressionVarGroupMode),
      creditCurve_(creditCurve), recoveryRate_(recoveryRate), maxGapDays_(maxGapDays),
      maxDiscretisationPoints_(maxDiscretisationPoints) {
    registerWith(model_);
    registerWith(creditCurve_);
    registerWith(recoveryRate_);
}

void McCamCurrencySwapEngine::calculate() const {

    std::vector<Currency> underlyingCcys;
    std::for_each(arguments_.underlyingCcys.begin(), arguments_.underlyingCcys.end(),
                  [&underlyingCcys]() { underlyingCcys.push_back(parseCurrency(c)); });

    leg_ = arguments_.underlying;
    currency_ = arguments_.underlyingCcys;
    payer_ = arguments_.underlyingPayer;
    exercise_ = arguments_.exercise;

    defaultCurve_ = creditCurve_;
    recoveryRate_ = recoveryRate_;

    Date today = Settings::instance().evaluationDate();

    rpaDiscretizationDates_ = RiskParticipationAgreementBaseEngine::buildDiscretisationGrid(
        today, arguments_.protectionStart, arguments_.protectionEnd, arguments_.underlying, maxGapDays_,
        maxDiscretizsationPoints_);

    // base engine extensions:
    // - nakedOption = true/false flag
    // - option premium leg
    // - fee leg

    McMultiLegBaseEngine::calculate();

    results_.value = resultValue_;
    results_.additionalResults["amcCalculator"] = amcCalculator();

} // calculate

} // namespace QuantExt
