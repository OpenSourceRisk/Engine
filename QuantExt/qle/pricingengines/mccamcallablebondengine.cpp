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

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/equitycashflow.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/iborfracoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/pricingengines/fdcallablebondevents.hpp>
#include <qle/pricingengines/mccamcallablebondengine.hpp>
#include <qle/pricingengines/numericlgmcallablebondengine.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>
#include <qle/termstructures/effectivebonddiscountcurve.hpp>

namespace QuantExt {

Real getCallPriceAmount(CallableBond::CallabilityData::PriceType priceType, bool includeAccrual, Real price,
                        Real notional, Real accruals) {
    Real priceAmt = price * notional;
    if (priceType == CallableBond::CallabilityData::PriceType::Clean)
        priceAmt += accruals;
    if (!includeAccrual)
        priceAmt -= accruals;
    return priceAmt;
}

McCamCallableBondBaseEngine::McCamCallableBondBaseEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    SobolRsg::DirectionIntegers directionIntegers, const Handle<QuantLib::YieldTermStructure>& referenceCurve,
    const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate,
    const bool spreadOnIncome, const bool generateAdditionalResults, const std::vector<Date>& simulationDates,
    const std::vector<Date>& stickyCloseOutDates, const std::vector<Size>& externalModelIndices,
    const bool minimalObsDate, const RegressionModel::RegressorModel regressorModel,
    const Real regressionVarianceCutoff, const bool recalibrateOnStickyCloseOutDates,
    const bool reevaluateExerciseInStickyRun, const Size cfOnCpnMaxSimTimes, const Period& cfOnCpnAddSimTimesCutoff,
    const Size regressionMaxSimTimesIr, const Size regressionMaxSimTimesFx, const Size regressionMaxSimTimesEq,
    const RegressionModel::VarGroupMode regressionVarGroupMode)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), polynomOrder_(polynomOrder), polynomType_(polynomType), ordering_(ordering),
      directionIntegers_(directionIntegers), referenceCurve_(referenceCurve), discountingSpread_(discountingSpread),
      creditCurve_(creditCurve), incomeCurve_(incomeCurve), recoveryRate_(recoveryRate),
      spreadOnIncome_(spreadOnIncome), generateAdditionalResults_(generateAdditionalResults),
      simulationDates_(simulationDates), stickyCloseOutDates_(stickyCloseOutDates),
      externalModelIndices_(externalModelIndices), minimalObsDate_(minimalObsDate), regressorModel_(regressorModel),
      regressionVarianceCutoff_(regressionVarianceCutoff),
      recalibrateOnStickyCloseOutDates_(recalibrateOnStickyCloseOutDates),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), cfOnCpnMaxSimTimes_(cfOnCpnMaxSimTimes),
      cfOnCpnAddSimTimesCutoff_(cfOnCpnAddSimTimesCutoff), regressionMaxSimTimesIr_(regressionMaxSimTimesIr),
      regressionMaxSimTimesFx_(regressionMaxSimTimesFx), regressionMaxSimTimesEq_(regressionMaxSimTimesEq),
      regressionVarGroupMode_(regressionVarGroupMode) {

    QL_REQUIRE(cfOnCpnMaxSimTimes >= 0, "McCamCallableBondBaseEngine: cfOnCpnMaxSimTimes must be non-negative");
    QL_REQUIRE(cfOnCpnAddSimTimesCutoff.length() >= 0,
               "McCamCallableBondBaseEngine: length of cfOnCpnAddSimTimesCutoff must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesIr >= 0,
               "McCamCallableBondBaseEngine: regressionMaxSimTimesIr must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesFx >= 0,
               "McCamCallableBondBaseEngine: regressionMaxSimTimesFx must be non-negative");
    QL_REQUIRE(regressionMaxSimTimesEq >= 0,
               "McCamCallableBondBaseEngine: regressionMaxSimTimesEq must be non-negative");
}

Real McCamCallableBondBaseEngine::time(const Date& d) const {
    return model_->irlgm1f(0)->termStructure()->timeFromReference(d);
}

Size McCamCallableBondBaseEngine::timeIndex(const Time t, const std::set<Real>& times) const {
    auto it = times.find(t);
    QL_REQUIRE(it != times.end(), "McCamCallableBondBaseEngine::cashflowPathValue(): time ("
                                      << t
                                      << ") not found in simulation times. This is an internal error. Contact dev.");
    return std::distance(times.begin(), it);
}

RandomVariable McCamCallableBondBaseEngine::cashflowPathValue(
    const CashflowInfo& cf, const std::vector<std::vector<RandomVariable>>& pathValues,
    const std::set<Real>& simulationTimes, const Handle<YieldTermStructure>& discountCurve) const {

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
    /*
    std::cout << "McCamCallableBondBaseEngine::cashflowPathValue(): calculating amount for cf legNo " << cf.legNo
              << " cfNo " << cf.cfNo << " payTime " << cf.payTime << " payCcyIndex " << cf.payCcyIndex << " payer "
              << cf.payer << " amount " << cf.amountCalculator(n, states)[0] << " " << cf.amountCalculator(n, states)[1]
              << "numerarie "
              << lgmVectorised_[0].numeraire(
                     cf.payTime, pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
                     discountCurve)[0]
              << " df " << discountCurve->discount(cf.payTime) << std::endl;
    */
    auto amount =
        cf.amountCalculator(n, states) /
        lgmVectorised_[0].numeraire(
            cf.payTime, pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::IR, 0)], discountCurve);

    if (cf.payCcyIndex > 0) {
        amount *= exp(pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::FX, cf.payCcyIndex - 1)]);
    }

    return amount * RandomVariable(n, cf.payer ? -1.0 : 1.0);
}

void McCamCallableBondBaseEngine::calculateModels(
    Handle<YieldTermStructure> discountCurve, const std::set<Real>& simulationTimes,
    const std::set<Real>& exerciseXvaTimes, const std::set<Real> exerciseTimes,
    const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
    const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes, const std::set<Real>& xvaTimes,
    const std::vector<CashflowInfo>& cashflowInfo, const std::vector<std::vector<RandomVariable>>& pathValues,
    const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
    std::vector<RegressionModel>& regModelUndDirty, std::vector<RegressionModel>& regModelContinuationValueCall,
    std::vector<RegressionModel>& regModelContinuationValuePut, std::vector<RegressionModel>& regModelOption,
    RandomVariable& pathValueUndDirty, RandomVariable& pathValueOption,
    std::vector<RandomVariable>& pathExerciseProbsCall, std::vector<RandomVariable>& pathExerciseProbsPut) const {

    // for each xva and exercise time collect the relevant cashflow amounts and train a model on them

    enum class CfStatus { open, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::vector<RandomVariable> amountCache(cashflowInfo.size());

    Size counter = exerciseXvaTimes.size() - 1;
    Size callTimeIdx = 1;
    Size putTimeIdx = 1;

    QuantExt::CurrentNotionalAccrualsCalculator notionalAccrualCalc(today_, notionals_.front(), leg_,
                                                                    *model_->irlgm1f(0)->termStructure());

    for (auto t = exerciseXvaTimes.rbegin(); t != exerciseXvaTimes.rend(); ++t) {

        bool isExerciseTime = exerciseTimes.find(*t) != exerciseTimes.end();
        /*
        std::cout << "McCamCallableBondBaseEngine::calculateModels(): processing time " << *t
                  << (isExerciseTime ? " (exercise time)" : "") << std::endl;
        */
        auto callDataIt = callTimes.find(*t);
        bool isCallTime = callDataIt != callTimes.end();
        auto putDataIt = putTimes.find(*t);
        bool isPutTime = putDataIt != putTimes.end();
        QL_REQUIRE(!isExerciseTime || (isCallTime || isPutTime),
                   "McCamCallableBondBaseEngine::calculateModels(): exercise time "
                       << *t << " is not marked as call or put time");
        /*
        bool isXvaTime = xvaTimes.find(*t) != xvaTimes.end();
        
        std::cout << "McCamCallableBondBaseEngine::calculateModels(): processing time " << *t
                  << (isExerciseTime ? " (exercise time)" : "") << (isXvaTime ? " (xva time)" : "") << std::endl;
        */
        for (Size i = 0; i < cashflowInfo.size(); ++i) {

            if (cfStatus[i] == CfStatus::done)
                continue;

            // We consider the accruals in the exercise decision already, so we include also accrued cashflows here

            bool isPartOfUnderlying = cashflowInfo[i].payTime > *t - (includeTodaysCashflows_ ? tinyTime : 0.0);
            /*
            std::cout << "  cashflow " << i << ": payTime " << cashflowInfo[i].payTime << ", exIntoCriterionTime "
                      << cashflowInfo[i].exIntoCriterionTime << ", isPartOfUnderlying " << isPartOfUnderlying
                      << ", status " << (cfStatus[i] == CfStatus::open ? "open" : "done") << std::endl;
            */
            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfUnderlying) {
                    auto tmp = cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes, discountCurve);
                    pathValueUndDirty += tmp;
                    amountCache[i] = tmp;
                    cfStatus[i] = CfStatus::done;
                }
            }
            //std::cout << ", status (after) " << (cfStatus[i] == CfStatus::open ? "open" : "done") << std::endl;
        }

        // Since we take the accruals into account in the exercise price, we
        /*
        for (size_t i = 0; i < 10; ++i) {
            std::cout << " pathValueUndirty[" << i << "] = " << pathValueUndDirty[i] << " todays value "
                      << pathValueUndDirty[i] * model_->numeraire(0, 0, 0, discountCurve) << std::endl;
        }
        */
        regModelUndDirty[counter] = RegressionModel(
            *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
            regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
            regressionMaxSimTimesEq_, regressionVarGroupMode_);
        regModelUndDirty[counter].train(polynomOrder_, polynomType_, pathValueUndDirty, pathValuesRef, simulationTimes);
        if (isExerciseTime) {
            RandomVariable exerciseValue = regModelUndDirty[counter].apply(model_->stateProcess()->initialValues(),
                                                                           pathValuesRef, simulationTimes);
            auto numeraire = lgmVectorised_[0].numeraire(
                *t, pathValues[timeIndex(*t, simulationTimes)][model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
                discountCurve);

            RandomVariable exerciseValueCall(calibrationSamples_, 0.0);
            if (isCallTime) // is call price
            {
                auto& [_, callData] = *callDataIt;
                QL_REQUIRE(callTimes.size() >= callTimeIdx,
                           "We processing the " << callTimeIdx << " call event, but there should be only "
                                                << callTimes.size() << " events.");
                auto callPriceAmount =
                    getCallPriceAmount(callData->priceType, callData->includeAccrual, callData->price,
                                       notionalAccrualCalc.notional(*t), notionalAccrualCalc.accrual(*t));
                auto strikePrice = RandomVariable(calibrationSamples_, callPriceAmount) / numeraire;
                RandomVariable itm = !putTimes.empty() ? pathValueOption : RandomVariable(calibrationSamples_, 0.0);

                exerciseValueCall = strikePrice - exerciseValue;
                regModelContinuationValueCall[counter] = RegressionModel(
                    *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                    regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                    regressionMaxSimTimesEq_, regressionVarGroupMode_);
                regModelContinuationValueCall[counter].train(
                    polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes,
                    exerciseValueCall < RandomVariable(calibrationSamples_, 0.0));
                auto continuationValue = regModelContinuationValueCall[counter].apply(
                    model_->stateProcess()->initialValues(), pathValuesRef, simulationTimes);

                auto exerciseFilter = exerciseValueCall < continuationValue;

                pathValueOption = conditionalResult(exerciseFilter, exerciseValueCall, pathValueOption);
                Size callIdx = callTimes.size() - callTimeIdx;
                callTimeIdx++;
                pathExerciseProbsCall[callIdx] =
                    conditionalResult(exerciseFilter, RandomVariable(calibrationSamples_, 1.0) / numeraire,
                                      pathExerciseProbsCall[callIdx]);
                for (int i = callIdx + 1; i < pathExerciseProbsCall.size(); ++i) {
                    pathExerciseProbsCall[i] = conditionalResult(
                        exerciseFilter, RandomVariable(calibrationSamples_, 0.0), pathExerciseProbsCall[i]);
                }
                if (!putTimes.empty()) {
                    Size putIdx = putTimes.size() - putTimeIdx;
                    for (int i = putIdx + 1; i < putTimes.size(); ++i) {
                        pathExerciseProbsPut[i] = conditionalResult(
                            exerciseFilter, RandomVariable(calibrationSamples_, 0.0), pathExerciseProbsPut[i]);
                    }
                }
            }

            if (isPutTime) {
                auto& [_, putData] = *putDataIt;
                QL_REQUIRE(putTimes.size() >= putTimeIdx, "We processing the "
                                                              << putTimeIdx << " put event, but there should be only "
                                                              << putTimes.size() << " events.");
                auto putPriceAmount =
                    getCallPriceAmount(putData->priceType, putData->includeAccrual, putData->price,
                                       notionalAccrualCalc.notional(*t), notionalAccrualCalc.accrual(*t));
                auto strikePrice = RandomVariable(calibrationSamples_, putPriceAmount) / numeraire;
                RandomVariable exerciseValuePut = strikePrice - exerciseValue;
                regModelContinuationValuePut[counter] = RegressionModel(
                    *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                    regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
                    regressionMaxSimTimesEq_, regressionVarGroupMode_);
                regModelContinuationValuePut[counter].train(
                    polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes,
                    exerciseValuePut > RandomVariable(calibrationSamples_, 0.0));
                auto continuationValue = regModelContinuationValuePut[counter].apply(
                    model_->stateProcess()->initialValues(), pathValuesRef, simulationTimes);
                pathValueOption =
                    conditionalResult(exerciseValuePut > continuationValue, exerciseValuePut, pathValueOption);
                Size putIdx = putTimes.size() - putTimeIdx;
                putTimeIdx++;

                pathExerciseProbsPut[putIdx] = conditionalResult(
                    exerciseValuePut > continuationValue && exerciseValuePut > RandomVariable(calibrationSamples_, 0.0),
                    RandomVariable(calibrationSamples_, 1.0) / numeraire, pathExerciseProbsPut[putIdx]);

                for (int i = putIdx + 1; i < putTimes.size(); ++i) {
                    pathExerciseProbsPut[i] =
                        conditionalResult(exerciseValuePut > continuationValue &&
                                              exerciseValuePut > RandomVariable(calibrationSamples_, 0.0),
                                          RandomVariable(calibrationSamples_, 0.0), pathExerciseProbsPut[i]);
                }
                if (!callTimes.empty()) {
                    Size callIdx = callTimes.size() - callTimeIdx;
                    for (int i = callIdx + 1; i < pathExerciseProbsCall.size(); ++i) {
                        pathExerciseProbsCall[i] =
                            conditionalResult(exerciseValuePut > continuationValue &&
                                                  exerciseValuePut > RandomVariable(calibrationSamples_, 0.0),
                                              RandomVariable(calibrationSamples_, 0.0), pathExerciseProbsCall[i]);
                    }
                }
            }
        }

        regModelOption[counter] = RegressionModel(
            *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
            regressorModel_, regressionVarianceCutoff_, regressionMaxSimTimesIr_, regressionMaxSimTimesFx_,
            regressionMaxSimTimesEq_, regressionVarGroupMode_);
        regModelOption[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes);

        --counter;
    }

    // add the remaining live cashflows to get the underlying value

    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirty += cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes, discountCurve);
    }
}

void McCamCallableBondBaseEngine::generatePathValues(const std::vector<Real>& simulationTimes,
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

void McCamCallableBondBaseEngine::calculate() const {
    
    today_ = model_->irlgm1f(0)->termStructure()->referenceDate();
    includeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();
    /*

        includeTodaysCashflows_ = Settings::instance().includeTodaysCashFlows()
                                      ? *Settings::instance().includeTodaysCashFlows()
                                      : includeReferenceDateEvents_;
    */
    // 0 if there are no cashflows in the underlying bond, we do not calculate anyything

    if (leg_.empty())
        return;

    // 1 set effective discount, income and credit curve

    QL_REQUIRE(!referenceCurve_.empty(), "NumericLgmCallableBondEngineBase::calculate(): reference curve is empty. "
                                         "Check reference data and errors from curve building.");

    auto effCreditCurve =
        creditCurve_.empty()
            ? Handle<DefaultProbabilityTermStructure>(
                  QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(today_, 0.0, referenceCurve_->dayCounter()))
            : creditCurve_;

    auto effIncomeCurve = incomeCurve_.empty() ? referenceCurve_ : incomeCurve_;
    if (spreadOnIncome_ && !discountingSpread_.empty())
        effIncomeCurve = Handle<YieldTermStructure>(
            QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(effIncomeCurve, discountingSpread_));

    auto effDiscountCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<EffectiveBondDiscountCurve>(
        referenceCurve_, creditCurve_, discountingSpread_, recoveryRate_));

    //std::cout << "built effDiscountCurve" << std::endl;

    // set up lgm vectorized instances for each currency

    QL_REQUIRE(model_->components(CrossAssetModel::AssetType::IR) > 0 &&
                   model_->components(CrossAssetModel::AssetType::IR) <= 2,
               "McCamCallableBondBaseEngine::calculate(): model nead at least on IR component and at most two IR0 "
               "components.");

    if (lgmVectorised_.empty()) {
        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            lgmVectorised_.push_back(LgmVectorised(model_->irlgm1f(i)));
        }
    }
    //std::cout << "set up lgm vectorised instances" << std::endl;
    // populate the info to generate the (alive) cashflow amounts

    std::vector<CashflowInfo> cashflowInfo;

    bool payer = false;
    Size cashflowNo = 0;
    for (auto const& cashflow : leg_) {
        // we can skip cashflows that are paid
        if (cashflow->date() < today_ || (!includeTodaysCashflows_ && cashflow->date() == today_))
            continue;
        // for an alive cashflow, populate the data
        cashflowInfo.push_back(CashflowInfo(cashflow, currency_, payer, 0, cashflowNo, model_, lgmVectorised_,
                                            exerciseIntoIncludeSameDayFlows_, tinyTime, cfOnCpnMaxSimTimes_,
                                            cfOnCpnAddSimTimesCutoff_));
        // increment counter
        ++cashflowNo;
    }

    // BUILD same grid as FD engine
    std::vector<NumericLgmMultiLegOptionEngine::CashflowInfo> cashflows;

    for (Size i = 0; i < leg_.size(); ++i) {
        cashflows.push_back(NumericLgmMultiLegOptionEngine::buildCashflowInfo(
            leg_[i], 1.0, [this](const Date& d) { return model_->irlgm1f(0)->termStructure()->timeFromReference(d); },
            Exercise::Type::American, true, 0 * Days, NullCalendar(), Unadjusted, "cashflow " + std::to_string(i)));
    }

    // 3 set up events

    FdCallableBondEvents events(today_, model_->irlgm1f(0)->termStructure()->dayCounter());

    // 3a bond cashflows

    for (Size i = 0; i < cashflows.size(); ++i) {
        if (cashflows[i].payDate > today_) {
            events.registerBondCashflow(cashflows[i]);
        }
    }

    // 3b call and put data

    for (auto const& c : callData_) {
        events.registerCall(c);
    }
    for (auto const& c : putData_) {
        events.registerPut(c);
    }

    // 4 set up time grid

    QL_REQUIRE(!events.times().empty(), "NumericLgmCallableBondEngine: internal error, times are empty");

    Size effectiveTimeStepsPerYear = americanExerciseInterval_;

    TimeGrid grid;
    if (effectiveTimeStepsPerYear == 0) {
        grid = TimeGrid(events.times().begin(), events.times().end());
    } else {
        Size steps = std::max<Size>(std::lround(effectiveTimeStepsPerYear * (*events.times().rbegin()) + 0.5), 1);
        grid = TimeGrid(events.times().begin(), events.times().end(), steps);
        /*
        std::cout << "NumericLgmCallableBondEngineBase::calculate(): using time grid with " << grid.size() - 1
                  << " steps (effectiveTimeStepsPerYear=" << effectiveTimeStepsPerYear << ")" << std::endl;
        for (const auto t : grid) {
            std::cout << "  grid time: " << t << std::endl;
        }
            */
    }
    events.finalise(grid);

    /* build exercise times, cash settlement times */

    std::set<Real> exerciseTimes;

    std::vector<Real> cashSettlementTimes;

    /* build cashflow generation times */

    std::set<Real> cashflowGenTimes;

    for (auto const& info : cashflowInfo) {
        cashflowGenTimes.insert(info.simulationTimes.begin(), info.simulationTimes.end());
        cashflowGenTimes.insert(info.payTime);
    }
    /*
    std::set<Real> callExerciseTimes;
    bool hasAmericanCallExercise = false;
    Date earliestAmericanCallExerciseDate = Date::maxDate();
    Date latestAmericanCallExerciseDate = Date::minDate();
    for (const auto& cd : callData_) {
        if (cd.exerciseDate > today_) {
            callExerciseTimes.insert(time(cd.exerciseDate));
        }
        hasAmericanCallExercise = hasAmericanCallExercise ||
                              (cd.exerciseType == CallableBond::CallabilityData::ExerciseType::FromThisDateOn);
        earliestAmericanCallExerciseDate = std::min(earliestAmericanCallExerciseDate, cd.exerciseDate);
        latestAmericanCallExerciseDate = std::max(latestAmericanCallExerciseDate, cd.exerciseDate);
    }

    std::set<Real> putExerciseTimes;
    Date earliestAmericanPutExerciseDate = Date::maxDate();
    Date latestAmericanPutExerciseDate = Date::minDate();
    bool hasAmericanPutExercise = false;
    for (const auto& cd : putData_) {
        if (cd.exerciseDate > today_) {
            putExerciseTimes.insert(time(cd.exerciseDate));
        }
        hasAmericanPutExercise = hasAmericanPutExercise ||
                              (cd.exerciseType == CallableBond::CallabilityData::ExerciseType::FromThisDateOn);
        earliestAmericanPutExerciseDate = std::min(earliestAmericanPutExerciseDate, cd.exerciseDate);
        latestAmericanPutExerciseDate = std::max(latestAmericanPutExerciseDate, cd.exerciseDate);
    }

    exerciseTimes.insert(callExerciseTimes.begin(), callExerciseTimes.end());
    exerciseTimes.insert(putExerciseTimes.begin(), putExerciseTimes.end());
    */
    std::map<double, ext::shared_ptr<CallableBond::CallabilityData>> callMap;
    for (const auto& cd : callData_) {
        callMap[time(cd.exerciseDate)] = QuantLib::ext::make_shared<CallableBond::CallabilityData>(cd);
    }

    std::map<double, ext::shared_ptr<CallableBond::CallabilityData>> putMap;
    for (const auto& pd : putData_) {
        putMap[time(pd.exerciseDate)] = QuantLib::ext::make_shared<CallableBond::CallabilityData>(pd);
    }

    std::map<double, ext::shared_ptr<CallableBond::CallabilityData>> callTimes;
    std::map<double, ext::shared_ptr<CallableBond::CallabilityData>> putTimes;

    for (size_t i = 0; i < grid.size(); ++i) {
        double t = grid[i];
        if (events.hasCall(i)) {
            auto cd = events.getCallData(i);
            callTimes[t] = QuantLib::ext::make_shared<CallableBond::CallabilityData>();
            callTimes[t]->includeAccrual = cd.includeAccrual;
            callTimes[t]->price = cd.price;
            callTimes[t]->priceType = cd.priceType;
            exerciseTimes.insert(t);
        }
        if (events.hasPut(i)) {
            auto pd = events.getPutData(i);
            putTimes[t] = QuantLib::ext::make_shared<CallableBond::CallabilityData>();
            putTimes[t]->includeAccrual = pd.includeAccrual;
            putTimes[t]->price = pd.price;
            putTimes[t]->priceType = pd.priceType;
            exerciseTimes.insert(t);
        }
    }

    /*
    if (events.hasAmericanExercise()) {
        Time start = std::max(time(today_), *exerciseTimes.begin());
        Time end = *exerciseTimes.rbegin();
        Size steps =  std::max<Size>(std::lround(americanExerciseInterval_ * (end - start) + 0.5), 1);
        TimeGrid exerciseGrid(exerciseTimes.begin(), exerciseTimes.end(), steps);
        for (Size i = 0; i < exerciseGrid.size(); ++i) {
            if (hasAmericanCallExercise && exerciseGrid[i] >= time(earliestAmericanCallExerciseDate) &&
                exerciseGrid[i] <= time(latestAmericanCallExerciseDate)) {
                // Find callability data for this
                auto it = callMap.lower_bound(exerciseGrid[i]);
                auto& [timeKey, callDataPtr] = *it;
                if (QuantLib::close_enough(timeKey, exerciseGrid[i])) {
                    callTimes[exerciseGrid[i]] = callDataPtr;
                    exerciseTimes.insert(exerciseGrid[i]);
                } else if (it != callMap.begin()) {
                    auto prevIt = std::prev(it);
                    auto& [prevTimeKey, prevCallDataPtr] = *prevIt;
                    if (prevCallDataPtr->exerciseType == CallableBond::CallabilityData::ExerciseType::FromThisDateOn){
                        callTimes[exerciseGrid[i]] = prevCallDataPtr;
                        exerciseTimes.insert(exerciseGrid[i]);
                    }
                }
            }
            if (hasAmericanPutExercise && exerciseGrid[i] >= time(earliestAmericanPutExerciseDate) &&
                exerciseGrid[i] <= time(latestAmericanPutExerciseDate)) {
                auto it = putMap.lower_bound(exerciseGrid[i]);
                auto& [timeKey, putDataPtr] = *it;
                if (QuantLib::close_enough(timeKey, exerciseGrid[i])) {
                    putTimes[exerciseGrid[i]] = putDataPtr;
                    exerciseTimes.insert(exerciseGrid[i]);
                } else if (it != callMap.begin()) {
                    auto prevIt = std::prev(it);
                    auto& [prevTimeKey, prevPutDataPtr] = *prevIt;
                    if (prevPutDataPtr->exerciseType == CallableBond::CallabilityData::ExerciseType::FromThisDateOn) {
                        putTimes[exerciseGrid[i]] = prevPutDataPtr;
                        exerciseTimes.insert(exerciseGrid[i]);
                    }
                }
            }
        }
    }
    */
    /* build xva times, truncate at max time seen so far, but ensure at least two xva times */

    Real maxTime = 0.0;
    if (auto m = std::max_element(exerciseTimes.begin(), exerciseTimes.end()); m != exerciseTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashSettlementTimes.begin(), cashSettlementTimes.end());
        m != cashSettlementTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashflowGenTimes.begin(), cashflowGenTimes.end()); m != cashflowGenTimes.end())
        maxTime = std::max(maxTime, *m);

    std::set<Real> xvaTimes{0.0};

    for (auto const& d : simulationDates_) {
        if (auto t = time(d); t < maxTime + tinyTime) {
            xvaTimes.insert(time(d));
        }
    }
    /*
    std::cout << "exercise Times" << std::endl;
    for (auto const& t : exerciseTimes) {
        //std::cout << "  " << t << std::endl;
    }
    */
    /* build combined time sets */

    std::set<Real> exerciseXvaTimes;
    std::set<Real> simulationTimes; // = cashflowGen + exercise + xva times

    exerciseXvaTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    exerciseXvaTimes.insert(xvaTimes.begin(), xvaTimes.end());

    simulationTimes.insert(cashflowGenTimes.begin(), cashflowGenTimes.end());
    simulationTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    simulationTimes.insert(xvaTimes.begin(), xvaTimes.end());

    // McEngineStats::instance().other_timer.stop();

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

    // McEngineStats::instance().path_timer.resume();

    QL_REQUIRE(!simulationTimes.empty(),
               "McCamCallableBondBaseEngine::calculate(): no simulation times, this is not expected.");

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

    // McEngineStats::instance().path_timer.stop();

    // McEngineStats::instance().calc_timer.resume();

    // setup the models

    std::vector<RegressionModel> regModelUndDirty(exerciseXvaTimes.size());              // available on xva times
    std::vector<RegressionModel> regModelContinuationValueCall(exerciseXvaTimes.size()); // available on ex times
    std::vector<RegressionModel> regModelContinuationValuePut(exerciseXvaTimes.size());  // available on ex times
    std::vector<RegressionModel> regModelOption(exerciseXvaTimes.size()); // available on xva and ex times
    RandomVariable pathValueUndDirty(calibrationSamples_);
    RandomVariable pathValueOption(calibrationSamples_);
    std::vector<RandomVariable> pathExercisesCall(exerciseTimes.size(), RandomVariable(calibrationSamples_));
    std::vector<RandomVariable> pathExercisesPut(exerciseTimes.size(), RandomVariable(calibrationSamples_));

    calculateModels(effDiscountCurve, simulationTimes, exerciseXvaTimes, exerciseTimes, callTimes, putTimes, xvaTimes,
                    cashflowInfo, pathValues, pathValuesRef, regModelUndDirty, regModelContinuationValueCall,
                    regModelContinuationValuePut, regModelOption, pathValueUndDirty, pathValueOption, pathExercisesCall,
                    pathExercisesPut);

    // setup the models on close-out grid if required or else copy them from valuation

    std::vector<RegressionModel> regModelUndDirtyCloseOut(regModelUndDirty);
    std::vector<RegressionModel> regModelContinuationValueCallCloseOut(
        regModelContinuationValueCall); // available on ex times
    std::vector<RegressionModel> regModelContinuationValuePutCloseOut(
        regModelContinuationValuePut); // available on ex times
    std::vector<RegressionModel> regModelOptionCloseOut(regModelOption);

    if (!simulationTimesWithCloseOutLag.empty()) {
        RandomVariable pathValueUndDirtyCloseOut(calibrationSamples_);
        RandomVariable pathValueUndExIntoCloseOut(calibrationSamples_);
        RandomVariable pathValueOptionCloseOut(calibrationSamples_);
        std::vector<RandomVariable> pathExercisesCallCloseOut(exerciseTimes.size(),
                                                              RandomVariable(calibrationSamples_));
        std::vector<RandomVariable> pathExercisesPutCloseOut(exerciseTimes.size(), RandomVariable(calibrationSamples_));

        // everything stays the same, we just use the lagged path values
        calculateModels(effDiscountCurve, simulationTimes, exerciseXvaTimes, exerciseTimes, callTimes, putTimes,
                        xvaTimes, cashflowInfo, closeOutPathValues, closeOutPathValuesRef, regModelUndDirtyCloseOut,
                        regModelContinuationValueCallCloseOut, regModelContinuationValuePutCloseOut,
                        regModelOptionCloseOut, pathValueUndDirtyCloseOut, pathValueUndExIntoCloseOut,
                        pathExercisesCallCloseOut, pathExercisesPutCloseOut);
    }

    // set the result value (= underlying value if no exercise is given, otherwise option value)

    resultUnderlyingNpv_ = expectation(pathValueUndDirty).at(0) * model_->numeraire(0, 0.0, 0.0, effDiscountCurve);
    resultValue_ = expectation(pathValueOption).at(0) * model_->numeraire(0, 0.0, 0.0, effDiscountCurve);

    auto notionalAccrualCalc = ext::make_shared<QuantExt::CurrentNotionalAccrualsCalculator>(today_, notionals_.front(), leg_,
                                                                    *model_->irlgm1f(0)->termStructure());

    amcCalculator_ = QuantLib::ext::make_shared<CallableBondAmcCalculator>(
        externalModelIndices_, exerciseXvaTimes, exerciseTimes, xvaTimes, callTimes, putTimes,
        std::array<std::vector<RegressionModel>, 2>{regModelUndDirty, regModelUndDirtyCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelContinuationValueCall,
                                                    regModelContinuationValueCallCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelContinuationValuePut, regModelContinuationValuePutCloseOut},
        std::array<std::vector<RegressionModel>, 2>{regModelOption, regModelOptionCloseOut}, resultValue_,
        model_->stateProcess()->initialValues(), model_->irlgm1f(0)->currency(), reevaluateExerciseInStickyRun_,
        includeTodaysCashflows_, includeReferenceDateEvents_, notionalAccrualCalc);

    std::vector<double> times;
    std::vector<double> pathCallPrices;
    std::vector<double> pathCallAccruals;
    std::vector<double> pathCallNotionals;
    for (size_t i = 0; i < exerciseXvaTimes.size(); ++i) {
        times.push_back(*std::next(exerciseXvaTimes.begin(), i));
        double t = times.back();
        pathCallAccruals.push_back(notionalAccrualCalc->accrual(t));
        pathCallNotionals.push_back(notionalAccrualCalc->notional(t));
        auto it = callTimes.find(t);
        bool isCallDate = it != callTimes.end();
        if (isCallDate) {
            auto& [_, callData] = *it;
            auto callPriceAmount = getCallPriceAmount(callData->priceType, callData->includeAccrual, callData->price,
                                                      notionalAccrualCalc->notional(t), notionalAccrualCalc->accrual(t));
            pathCallPrices.push_back(callPriceAmount);
        } else {
            pathCallPrices.push_back(0.0);
        }
        /*
        std::cout << i << "," << t << "," << "," << pathCallAccruals.back() << "," << pathCallNotionals.back() << ","
                  << (isCallDate ? "1" : "0") << "," << pathCallPrices.back() << "," << std::endl;
        */

    }

    for (size_t i = 0; i < pathExercisesCall.size(); ++i) {
        callProbs_.push_back(expectation(pathExercisesCall[i]).at(0) *
                             model_->numeraire(0, 0.0, 0.0, effDiscountCurve));
        //std::cout << "Exercise Time" << i << ": call prob " << callProbs_.back() << std::endl;
    }

    for (size_t i = 0; i < pathExercisesPut.size(); ++i) {
        putProbs_.push_back(expectation(pathExercisesPut[i]).at(0) * model_->numeraire(0, 0.0, 0.0, effDiscountCurve));
        //std::cout << "Exercise Time" << i << ": put prob " << putProbs_.back() << std::endl;
    }

    resultUnderlyingSettlementValue_ = resultUnderlyingNpv_ / effIncomeCurve->discount(settlementDate_) /
                                       effCreditCurve->survivalProbability(settlementDate_) *
                                       effCreditCurve->survivalProbability(settlementDate_);
    resultSettlementValue_ = resultValue_ / effIncomeCurve->discount(settlementDate_) /
                             effCreditCurve->survivalProbability(settlementDate_) *
                             effCreditCurve->survivalProbability(settlementDate_);
}

QuantLib::ext::shared_ptr<AmcCalculator> McCamCallableBondBaseEngine::amcCalculator() const { return amcCalculator_; }

McCamCallableBondBaseEngine::CallableBondAmcCalculator::CallableBondAmcCalculator(
    const std::vector<Size>& externalModelIndices, const std::set<Real>& exerciseXvaTimes,
    const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
    const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& callTimes,
    const std::map<Real, ext::shared_ptr<CallableBond::CallabilityData>>& putTimes,
    const std::array<std::vector<RegressionModel>, 2>& regModelUndDirty,
    const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValueCall,
    const std::array<std::vector<RegressionModel>, 2>& regModelContinuationValuePut,
    const std::array<std::vector<RegressionModel>, 2>& regModelOption, const Real resultValue,
    const Array& initialState, const Currency& baseCurrency, const bool reevaluateExerciseInStickyRun,
    const bool includeTodaysCashflows, const bool includeReferenceDateEvents,
    const ext::shared_ptr<QuantExt::CurrentNotionalAccrualsCalculator>& notionalAccrualCalc)
    : externalModelIndices_(externalModelIndices), exerciseXvaTimes_(exerciseXvaTimes), exerciseTimes_(exerciseTimes),
      xvaTimes_(xvaTimes), callTimes_(callTimes), putTimes_(putTimes), regModelUndDirty_(regModelUndDirty),
      regModelContinuationValueCall_(regModelContinuationValueCall),
      regModelContinuationValuePut_(regModelContinuationValuePut), regModelOption_(regModelOption),
      resultValue_(resultValue), initialState_(initialState), baseCurrency_(baseCurrency),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), includeTodaysCashflows_(includeTodaysCashflows),
      includeReferenceDateEvents_(includeReferenceDateEvents), notionalAccrualCalculator_(notionalAccrualCalc) {}

std::vector<QuantExt::RandomVariable> McCamCallableBondBaseEngine::CallableBondAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {
    
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

        exercisedCall_ = std::vector<Filter>(exerciseTimes_.size() + 1, Filter(samples, false));
        exercisedPut_ = std::vector<Filter>(exerciseTimes_.size() + 1, Filter(samples, false));
        Size counter = 0;

        Filter wasExercisedCall(samples, false);
        Filter wasExercisedPut(samples, false);

        for (auto t : exerciseTimes_) {

            auto itCall = callTimes_.find(t);
            auto itPut = putTimes_.find(t);
            bool isCallEvent = itCall != callTimes_.end();
            bool isPutEvent = itPut != putTimes_.end();

            if (xvaTimes_.size() == 0)
                break;

            // find the time in the exerciseXvaTimes vector
            Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
            QL_REQUIRE(ind != exerciseXvaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, exercise time "
                           << t << " not found in exerciseXvaTimes vector.");

            // make the exercise decision

            RandomVariable underlyingValue =
                regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

            if (isCallEvent) {
                auto callAmount =
                    getCallPriceAmount(itCall->second->priceType, itCall->second->includeAccrual, itCall->second->price,
                                       notionalAccrualCalculator_->notional(t), notionalAccrualCalculator_->accrual(t));

                RandomVariable continuationValueCall =
                    regModelContinuationValueCall_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

                auto exerciseValue = RandomVariable(samples, callAmount) - underlyingValue;

                exercisedCall_[counter + 1] =
                    !wasExercisedPut && !wasExercisedCall && exerciseValue < continuationValueCall;
                wasExercisedCall = wasExercisedCall || exercisedCall_[counter + 1];
            }
            if (isPutEvent) {
                auto putAmount =
                    getCallPriceAmount(itPut->second->priceType, itPut->second->includeAccrual, itPut->second->price,
                                       notionalAccrualCalculator_->notional(t), notionalAccrualCalculator_->accrual(t));

                RandomVariable continuationValuePut =
                    regModelContinuationValuePut_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

                auto exerciseValue = RandomVariable(samples, putAmount) - underlyingValue;

                exercisedPut_[counter + 1] = !wasExercisedPut && exerciseValue > continuationValuePut;
                wasExercisedPut = wasExercisedPut || exercisedPut_[counter + 1];
            }

            if (isPutEvent && isCallEvent) {
                exercisedCall_[counter + 1] = exercisedCall_[counter + 1] && !exercisedPut_[counter + 1];
                wasExercisedCall = wasExercisedCall && !wasExercisedPut;
            }

            ++counter;
        }
    }

    // now we can populate the result using the exercise indicators

    Size counter = 0;
    Size xvaCounter = 0;
    Size exerciseCounter = 0;

    Filter wasExercised(samples, false);

    for (auto t : exerciseXvaTimes_) {
        RandomVariable exercisePayments = RandomVariable(samples, 0.0);
        if (auto it = exerciseTimes_.find(t); it != exerciseTimes_.end()) {
            // if t is an exercise time we pay the exercise amount, for all following times the exerise amount is zero
            ++exerciseCounter;
            wasExercised = wasExercised || exercisedCall_[exerciseCounter] || exercisedPut_[exerciseCounter];
            exercisePayments = conditionalResult(
                exercisedCall_[exerciseCounter],
                RandomVariable(samples,
                               getCallPriceAmount(callTimes_.at(t)->priceType, callTimes_.at(t)->includeAccrual,
                                                  callTimes_.at(t)->price, notionalAccrualCalculator_->notional(t),
                                                  notionalAccrualCalculator_->accrual(t))),
                exercisePayments);
            exercisePayments = conditionalResult(
                exercisedPut_[exerciseCounter],
                RandomVariable(samples,
                               getCallPriceAmount(putTimes_.at(t)->priceType, putTimes_.at(t)->includeAccrual,
                                                  putTimes_.at(t)->price, notionalAccrualCalculator_->notional(t),
                                                  notionalAccrualCalculator_->accrual(t))),
                exercisePayments);
        }

        if (xvaTimes_.find(t) != xvaTimes_.end()) {
            // there is no continuation value on the last exercise date
            RandomVariable futureOptionValue =
                exerciseCounter == exerciseTimes_.size()
                    ? RandomVariable(samples, 0.0)
                    : max(RandomVariable(samples, 0.0),
                          regModelOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));

            RandomVariable underlyingValue =
                regModelUndDirty_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_);

            result[xvaCounter + 1] =
                conditionalResult(wasExercised, exercisePayments, underlyingValue + futureOptionValue);

            ++xvaCounter;
        }

        ++counter;
    }

    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

template <class Archive>
void McCamCallableBondBaseEngine::CallableBondAmcCalculator::serialize(Archive& ar, const unsigned int version) {
    ar.template register_type<McCamCallableBondBaseEngine::CallableBondAmcCalculator>();
    ar& boost::serialization::base_object<AmcCalculator>(*this);

    ar & externalModelIndices_;

    ar & exerciseXvaTimes_;
    ar & exerciseTimes_;
    ar & xvaTimes_;

    ar & regModelUndDirty_;
    ar & regModelContinuationValueCall_;
    ar & regModelContinuationValuePut_;
    ar & regModelOption_;
    ar & resultValue_;
    ar & initialState_;
    ar & baseCurrency_;
    ar & reevaluateExerciseInStickyRun_;
    ar & includeTodaysCashflows_;
    ar & includeReferenceDateEvents_;
}

template void
QuantExt::McCamCallableBondBaseEngine::CallableBondAmcCalculator::serialize(boost::archive::binary_iarchive& ar,
                                                                            const unsigned int version);
template void
QuantExt::McCamCallableBondBaseEngine::CallableBondAmcCalculator::serialize(boost::archive::binary_oarchive& ar,
                                                                            const unsigned int version);

} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::McCamCallableBondBaseEngine::CallableBondAmcCalculator);