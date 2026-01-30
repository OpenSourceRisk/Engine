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

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <qle/instruments/callablebond.hpp>
#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crlgmvectorised.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmvectorised.hpp>
#include <qle/pricingengines/mccashflowinfo.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/pricingengines/mcregressionmodel.hpp>
#include <qle/utilities/callablebond.hpp>

namespace QuantExt {

class McCamCallableBondBaseEngine {
public:
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
        const Size americanExerciseTimeStepsPerYear = 24, const bool generateAdditionalResults = true,
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const McRegressionModel::RegressorModel regressorModel = McRegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const McRegressionModel::VarGroupMode regressionVarGroupMode = McRegressionModel::VarGroupMode::Global);

    //! Destructor
    virtual ~McCamCallableBondBaseEngine() {}

    // run calibration and pricing (called from derived engines)
    void calculate() const;

    // return AmcCalculator instance (called from derived engines, calculate must be called before)
    QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator() const;

    // the implementation of the amc calculator interface used by the amc valuation engine
    class CallableBondAmcCalculator : public AmcCalculator {
    public:
        CallableBondAmcCalculator() = default;
        CallableBondAmcCalculator(
            const std::vector<Size>& externalModelIndices, const std::set<Real>& exerciseXvaTimes,
            const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
            const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
            const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes,
            const std::array<std::vector<McRegressionModel>, 2>& regModelUndDirty,
            const std::array<std::vector<McRegressionModel>, 2>& regModelContinuationValueCall,
            const std::array<std::vector<McRegressionModel>, 2>& regModelContinuationValuePut,
            const std::array<std::vector<McRegressionModel>, 2>& regModelOption,
            const std::array<std::vector<McRegressionModel>, 2>& regModelCallExerciseValue,
            const std::array<std::vector<McRegressionModel>, 2>& regModelPutExerciseValue, const Real resultValue,
            const Array& initialState, const Currency& baseCurrency, const bool reevaluateExerciseInStickyRun,
            const bool includeTodaysCashflows, const bool includeReferenceDateEvents,
            const ext::shared_ptr<QuantExt::CallableBondNotionalAndAccrualCalculator>& notionalAccrualCalculator);

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
        std::array<std::vector<McRegressionModel>, 2> regModelUndDirty_;
        std::array<std::vector<McRegressionModel>, 2> regModelContinuationValueCall_;
        std::array<std::vector<McRegressionModel>, 2> regModelContinuationValuePut_;
        std::array<std::vector<McRegressionModel>, 2> regModelOption_;
        std::array<std::vector<McRegressionModel>, 2> regModelCallExerciseValue_;
        std::array<std::vector<McRegressionModel>, 2> regModelPutExerciseValue_;

        Real resultValue_;
        Array initialState_;
        Currency baseCurrency_;
        bool reevaluateExerciseInStickyRun_;

        // set from global settings via base engine
        bool includeTodaysCashflows_;
        bool includeReferenceDateEvents_;

        std::vector<Filter> exercisedCall_;
        std::vector<Filter> exercisedPut_;
        ext::shared_ptr<QuantExt::CallableBondNotionalAndAccrualCalculator> notionalAccrualCalculator_;
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
    mutable ext::shared_ptr<QuantExt::CallableBondNotionalAndAccrualCalculator> notionalAccrualCalculator_;
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
    Size americanExerciseTimeStepsPerYear_;
    bool generateAdditionalResults_;

    std::vector<Date> simulationDates_;
    std::vector<Date> stickyCloseOutDates_;
    std::vector<Size> externalModelIndices_;
    bool minimalObsDate_;
    McRegressionModel::RegressorModel regressorModel_;
    Real regressionVarianceCutoff_;
    bool recalibrateOnStickyCloseOutDates_;
    bool reevaluateExerciseInStickyRun_;
    Size cfOnCpnMaxSimTimes_;
    Period cfOnCpnAddSimTimesCutoff_;
    Size regressionMaxSimTimesIr_;
    Size regressionMaxSimTimesFx_;
    Size regressionMaxSimTimesEq_;
    McRegressionModel::VarGroupMode regressionVarGroupMode_;

    // set from global settings
    mutable bool includeTodaysCashflows_;
    mutable bool includeReferenceDateEvents_;

    // the generated amc calculator
    mutable QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator_;

    // results, these are read from derived engines
    mutable Real resultUnderlyingNpv_, resultOptionValue_, resultTotalNpv_;
    mutable Real resultUnderlyingSettlementValue_, resultSettlementValue_, resultOptionSettlementValue_;
    static constexpr Real tinyTime = 1E-10;
    // generate the mc path values of the model process
    void generatePathValues(const std::vector<Real>& simulationTimes,
                            std::vector<std::vector<RandomVariable>>& pathValues) const;

    // the model training logic
    void calculateModels(
        Handle<YieldTermStructure> discountCurve, const std::set<Real>& simulationTimes,
        const std::set<Real>& exerciseXvaTimes, const std::set<Real> exerciseTimes,
        const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
        const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes, const std::set<Real>& xvaTimes,
        const std::vector<McCashflowInfo>& cashflowInfo, const std::vector<std::vector<RandomVariable>>& pathValues,
        const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
        std::vector<McRegressionModel>& regModelUndDirty, std::vector<McRegressionModel>& regModelContinuationValueCall,
        std::vector<McRegressionModel>& regModelContinuationValuePut, std::vector<McRegressionModel>& regModelOption,
        std::vector<McRegressionModel>& regModelCallExerciseValue, std::vector<McRegressionModel>& regModelPutExerciseValue,
        RandomVariable& pathValueUndDirty, RandomVariable& pathValueOption) const;

    void generateExerciseDates(std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
                               std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes,
                               std::set<Real>& exerciseTimes) const;

    // convert a date to a time w.r.t. the valuation date
    Real time(const Date& d) const;

    // get the index of a time in the given simulation times set
    Size timeIndex(const Time t, const std::set<Real>& simulationTimes) const;

    // compute a cashflow path value (in model base ccy)
    RandomVariable cashflowPathValue(const McCashflowInfo& cf, const std::vector<std::vector<RandomVariable>>& pathValues,
                                     const std::set<Real>& simulationTimes,
                                     const Handle<YieldTermStructure>& discountCurve) const;

    // valuation date
    mutable Date today_;

    // lgm vectorised instances for each ccy
    mutable std::vector<LgmVectorised> lgmVectorised_;
    mutable QuantLib::ext::shared_ptr<CrLgmVectorised> crlgmVectorised_;
    mutable std::map<std::string, QuantLib::ext::any> additionalResults_;

protected:
    std::set<Time> getExerciseTimes(const std::vector<CallableBond::CallabilityData>& callData) const;
    RandomVariable creditRiskDiscountFactor(const size_t timeIdx, const Time t,
                                            const std::vector<std::vector<RandomVariable>>& pathValues) const;
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
        const Size americanExerciseTimeStepsPerYear = 24, const bool generateAdditionalResults = true,
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const McRegressionModel::RegressorModel regressorModel = McRegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const McRegressionModel::VarGroupMode regressionVarGroupMode = McRegressionModel::VarGroupMode::Global)
        : McCamCallableBondEngine(
              Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
                  std::vector<QuantLib::ext::shared_ptr<IrModel>>(1, model),
                  std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>())),
              calibrationPathGenerator, pricingPathGenerator, calibrationSamples, pricingSamples, calibrationSeed,
              pricingSeed, polynomOrder, polynomType, ordering, directionIntegers, referenceCurve, discountingSpread,
              creditCurve, incomeCurve, recoveryRate, spreadOnIncome, americanExerciseTimeStepsPerYear,
              generateAdditionalResults, simulationDates, stickyCloseOutDates, externalModelIndices, minimalObsDate,
              regressorModel, regressionVarianceCutoff, recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun,
              cfOnCpnMaxSimTimes, cfOnCpnAddSimTimesCutoff, regressionMaxSimTimesIr, regressionMaxSimTimesFx,
              regressionMaxSimTimesEq, regressionVarGroupMode) {};

    McCamCallableBondEngine(
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
        const Size americanExerciseTimeStepsPerYear = 24, const bool generateAdditionalResults = true,
        const std::vector<Date>& simulationDates = std::vector<Date>(),
        const std::vector<Date>& stickyCloseOutDates = std::vector<Date>(),
        const std::vector<Size>& externalModelIndices = std::vector<Size>(), const bool minimalObsDate = true,
        const McRegressionModel::RegressorModel regressorModel = McRegressionModel::RegressorModel::Simple,
        const Real regressionVarianceCutoff = Null<Real>(), const bool recalibrateOnStickyCloseOutDates = false,
        const bool reevaluateExerciseInStickyRun = false, const Size cfOnCpnMaxSimTimes = 1,
        const Period& cfOnCpnAddSimTimesCutoff = Period(), const Size regressionMaxSimTimesIr = 0,
        const Size regressionMaxSimTimesFx = 0, const Size regressionMaxSimTimesEq = 0,
        const McRegressionModel::VarGroupMode regressionVarGroupMode = McRegressionModel::VarGroupMode::Global)
        : McCamCallableBondBaseEngine(model, calibrationPathGenerator, pricingPathGenerator, calibrationSamples,
                                      pricingSamples, calibrationSeed, pricingSeed, polynomOrder, polynomType, ordering,
                                      directionIntegers, referenceCurve, discountingSpread, creditCurve, incomeCurve,
                                      recoveryRate, spreadOnIncome, americanExerciseTimeStepsPerYear,
                                      generateAdditionalResults, simulationDates, stickyCloseOutDates,
                                      externalModelIndices, minimalObsDate, regressorModel, regressionVarianceCutoff,
                                      recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun,
                                      cfOnCpnMaxSimTimes, cfOnCpnAddSimTimesCutoff, regressionMaxSimTimesIr,
                                      regressionMaxSimTimesFx, regressionMaxSimTimesEq, regressionVarGroupMode) {
        registerWith(model);
        registerWith(referenceCurve);
        registerWith(discountingSpread);
        registerWith(creditCurve);
        registerWith(incomeCurve);
        registerWith(recoveryRate);
    }

private:
    void calculate() const override {
        // std::cout << "Calculate McCamCallableBondEngine" << std::endl;
        leg_ = arguments_.cashflows;
        currency_ = model_->irlgm1f(0)->currency();
        notionals_ = arguments_.notionals;
        callData_ = arguments_.callData;
        putData_ = arguments_.putData;
        settlementDate_ = arguments_.settlementDate;
        includeTodaysCashflows_ = false;
        notionalAccrualCalculator_ = ext::make_shared<QuantExt::CallableBondNotionalAndAccrualCalculator>(
            today_, notionals_.front(), leg_, *model_->irlgm1f(0)->termStructure());
        McCamCallableBondBaseEngine::calculate();
        results_.value = resultTotalNpv_;
        results_.settlementValue = resultSettlementValue_;
        results_.additionalResults = additionalResults_;
        results_.additionalResults["amcCalculator"] = amcCalculator();
    }
};

} // namespace QuantExt
