/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file mclgmfraengine.hpp
    \brief MC LGM engine for FRA instrument
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <qle/instruments/forwardrateagreement.hpp>

namespace QuantExt {

class McLgmFraEngine : public GenericEngine<ForwardRateAgreement::arguments, ForwardRateAgreement::results>,
                        public McMultiLegBaseEngine {
public:
    McLgmFraEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const SequenceType calibrationPathGenerator,
                   const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
                   const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
                   const LsmBasisSystem::PolynomialType polynomType,
                   const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                   const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                   const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                   const std::vector<Date> simulationDates = std::vector<Date>(),
                   const std::vector<Size> externalModelIndices = std::vector<Size>(),
                   const bool minimalObsDate = true, const bool regressionOnExerciseOnly = false)
        : GenericEngine<ForwardRateAgreement::arguments, ForwardRateAgreement::results>(),
          McMultiLegBaseEngine(Handle<CrossAssetModel>(boost::make_shared<CrossAssetModel>(
                                   std::vector<boost::shared_ptr<IrModel>>(1, model),
                                   std::vector<boost::shared_ptr<FxBsParametrization>>())),
                               calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                               calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                               {discountCurve}, simulationDates, externalModelIndices, minimalObsDate,
                               regressionOnExerciseOnly) {
        registerWith(model);
    }

    void calculate() const override;
};

} // namespace QuantExt
