/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file mclgmbondengine.hpp
    \brief MC LGM bond engines
    \ingroup engines
*/

#pragma once

#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <ql/instruments/bond.hpp>

namespace QuantExt {

class McLgmBondEngine : public GenericEngine<QuantLib::Bond::arguments, QuantLib::Bond::results>,
                        public McMultiLegBaseEngine {
public:
    McLgmBondEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                    const SequenceType calibrationPathGenerator, const SequenceType pricingPathGenerator,
                    const Size calibrationSamples, const Size pricingSamples, const Size calibrationSeed,
                    const Size pricingSeed, const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
                    const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                    const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                    const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                    const Handle<YieldTermStructure>& ccyDiscount = Handle<YieldTermStructure>(),
                    const std::vector<Date> simulationDates = std::vector<Date>(),
                    const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
                    const std::vector<Size> externalModelIndices = std::vector<Size>(),
                    const bool minimalObsDate = true, const RegressorModel regressorModel = RegressorModel::Simple,
                    const Real regressionVarianceCutoff = Null<Real>(),
                    const bool recalibrateOnStickyCloseOutDates = false,
                    const bool reevaluateExerciseInStickyRun = false)
        : GenericEngine<QuantLib::Bond::arguments, QuantLib::Bond::results>(),
          McMultiLegBaseEngine(Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                                   std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                                   std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
                               calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples,
                               calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering, directionIntegers,
                               {discountCurve}, simulationDates, stickyCloseOutDates, externalModelIndices,
                               minimalObsDate, regressorModel, regressionVarianceCutoff,
                               recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun) {
        registerWith(model);
        for (auto& h : discountCurves_)
            registerWith(h);
        ccyDiscount_ = ccyDiscount;
        registerWith(ccyDiscount_);
    }

    void calculate() const override;

    RandomVariable
    overwritePathValueUndDirty(double t, const RandomVariable& pathValueUndDirty,
                               const std::set<Real>& exerciseXvaTimes,
                               const std::vector<std::vector<QuantExt::RandomVariable>>& paths) const override;

    class BondAmcCalculator : public McMultiLegBaseEngine::MultiLegBaseAmcCalculator {
    public:
        BondAmcCalculator(McMultiLegBaseEngine::MultiLegBaseAmcCalculator c)
            : McMultiLegBaseEngine::MultiLegBaseAmcCalculator(c) {};

        std::vector<QuantExt::RandomVariable>
        simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                     const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                     const std::vector<size_t>& relevantPathIndex,
                     const std::vector<size_t>& relevantTimeIndex) override;

        Currency npvCurrency() override { return baseCurrency_; }

        void addEngine(const QuantExt::McLgmBondEngine& engine) {
            engine_ = boost::make_shared<QuantExt::McLgmBondEngine>(engine);
        };

    private:
        boost::shared_ptr<QuantExt::McLgmBondEngine> engine_;
    };

private:
    Handle<YieldTermStructure> ccyDiscount_;
};

} // namespace QuantExt
