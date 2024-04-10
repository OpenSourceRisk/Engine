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

McCamFxOptionEngine::McCamFxOptionEngine(
    const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
    const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
    const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
    const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
    const SobolBrownianGenerator::Ordering ordering, const SobolRsg::DirectionIntegers directionIntegers,
    const std::vector<Handle<YieldTermStructure>>& discountCurves, const std::vector<Date>& simulationDates,
    const std::vector<Size>& externalModelIndices, const bool minimalObsDate, const RegressorModel regressorModel,
    const Real regressionVarianceCutoff)
    : McMultiLegBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                           calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                           discountCurves, simulationDates, externalModelIndices, minimalObsDate, regressorModel,
                           regressionVarianceCutoff),
      domesticCcy_(domesticCcy), foreignCcy_(foreignCcy), npvCcy_(npvCcy) {
    registerWith(model_);
    for (auto const& h : discountCurves)
        registerWith(h);
}

void McCamFxOptionEngine::calculate() const {

    auto payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff, "McCamFxOptionEngine: non-striked payoff given");
    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "McCamFxOptionEngine: not an European option");
    QL_REQUIRE(!arguments_.exercise->dates().empty(), "McCamFxOptionEngine: exercise dates are empty");

    Date payDate = arguments_.exercise->dates().front() + 1;

    Real w = payoff->optionType() == Option::Call ? 1.0 : -1.0;
    Leg domesticLeg{QuantLib::ext::make_shared<SimpleCashFlow>(-w * payoff->strike(), payDate)};
    Leg foreignLeg{QuantLib::ext::make_shared<SimpleCashFlow>(w, payDate)};

    leg_ = {domesticLeg, foreignLeg};
    currency_ = {domesticCcy_, foreignCcy_};
    payer_ = {false, false};
    exercise_ = arguments_.exercise;
    optionSettlement_ = Settlement::Cash;

    McMultiLegBaseEngine::calculate();

    // convert base ccy result from McMultiLegbaseEngine to desired npv currency
    Real fxSpot = 1.0;
    Size npvCcyIndex = model_->ccyIndex(npvCcy_);
    if (npvCcyIndex > 0)
        fxSpot = model_->fxbs(npvCcyIndex - 1)->fxSpotToday()->value();
    results_.value = resultValue_ / fxSpot;
    results_.additionalResults["underlyingNpv"] = resultUnderlyingNpv_ / fxSpot;
    results_.additionalResults["amcCalculator"] = amcCalculator();
} // calculate

} // namespace QuantExt
