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
    const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const McRegressionModel::RegressorModel regressorModel, const Real regressionVarianceCutoff,
    const bool recalibrateOnStickyCloseOutDates, const bool reevaluateExerciseInStickyRun,
    const Size cfOnCpnMaxSimTimes, const Period& cfOnCpnAddSimTimesCutoff, const Size regressionMaxSimTimesIr,
    const Size regressionMaxSimTimesFx, const Size regressionMaxSimTimesEq,
    const McRegressionModel::VarGroupMode regressionVarGroupMode, const bool generateAdditionalResults)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), polynomOrder_(polynomOrder), polynomType_(polynomType), ordering_(ordering),
      directionIntegers_(directionIntegers), discountCurves_(discountCurves), simulationDates_(simulationDates),
      stickyCloseOutDates_(stickyCloseOutDates), externalModelIndices_(externalModelIndices),
      minimalObsDate_(minimalObsDate), regressorModel_(regressorModel),
      regressionVarianceCutoff_(regressionVarianceCutoff),
      recalibrateOnStickyCloseOutDates_(recalibrateOnStickyCloseOutDates),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), cfOnCpnMaxSimTimes_(cfOnCpnMaxSimTimes),
      cfOnCpnAddSimTimesCutoff_(cfOnCpnAddSimTimesCutoff), regressionMaxSimTimesIr_(regressionMaxSimTimesIr),
      regressionMaxSimTimesFx_(regressionMaxSimTimesFx), regressionMaxSimTimesEq_(regressionMaxSimTimesEq),
      regressionVarGroupMode_(regressionVarGroupMode), generateAdditionalResults_(generateAdditionalResults) {

    if (discountCurves_.empty())
        discountCurves_.resize(model_->components(CrossAssetModel::AssetType::IR));
    else {
        QL_REQUIRE(discountCurves_.size() == model_->components(CrossAssetModel::AssetType::IR),
                   "McMultiLegBaseEngine: " << discountCurves_.size() << " discount curves given, but model has "
                                            << model_->components(CrossAssetModel::AssetType::IR) << " IR components.");
    }
    QL_REQUIRE(cfOnCpnMaxSimTimes >= 0, "McMultiLegBaseEngine: cfOnCpnMaxSimTimes must be non-negative");
    QL_REQUIRE(cfOnCpnAddSimTimesCutoff.length() >= 0,
               "McMultiLegBaseEngine: length of cfOnCpnAddSimTimesCutoff must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesIr >= 0, "McMultiLegBaseEngine: regressionMaxSimTimesIr must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesFx >= 0, "McMultiLegBaseEngine: regressionMaxSimTimesFx must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesEq >= 0, "McMultiLegBaseEngine: regressionMaxSimTimesEq must be non-negative");
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

RandomVariable McMultiLegBaseEngine::cashflowPathValue(const McCashflowInfo& cf,
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
    const std::set<Real>& simulationTimes, const std::set<Real>& exerciseXvaRpaTimes,
    const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes, const std::set<Real>& rpaTimes,
    const Real firstRpaTime, const std::vector<McCashflowInfo>& cashflowInfo,
    const std::vector<McCashflowInfo>& rpaFeeCashflowInfo, const std::vector<std::vector<RandomVariable>>& pathValues,
    const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
    std::vector<McRegressionModel>& regModelUndDirty, std::vector<McRegressionModel>& regModelUndExInto,
    std::vector<McRegressionModel>& regModelRebate, std::vector<McRegressionModel>& regModelContinuationValue,
    std::vector<McRegressionModel>& regModelOption, std::vector<McRegressionModel>& regModelRpaUndDirty,
    std::vector<McRegressionModel>& regModelRpaOption, std::vector<McRegressionModel>& regModelRpaFee,
    RandomVariable& pathValueUndDirty, RandomVariable& pathValueUndExInto, RandomVariable& pathValueOption,
    RandomVariable& pathValueRpaUndDirty, RandomVariable& pathValueRpaOption, RandomVariable& pathValueRpaFee) const {

    // for each xva, exercise, rpa time collect the relevant cashflow amounts and train a model on them

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);
    std::vector<CfStatus> cfStatusRpaFee(rpaFeeCashflowInfo.size(), CfStatus::open);

    std::vector<RandomVariable> amountCache(cashflowInfo.size());

    Size counter = exerciseXvaRpaTimes.size() - 1;
    Size exerciseCounter = exerciseTimes.size();
    auto previousExerciseTime = exerciseTimes.rbegin();

    RandomVariable pathValueRebate;
    Real prevRpaTime = firstRpaTime;

    auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);
    Size rebateIndex = rebatedExercise ? rebatedExercise->rebates().size() - 1 : Null<Size>();

    std::vector<Filter> exercised(exerciseTimes.size() + 1, Filter(calibrationSamples_, false));

    for (auto t = exerciseXvaRpaTimes.rbegin(); t != exerciseXvaRpaTimes.rend(); ++t) {

        bool isExerciseTime = exerciseTimes.find(*t) != exerciseTimes.end();
        bool isXvaTime = xvaTimes.find(*t) != xvaTimes.end();
        bool isRpaTime = rpaTimes.find(*t) != rpaTimes.end();

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

        for (Size i = 0; i < rpaFeeCashflowInfo.size(); ++i) {

            if (cfStatus[i] == CfStatus::done)
                continue;
            bool isPartOfUnderlying = rpaFeeCashflowInfo[i].payTime > *t - (includeTodaysCashflows_ ? tinyTime : 0.0);

            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfUnderlying) {
                    auto tmp = cashflowPathValue(rpaFeeCashflowInfo[i], pathValues, simulationTimes);
                    // ignore rpsSettlesAccrual_
                    pathValueRpaFee += tmp * rpaCreditCurve_->survivalProbability(rpaFeeCashflowInfo[i].payTime);
                    cfStatus[i] = CfStatus::done;
                }
            }
        }

        // update path value rebate

        if (isExerciseTime && rebatedExercise != nullptr) {
            pathValueRebate = RandomVariable(calibrationSamples_, 0.0);
            for (Size k = 0; k < rebatedExercise->rebateCurrencies().size(); ++k) {
                if (rebatedExercise->rebate(rebateIndex, k) != 0.0) {
                    Size ccyIndex = rebatedExercise->rebateCurrency(k).empty()
                                        ? 0
                                        : model_->ccyIndex(rebatedExercise->rebateCurrency(k));
                    Size simulationTimes_idx = std::distance(simulationTimes.begin(), simulationTimes.find(*t));
                    Real payTime = time(rebatedExercise->rebatePaymentDate(rebateIndex));
                    if (payTime >= 0.0) {
                        auto tmpRebate = lgmVectorised_[0].reducedDiscountBond(
                                             *t, payTime, pathValues[simulationTimes_idx][0], discountCurves_[0]) *
                                         rebatedExercise->rebate(rebateIndex, k);
                        if (ccyIndex > 0) {
                            tmpRebate *= exp(pathValues[simulationTimes_idx]
                                                       [model_->pIdx(CrossAssetModel::AssetType::FX, ccyIndex - 1)]);
                        }
                        pathValueRebate += tmpRebate;
                    }
                }
            }
            --rebateIndex;
        }

        // update underlying exercise into and rebate regression models

        if (exercise_ != nullptr) {

            regModelUndExInto[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelUndExInto[counter].train(polynomOrder_, polynomType_, pathValueUndExInto, pathValuesRef,
                                             simulationTimes);

            if (pathValueRebate.initialised()) {
                // FIXME do we need this? the pathValueRebate is known at time t
                regModelRebate[counter] = McRegressionModel(
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

            auto exerciseValue = (nakedOption_ ? 1.0 : (exerciseLong_ ? -1.0 : 1.0)) *
                                     regModelUndExInto[counter].apply(model_->stateProcess()->initialValues(),
                                                                      pathValuesRef, simulationTimes) +
                                 rebate;

            // calculate continuation value, take exercise decision and update option path value

            regModelContinuationValue[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelContinuationValue[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef,
                                                     simulationTimes,
                                                     exerciseValue > RandomVariable(calibrationSamples_, 0.0));
            auto continuationValue = regModelContinuationValue[counter].apply(model_->stateProcess()->initialValues(),
                                                                              pathValuesRef, simulationTimes);
            exercised[exerciseCounter] =
                exerciseValue > continuationValue && exerciseValue > RandomVariable(calibrationSamples_, 0.0);

            pathValueOption =
                conditionalResult(exercised[exerciseCounter], pathValueUndExInto + rebate, pathValueOption);

            --exerciseCounter;
        }

        if (isXvaTime || isRpaTime) {
            regModelUndDirty[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] != CfStatus::open; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelUndDirty[counter].train(
                polynomOrder_, polynomType_,
                useOverwritePathValueUndDirty()
                    ? overwritePathValueUndDirty(*t, pathValueUndDirty, exerciseXvaRpaTimes, pathValues)
                    : pathValueUndDirty,
                pathValuesRef, simulationTimes);
        }

        if (exercise_ != nullptr) {
            regModelOption[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelOption[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes);
        }

        if (isRpaTime) {
            double rpaWeight = rpaParticipationRate_ * rpaCreditCurve_->defaultProbability(prevRpaTime, *t) *
                               (1.0 - rpaRecoveryRate_->value());
            auto undPv = regModelUndDirty[counter].apply(model_->stateProcess()->initialValues(), pathValuesRef,
                                                         simulationTimes);
            pathValueRpaUndDirty += max(0.0, undPv) * rpaWeight;
            if (exercise_ != nullptr) {
                auto optionPv = regModelOption[counter].apply(model_->stateProcess()->initialValues(), pathValuesRef,
                                                              simulationTimes);
                if (nakedOption_) {
                    optionPv = exerciseLong_ * optionPv;
                } else {
                    optionPv = exerciseLong_ * optionPv + undPv;
                }
                pathValueRpaOption += max(0.0, optionPv) * rpaWeight;
            }
            prevRpaTime = *t;
        }

        if (!rpaDiscretizationDates_.empty() && isXvaTime) {
            regModelRpaUndDirty[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] != CfStatus::open; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelRpaOption[counter] = McRegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelRpaFee[counter] = McRegressionModel(
                *t, rpaFeeCashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] != CfStatus::open; }, **model_,
                regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                regressionMaxSimTimesEq_, regressionVarGroupMode_);
            regModelRpaUndDirty[counter].train(polynomOrder_, polynomType_, pathValueRpaUndDirty, pathValuesRef,
                                               simulationTimes);
            regModelRpaOption[counter].train(polynomOrder_, polynomType_, pathValueRpaOption, pathValuesRef,
                                             simulationTimes);
            regModelRpaUndDirty[counter].train(polynomOrder_, polynomType_, pathValueRpaFee, pathValuesRef,
                                               simulationTimes);
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

    for (Size i = 0; i < rpaFeeCashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open) {
            auto tmp = cashflowPathValue(rpaFeeCashflowInfo[i], pathValues, simulationTimes);
            pathValueRpaUndDirty += tmp;
            if (exercise_ != nullptr && !nakedOption_)
                pathValueRpaOption += tmp;
        }
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

std::vector<McCashflowInfo> McMultiLegBaseEngine::generateCashflowInfo(const std::vector<Leg>& legs,
                                                                       const std::vector<Currency>& currencies,
                                                                       const std::vector<bool>& payers) const {
    std::vector<McCashflowInfo> cashflowInfo;
    Size legNo = 0;
    for (auto const& leg : legs) {
        Currency currency = currencies[legNo];
        bool payer = payers[legNo];
        Size cashflowNo = 0;
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() < today_ || (!includeTodaysCashflows_ && cashflow->date() == today_))
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(McCashflowInfo(cashflow, currency, payer, legNo, cashflowNo, model_, lgmVectorised_,
                                                  exerciseIntoIncludeSameDayFlows_, tinyTime, cfOnCpnMaxSimTimes_,
                                                  cfOnCpnAddSimTimesCutoff_));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }
    return cashflowInfo;
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

    auto cashflowInfo = generateCashflowInfo(leg_, currency_, payer_);


    /* populate the info to generate the rpa fee cashflow amounts, we need to treat the separately from
       the "main" cashflows stored in leg which are the underlying of the rpa */

    auto rpaFeeCashflowInfo = generateCashflowInfo(rpaProtectionFee_, rpaProtectionFeeCurrency_,
                                                   std::vector<bool>(rpaProtectionFee_.size(), true));

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

    /* build rpa discretization times */

    std::set<Real> rpaTimes;
    Real firstRpaTime = Null<Real>();
    for (auto const& d : rpaDiscretizationDates_) {
        if (d < today_)
            continue;
        firstRpaTime = time(d);
        if (d == today_)
            continue;
        rpaTimes.insert(time(d));
    }

    /* build cashflow generation times */

    std::set<Real> cashflowGenTimes;

    for (auto const& info : cashflowInfo) {
        cashflowGenTimes.insert(info.simulationTimes.begin(), info.simulationTimes.end());
        cashflowGenTimes.insert(info.payTime);
    }

    for (auto const& info : rpaFeeCashflowInfo) {
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
    if (auto m = std::max_element(rpaTimes.begin(), rpaTimes.end()); m != rpaTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashflowGenTimes.begin(), cashflowGenTimes.end()); m != cashflowGenTimes.end())
        maxTime = std::max(maxTime, *m);

    std::set<Real> xvaTimes;
    bool overshoot = false;
    for (auto d = simulationDates_.begin(); d != simulationDates_.end() && !overshoot; ++d) {
        double t = time(*d);
        xvaTimes.insert(t);
        overshoot = t > maxTime + tinyTime;
    }

    /* build combined time sets */

    std::set<Real> exerciseXvaRpaTimes; // = exercise + xva times + rpa times
    std::set<Real> simulationTimes;     // = cashflowGen + exercise + xva times + rpa times

    exerciseXvaRpaTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    exerciseXvaRpaTimes.insert(xvaTimes.begin(), xvaTimes.end());
    exerciseXvaRpaTimes.insert(rpaTimes.begin(), rpaTimes.end());

    simulationTimes.insert(cashflowGenTimes.begin(), cashflowGenTimes.end());
    simulationTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    simulationTimes.insert(xvaTimes.begin(), xvaTimes.end());
    simulationTimes.insert(rpaTimes.begin(), rpaTimes.end());

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

    /* Interpretation of the reg models:

       nakedOption_ == true:

       UndDirty           :  underlying dirty value              (as seen from us)
       UndExInto          :  underlying exercise-into value      (as seen from us)
       Rebate             :  rebate value                        (assume option long + as seen from us)
       ContinuationValue  :  continuation value, always positive (assume option long + as seen from us)
       Option             :  future option value                 (assume option long + as seen from us)
       RpaUndDirty        :  rpa value on underlying dirty
       RpaOption          :  rpa value on option                 (option including long / short)

       nakedOption_ == false:

       UndDirty           :  as above
       UndExInto          :  as above
       Rebate             :  as above
       ContinuationValue  :  as above
       Option             :  as above
       RpaUndDirty        :  as above
       RpaOption          :  rpa value on underlying + option    (option including long / short) */

    /* The models are available on different times, as indicated */

    std::vector<McRegressionModel> regModelUndDirty(exerciseXvaRpaTimes.size());          // xva, rpa times
    std::vector<McRegressionModel> regModelUndExInto(exerciseXvaRpaTimes.size());         // xva, rpa, ex times
    std::vector<McRegressionModel> regModelRebate(exerciseXvaRpaTimes.size());            // xva and ex times
    std::vector<McRegressionModel> regModelContinuationValue(exerciseXvaRpaTimes.size()); // ex times
    std::vector<McRegressionModel> regModelOption(exerciseXvaRpaTimes.size());            // xva, ex, rpa times
    std::vector<McRegressionModel> regModelRpaUndDirty(exerciseXvaRpaTimes.size());       // xva times
    std::vector<McRegressionModel> regModelRpaOption(exerciseXvaRpaTimes.size());         // xva times
    std::vector<McRegressionModel> regModelRpaFee(exerciseXvaRpaTimes.size());            // xva times

    RandomVariable pathValueUndDirty(calibrationSamples_);
    RandomVariable pathValueUndExInto(calibrationSamples_);
    RandomVariable pathValueOption(calibrationSamples_);
    RandomVariable pathValueRpaUndDirty(calibrationSamples_);
    RandomVariable pathValueRpaOption(calibrationSamples_);
    RandomVariable pathValueRpaFee(calibrationSamples_);

    calculateModels(simulationTimes, exerciseXvaRpaTimes, exerciseTimes, xvaTimes, rpaTimes, firstRpaTime, cashflowInfo,
                    rpaFeeCashflowInfo, pathValues, pathValuesRef, regModelUndDirty, regModelUndExInto, regModelRebate,
                    regModelContinuationValue, regModelOption, regModelRpaUndDirty, regModelRpaOption, regModelRpaFee,
                    pathValueUndDirty, pathValueUndExInto, pathValueOption, pathValueRpaUndDirty, pathValueRpaOption,
                    pathValueRpaFee);

    // setup the models on close-out grid if required or else copy them from valuation-grid

    std::vector<McRegressionModel> regModelUndDirtyCloseOut(regModelUndDirty);
    std::vector<McRegressionModel> regModelUndExIntoCloseOut(regModelUndExInto);
    std::vector<McRegressionModel> regModelRebateCloseOut(regModelRebate);
    std::vector<McRegressionModel> regModelContinuationValueCloseOut(regModelContinuationValue);
    std::vector<McRegressionModel> regModelOptionCloseOut(regModelOption);
    std::vector<McRegressionModel> regModelRpaUndDirtyCloseOut(exerciseXvaRpaTimes.size());
    std::vector<McRegressionModel> regModelRpaOptionCloseOut(exerciseXvaRpaTimes.size());
    std::vector<McRegressionModel> regModelRpaFeeCloseOut(exerciseXvaRpaTimes.size());

    if (!simulationTimesWithCloseOutLag.empty()) {
        RandomVariable pathValueUndDirtyCloseOut(calibrationSamples_);
        RandomVariable pathValueUndExIntoCloseOut(calibrationSamples_);
        RandomVariable pathValueOptionCloseOut(calibrationSamples_);
        RandomVariable pathValueRpaUndDirty(calibrationSamples_);
        RandomVariable pathValueRpaOption(calibrationSamples_);
        RandomVariable pathValueRpaFee(calibrationSamples_);
        // everything stays the same, we just use the lagged path values
        calculateModels(simulationTimes, exerciseXvaRpaTimes, exerciseTimes, xvaTimes, rpaTimes, firstRpaTime,
                        cashflowInfo, rpaFeeCashflowInfo, closeOutPathValues, closeOutPathValuesRef,
                        regModelUndDirtyCloseOut, regModelUndExIntoCloseOut, regModelRebateCloseOut,
                        regModelContinuationValueCloseOut, regModelOptionCloseOut, regModelRpaUndDirtyCloseOut,
                        regModelRpaOptionCloseOut, regModelRpaFeeCloseOut, pathValueUndDirtyCloseOut,
                        pathValueUndExIntoCloseOut, pathValueOptionCloseOut, pathValueRpaUndDirty, pathValueRpaOption,
                        pathValueRpaFee);
    }

    // set the result value for t0

    resultUnderlyingNpv_ = expectation(pathValueUndDirty).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);

    if (rpaDiscretizationDates_.empty()) {

        // regular (non-rpa) case

        resultValue_ = resultUnderlyingNpv_;
        if (exercise_ != nullptr) {
            auto optionPv = expectation(pathValueOption).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
            if (nakedOption_)
                resultValue_ = optionPv;
            else
                resultValue_ += (exerciseLong_ ? 1.0 : -1.0) * optionPv;
        }

    } else {

        // rpa case

        resultValue_ = (rpaProtectionFeePayer_ ? 1.0 : -1.0) *
                           expectation(exercise_ == nullptr ? pathValueUndDirty : pathValueRpaOption).at(0) -
                       expectation(pathValueRpaFee).at(0);
    }

    McEngineStats::instance().calc_timer.stop();

    // construct the amc calculator

    amcCalculator_ = QuantLib::ext::make_shared<MultiLegBaseAmcCalculator>(
        externalModelIndices_, optionSettlement_, cashSettlementTimes, exerciseXvaRpaTimes, exerciseTimes, xvaTimes,
        rpaTimes, exercise_ != nullptr, nakedOption_, exerciseLong_,
        std::array<std::vector<McRegressionModel>, 2>{regModelUndDirty, regModelUndDirtyCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelUndExInto, regModelUndExIntoCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelRebate, regModelRebateCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelContinuationValue, regModelContinuationValueCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelOption, regModelOptionCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelRpaUndDirty, regModelRpaUndDirtyCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelRpaOption, regModelRpaOptionCloseOut},
        std::array<std::vector<McRegressionModel>, 2>{regModelRpaFee, regModelRpaFeeCloseOut}, resultValue_,
        model_->stateProcess()->initialValues(), model_->irlgm1f(0)->currency(), reevaluateExerciseInStickyRun_,
        includeTodaysCashflows_, includeReferenceDateEvents_, !rpaDiscretizationDates_.empty(), rpaProtectionFeePayer_,
        rpaParticipationRate_);
}

QuantLib::ext::shared_ptr<AmcCalculator> McMultiLegBaseEngine::amcCalculator() const { return amcCalculator_; }

McMultiLegBaseEngine::MultiLegBaseAmcCalculator::MultiLegBaseAmcCalculator(
    const std::vector<Size>& externalModelIndices, const Settlement::Type settlement,
    const std::vector<Time>& cashSettlementTimes, const std::set<Real>& exerciseXvaRpaTimes,
    const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes, const std::set<Real>& rpaTimes,
    const bool haveExercise, const bool nakedOption, const bool exerciseLong,
    const std::array<std::vector<McRegressionModel>, 2>& regModelUndDirty,
    const std::array<std::vector<McRegressionModel>, 2>& regModelUndExInto,
    const std::array<std::vector<McRegressionModel>, 2>& regModelRebate,
    const std::array<std::vector<McRegressionModel>, 2>& regModelContinuationValue,
    const std::array<std::vector<McRegressionModel>, 2>& regModelOption,
    const std::array<std::vector<McRegressionModel>, 2>& regModelRpaUndDirty,
    const std::array<std::vector<McRegressionModel>, 2>& regModelRpaOption,
    const std::array<std::vector<McRegressionModel>, 2>& regModelRpaFee, const Real resultValue,
    const Array& initialState, const Currency& baseCurrency, const bool reevaluateExerciseInStickyRun,
    const bool includeTodaysCashflows, const bool includeReferenceDateEvents, const bool isRpa,
    const bool rpaProtectionFeePayer, const Real rpaParticipationRate)
    : externalModelIndices_(externalModelIndices), settlement_(settlement), cashSettlementTimes_(cashSettlementTimes),
      exerciseXvaRpaTimes_(exerciseXvaRpaTimes), exerciseTimes_(exerciseTimes), xvaTimes_(xvaTimes),
      rpaTimes_(rpaTimes), haveExercise_(haveExercise), nakedOption_(nakedOption), exerciseLong_(exerciseLong),
      regModelUndDirty_(regModelUndDirty), regModelUndExInto_(regModelUndExInto), regModelRebate_(regModelRebate),
      regModelContinuationValue_(regModelContinuationValue), regModelOption_(regModelOption),
      regModelRpaUndDirty_(regModelRpaUndDirty), regModelRpaOption_(regModelRpaOption), regModelRpaFee_(regModelRpaFee),
      resultValue_(resultValue), initialState_(initialState), baseCurrency_(baseCurrency),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), includeTodaysCashflows_(includeTodaysCashflows),
      includeReferenceDateEvents_(includeReferenceDateEvents), isRpa_(isRpa),
      rpaProtectionFeePayer_(rpaProtectionFeePayer), rpaParticipationRate_(rpaParticipationRate) {

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

    if (!haveExercise_) {
        Size counter = 0;
        for (auto t : xvaTimes_) {
            Size ind = std::distance(exerciseXvaRpaTimes_.begin(), exerciseXvaRpaTimes_.find(t));
            QL_REQUIRE(ind < exerciseXvaRpaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, xva time "
                           << t << " not found in exerciseXvaRpaTimes vector.");
            if(isRpa_) {
                result[++counter] = regModelRpaUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_) -
                                    regModelRpaFee_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
            } else {
                result[++counter] = regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
            }
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

            // find the time in the exerciseXvaRpaTimes vector
            Size ind = std::distance(exerciseXvaRpaTimes_.begin(), exerciseXvaRpaTimes_.find(t));
            QL_REQUIRE(ind != exerciseXvaRpaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, exercise time "
                           << t << " not found in exerciseXvaRpaTimes vector.");

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

    for (auto t : exerciseXvaRpaTimes_) {

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

            // we distinguish four cases: {haveNakedOption_, !haveNakedOption_} x {isRpa_, !isRpa}

            if (!nakedOption_) {

                if (!isRpa_) {
                    result[xvaCounter + 1] =
                        (regModelUndDirty_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_) +
                         (exerciseLong_ ? 1.0 : -1.0) * max(0.0, regModelOption_[regModelIndex][counter].apply(
                                                                     initialState_, effPaths, xvaTimes_)));
                } else {
                    result[xvaCounter + 1] =
                        (rpaProtectionFeePayer_ ? 1.0 : -1.0) *
                        (rpaParticipationRate_ *
                             regModelRpaOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_) -
                         regModelRpaFee_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));
                }

                result[xvaCounter + 1] = applyInverseFilter(result[xvaCounter + 1], wasExercised);

            } else {

                // there is no continuation value on the last exercise date

                RandomVariable futureOptionValue =
                    exerciseCounter == exerciseTimes_.size()
                        ? RandomVariable(samples, 0.0)
                        : max(0.0, regModelOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));

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

                if (!isRpa_) {
                    result[xvaCounter + 1] =
                        (exerciseLong_ ? 1.0 : -1.0) *
                        conditionalResult(wasExercised, exercisedValue, exerciseLong_ * futureOptionValue);
                } else {
                    result[xvaCounter + 1] =
                        (rpaProtectionFeePayer_ ? 1.0 : -1.0) *
                        (rpaParticipationRate_ *
                             conditionalResult(
                                 wasExercised,
                                 regModelRpaUndDirty_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_),
                                 regModelRpaOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_)) -
                         regModelRpaFee_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));
                }
            }

            ++xvaCounter;
        }

        ++counter;
    }

    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

template <class Archive>
void McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(Archive& ar, const unsigned int version) {
    ar.template register_type<McMultiLegBaseEngine::MultiLegBaseAmcCalculator>();
    ar& boost::serialization::base_object<AmcCalculator>(*this);

    ar & externalModelIndices_;
    ar & settlement_;
    ar & cashSettlementTimes_;
    ar & exerciseXvaRpaTimes_;
    ar & exerciseTimes_;
    ar & xvaTimes_;
    ar & rpaTimes_;
    ar & haveExercise_;
    ar & nakedOption_;
    ar & exerciseLong_;

    ar & regModelUndDirty_;
    ar & regModelUndExInto_;
    ar & regModelRebate_;
    ar & regModelContinuationValue_;
    ar & regModelOption_;
    ar & regModelRpaUndDirty_;
    ar & regModelRpaOption_;
    ar & regModelRpaFee_;
    ar & resultValue_;
    ar & initialState_;
    ar & baseCurrency_;
    ar & reevaluateExerciseInStickyRun_;
    ar & includeTodaysCashflows_;
    ar & includeReferenceDateEvents_;
    ar & isRpa_;
    ar & rpaProtectionFeePayer_;
    ar & rpaParticipationRate_;
}

template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_iarchive& ar,
                                                                                   const unsigned int version);
template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_oarchive& ar,
                                                                                   const unsigned int version);

} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator);
