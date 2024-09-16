/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/mcmultilegoptionengine.hpp>

namespace QuantExt {

McMultiLegOptionEngine::McMultiLegOptionEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    const SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices, const bool minObsDate,
    const RegressorModel regressorModel, const Real regressionVarianceCutoff)
    : McMultiLegBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                           calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                           discountCurves, simulationDates, externalModelIndices, minObsDate, regressorModel,
                           regressionVarianceCutoff) {
    registerWith(model_);
    for (auto& h : discountCurves_) {
        registerWith(h);
    }
}

McMultiLegOptionEngine::McMultiLegOptionEngine(
    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    const SobolRsg::DirectionIntegers directionIntegers, const Handle<YieldTermStructure>& discountCurve,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const RegressorModel regressorModel, const Real regressionVarianceCutoff)
    : McMultiLegOptionEngine(Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                                 std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                                 std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
                             calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                             calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                             {discountCurve}, simulationDates, externalModelIndices, minimalObsDate, regressorModel,
                             regressionVarianceCutoff) {}

void McMultiLegOptionEngine::calculate() const {

    leg_ = arguments_.legs;
    currency_ = arguments_.currency;
    payer_ = arguments_.payer;
    exercise_ = arguments_.exercise;
    optionSettlement_ = arguments_.settlementType;

    McMultiLegBaseEngine::calculate();

    // convert base ccy result from McMultiLegbaseEngine to desired npv currency
    Real fxSpot = 1.0;
    Size npvCcyIndex = model_->ccyIndex(currency_.front());
    if (npvCcyIndex > 0)
        fxSpot = model_->fxbs(npvCcyIndex - 1)->fxSpotToday()->value();
    results_.value = resultValue_ / fxSpot;
    results_.additionalResults["underlyingNpv"] = resultUnderlyingNpv_ / fxSpot;
    results_.additionalResults["amcCalculator"] = amcCalculator();
} // calculate

} // namespace QuantExt
