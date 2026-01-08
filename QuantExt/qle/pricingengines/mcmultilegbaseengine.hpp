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

#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/multilegoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmvectorised.hpp>
#include <qle/pricingengines/amccalculator.hpp>
#include <qle/pricingengines/mccashflowinfo.hpp>
#include <qle/pricingengines/mcregressionmodel.hpp>
#include <qle/utilities/mcstats.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>

//#include <boost/archive/binary_iarchive.hpp>
//#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/export.hpp>

namespace QuantExt {

class McMultiLegBaseEngine {
public:
    

//protected:
    /*! The npv is computed in the model's base currency, discounting curves are taken from the model. simulationDates
        are additional simulation dates. The cross asset model here must be consistent with the multi path that is the
        input to AmcCalculator::simulatePath().

        Current limitations:
        - the parameter minimalObsDate is ignored, the corresponding optimization is not implemented yet
        - pricingSamples are ignored, the npv from the training phase is used alway
    */
    McMultiLegBaseEngine(
        const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
        const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
        const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
        const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
        const SobolRsg::DirectionIntegers directionIntegers,
        const std::vector<Handle<YieldTermStructure>>& discountCurves = std::vector<Handle<YieldTermStructure>>(),
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
    virtual ~McMultiLegBaseEngine() {}

    // run calibration and pricing (called from derived engines)
    void calculate() const;

    // return AmcCalculator instance (called from derived engines, calculate must be called before)
    QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator() const;

    // input data from the derived pricing engines, to be set in these engines
    mutable std::vector<Leg> leg_;
    mutable std::vector<Currency> currency_;
    mutable std::vector<bool> payer_;
    mutable QuantLib::ext::shared_ptr<Exercise> exercise_; // may be empty, if underlying is the actual trade
    mutable Settlement::Type optionSettlement_ = Settlement::Physical;
    mutable std::vector<QuantLib::Date> cashSettlementDates_;
    mutable bool exerciseIntoIncludeSameDayFlows_ = false;

    // data members
    Handle<CrossAssetModel> model_;
    SequenceType calibrationPathGenerator_, pricingPathGenerator_;
    Size calibrationSamples_, pricingSamples_, calibrationSeed_, pricingSeed_;
    Size polynomOrder_;
    LsmBasisSystem::PolynomialType polynomType_;
    SobolBrownianGenerator::Ordering ordering_;
    SobolRsg::DirectionIntegers directionIntegers_;
    std::vector<Handle<YieldTermStructure>> discountCurves_;
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

    // set from global settings
    mutable bool includeTodaysCashflows_;
    mutable bool includeReferenceDateEvents_;

    // the generated amc calculator
    mutable QuantLib::ext::shared_ptr<AmcCalculator> amcCalculator_;

    // results, these are read from derived engines
    mutable Real resultUnderlyingNpv_, resultValue_;

    static constexpr Real tinyTime = 1E-10;

    // data structure storing info needed to generate the amount for a cashflow
    
    // overwrite function to transform values going into regression
    // current usage in the fwd bond case
    virtual RandomVariable overwritePathValueUndDirty(double t, const RandomVariable& pathValueUndDirty,
                                                      const std::set<Real>& exerciseXvaTimes,
                                                      const std::vector<std::vector<QuantExt::RandomVariable>>& paths) const {
        return pathValueUndDirty;
    };

    virtual bool useOverwritePathValueUndDirty() const { return false; };

    

    // the implementation of the amc calculator interface used by the amc valuation engine
    class MultiLegBaseAmcCalculator : public AmcCalculator {
    public:
        MultiLegBaseAmcCalculator() = default;
        MultiLegBaseAmcCalculator(
            const std::vector<Size>& externalModelIndices, const Settlement::Type settlement,
            const std::vector<Real>& cashSettlementTimes, const std::set<Real>& exerciseXvaTimes,
            const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
            const std::array<std::vector<RegressionModel>, 2>& regModelUndDirty,
            const std::array<std::vector<RegressionModel>, 2>& regModelUndExInto,
            const std::array<std::vector<RegressionModel>, 2>& regModelRebate,
            const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValue,
            const std::array<std::vector<RegressionModel>, 2>& regModelOption,
            const Real resultValue, const Array& initialState, const Currency& baseCurrency,
            const bool reevaluateExerciseInStickyRun, const bool includeTodaysCashflows,
            const bool includeReferenceDateEvents);

        Currency npvCurrency() override { return baseCurrency_; }
        std::vector<QuantExt::RandomVariable>
        simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                     const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                     const std::vector<size_t>& relevantPathIndex,
                     const std::vector<size_t>& relevantTimeIndex) override;

    protected:
        std::vector<Size> externalModelIndices_;
        Settlement::Type settlement_;
        std::vector<Real> cashSettlementTimes_;
        std::set<Real> exerciseXvaTimes_;
        std::set<Real> exerciseTimes_;
        std::set<Real> xvaTimes_;
        std::array<std::vector<RegressionModel>, 2> regModelUndDirty_;
        std::array<std::vector<RegressionModel>, 2> regModelUndExInto_;
        std::array<std::vector<RegressionModel>, 2> regModelRebate_;
        std::array<std::vector<RegressionModel>, 2> regModelContinuationValue_;
        std::array<std::vector<RegressionModel>, 2> regModelOption_;
        Real resultValue_;
        Array initialState_;
        Currency baseCurrency_;
        bool reevaluateExerciseInStickyRun_;

        // set from global settings via base engine
        bool includeTodaysCashflows_;
        bool includeReferenceDateEvents_;

        std::vector<Filter> exercised_;

        // used for serialisation of amc trianing
        friend class boost::serialization::access;
        template <class Archive> void serialize(Archive& ar, const unsigned int version);

    };

    // generate the mc path values of the model process
    void generatePathValues(const std::vector<Real>& simulationTimes,
                            std::vector<std::vector<RandomVariable>>& pathValues) const;

    // the model training logic
    void calculateModels(const std::set<Real>& simulationTimes, const std::set<Real>& exerciseXvaTimes,
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


    // get the index of a time in the given simulation times set
    Size timeIndex(const Time t, const std::set<Real>& simulationTimes) const;

    // compute a cashflow path value (in model base ccy)
    RandomVariable cashflowPathValue(const CashflowInfo& cf, const std::vector<std::vector<RandomVariable>>& pathValues,
                                     const std::set<Real>& simulationTimes) const;

    // valuation date
    mutable Date today_;

    // lgm vectorised instances for each ccy
    mutable std::vector<LgmVectorised> lgmVectorised_;
};


} // namespace QuantExt
