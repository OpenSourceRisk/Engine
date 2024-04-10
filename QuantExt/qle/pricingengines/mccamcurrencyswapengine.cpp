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

#include <qle/pricingengines/mccamcurrencyswapengine.hpp>

namespace QuantExt {

using namespace QuantLib;

McCamCurrencySwapEngine::McCamCurrencySwapEngine(
    const Handle<CrossAssetModel>& model, const std::vector<Currency>& currencies, const Currency& npvCcy,
    const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator, const Size calibrationSamples,
    const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    const SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const RegressorModel regressorModel, const Real regressionVarianceCutoff)
    : McMultiLegBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                           calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                           discountCurves, simulationDates, externalModelIndices, minimalObsDate, regressorModel,
                           regressionVarianceCutoff),
      currencies_(currencies), npvCcy_(npvCcy) {
    registerWith(model_);
    for (auto const& h : discountCurves)
        registerWith(h);
}

void McCamCurrencySwapEngine::calculate() const {

    leg_ = arguments_.legs;
    currency_ = arguments_.currency;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    // convert base ccy result from McMultiLegbaseEngine to desired npv currency
    Real fxSpot = 1.0;
    Size npvCcyIndex = model_->ccyIndex(npvCcy_);
    if (npvCcyIndex > 0)
        fxSpot = model_->fxbs(npvCcyIndex - 1)->fxSpotToday()->value();
    results_.value = resultValue_ / fxSpot;
    results_.additionalResults["amcCalculator"] = amcCalculator();
} // calculate

} // namespace QuantExt
