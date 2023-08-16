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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>

#include <boost/timer/timer.hpp>

namespace QuantExt {

McMultiLegBaseEngine::McMultiLegBaseEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const bool regressionOnExerciseOnly)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), polynomOrder_(polynomOrder), polynomType_(polynomType), ordering_(ordering),
      directionIntegers_(directionIntegers), discountCurves_(discountCurves), simulationDates_(simulationDates),
      externalModelIndices_(externalModelIndices), minimalObsDate_(minimalObsDate),
      regressionOnExerciseOnly_(regressionOnExerciseOnly) {

    if (discountCurves_.empty())
        discountCurves_.resize(model_->components(CrossAssetModel::AssetType::IR));
    else {
        QL_REQUIRE(discountCurves_.size() == model_->components(CrossAssetModel::AssetType::IR),
                   "McMultiLegBaseEngine: " << discountCurves_.size() << " discount curves given, but model has "
                                            << model_->components(CrossAssetModel::AssetType::IR) << " IR components.");
    }
}

Real McMultiLegBaseEngine::time(const Date& d) const {
    return model_->irlgm1f(0)->termStructure()->timeFromReference(d);
}

McMultiLegBaseEngine::CashflowInfo McMultiLegBaseEngine::createCashflowInfo(const boost::shared_ptr<CashFlow>& flow,
                                                                            const Currency& payCcy, Real payer,
                                                                            Size legNo, Size cashflowNo) const {
    CashflowInfo info;

    info.payTime = time(flow->date());
    info.payCcyIndex = model_->ccyIndex(payCcy);
    info.payer = payer;

    if (boost::dynamic_pointer_cast<FixedRateCoupon>(flow) != nullptr ||
        boost::dynamic_pointer_cast<SimpleCashFlow>(flow) != nullptr) {
        info.isFixed = true;
        info.fixedAmount = flow->amount();
        return info;
    }

    if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(flow)) {
        if (ibor->fixingDate() <= today_) {
            info.isFixed = true;
            info.fixedAmount = flow->amount();
        } else {
            Size indexCcyIdx = model_->ccyIndex(ibor->index()->currency());
            Real simTime = time(ibor->fixingDate());
            info.isFixed = false;
            info.simulationTimes.push_back(simTime);
            info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
            info.amountCalculator = [this, indexCcyIdx, ibor,
                                     simTime](const std::vector<std::vector<const RandomVariable*>>& states) {
                return lgmVectorised_[indexCcyIdx].fixing(ibor->index(), ibor->fixingDate(), simTime,
                                                          *states.at(0).at(0));
            };
        }
        return info;
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cashflowNo);
}

Size McMultiLegBaseEngine::timeIndex(const Time t, const std::set<Real>& times) const {
    auto it = times.find(t);
    QL_REQUIRE(it != times.end(),
               "McMultiLegBaseEngine::cashflowPathValue(): time ("
                   << t << ") not found in simulation times. This is an internal error. Contact dev.");
    return std::distance(times.begin(), it);
}

RandomVariable McMultiLegBaseEngine::cashflowPathValue(const CashflowInfo& cf,
                                                       const std::vector<std::vector<RandomVariable>>& pathValues,
                                                       const std::set<Real>& simulationTimes) const {

    auto simTimesPayIdx = timeIndex(cf.payTime, simulationTimes);

    std::vector<std::vector<const RandomVariable*>> states(cf.simulationTimes.size());
    for (Size i = 0; i < cf.simulationTimes.size(); ++i) {
        auto simTimesIdx = timeIndex(cf.simulationTimes[i], simulationTimes);
        std::vector<const RandomVariable*> tmp(cf.modelIndices[i].size());
        for (Size j = 0; j < cf.modelIndices[i].size(); ++j) {
            tmp[j] = &pathValues[simTimesIdx][cf.modelIndices[i][j]];
        }
        states[i] = tmp;
    }

    auto amount = (cf.isFixed ? RandomVariable(pathValues[0][0].size(), cf.fixedAmount) : cf.amountCalculator(states)) /
                  lgmVectorised_[0].numeraire(
                      cf.payTime, pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
                      discountCurves_[0]);
    
    if (cf.payCcyIndex > 0) {
        amount *= exp(pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::FX, cf.payCcyIndex - 1)]);
    }

    return amount;
}

void McMultiLegBaseEngine::calculate() const {

    std::cout << "McMultiLegBaseEngine::calculate()" << std::endl;
    boost::timer::cpu_timer timer;

    // check data set by derived engines

    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");

    // set today's date

    today_ = model_->irlgm1f(0)->termStructure()->referenceDate();

    // set up lgm vectorized instances for each currency

    if (lgmVectorised_.empty()) {
        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            lgmVectorised_.push_back(LgmVectorised(model_->irlgm1f(i)));
        }
    }

    std::cout << "init " << timer.elapsed().wall / 1E6 << "ms" << std::endl;

    // populate the info to generate the (alive) cashflow amounts

    std::vector<CashflowInfo> cashflowInfo;

    Size legNo = 0, cashflowNo = 0;
    for (auto const& leg : leg_) {
        Currency currency = currency_[legNo];
        Real payer = payer_[legNo];
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() <= today_)
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(createCashflowInfo(cashflow, currency, payer, legNo, cashflowNo));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    std::cout << "cashflow init " << timer.elapsed().wall / 1E6 << "ms" << std::endl;

    /* build the vector of required path simulation times which are given by
       - cashflow generation times
       - exercise times
       - xva simulation times */

    std::set<Real> simulationTimes;
    std::set<Real> exerciseXvaTimes;

    std::set<Real> cashflowGenTimes;
    std::set<Real> exerciseTimes;
    std::set<Real> xvaTimes;

    for (auto const& info : cashflowInfo) {
        cashflowGenTimes.insert(info.simulationTimes.begin(), info.simulationTimes.end());
        cashflowGenTimes.insert(info.payTime);
    }

    if (exercise_ != nullptr) {
        for (auto const& d : exercise_->dates()) {
            if (d <= today_)
                continue;
            exerciseTimes.insert(time(d));
        }
    }

    for (auto const& d : simulationDates_) {
        xvaTimes.insert(time(d));
    }

    simulationTimes.insert(cashflowGenTimes.begin(), cashflowGenTimes.end());
    simulationTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    simulationTimes.insert(xvaTimes.begin(), xvaTimes.end());

    exerciseXvaTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    exerciseXvaTimes.insert(xvaTimes.begin(), xvaTimes.end());

    std::cout << "sim times build " << timer.elapsed().wall / 1E6 << "ms" << std::endl;

    // simulate the paths for the calibration

    std::vector<std::vector<RandomVariable>> pathValues(
        simulationTimes.size(),
        std::vector<RandomVariable>(model_->stateProcess()->size(), RandomVariable(calibrationSamples_)));

    std::cout << "calibration path alloc " << timer.elapsed().wall / 1E6 << "ms" << std::endl;

    TimeGrid timeGrid(simulationTimes.begin(), simulationTimes.end());
    auto pathGenerator = makeMultiPathGenerator(calibrationPathGenerator_, model_->stateProcess(), timeGrid,
                                                calibrationSeed_, ordering_, directionIntegers_);

    for (Size i = 0; i < calibrationSamples_; ++i) {
        MultiPath path = pathGenerator->next().value;
        for (Size j = 0; j < simulationTimes.size(); ++j) {
            for (Size k = 0; k < model_->stateProcess()->size(); ++k) {
                pathValues[j][k].set(i, path[k][j]);
            }
        }
    }

    std::cout << "calibration path build " << timer.elapsed().wall / 1E6 << "ms" << std::endl;

    // for each xva sim time collect the relevant cashflow amounts and train a model on them

    std::vector<Array> coeffs(exerciseXvaTimes.size());

    auto basisFns = RandomVariableLsmBasisSystem::multiPathBasisSystem(model_->stateProcess()->size(), polynomOrder_);

    std::vector<bool> cfDone(cashflowInfo.size(), false);
    std::vector<const RandomVariable*> regressor(model_->stateProcess()->size());

    RandomVariable cumulatedAmount(calibrationSamples_);
    Size counter = exerciseXvaTimes.size() - 1;

    std::cout << "got " << simulationTimes.size() << " simulation times and " << exerciseXvaTimes.size()
              << " exercise/XVA times." << std::endl;

    for (auto t = exerciseXvaTimes.rbegin(); t != exerciseXvaTimes.rend(); ++t) {

        for (Size i = 0; i < cashflowInfo.size(); ++i) {

            if (cfDone[i])
                continue;

            if (cashflowInfo[i].payTime > *t) {
                cumulatedAmount += cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
                cfDone[i] = true;
            }
        }

        for (Size i = 0; i < model_->stateProcess()->size(); ++i) {
            regressor[i] = &pathValues[timeIndex(*t, simulationTimes)][i];
        }

        coeffs[counter] = regressionCoefficients(cumulatedAmount, regressor, basisFns);
        if (getenv("DEBUG")) {
            std::cout << "trained coeffs at " << counter << ": " << coeffs[counter] << std::endl;
        }
        --counter;
    }

    // add the remaining live cashflows to get the underlying value

    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (!cfDone[i])
            cumulatedAmount += cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
    }

    // set the result value (= underlying value if no exercise is given, otherwise option value)

    std::cout << "total    " << timer.elapsed().wall / 1E6 << "ms" << std::endl;
}

boost::shared_ptr<AmcCalculator> McMultiLegBaseEngine::amcCalculator() const { return amcCalculator_; }

std::vector<QuantExt::RandomVariable>
MultiLegBaseAmcCalculator::simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                                        std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                                        const std::vector<bool>& isRelevantTime, const bool stickyCloseOutRun) {

    QL_REQUIRE(!paths.empty(), "MultiLegBaseAmcCalculator: no future path times, this is not allowed.");
    QL_REQUIRE(pathTimes.size() == paths.size(), "MultiLegBaseAmcCalculator: inconsistent pathTimes size ("
                                                     << pathTimes.size() << ") and paths size (" << paths.size()
                                                     << ") - internal error.");

    return {};
}

} // namespace QuantExt
