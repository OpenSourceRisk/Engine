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

/*! \file mcmultilegoptionengine.hpp
    \brief MC engine for multi leg option instrument
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>

namespace QuantExt {

class McMultiLegOptionEngine
    : public McMultiLegBaseEngine,
      public GenericEngine<QuantExt::MultiLegOption::arguments, QuantExt::MultiLegOption::results> {

public:
    McMultiLegOptionEngine(
        const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
        const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
        const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
        const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>());
    McMultiLegOptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                           const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
                           const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed,
                           const Size pricingSeed, const Size polynomOrder,
                           const LsmBasisSystem::PolynomialType polynomType,
                           const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                           const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                           const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                           const std::vector<Date>& simulationDates = std::vector<Date>(),
                           const std::vector<Size>& externalModelIndices = std::vector<Size>(),
                           const bool minimalObsDate = true,
                           const RegressorModel regressorModel = RegressorModel::Simple,
                           const Real regressionVarianceCutoff = Null<Real>());

    void calculate() const override;
    const Handle<CrossAssetModel>& model() const { return model_; }
};

} // namespace QuantExt
