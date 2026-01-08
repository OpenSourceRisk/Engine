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


#include <qle/instruments/rebatedexercise.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>


#include <ql/math/interpolations/linearinterpolation.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>

namespace QuantExt {

McMultiLegBaseEngine::McMultiLegBaseEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Date>& stickyCloseOutDates,
    const std::vector<Size>& externalModelIndices, const bool minimalObsDate, const RegressionModel::RegressorModel regressorModel,
    const Real regressionVarianceCutoff, const bool recalibrateOnStickyCloseOutDates,
    const bool reevaluateExerciseInStickyRun, const Size cfOnCpnMaxSimTimes,
    const Period& cfOnCpnAddSimTimesCutoff, const Size regressionMaxSimTimesIr, const Size regressionMaxSimTimesFx,
    const Size regressionMaxSimTimesEq, const RegressionModel::VarGroupMode regressionVarGroupMode)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), polynomOrder_(polynomOrder), polynomType_(polynomType), ordering_(ordering),
      directionIntegers_(directionIntegers), discountCurves_(discountCurves), simulationDates_(simulationDates),
      stickyCloseOutDates_(stickyCloseOutDates), externalModelIndices_(externalModelIndices),
      minimalObsDate_(minimalObsDate), regressorModel_(regressorModel),
      regressionVarianceCutoff_(regressionVarianceCutoff),
      recalibrateOnStickyCloseOutDates_(recalibrateOnStickyCloseOutDates),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun),
      cfOnCpnMaxSimTimes_(cfOnCpnMaxSimTimes),
      cfOnCpnAddSimTimesCutoff_(cfOnCpnAddSimTimesCutoff),
      regressionMaxSimTimesIr_(regressionMaxSimTimesIr),
      regressionMaxSimTimesFx_(regressionMaxSimTimesFx),
      regressionMaxSimTimesEq_(regressionMaxSimTimesEq),
      regressionVarGroupMode_(regressionVarGroupMode) {

    if (discountCurves_.empty())
        discountCurves_.resize(model_->components(CrossAssetModel::AssetType::IR));
    else {
        QL_REQUIRE(discountCurves_.size() == model_->components(CrossAssetModel::AssetType::IR),
                   "McMultiLegBaseEngine: " << discountCurves_.size() << " discount curves given, but model has "
                                            << model_->components(CrossAssetModel::AssetType::IR) << " IR components.");
    }
    QL_REQUIRE(cfOnCpnMaxSimTimes >= 0,
               "McMultiLegBaseEngine: cfOnCpnMaxSimTimes must be non-negative");
    QL_REQUIRE(cfOnCpnAddSimTimesCutoff.length() >= 0,
               "McMultiLegBaseEngine: length of cfOnCpnAddSimTimesCutoff must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesIr >= 0,
               "McMultiLegBaseEngine: regressionMaxSimTimesIr must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesFx >= 0,
               "McMultiLegBaseEngine: regressionMaxSimTimesFx must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesEq >= 0,
               "McMultiLegBaseEngine: regressionMaxSimTimesEq must be non-negative");
}

Real McMultiLegBaseEngine::time(const Date& d) const {
    return model_->irlgm1f(0)->termStructure()->timeFromReference(d);
}

Size McMultiLegBaseEngine::timeIndex(const Time t, const std::set<Real>& times) const {
    auto it = times.find(t);
    QL_REQUIRE(it != times.end(), "McMultiLegBaseEngine::cashflowPathValue(): time ("
                                      << t
                                      << ") not found in simulation times. This is an internal error. Contact dev.");
    return std::distance(times.begin(), it);
}

RandomVariable McMultiLegBaseEngine::cashflowPathValue(const CashflowInfo& cf,
                                                       const std::vector<std::vector<RandomVariable>>& pathValues,
                                                       const std::set<Real>& simulationTimes) const {

    Size n = pathValues[0][0].size();
    auto simTimesPayIdx = timeIndex(cf.payTime, simulationTimes);

    std::vector<RandomVariable> initialValues(model_->stateProcess()->initialValues().size());
    for (Size i = 0; i < model_->stateProcess()->initialValues().size(); ++i)
        initialValues[i] = RandomVariable(n, model_->stateProcess()->initialValues()[i]);

    std::vector<std::vector<const RandomVariable*>> states(cf.simulationTimes.size());
    for (Size i = 0; i < cf.simulationTimes.size(); ++i) {
        std::vector<const RandomVariable*> tmp(cf.modelIndices[i].size());
        if (cf.simulationTimes[i] == 0.0) {
            for (Size j = 0; j < cf.modelIndices[i].size(); ++j) {
                tmp[j] = &initialValues[cf.modelIndices[i][j]];
            }
        } else {
            auto simTimesIdx = timeIndex(cf.simulationTimes[i], simulationTimes);
            for (Size j = 0; j < cf.modelIndices[i].size(); ++j) {
                tmp[j] = &pathValues[simTimesIdx][cf.modelIndices[i][j]];
            }
        }
        states[i] = tmp;
    }

    auto amount = cf.amountCalculator(n, states) /
                  lgmVectorised_[0].numeraire(
                      cf.payTime, pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
                      discountCurves_[0]);

    if (cf.payCcyIndex > 0) {
        amount *= exp(pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::FX, cf.payCcyIndex - 1)]);
    }

    return amount * RandomVariable(n, cf.payer ? -1.0 : 1.0);
}

void McMultiLegBaseEngine::calculateModels(
    const std::set<Real>& simulationTimes, const std::set<Real>& exerciseXvaTimes, const std::set<Real>& exerciseTimes,
    const std::set<Real>& xvaTimes, const std::vector<CashflowInfo>& cashflowInfo,
    const std::vector<std::vector<RandomVariable>>& pathValues,
    const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
    std::vector<RegressionModel>& regModelUndDirty, std::vector<RegressionModel>& regModelUndExInto,
    std::vector<RegressionModel>& regModelRebate, std::vector<RegressionModel>& regModelContinuationValue,
    std::vector<RegressionModel>& regModelOption, RandomVariable& pathValueUndDirty, RandomVariable& pathValueUndExInto,
    RandomVariable& pathValueOption) const {

    // for each xva and exercise time collect the relevant cashflow amounts and train a model on them

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::vector<RandomVariable> amountCache(cashflowInfo.size());

    Size counter = exerciseXvaTimes.size() - 1;
    auto previousExerciseTime = exerciseTimes.rbegin();

    RandomVariable pathValueRebate;

    auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);
    Size rebateIndex = rebatedExercise ? rebatedExercise->rebates().size() - 1 : Null<Size>();

    for (auto t = exerciseXvaTimes.rbegin(); t != exerciseXvaTimes.rend(); ++t) {

        bool isExerciseTime = exerciseTimes.find(*t) != exerciseTimes.end();
        bool isXvaTime = xvaTimes.find(*t) != xvaTimes.end();

        for (Size i = 0; i < cashflowInfo.size(); ++i) {

            if (cfStatus[i] == CfStatus::done)
                continue;

            /* We assume here that for each time t below the following condition holds: If a cashflow belongs to the
              "exercise into" part of the underlying, it also belongs to the underlying itself on each time t.

              Apart from that we allow for the possibility that a cashflow belongs to the underlying npv without
              belonging to the exercise into underlying at a time t. Such a cashflow would be marked as "cached" at time
              t and transferred to the exercise-into value at the appropriate time t' < t.
            */

            bool isPartOfExercise =
                cashflowInfo[i].payTime >
                    *t - (includeTodaysCashflows_ || exerciseIntoIncludeSameDayFlows_ ? tinyTime : 0.0) &&
                (previousExerciseTime == exerciseTimes.rend() ||
                 cashflowInfo[i].exIntoCriterionTime > *previousExerciseTime);

            bool isPartOfUnderlying = cashflowInfo[i].payTime > *t - (includeTodaysCashflows_ ? tinyTime : 0.0);

            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfExercise) {
                    auto tmp = cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
                    pathValueUndDirty += tmp;
                    pathValueUndExInto += tmp;
                    cfStatus[i] = CfStatus::done;
                } else if (isPartOfUnderlying) {
                    auto tmp = cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
                    pathValueUndDirty += tmp;
                    amountCache[i] = tmp;
                    cfStatus[i] = CfStatus::cached;
                }
            } else if (cfStatus[i] == CfStatus::cached) {
                if (isPartOfExercise) {
                    pathValueUndExInto += amountCache[i];
                    cfStatus[i] = CfStatus::done;
                    amountCache[i].clear();
                }
            }
        }

        // update path value rebate

        if (isExerciseTime && rebatedExercise != nullptr) {
            if (rebatedExercise->rebate(rebateIndex) != 0.0) {
                Size simulationTimes_idx = std::distance(simulationTimes.begin(), simulationTimes.find(*t));
                Real payTime = time(rebatedExercise->rebatePaymentDate(rebateIndex));
                if (payTime >= 0.0) {
                    pathValueRebate = lgmVectorised_[0].reducedDiscountBond(
                                          *t, payTime, pathValues[simulationTimes_idx][0], discountCurves_[0]) *
                                      RandomVariable(calibrationSamples_, rebatedExercise->rebate(rebateIndex));
                } else {
                    pathValueRebate = RandomVariable(calibrationSamples_, 0.0);
                }
            }
            --rebateIndex;
        }

        // update underlying exercise into and rebate regression models

        if (exercise_ != nullptr) {

            regModelUndExInto[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelUndExInto[counter].train(polynomOrder_, polynomType_, pathValueUndExInto, pathValuesRef,
                                             simulationTimes);

            if (pathValueRebate.initialised()) {
                regModelRebate[counter] = RegressionModel(
                    *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                    regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                    regressionMaxSimTimesEq_, regressionVarGroupMode_);
                regModelRebate[counter].train(polynomOrder_, polynomType_, pathValueRebate, pathValuesRef,
                                              simulationTimes);
            }
        }

        if (isExerciseTime) {

            // calculate exerciseValue including rebate

            RandomVariable rebate(calibrationSamples_, 0.0);
            if (regModelRebate[counter].isTrained()) {
                rebate = regModelRebate[counter].apply(model_->stateProcess()->initialValues(), pathValuesRef,
                                                       simulationTimes);
            }

            auto exerciseValue = regModelUndExInto[counter].apply(model_->stateProcess()->initialValues(),
                                                                  pathValuesRef, simulationTimes) +rebate;


            // calculate continuation value, take exercise decition and update option path value

            regModelContinuationValue[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelContinuationValue[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef,
                                                     simulationTimes,
                                                     exerciseValue > RandomVariable(calibrationSamples_, 0.0));
            auto continuationValue = regModelContinuationValue[counter].apply(model_->stateProcess()->initialValues(),
                                                                              pathValuesRef, simulationTimes);
            pathValueOption = conditionalResult(exerciseValue > continuationValue &&
                                                    exerciseValue > RandomVariable(calibrationSamples_, 0.0),
                                                pathValueUndExInto + rebate, pathValueOption);
        }

        if (isXvaTime) {

            regModelUndDirty[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] != CfStatus::open; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelUndDirty[counter].train(
                polynomOrder_, polynomType_,
                useOverwritePathValueUndDirty()
                    ? overwritePathValueUndDirty(*t, pathValueUndDirty, exerciseXvaTimes, pathValues)
                    : pathValueUndDirty,
                pathValuesRef, simulationTimes);
        }

        if (exercise_ != nullptr) {
            regModelOption[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelOption[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes);
        }

        if (isExerciseTime && previousExerciseTime != exerciseTimes.rend())
            std::advance(previousExerciseTime, 1);

        --counter;
    }

    // add the remaining live cashflows to get the underlying value

    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirty += cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
    }
}

void McMultiLegBaseEngine::generatePathValues(const std::vector<Real>& simulationTimes,
                                              std::vector<std::vector<RandomVariable>>& pathValues) const {

    if (simulationTimes.empty())
        return;

    std::set<Real> times(simulationTimes.begin(), simulationTimes.end());

    TimeGrid timeGrid(times.begin(), times.end());

    QuantLib::ext::shared_ptr<StochasticProcess> process = model_->stateProcess();
    if (model_->dimension() == 1) {
        // use lgm process if possible for better performance
        auto tmp = QuantLib::ext::make_shared<IrLgm1fStateProcess>(model_->irlgm1f(0));
        tmp->resetCache(timeGrid.size() - 1);
        process = tmp;
    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        // enable cache
        tmp->resetCache(timeGrid.size() - 1);
    }

    auto pathGenerator = makeMultiPathGenerator(calibrationPathGenerator_, process, timeGrid, calibrationSeed_,
                                                ordering_, directionIntegers_);

    // generated paths always contain t = 0 but simulationTimes might or might not contain t = 0
    Size offset = QuantLib::close_enough(simulationTimes.front(), 0.0) ? 0 : 1;

    for (Size i = 0; i < calibrationSamples_; ++i) {
        const MultiPath& path = pathGenerator->next().value;
        for (Size j = 0; j < simulationTimes.size(); ++j) {
            for (Size k = 0; k < model_->stateProcess()->size(); ++k) {
                pathValues[j][k].data()[i] = path[k][j + offset];
            }
        }
    }
}

void McMultiLegBaseEngine::calculate() const {

    includeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();
    includeTodaysCashflows_ = Settings::instance().includeTodaysCashFlows()
                                  ? *Settings::instance().includeTodaysCashFlows()
                                  : includeReferenceDateEvents_;

    McEngineStats::instance().other_timer.resume();

    // check data set by derived engines

    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");
    QL_REQUIRE(exercise_ == nullptr || optionSettlement_ != Settlement::Cash ||
                   cashSettlementDates_.size() == exercise_->dates().size(),
               "McMultiLegBaseEngine: cash settled exercise is given but cash settlement dates size ("
                   << cashSettlementDates_.size() << ") does not match exercise dates size ("
                   << exercise_->dates().size()
                   << ". Check derived engine and make sure the settlement date is set for cash settled options.");

    // set today's date

    today_ = model_->irlgm1f(0)->termStructure()->referenceDate();

    // set up lgm vectorized instances for each currency

    if (lgmVectorised_.empty()) {
        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            lgmVectorised_.push_back(LgmVectorised(model_->irlgm1f(i)));
        }
    }

    // populate the info to generate the (alive) cashflow amounts

    std::vector<CashflowInfo> cashflowInfo;

    Size legNo = 0;
    for (auto const& leg : leg_) {
        Currency currency = currency_[legNo];
        bool payer = payer_[legNo];
        Size cashflowNo = 0;
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() < today_ || (!includeTodaysCashflows_ && cashflow->date() == today_))
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(CashflowInfo(cashflow, currency, payer, legNo, cashflowNo, model_, lgmVectorised_,
                                                exerciseIntoIncludeSameDayFlows_, tinyTime, cfOnCpnMaxSimTimes_,
                                                cfOnCpnAddSimTimesCutoff_));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    /* build exercise times, cash settlement times */

    std::set<Real> exerciseTimes;
    std::vector<Real> cashSettlementTimes;

    if (exercise_ != nullptr) {

        QL_REQUIRE(exercise_->type() != Exercise::American,
                   "McMultiLegBaseEngine::calculate(): exercise style American is not supported yet.");

        Size counter = 0;
        for (auto const& d : exercise_->dates()) {
            if (d < today_ || (!includeReferenceDateEvents_ && d == today_))
                continue;
            exerciseTimes.insert(time(d));
            if (optionSettlement_ == Settlement::Type::Cash)
                cashSettlementTimes.push_back(time(cashSettlementDates_[counter++]));
        }
    }

    /* build cashflow generation times */

    std::set<Real> cashflowGenTimes;

    for (auto const& info : cashflowInfo) {
        cashflowGenTimes.insert(info.simulationTimes.begin(), info.simulationTimes.end());
        cashflowGenTimes.insert(info.payTime);
    }

    /* build xva times, truncate at max time seen so far, but ensure at least two xva times */

    Real maxTime = 0.0;
    if (auto m = std::max_element(exerciseTimes.begin(), exerciseTimes.end()); m != exerciseTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashSettlementTimes.begin(), cashSettlementTimes.end());
        m != cashSettlementTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashflowGenTimes.begin(), cashflowGenTimes.end()); m != cashflowGenTimes.end())
        maxTime = std::max(maxTime, *m);

    std::set<Real> xvaTimes;

    for (auto const& d : simulationDates_) {
        if (auto t = time(d); t < maxTime + tinyTime) {
            xvaTimes.insert(time(d));
        }
    }

    /* build combined time sets */

    std::set<Real> exerciseXvaTimes; // = exercise + xva times
    std::set<Real> simulationTimes;  // = cashflowGen + exercise + xva times

    exerciseXvaTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    exerciseXvaTimes.insert(xvaTimes.begin(), xvaTimes.end());

    simulationTimes.insert(cashflowGenTimes.begin(), cashflowGenTimes.end());
    simulationTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    simulationTimes.insert(xvaTimes.begin(), xvaTimes.end());

    McEngineStats::instance().other_timer.stop();

    // build simulation times corresponding to close-out grid for sticky runs (if required)

    std::vector<Real> simulationTimesWithCloseOutLag;
    if (recalibrateOnStickyCloseOutDates_ && !stickyCloseOutDates_.empty() && xvaTimes.size() > 0) {
        std::vector<Real> xvaTimesWithCloseOutLag(1, 0.0);
        for (auto const& d : stickyCloseOutDates_) {
            xvaTimesWithCloseOutLag.push_back(time(d));
        }
        std::vector<Real> xvaTimesVec(1, 0.0);
        xvaTimesVec.insert(xvaTimesVec.end(), xvaTimes.begin(), xvaTimes.end());
        Interpolation l = Linear().interpolate(xvaTimesVec.begin(), xvaTimesVec.end(), xvaTimesWithCloseOutLag.begin());
        l.enableExtrapolation();
        std::transform(simulationTimes.begin(), simulationTimes.end(),
                       std::back_inserter(simulationTimesWithCloseOutLag), [&l](const Real t) { return l(t); });
    }

    // simulate the paths for the calibration

    McEngineStats::instance().path_timer.resume();

    QL_REQUIRE(!simulationTimes.empty(),
               "McMultiLegBaseEngine::calculate(): no simulation times, this is not expected.");

    std::vector<std::vector<RandomVariable>> pathValues(
        simulationTimes.size(),
        std::vector<RandomVariable>(model_->stateProcess()->size(), RandomVariable(calibrationSamples_)));
    std::vector<std::vector<const RandomVariable*>> pathValuesRef(
        simulationTimes.size(), std::vector<const RandomVariable*>(model_->stateProcess()->size()));

    for (Size i = 0; i < pathValues.size(); ++i) {
        for (Size j = 0; j < pathValues[i].size(); ++j) {
            pathValues[i][j].expand();
            pathValuesRef[i][j] = &pathValues[i][j];
        }
    }

    std::vector<std::vector<RandomVariable>> closeOutPathValues(
        simulationTimesWithCloseOutLag.size(),
        std::vector<RandomVariable>(model_->stateProcess()->size(), RandomVariable(calibrationSamples_)));
    std::vector<std::vector<const RandomVariable*>> closeOutPathValuesRef(
        simulationTimesWithCloseOutLag.size(), std::vector<const RandomVariable*>(model_->stateProcess()->size()));

    for (Size i = 0; i < closeOutPathValues.size(); ++i) {
        for (Size j = 0; j < closeOutPathValues[i].size(); ++j) {
            closeOutPathValues[i][j].expand();
            closeOutPathValuesRef[i][j] = &closeOutPathValues[i][j];
        }
    }

    generatePathValues(std::vector<Real>(simulationTimes.begin(), simulationTimes.end()), pathValues);
    generatePathValues(simulationTimesWithCloseOutLag, closeOutPathValues);

    McEngineStats::instance().path_timer.stop();

    McEngineStats::instance().calc_timer.resume();

    // setup the models

    std::vector<RegressionModel> regModelUndDirty(exerciseXvaTimes.size());          // available on xva times
    std::vector<RegressionModel> regModelUndExInto(exerciseXvaTimes.size());         // available on xva and ex times
    std::vector<RegressionModel> regModelRebate(exerciseXvaTimes.size());            // available on xva and ex times
    std::vector<RegressionModel> regModelContinuationValue(exerciseXvaTimes.size()); // available on ex times
    std::vector<RegressionModel> regModelOption(exerciseXvaTimes.size());            // available on xva and ex times
    RandomVariable pathValueUndDirty(calibrationSamples_);
    RandomVariable pathValueUndExInto(calibrationSamples_);
    RandomVariable pathValueOption(calibrationSamples_);

    calculateModels(simulationTimes, exerciseXvaTimes, exerciseTimes, xvaTimes, cashflowInfo, pathValues, pathValuesRef,
                    regModelUndDirty, regModelUndExInto, regModelRebate, regModelContinuationValue, regModelOption,
                    pathValueUndDirty, pathValueUndExInto, pathValueOption);

    // setup the models on close-out grid if required or else copy them from valuation

    std::vector<RegressionModel> regModelUndDirtyCloseOut(regModelUndDirty);
    std::vector<RegressionModel> regModelUndExIntoCloseOut(regModelUndExInto);
    std::vector<RegressionModel> regModelRebateCloseOut(regModelRebate);
    std::vector<RegressionModel> regModelContinuationValueCloseOut(regModelContinuationValue);
    std::vector<RegressionModel> regModelOptionCloseOut(regModelOption);

    if (!simulationTimesWithCloseOutLag.empty()) {
        RandomVariable pathValueUndDirtyCloseOut(calibrationSamples_);
        RandomVariable pathValueUndExIntoCloseOut(calibrationSamples_);
        RandomVariable pathValueOptionCloseOut(calibrationSamples_);
        // everything stays the same, we just use the lagged path values
        calculateModels(simulationTimes, exerciseXvaTimes, exerciseTimes, xvaTimes, cashflowInfo, closeOutPathValues,
                        closeOutPathValuesRef, regModelUndDirtyCloseOut, regModelUndExIntoCloseOut,
                        regModelRebateCloseOut, regModelContinuationValueCloseOut, regModelOptionCloseOut,
                        pathValueUndDirtyCloseOut, pathValueUndExIntoCloseOut, pathValueOptionCloseOut);
    }

    // set the result value (= underlying value if no exercise is given, otherwise option value)

    resultUnderlyingNpv_ = expectation(pathValueUndDirty).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
    resultValue_ = exercise_ == nullptr
                       ? resultUnderlyingNpv_
                       : expectation(pathValueOption).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);

    McEngineStats::instance().calc_timer.stop();

    // construct the amc calculator

    amcCalculator_ = QuantLib::ext::make_shared<MultiLegBaseAmcCalculator>(
        externalModelIndices_, optionSettlement_, cashSettlementTimes, exerciseXvaTimes, exerciseTimes, xvaTimes,
        std::array<std::vector<RegressionModel>, 2>{regModelUndDirty, regModelUndDirtyCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelUndExInto, regModelUndExIntoCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelRebate, regModelRebateCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelContinuationValue,
                                                                          regModelContinuationValueCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelOption, regModelOptionCloseOut},
        resultValue_, model_->stateProcess()->initialValues(), model_->irlgm1f(0)->currency(),
        reevaluateExerciseInStickyRun_, includeTodaysCashflows_, includeReferenceDateEvents_);
}

QuantLib::ext::shared_ptr<AmcCalculator> McMultiLegBaseEngine::amcCalculator() const { return amcCalculator_; }

McMultiLegBaseEngine::MultiLegBaseAmcCalculator::MultiLegBaseAmcCalculator(
    const std::vector<Size>& externalModelIndices, const Settlement::Type settlement,
    const std::vector<Time>& cashSettlementTimes, const std::set<Real>& exerciseXvaTimes,
    const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
    const std::array<std::vector<RegressionModel>, 2>& regModelUndDirty,
    const std::array<std::vector<RegressionModel>, 2>& regModelUndExInto,
    const std::array<std::vector<RegressionModel>, 2>& regModelRebate,
    const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValue,
    const std::array<std::vector<RegressionModel>, 2>& regModelOption, const Real resultValue,
    const Array& initialState, const Currency& baseCurrency, const bool reevaluateExerciseInStickyRun,
    const bool includeTodaysCashflows, const bool includeReferenceDateEvents)
    : externalModelIndices_(externalModelIndices), settlement_(settlement), cashSettlementTimes_(cashSettlementTimes),
      exerciseXvaTimes_(exerciseXvaTimes), exerciseTimes_(exerciseTimes), xvaTimes_(xvaTimes),
      regModelUndDirty_(regModelUndDirty), regModelUndExInto_(regModelUndExInto), regModelRebate_(regModelRebate),
      regModelContinuationValue_(regModelContinuationValue), regModelOption_(regModelOption), resultValue_(resultValue),
      initialState_(initialState), baseCurrency_(baseCurrency),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), includeTodaysCashflows_(includeTodaysCashflows),
      includeReferenceDateEvents_(includeReferenceDateEvents) {

    QL_REQUIRE(settlement_ != Settlement::Type::Cash || cashSettlementTimes.size() == exerciseTimes.size(),
               "MultiLegBaseAmcCalculator: settlement type is cash, but cash settlement times ("
                   << cashSettlementTimes.size() << ") does not match exercise times (" << exerciseTimes.size() << ")");
}

std::vector<QuantExt::RandomVariable> McMultiLegBaseEngine::MultiLegBaseAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {

    // check input path consistency

    QL_REQUIRE(!paths.empty(), "MultiLegBaseAmcCalculator::simulatePath(): no future path times, this is not allowed.");
    QL_REQUIRE(pathTimes.size() == paths.size(),
               "MultiLegBaseAmcCalculator::simulatePath(): inconsistent pathTimes size ("
                   << pathTimes.size() << ") and paths size (" << paths.size() << ") - internal error.");
    QL_REQUIRE(relevantPathIndex.size() >= xvaTimes_.size(),
               "MultiLegBaseAmcCalculator::simulatePath() relevant path indexes ("
                   << relevantPathIndex.size() << ") >= xvaTimes (" << xvaTimes_.size()
                   << ") required - internal error.");

    bool stickyCloseOutRun = false;
    std::size_t regModelIndex = 0;

    for (size_t i = 0; i < relevantPathIndex.size(); ++i) {
        if (relevantPathIndex[i] != relevantTimeIndex[i]) {
            stickyCloseOutRun = true;
            regModelIndex = 1;
            break;
        }
    }

    /* put together the relevant simulation times on the input paths and check for consistency with xva times,
       also put together the effective paths by filtering on relevant simulation times and model indices */
    std::vector<std::vector<const RandomVariable*>> effPaths(
        xvaTimes_.size(), std::vector<const RandomVariable*>(externalModelIndices_.size()));

    Size timeIndex = 0;
    for (Size i = 0; i < xvaTimes_.size(); ++i) {
        size_t pathIdx = relevantPathIndex[i];
        for (Size j = 0; j < externalModelIndices_.size(); ++j) {
            effPaths[timeIndex][j] = &paths[pathIdx][externalModelIndices_[j]];
        }
        ++timeIndex;
    }

    // init result vector

    Size samples = paths.front().front().size();
    std::vector<RandomVariable> result(xvaTimes_.size() + 1, RandomVariable(paths.front().front().size(), 0.0));

    // simulate the path: result at first time index is simply the reference date npv

    result[0] = RandomVariable(samples, resultValue_);

    // if we don't have an exercise, we return the dirty npv of the underlying at all times

    if (exerciseTimes_.empty()) {
        Size counter = 0;
        for (auto t : xvaTimes_) {
            Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
            QL_REQUIRE(ind < exerciseXvaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, xva time "
                           << t << " not found in exerciseXvaTimes vector.");
            result[++counter] = regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
        }
        result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
        return result;
    }

    /* if we have an exercise we need to determine the exercise indicators except for a sticky run
       where we reuse the last saved indicators */

    if (!stickyCloseOutRun || reevaluateExerciseInStickyRun_) {

        exercised_ = std::vector<Filter>(exerciseTimes_.size() + 1, Filter(samples, false));
        Size counter = 0;

        Filter wasExercised(samples, false);

        for (auto t : exerciseTimes_) {

            if (xvaTimes_.size() == 0)
                break;

            // find the time in the exerciseXvaTimes vector
            Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
            QL_REQUIRE(ind != exerciseXvaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, exercise time "
                           << t << " not found in exerciseXvaTimes vector.");

            // make the exercise decision

            RandomVariable exerciseValue =
                regModelUndExInto_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

            if (regModelRebate_[regModelIndex][ind].isTrained()) {
                exerciseValue += regModelRebate_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
            }

            RandomVariable continuationValue =
                regModelContinuationValue_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

            exercised_[counter + 1] =
                !wasExercised && exerciseValue > continuationValue && exerciseValue > RandomVariable(samples, 0.0);
            wasExercised = wasExercised || exercised_[counter + 1];

            ++counter;
        }
    }

    // now we can populate the result using the exercise indicators

    Size counter = 0;
    Size xvaCounter = 0;
    Size exerciseCounter = 0;

    Filter wasExercised(samples, false);
    std::map<Real, RandomVariable> cashSettlements;

    for (auto t : exerciseXvaTimes_) {

        if (auto it = exerciseTimes_.find(t); it != exerciseTimes_.end()) {

            // update was exercised based on exercise at the exercise time

            ++exerciseCounter;
            wasExercised = wasExercised || exercised_[exerciseCounter];

            // if cash settled, determine the amount on exercise and until when it is to be included in exposure

            if (settlement_ == Settlement::Type::Cash) {
                RandomVariable cashPayment =
                    regModelUndExInto_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_);
                cashPayment = applyFilter(cashPayment, exercised_[exerciseCounter]);
                cashSettlements[cashSettlementTimes_[exerciseCounter - 1]] = cashPayment;
            }
        }

        if (xvaTimes_.find(t) != xvaTimes_.end()) {

            // there is no continuation value on the last exercise date

            RandomVariable futureOptionValue =
                exerciseCounter == exerciseTimes_.size()
                    ? RandomVariable(samples, 0.0)
                    : max(RandomVariable(samples, 0.0),
                          regModelOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));

            /* Physical Settlement:

               Exercise value is "undExInto" if we are in the period between the date on which the exercise happend
               and the next exercise date after that, otherwise it is the full dirty npv. This assumes that two
               exercise dates d1, d2 are not so close together that a coupon

               - pays after d1, d2
               - but does not belong to the exercise-into underlying for both d1 and d2

               This assumption seems reasonable, since we would never exercise on d1 but wait until d2 since the
               underlying which we exercise into is the same in both cases.
               We don't introduce a hard check for this, but we rather assume that the exercise dates are set up
               appropriately adjusted to the coupon periods. The worst that can happen is that the exercised value
               uses the full dirty npv at a too early time.

               Cash Settlement:

               We use the cashSettlements map constructed on each exercise date.

            */

            RandomVariable exercisedValue(samples, 0.0);

            if (settlement_ == Settlement::Type::Physical) {
                exercisedValue = conditionalResult(
                    exercised_[exerciseCounter],
                    regModelUndExInto_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_),
                    regModelUndDirty_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));
            } else {
                for (auto it = cashSettlements.begin(); it != cashSettlements.end();) {
                    if (t < it->first + (includeTodaysCashflows_ ? tinyTime : -tinyTime)) {
                        exercisedValue += it->second;
                        ++it;
                    } else {
                        it = cashSettlements.erase(it);
                    }
                }
            }

            // update for rebate payments

            if (regModelRebate_[regModelIndex][counter].isTrained()) {
                RandomVariable rebate =
                    regModelRebate_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_);
                rebate = applyFilter(rebate, exercised_[exerciseCounter]);
                exercisedValue += rebate;
            }

            result[xvaCounter + 1] = conditionalResult(wasExercised, exercisedValue, futureOptionValue);

            ++xvaCounter;
        }

        ++counter;
    }

    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

template <class Archive> void McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(Archive& ar, const unsigned int version) {
    ar.template register_type<McMultiLegBaseEngine::MultiLegBaseAmcCalculator>();
    ar& boost::serialization::base_object<AmcCalculator>(*this);

    ar& externalModelIndices_;
    ar& settlement_;
    ar& cashSettlementTimes_;
    ar& exerciseXvaTimes_;
    ar& exerciseTimes_;
    ar& xvaTimes_;

    ar& regModelUndDirty_;
    ar& regModelUndExInto_;
    ar& regModelRebate_;
    ar& regModelContinuationValue_;
    ar& regModelOption_;
    ar& resultValue_;
    ar& initialState_;
    ar& baseCurrency_;
    ar& reevaluateExerciseInStickyRun_;
    ar& includeTodaysCashflows_;
    ar& includeReferenceDateEvents_;
}


template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_iarchive& ar,
                                                                         const unsigned int version);
template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_oarchive& ar,
                                                                         const unsigned int version);


} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator);

