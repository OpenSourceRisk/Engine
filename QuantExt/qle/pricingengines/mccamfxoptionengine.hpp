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

/*! \file mccamfxoptionengine.hpp
    \brief MC CAM engine for FX Option instrument
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>

#include <ql/instruments/vanillaoption.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>
#include <qle/instruments/vanillaforwardoption.hpp>

namespace QuantExt {

class McCamFxOptionEngineBase : public McMultiLegBaseEngine {
public:
    McCamFxOptionEngineBase(
        const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
        const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false);

    void setupLegs() const;
    void calculateFxOptionBase() const;
    const Handle<CrossAssetModel>& model() const { return model_; }

protected:
    Currency domesticCcy_, foreignCcy_, npvCcy_;

    mutable QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff> payoff_;
    mutable Date payDate_;

    mutable Real fxOptionResultValue_;
    mutable Real fxOptionUnderlyingNpv_;
};

class McCamFxOptionEngine : public McCamFxOptionEngineBase, public VanillaOption::engine {
public:
    McCamFxOptionEngine(
        const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
        const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false)
        : McCamFxOptionEngineBase(model, domesticCcy, foreignCcy, npvCcy, calibrationPathGenerator,
                                  pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
                                  pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, discountCurves,
                                  simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate,
                                  regressorModel, regressionVarianceCutoff, recalibrateOnStickyCloseOutDates,
                                  reevaluateExerciseInStickyRun) {
        registerWith(model_);
        for (auto const& h : discountCurves_)
            registerWith(h);
    }
    void calculate() const override;
};

class McCamFxEuropeanForwardOptionEngine : public McCamFxOptionEngineBase, public VanillaForwardOption::engine {
public:
    McCamFxEuropeanForwardOptionEngine(
        const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
        const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false)
        : McCamFxOptionEngineBase(model, domesticCcy, foreignCcy, npvCcy, calibrationPathGenerator,
                                  pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
                                  pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, discountCurves,
                                  simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate,
                                  regressorModel, regressionVarianceCutoff, recalibrateOnStickyCloseOutDates,
                                  reevaluateExerciseInStickyRun) {
        registerWith(model_);
        for (auto const& h : discountCurves_)
            registerWith(h);
    }
    void calculate() const override;
};

class McCamFxEuropeanCSOptionEngine : public McCamFxOptionEngineBase, public CashSettledEuropeanOption::engine {
public:
    McCamFxEuropeanCSOptionEngine(
        const Handle<CrossAssetModel>& model, const Currency& domesticCcy, const Currency& foreignCcy,
        const Currency& npvCcy, const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
        const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed, const Size pricingSeed,
        const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
        const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
        const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false)
        : McCamFxOptionEngineBase(model, domesticCcy, foreignCcy, npvCcy, calibrationPathGenerator,
                                  pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
                                  pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, discountCurves,
                                  simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate,
                                  regressorModel, regressionVarianceCutoff, recalibrateOnStickyCloseOutDates,
                                  reevaluateExerciseInStickyRun) {
        registerWith(model_);
        for (auto const& h : discountCurves_)
            registerWith(h);
    }
    void calculate() const override;
};

} // namespace QuantExt
