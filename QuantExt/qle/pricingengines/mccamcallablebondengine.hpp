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


#include <qle/instruments/callablebond.hpp>
#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmvectorised.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <qle/pricingengines/mcregressionmodel.hpp>
#include <qle/pricingengines/mccashflowinfo.hpp>
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
        const RegressionModel::RegressorModel regressorModel = RegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const RegressionModel::VarGroupMode regressionVarGroupMode = RegressionModel::VarGroupMode::Global);

    //! Destructor
    virtual ~McCamCallableBondBaseEngine() {}

    // run calibration and pricing (called from derived engines)
    void calculate() const;

    void setAmericanExerciseInterval(const Size obsPerYear) { americanExerciseInterval_ = obsPerYear; }

    // return AmcCalculator instance (called from derived engines, calculate must be called before)
    QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator() const;


// the implementation of the amc calculator interface used by the amc valuation engine
    class CallableBondAmcCalculator : public AmcCalculator {
    public:
        CallableBondAmcCalculator() = default;
        CallableBondAmcCalculator(
            const std::vector<Size>& externalModelIndices, 
            const std::set<Real>& exerciseXvaTimes,
            const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
            const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
            const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes,
            const std::array<std::vector<RegressionModel>, 2>& regModelUndDirty,
            const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValueCall,
            const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValuePut,
            const std::array<std::vector<RegressionModel>, 2>& regModelOption,
            const Real resultValue, const Array& initialState, const Currency& baseCurrency,
            const bool reevaluateExerciseInStickyRun, const bool includeTodaysCashflows,
            const bool includeReferenceDateEvents, const QuantExt::CurrentNotionalAccrualsCalculator&
                notionalAccrualCalculator);

        Currency npvCurrency() override { return baseCurrency_; }
        std::vector<QuantExt::RandomVariable>
        simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                     const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                     const std::vector<size_t>& relevantPathIndex,
                     const std::vector<size_t>& relevantTimeIndex) override;

    protected:
        std::vector<Size> externalModelIndices_;
        std::set<Real> exerciseXvaTimes_;
        std::set<Real> exerciseTimes_;
        std::set<Real> xvaTimes_;
        std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>> callTimes_;
        std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>> putTimes_;
        std::array<std::vector<RegressionModel>, 2> regModelUndDirty_;
        std::array<std::vector<RegressionModel>, 2> regModelContinuationValueCall_;
        std::array<std::vector<RegressionModel>, 2> regModelContinuationValuePut_;
        std::array<std::vector<RegressionModel>, 2> regModelOption_;
        Real resultValue_;
        Array initialState_;
        Currency baseCurrency_;
        bool reevaluateExerciseInStickyRun_;

        // set from global settings via base engine
        bool includeTodaysCashflows_;
        bool includeReferenceDateEvents_;

        std::vector<Filter> exercisedCall_;
        std::vector<Filter> exercisedPut_;
        QuantExt::CurrentNotionalAccrualsCalculator notionalAccrualCalculator_;
        // used for serialisation of amc trianing
        friend class boost::serialization::access;
        template <class Archive> void serialize(Archive& ar, const unsigned int version);

    };



    // input data from the derived pricing engines, to be set in these engines
    mutable Leg leg_;
    mutable std::vector<CallableBond::CallabilityData> callData_;
    mutable std::vector<CallableBond::CallabilityData> putData_;
    mutable std::vector<Real> notionals_;
    bool exerciseIntoIncludeSameDayFlows_ = true;
    mutable Currency currency_;
    mutable Date settlementDate_;
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
    RegressionModel::RegressorModel regressorModel_;
    Real regressionVarianceCutoff_;
    bool recalibrateOnStickyCloseOutDates_;
    bool reevaluateExerciseInStickyRun_;
    Size cfOnCpnMaxSimTimes_;
    Period cfOnCpnAddSimTimesCutoff_;
    Size regressionMaxSimTimesIr_;
    Size regressionMaxSimTimesFx_;
    Size regressionMaxSimTimesEq_;
    RegressionModel::VarGroupMode regressionVarGroupMode_;
    Size americanExerciseInterval_ = 24;

    // set from global settings
    mutable bool includeTodaysCashflows_;
    mutable bool includeReferenceDateEvents_;

    // the generated amc calculator
    mutable QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator_;

    // results, these are read from derived engines
    mutable Real resultUnderlyingNpv_, resultValue_;
    mutable Real resultUnderlyingSettlementValue_, resultSettlementValue_;
    mutable std::vector<Real> callProbs_;
    mutable std::vector<Real> putProbs_;
    static constexpr Real tinyTime = 1E-10;
    // generate the mc path values of the model process
    void generatePathValues(const std::vector<Real>& simulationTimes,
                            std::vector<std::vector<RandomVariable>>& pathValues) const;

    // the model training logic
    void calculateModels(Handle<YieldTermStructure> discountCurve, const std::set<Real>& simulationTimes,
                         const std::set<Real>& exerciseXvaTimes, const std::set<Real> exerciseTimes,
                         const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes, const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes,
                         const std::set<Real>& xvaTimes, const std::vector<CashflowInfo>& cashflowInfo,
                         const std::vector<std::vector<RandomVariable>>& pathValues,
                         const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
                         std::vector<RegressionModel>& regModelUndDirty,
                         std::vector<RegressionModel>& regModelContinuationValueCall,
                         std::vector<RegressionModel>& regModelContinuationValuePut,
                         std::vector<RegressionModel>& regModelOption, RandomVariable& pathValueUndDirty,
                         RandomVariable& pathValueOption, std::vector<RandomVariable>& pathExerciseProbsCall,
                         std::vector<RandomVariable>& pathExerciseProbsPut) const;

    // convert a date to a time w.r.t. the valuation date
    Real time(const Date& d) const;

    // get the index of a time in the given simulation times set
    Size timeIndex(const Time t, const std::set<Real>& simulationTimes) const;

    // compute a cashflow path value (in model base ccy)
    RandomVariable cashflowPathValue(const CashflowInfo& cf, const std::vector<std::vector<RandomVariable>>& pathValues,
                                     const std::set<Real>& simulationTimes, const Handle<YieldTermStructure>& discountCurve) const;

    // valuation date
    mutable Date today_;

    // lgm vectorised instances for each ccy
    mutable std::vector<LgmVectorised> lgmVectorised_;

protected:
        std::set<Time> getExerciseTimes(const std::vector<CallableBond::CallabilityData>& callData) const;
};

class McCamCallableBondEngine : public McCamCallableBondBaseEngine, public CallableBond::engine {
public:
    McCamCallableBondEngine(
        const QuantLib::ext::shared_ptr<IrModel>& model, const SequenceType calibrationPathGenerator,
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
        const RegressionModel::RegressorModel regressorModel = RegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const RegressionModel::VarGroupMode regressionVarGroupMode = RegressionModel::VarGroupMode::Global)
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
        std::cout <<"Calculate McCamCallableBondEngine" << std::endl;
        leg_ = arguments_.cashflows;
        currency_ =  model_->irlgm1f(0)->currency();
        notionals_ = arguments_.notionals;
        callData_ = arguments_.callData;
        putData_ = arguments_.putData;
        settlementDate_ = arguments_.settlementDate;
        includeTodaysCashflows_ = false;
        McCamCallableBondBaseEngine::calculate();
        results_.value = resultUnderlyingNpv_ +  resultValue_;
        results_.settlementValue = resultUnderlyingSettlementValue_ +  resultSettlementValue_;




         std::cout <<"  number of cashflows: " << leg_.size() << std::endl   ;
        std::cout <<" number of notionals: " << notionals_.size() << std::endl;
        std::cout <<" number of callData: " << callData_.size() << std::endl;
        std::cout <<" number of putData: " << putData_.size() << std::endl;
        std::cout <<"  result value: " << resultValue_ << std::endl;\
        std::cout << "  result underlying npv: " << resultUnderlyingNpv_ << std::endl;
        std::cout << " underlying settlement value: " << resultUnderlyingSettlementValue_ << std::endl;
        std::cout << " final npv: " << resultUnderlyingNpv_ +  resultValue_ << std::endl;
        std::cout << " final settlement value: " << resultUnderlyingSettlementValue_ +  resultSettlementValue_ << std::endl;
        
        
        
       

    }
};

} // namespace QuantExt
