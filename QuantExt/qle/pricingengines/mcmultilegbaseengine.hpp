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

/*! \file mcmultilegbaseengine.hpp
    \brief base MC engine for multileg (option) instruments
*/

#pragma once

#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/amccalculator.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/version.hpp>

namespace QuantExt {

class McMultiLegBaseEngine {

protected:
    /*! The npv is computed in the model's base currency, discounting curves are taken
      from the model. simulationDates are additional simulation dates.
      The cross asset model here must be consistent with the multi path that is input in AmcCalculator */
    McMultiLegBaseEngine(
        const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
        const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
        const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
        const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
        const SobolRsg::DirectionIntegers directionIntegers,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const bool regressionOnExerciseOnly = false);

    // compute path exercise into and (dirty) simulation underlying values
    void computePath(const MultiPath& p) const;
    // rollback for calibration or pricing
    void rollback(const bool calibration) const;
    // run calibration and pricing
    void calculate() const;
    // return AmcCalculator instance (calculate must be called before)
    boost::shared_ptr<AmcCalculator> amcCalculator() const;

    // input data from the derived pricing engines, to be set in these engines
    mutable std::vector<Leg> leg_;
    mutable std::vector<Currency> currency_;
    mutable std::vector<Real> payer_;
    mutable boost::shared_ptr<Exercise> exercise_; // may be empty, if underlying is the actual trade
    mutable Settlement::Type optionSettlement_;

    // data members
    const Handle<CrossAssetModel> model_;
    const SequenceType calibrationPathGenerator_, pricingPathGenerator_;
    const Size calibrationSamples_, pricingSamples_, calibrationSeed_, pricingSeed_;
    std::vector<Handle<YieldTermStructure>> discountCurves_;
    const std::vector<Date> simulationDates_;
    const std::vector<Size> externalModelIndices_;
#if QL_HEX_VERSION > 0x01150000
    const std::vector<ext::function<Real(Array)>> basisFns_;
#else
    const std::vector<boost::function1<Real, Array>> basisFns_;
#endif
    const SobolBrownianGenerator::Ordering ordering_;
    SobolRsg::DirectionIntegers directionIntegers_;
    const bool minimalObsDate_, regressionOnExerciseOnly_;

    // precomputed values for simulation
    mutable std::vector<Real> times_;
    mutable std::vector<Size> indexes_;
    mutable std::vector<Size> exerciseIdx_, simulationIdx_;
    mutable std::vector<bool> isTrappedDate_;
    mutable Size numEx_, numSim_;
    mutable std::vector<Real> pathUndEx_, pathUndDirty_, pathUndTrapped_;
    mutable std::vector<std::vector<Size>> indexCcyIndex_, payCcyNum_, payCcyIndex_, payFxIndex_, maxExerciseIndex_,
        maxDirtyNpvIndex_;
    mutable std::vector<std::vector<std::vector<Size>>> trappedCoupons_;
    mutable std::vector<std::vector<boost::shared_ptr<LgmImpliedYieldTermStructure>>> indexFwdCurve_, indexDscCurve_;
    mutable std::vector<std::vector<boost::shared_ptr<InterestRateIndex>>> modelIndex_;
    mutable std::vector<std::vector<Size>> observationTimeIndex_;
    mutable std::vector<std::vector<Date>> fixingDate_;
    mutable std::vector<std::vector<Real>> gearing_, spread_, accrualTime_, nominal_, payTime_, cappedRate_,
        flooredRate_;
    mutable Size maxUndValDirtyIdx_;
    mutable std::vector<Date> pathDates_; // dates coresponding to path times

    // pathGenerators
    mutable boost::shared_ptr<MultiPathGeneratorBase> pathGeneratorCalibration_, pathGeneratorPricing_;

    // regression coefficients
    mutable std::vector<Array> coeffsItm_;
    mutable std::vector<Array> coeffsUndEx_;
    mutable std::vector<Array> coeffsFull_;
    mutable std::vector<Array> coeffsUndTrapped_;
    mutable std::vector<Array> coeffsUndDirty_;

    // results
    mutable Real resultUnderlyingNpv_, resultValue_;
};

class MultiLegBaseAmcCalculator : public AmcCalculatorSinglePath {
public:
    MultiLegBaseAmcCalculator(const Currency& baseCurrency, const std::vector<Size>& externalModelIndices,
                              const bool hasExercise, const bool isPhysicalSettlement, const std::vector<Real>& times,
                              const std::vector<Size>& indexes, const std::vector<Size>& exerciseIdx,
                              const std::vector<Size>& simulationIdx, const std::vector<bool>& isTrappedDate,
                              const Size numSim, const Real resultValue, const std::vector<Array> coeffsItm,
                              const std::vector<Array>& coeffsUndEx, const std::vector<Array>& coeffsFull,
                              const std::vector<Array>& coeffsUndTrapped, const std::vector<Array>& coeffsUndDirty,
#if QL_HEX_VERSION > 0x01150000
                              const std::vector<ext::function<Real(Array)>>& basisFns)
#else
                              const std::vector<boost::function1<Real, Array>>& basisFns)
#endif
        : baseCurrency_(baseCurrency), externalModelIndices_(externalModelIndices), hasExercise_(hasExercise),
          isPhysicalSettlement_(isPhysicalSettlement), times_(times), indexes_(indexes), exerciseIdx_(exerciseIdx),
          simulationIdx_(simulationIdx), isTrappedDate_(isTrappedDate), numSim_(numSim), resultValue_(resultValue),
          coeffsItm_(coeffsItm), coeffsUndEx_(coeffsUndEx), coeffsFull_(coeffsFull),
          coeffsUndTrapped_(coeffsUndTrapped), coeffsUndDirty_(coeffsUndDirty), basisFns_(basisFns) {
    }
    Array simulatePath(const MultiPath& path, const bool reuseLastEvents) override;
    Currency npvCurrency() override { return baseCurrency_; }

private:
    const Currency baseCurrency_;
    const std::vector<Size> externalModelIndices_;
    const bool hasExercise_;
    const bool isPhysicalSettlement_;
    const std::vector<Real> times_;
    const std::vector<Size> indexes_;
    const std::vector<Size> exerciseIdx_, simulationIdx_;
    const std::vector<bool> isTrappedDate_;
    const Size numSim_;
    const Real resultValue_;
    const std::vector<Array> coeffsItm_;
    const std::vector<Array> coeffsUndEx_;
    const std::vector<Array> coeffsFull_;
    const std::vector<Array> coeffsUndTrapped_;
    const std::vector<Array> coeffsUndDirty_;
#if QL_HEX_VERSION > 0x01150000
    const std::vector<ext::function<Real(Array)>> basisFns_;
#else
    const std::vector<boost::function1<Real, Array>> basisFns_;
#endif
    Size storedExerciseIndex_ = Null<Size>();
};

} // namespace QuantExt
