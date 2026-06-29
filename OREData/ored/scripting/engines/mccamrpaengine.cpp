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

#include <ored/scripting/engines/mccamrpaengine.hpp>
#include <ored/scripting/engines/riskparticipationagreementbaseengine.hpp>

#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore::data {

McCamRpaEngine::McCamRpaEngine(
    const Handle<CrossAssetModel>& model, const std::vector<Currency>& currencies, const Currency& npvCcy,
    const Handle<DefaultProbabilityTermStructure>& creditCurve, const Handle<Quote>& recoveryRate,
    const Size maxGapDays, const Size maxDiscretisationPoints, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
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
      currencies_(currencies), npvCcy_(npvCcy), creditCurve_(creditCurve), recoveryRate_(recoveryRate),
      maxGapDays_(maxGapDays), maxDiscretisationPoints_(maxDiscretisationPoints) {
    registerWith(model_);
    registerWith(creditCurve_);
    registerWith(recoveryRate_);
}

void McCamRpaEngine::calculate() const {

    currency_.clear();
    rpaProtectionFeeCurrency_.clear();

    std::for_each(arguments_.underlyingCcys.begin(), arguments_.underlyingCcys.end(),
                  [this](auto const& c) { currency_.push_back(ore::data::parseCurrency(c)); });
    std::for_each(arguments_.protectionFeeCcys.begin(), arguments_.protectionFeeCcys.end(),
                  [this](auto const& c) { rpaProtectionFeeCurrency_.push_back(ore::data::parseCurrency(c)); });

    leg_ = arguments_.underlying;
    payer_ = arguments_.underlyingPayer;
    exercise_ = arguments_.exercise;
    nakedOption_ = arguments_.nakedOption ? 1.0 : -1.0;
    exerciseLong_ = arguments_.exerciseIsLong ? 1.0 : -1.0;
    optionPremium_ = arguments_.premium;

    rpaProtectionFee_ = arguments_.protectionFee;
    rpaParticipationRate_ = arguments_.participationRate;
    rpaSettlesAccrual_ = arguments_.settlesAccrual;
    rpaProtectionFeePayer_ = arguments_.protectionFeePayer;

    rpaCreditCurve_ = creditCurve_;
    rpaRecoveryRate_ = arguments_.fixedRecoveryRate == Null<Real>()
                           ? recoveryRate_
                           : Handle<Quote>(ext::make_shared<SimpleQuote>(arguments_.fixedRecoveryRate));

    Date today = Settings::instance().evaluationDate();

    rpaDiscretizationDates_ = RiskParticipationAgreementBaseEngine::buildDiscretisationGrid(
        today, arguments_.protectionStart, arguments_.protectionEnd, arguments_.underlying, maxGapDays_,
        maxDiscretisationPoints_);

    McMultiLegBaseEngine::calculate();

    // convert base ccy result from McMultiLegbaseEngine to desired npv currency
    Real fxSpot = 1.0;
    Size npvCcyIndex = model_->ccyIndex(npvCcy_);
    if (npvCcyIndex > 0)
        fxSpot = model_->fxModel(npvCcyIndex - 1)->fxSpotToday()->value();
    results_.value = resultValue_ / fxSpot;

    // set amc calculator
    results_.additionalResults["amcCalculator"] = amcCalculator();

} // calculate

} // namespace ore::data
