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

/*! \file mclgmswaptionengine.hpp
    \brief MC LGM swaption engines
    \ingroup engines
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>

namespace QuantExt {

class McLgmSwaptionEngine : public GenericEngine<QuantLib::Swaption::arguments, QuantLib::Swaption::results>,
                            public McMultiLegBaseEngine {
public:
    McLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                        const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
                        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed,
                        const Size pricingSeed, const Size polynomOrder,
                        const LsmBasisSystem::PolynomialType polynomType,
                        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                        const std::vector<Date> simulationDates = std::vector<Date>(),
                        const std::vector<Size> externalModelIndices = std::vector<Size>(),
                        const bool minimalObsDate = true, const RegressorModel regressorModel = RegressorModel::Simple,
                        const Real regressionVarianceCutoff = Null<Real>())
        : GenericEngine<QuantLib::Swaption::arguments, QuantLib::Swaption::results>(),
          McMultiLegBaseEngine(Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                                   std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                                   std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
                               calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                               calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                               {discountCurve}, simulationDates, externalModelIndices, minimalObsDate, regressorModel) {
        registerWith(model);
    }

    void calculate() const override;
};

class McLgmNonstandardSwaptionEngine
    : public GenericEngine<QuantLib::NonstandardSwaption::arguments, QuantLib::NonstandardSwaption::results>,
      public McMultiLegBaseEngine {
public:
    McLgmNonstandardSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                   const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
                                   const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed,
                                   const Size pricingSeed, const Size polynomOrder,
                                   const LsmBasisSystem::PolynomialType polynomType,
                                   const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                                   const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                                   const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                   const std::vector<Date> simulationDates = std::vector<Date>(),
                                   const std::vector<Size> externalModelIndices = std::vector<Size>(),
                                   const bool minimalObsDate = true,
                                   const RegressorModel regressorModel = RegressorModel::Simple)
        : GenericEngine<QuantLib::NonstandardSwaption::arguments, QuantLib::NonstandardSwaption::results>(),
          McMultiLegBaseEngine(Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                                   std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                                   std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
                               calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                               calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                               {discountCurve}, simulationDates, externalModelIndices, minimalObsDate, regressorModel) {
        registerWith(model);
    }

    void calculate() const override;
};

} // namespace QuantExt
