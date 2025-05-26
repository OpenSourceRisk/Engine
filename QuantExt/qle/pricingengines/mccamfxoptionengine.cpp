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

#include <qle/pricingengines/mccamfxoptionengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>

namespace QuantExt {

using namespace QuantLib;

McCamFxOptionEngineBase::McCamFxOptionEngineBase(
    const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
    const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
    const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
    const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
    const SobolBrownianGenerator::Ordering ordering, const SobolRsg::DirectionIntegers directionIntegers,
    const std::vector<Handle<YieldTermStructure>>& discountCurves, const std::vector<Date>& simulationDates,
    const std::vector<Date>& stickyCloseOutDates, const std::vector<Size>& externalModelIndices,
    const bool minimalObsDate, const RegressorModel regressorModel, const Real regressionVarianceCutoff,
    const bool recalibrateOnStickyCloseOutDates, const bool reevaluateExerciseInStickyRun)
    : McMultiLegBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                           calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                           discountCurves, simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate,
                           regressorModel, regressionVarianceCutoff, recalibrateOnStickyCloseOutDates,
                           reevaluateExerciseInStickyRun),
      domesticCcy_(domesticCcy), foreignCcy_(foreignCcy), npvCcy_(npvCcy) {}

void McCamFxOptionEngineBase::setupLegs() const {
    QL_REQUIRE(payoff_, "McCamFxOptionEngineBase: payoff has unexpected type");

    if (payDate_ == Null<Date>())
        payDate_ = exercise_->dates().back();

    Real w = payoff_->optionType() == Option::Call ? 1.0 : -1.0;
    Leg domesticLeg{QuantLib::ext::make_shared<SimpleCashFlow>(-w * payoff_->strike(), payDate_)};
    Leg foreignLeg{QuantLib::ext::make_shared<SimpleCashFlow>(w, payDate_)};

    leg_ = {domesticLeg, foreignLeg};
    currency_ = {domesticCcy_, foreignCcy_};
    payer_ = {false, false};
}

void McCamFxOptionEngineBase::calculateFxOptionBase() const {
    QL_REQUIRE(exercise_->type() == Exercise::European, "McCamFxOptionEngineBase: not an European option");
    QL_REQUIRE(!exercise_->dates().empty(), "McCamFxOptionEngineBase: exercise dates are empty");

    exerciseIntoIncludeSameDayFlows_ = true;

    McMultiLegBaseEngine::calculate();

    // convert base ccy result from McMultiLegbaseEngine to desired npv currency
    Real fxSpot = 1.0;
    Size npvCcyIndex = model_->ccyIndex(npvCcy_);
    if (npvCcyIndex > 0)
        fxSpot = model_->fxbs(npvCcyIndex - 1)->fxSpotToday()->value();

    fxOptionResultValue_ = resultValue_ / fxSpot;
    fxOptionUnderlyingNpv_ = resultUnderlyingNpv_ / fxSpot;
}

void McCamFxOptionEngine::calculate() const {

    payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    exercise_ = arguments_.exercise;
    optionSettlement_ = Settlement::Physical;
    payDate_ = Date(); // will be set in calculateFxOptionBase()

    McCamFxOptionEngineBase::setupLegs();
    McCamFxOptionEngineBase::calculateFxOptionBase();

    results_.value = fxOptionResultValue_;
    results_.additionalResults["underlyingNpv"] = fxOptionUnderlyingNpv_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
}

void McCamFxEuropeanForwardOptionEngine::calculate() const {

    payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    exercise_ = arguments_.exercise;
    payDate_ = arguments_.paymentDate; // might be null, in which case it will be set in calculateFxOptionBase()
    optionSettlement_ = Settlement::Physical;

    McCamFxOptionEngineBase::setupLegs();
    McCamFxOptionEngineBase::calculateFxOptionBase();

    results_.value = fxOptionResultValue_;
    results_.additionalResults["underlyingNpv"] = fxOptionUnderlyingNpv_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
}

void McCamFxEuropeanCSOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->dates().size() == 1,
               "McCamFxEuropeanCSOptionEngine::calculate(): expected 1 exercise date, got "
                   << arguments_.exercise->dates().size());

    Date today = Settings::instance().evaluationDate();

    payDate_ = arguments_.paymentDate; // always given
    cashSettlementDates_ = {payDate_};

    if (arguments_.exercise->dates().back() < today) {

        // handle option expiry in the past, this means we have a deterministic payoff

        Real payoffAmount = 0.0;
        if (arguments_.automaticExercise) {
            QL_REQUIRE(arguments_.underlying, "Expect a valid underlying index when exercise is automatic.");
            payoffAmount = (*arguments_.payoff)(arguments_.underlying->fixing(arguments_.exercise->dates().back()));
        } else if (arguments_.exercised) {
            QL_REQUIRE(arguments_.priceAtExercise != Null<Real>(), "Expect a valid price at exercise when option "
                                                                       << "has been manually exercised.");
            payoffAmount = (*arguments_.payoff)(arguments_.priceAtExercise);
        }

        leg_ = {Leg{QuantLib::ext::make_shared<SimpleCashFlow>(payoffAmount, payDate_)}};
        currency_ = {domesticCcy_};
        payer_ = {false};

        McCamFxOptionEngineBase::calculateFxOptionBase();

    } else {

        // handle option expiry in the future (or today)

        payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        exercise_ = arguments_.exercise;
        optionSettlement_ = Settlement::Cash;

        McCamFxOptionEngineBase::setupLegs();
        McCamFxOptionEngineBase::calculateFxOptionBase();
    }

    // set results

    results_.value = fxOptionResultValue_;
    results_.additionalResults["underlyingNpv"] = fxOptionUnderlyingNpv_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
}

} // namespace QuantExt
