/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file mccamcallablebondengine.hpp
    \brief MC engine for callable bonds with AMC
*/

#pragma once

#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/callablebond.hpp>
#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmvectorised.hpp>
#include <qle/pricingengines/amccalculator.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>

// #include <boost/archive/binary_iarchive.hpp>
// #include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

namespace QuantExt {

class McCamCallableBondBaseEngine {
public:
    enum RegressorModel { Simple, Lagged, LaggedIR, LaggedFX, LaggedEQ };
    enum VarGroupMode { Global, Trivial };

    // protected:
    /*! The npv is computed in the model's base currency, discounting curves are taken from the model. simulationDates
        are additional simulation dates. The cross asset model here must be consistent with the multi path that is the
        input to AmcCalculator::simulatePath().

        Current limitations:
        - the parameter minimalObsDate is ignored, the corresponding optimization is not implemented yet
        - pricingSamples are ignored, the npv from the training phase is used alway
    */
    McCamCallableBondBaseEngine(
        const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
        const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
        const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
        const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
        const SobolRsg::DirectionIntegers directionIntegers,
        const Handle<QuantLib::YieldTermStructure>& referenceCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::YieldTermStructure>& incomeCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool spreadOnIncome = true,
        const bool generateAdditionalResults = true,
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const VarGroupMode regressionVarGroupMode = VarGroupMode::Global);

    //! Destructor
    virtual ~McCamCallableBondBaseEngine() {}

    // run calibration and pricing (called from derived engines)
    void calculate() const;

    // return AmcCalculator instance (called from derived engines, calculate must be called before)
    QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator() const;

    // input data from the derived pricing engines, to be set in these engines
    mutable Leg leg_;
    mutable std::vector<CallableBond::CallabilityData> callData_;
    mutable std::vector<CallableBond::CallabilityData> putData_;
    mutable std::vector<Real> notionals_;
    bool exerciseIntoIncludeSameDayFlows_ = true;
    mutable Currency currency_;

    // data members
    Handle<CrossAssetModel> model_;
    SequenceType calibrationPathGenerator_, pricingPathGenerator_;
    Size calibrationSamples_, pricingSamples_, calibrationSeed_, pricingSeed_;
    Size polynomOrder_;
    LsmBasisSystem::PolynomialType polynomType_;
    SobolBrownianGenerator::Ordering ordering_;
    SobolRsg::DirectionIntegers directionIntegers_;
    
    Handle<QuantLib::YieldTermStructure> referenceCurve_;
    Handle<QuantLib::Quote> discountingSpread_;
    Handle<QuantLib::DefaultProbabilityTermStructure> creditCurve_;
    Handle<QuantLib::YieldTermStructure> incomeCurve_;
    Handle<QuantLib::Quote> recoveryRate_;
    
    bool spreadOnIncome_;
    bool generateAdditionalResults_;

    std::vector<Date> simulationDates_;
    std::vector<Date> stickyCloseOutDates_;
    std::vector<Size> externalModelIndices_;
    bool minimalObsDate_;
    RegressorModel regressorModel_;
    Real regressionVarianceCutoff_;
    bool recalibrateOnStickyCloseOutDates_;
    bool reevaluateExerciseInStickyRun_;
    Size cfOnCpnMaxSimTimes_;
    Period cfOnCpnAddSimTimesCutoff_;
    Size regressionMaxSimTimesIr_;
    Size regressionMaxSimTimesFx_;
    Size regressionMaxSimTimesEq_;
    VarGroupMode regressionVarGroupMode_;

    // set from global settings
    mutable bool includeTodaysCashflows_;
    mutable bool includeReferenceDateEvents_;

    // the generated amc calculator
    mutable QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator_;

    // results, these are read from derived engines
    mutable Real resultUnderlyingNpv_, resultValue_;

    static constexpr Real tinyTime = 1E-10;

    // data structure storing info needed to generate the amount for a cashflow
    struct CashflowInfo {
        Size legNo = Null<Size>(), cfNo = Null<Size>();
        Real payTime = Null<Real>();
        Real exIntoCriterionTime = Null<Real>();
        Size payCcyIndex = Null<Size>();
        bool payer = false;
        std::vector<Real> simulationTimes;
        std::vector<std::vector<Size>> modelIndices;
        std::function<RandomVariable(const Size n, const std::vector<std::vector<const RandomVariable*>>&)>
            amountCalculator;
    };

    // class representing a regression model for a certain observation (= xva, exercise) time
    class RegressionModel {
    public:
        RegressionModel() = default;
        RegressionModel(const Real observationTime, const std::vector<CashflowInfo>& cashflowInfo,
                        const std::function<bool(std::size_t)>& cashflowRelevant, const CrossAssetModel& model,
                        const RegressorModel regressorModel, const Real regressionVarianceCutoff = Null<Real>(),
                        const Size regressionMaxSimTimesIr = 0, const Size regressionMaxSimTimesFx = 0,
                        const Size regressionMaxSimTimesEq = 0,
                        const VarGroupMode regressionVarGroupMode = VarGroupMode::Global);
        // pathTimes must contain the observation time and the relevant cashflow simulation times
        void train(const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
                   const RandomVariable& regressand, const std::vector<std::vector<const RandomVariable*>>& paths,
                   const std::set<Real>& pathTimes, const Filter& filter = Filter());
        // pathTimes do not need to contain the observation time or the relevant cashflow simulation times
        RandomVariable apply(const Array& initialState, const std::vector<std::vector<const RandomVariable*>>& paths,
                             const std::set<Real>& pathTimes) const;
        // is this model initialized and trained?
        bool isTrained() const { return isTrained_; }

    private:
        Real observationTime_ = Null<Real>();
        Real regressionVarianceCutoff_ = Null<Real>();
        bool isTrained_ = false;
        std::set<std::pair<Real, Size>> regressorTimesModelIndices_;
        Matrix coordinateTransform_;
        std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>> basisFns_ =
            std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>{};
        Array regressionCoeffs_;

        Size basisDim_ = 0;
        Size basisOrder_ = 0;
        LsmBasisSystem::PolynomialType basisType_ = LsmBasisSystem::PolynomialType::Monomial;
        Size basisSystemSizeBound_ = Null<Size>();
        std::set<std::set<Size>> varGroups_ = {};

        friend class boost::serialization::access;
        template <class Archive> void serialize(Archive& ar, const unsigned int version);
    };

    // generate the mc path values of the model process
    void generatePathValues(const std::vector<Real>& simulationTimes,
                            std::vector<std::vector<RandomVariable>>& pathValues) const;

    // the model training logic
    void calculateModels(Handle<YieldTermStructure> discountCurve,
                         const std::set<Real>& simulationTimes, const std::set<Real>& exerciseXvaTimes,
                         const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
                         const std::vector<CashflowInfo>& cashflowInfo,
                         const std::vector<std::vector<RandomVariable>>& pathValues,
                         const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
                         std::vector<RegressionModel>& regModelUndDirty,
                         std::vector<RegressionModel>& regModelUndExInto, std::vector<RegressionModel>& regModelRebate,
                         std::vector<RegressionModel>& regModelContinuationValue,
                         std::vector<RegressionModel>& regModelOption, RandomVariable& pathValueUndDirty,
                         RandomVariable& pathValueUndExInto, RandomVariable& pathValueOption) const;

    // convert a date to a time w.r.t. the valuation date
    Real time(const Date& d) const;

    // create the info for a given flow
    CashflowInfo createCashflowInfo(QuantLib::ext::shared_ptr<CashFlow> flow, const Currency& payCcy, bool payer,
                                    Size legNo, Size cfNo) const;

    // get the index of a time in the given simulation times set
    Size timeIndex(const Time t, const std::set<Real>& simulationTimes) const;

    // compute a cashflow path value (in model base ccy)
    RandomVariable cashflowPathValue(const CashflowInfo& cf, const std::vector<std::vector<RandomVariable>>& pathValues,
                                     const std::set<Real>& simulationTimes, const Handle<YieldTermStructure>& discountCurve) const;

    // valuation date
    mutable Date today_;

    // lgm vectorised instances for each ccy
    mutable std::vector<LgmVectorised> lgmVectorised_;
};

class McCamCallableBondEngine : public McCamCallableBondBaseEngine, public CallableBond::engine {
public:
    McCamCallableBondEngine(
        const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const SequenceType calibrationPathGenerator,
        const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
        const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
        const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
        const SobolRsg::DirectionIntegers directionIntegers,
        const Handle<QuantLib::YieldTermStructure>& referenceCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::YieldTermStructure>& incomeCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool spreadOnIncome = true,
        const bool generateAdditionalResults = true, const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const RegressorModel regressorModel = RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const VarGroupMode regressionVarGroupMode = VarGroupMode::Global)
        : McCamCallableBondBaseEngine(
              Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                  std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                  std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
              calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
              pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, referenceCurve, discountingSpread,
              creditCurve, incomeCurve, recoveryRate, spreadOnIncome, generateAdditionalResults, simulationDates,
              stickyCloseOutDates, externalModelIndices, minimalObsDate, regressorModel, regressionVarianceCutoff,
              recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun, cfOnCpnMaxSimTimes,
              cfOnCpnAddSimTimesCutoff, regressionMaxSimTimesIr, regressionMaxSimTimesFx, regressionMaxSimTimesEq,
              regressionVarGroupMode) {
        registerWith(model);
        registerWith(referenceCurve);
        registerWith(discountingSpread);
        registerWith(creditCurve);
        registerWith(incomeCurve);
        registerWith(recoveryRate);
    }

private:
    void calculate() const override {

        leg_ = arguments_.cashflows;
        currency_ =  model_->irlgm1f(0)->currency();
        notionals_ = arguments_.notionals;
        callData_ = arguments_.callData;
        putData_ = arguments_.putData;
        McCamCallableBondBaseEngine::calculate();
        results_.value = resultValue_;
    }
};

} // namespace QuantExt
