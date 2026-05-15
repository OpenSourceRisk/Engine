/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/model/assetmodelbuilderbase.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

AssetModelBuilderBase::AssetModelBuilderBase(const Handle<YieldTermStructure>& curve,
                                             const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                             const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                                             const Size timeStepsPerYear, const Handle<YieldTermStructure>& baseCurve,
                                             const bool observeContinuum)
    : AssetModelBuilderBase(std::vector<Handle<YieldTermStructure>>{curve},
                            std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>{process},
                            simulationDates, addDates, timeStepsPerYear, baseCurve, observeContinuum) {}

AssetModelBuilderBase::AssetModelBuilderBase(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const Handle<YieldTermStructure>& baseCurve, const bool observeContinuum)
    : curves_(curves), baseCurve_(baseCurve), processes_(processes), simulationDates_(simulationDates),
      addDates_(addDates), timeStepsPerYear_(timeStepsPerYear), observeContinuum_(observeContinuum) {

    QL_REQUIRE(!curves_.empty(), "AssetModelBuilderBase: no curves given");

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();

    for (auto const& c : curves_)
        registerWith(c);
    registerWith(baseCurve_);

    for (auto const& p : processes_) {
        registerWith(p->blackVolatility());
        registerWith(p->riskFreeRate());
        registerWith(p->dividendYield());
        marketObserver_->registerWith(p->stateVariable());
        if (observeContinuum_)
            marketObserver_->registerWith(p->blackVolatility());
    }

    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    allCurves_ = curves_;
    if (!baseCurve_.empty())
        allCurves_.push_back(baseCurve_);
    for (auto const& p : processes_) {
        vols_.push_back(p->blackVolatility());
        allCurves_.push_back(p->riskFreeRate());
        allCurves_.push_back(p->dividendYield());
    }
}

AssetModelBuilderBase::AssetModelBuilderBase(const Handle<YieldTermStructure>& curve,
                                             const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process)
    : AssetModelBuilderBase(curve, process, {}, {}, 1) {}

Handle<AssetModelWrapper> AssetModelBuilderBase::model() const {
    calculate();
    return model_;
}

bool AssetModelBuilderBase::requiresRecalibration() const {
    setupDatesAndTimes();
    if (!initialCalibrationIsDone_ || forceCalibration_ || marketObserver_->hasUpdated(false))
        return true;
    auto [calibrationPointsChangedBuilder, calibrationPointsChangedModel] = calibrationPointsChanged(false);
    return calibrationPointsChangedBuilder || calibrationPointsChangedModel;
}

void AssetModelBuilderBase::newCalcWithoutRecalibration() const { calculate(); }

void AssetModelBuilderBase::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

void AssetModelBuilderBase::setupDatesAndTimes() const {
    Date referenceDate = curves_.front()->referenceDate();
    effectiveSimulationDates_.clear();
    effectiveSimulationDates_.insert(referenceDate);
    for (auto const& d : simulationDates_) {
        if (d >= referenceDate)
            effectiveSimulationDates_.insert(d);
    }

    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        times.push_back(curves_.front()->timeFromReference(d));
    }

    Size steps = std::max(std::lround(timeStepsPerYear_ * times.back() + 0.5), 1l);
    discretisationTimeGrid_ = TimeGrid(times.begin(), times.end(), steps);
}

void AssetModelBuilderBase::performCalculations() const {
    if (requiresRecalibration()) {

        // update vol and curves cache

        auto [calibrationPointsChangedBuilder, calibrationPointsChangedModel] = calibrationPointsChanged(true);

        // reset market observer's updated flag

        marketObserver_->hasUpdated(true);

        // clear points used for notification filtering (will be set in getCalibratedProcesses)

        curveTimes_.clear();
        volTimesStrikes_.clear();

        // setup model

        if (!initialCalibrationIsDone_ || calibrationPointsChangedBuilder) {
            model_.linkTo(QuantLib::ext::make_shared<AssetModelWrapper>(
                processType(), getCalibratedProcesses(), effectiveSimulationDates_, discretisationTimeGrid_,
                model_.empty() ? std::set<Real>{} : model_->getCurveTimes(),
                model_.empty() ? std::vector<std::set<std::pair<Real, Real>>>{} : model_->getVolTimesStrikes(),
                calibrationResults_));
            initialCalibrationIsDone_ = true;
        }

        // notify model observers

        model_->notifyObservers();

        // populate points for notification filtering provided by model

        curveTimesModel_ = model_->getCurveTimes();
        volTimesStrikesModel_ = model_->getVolTimesStrikes();
        std::cout << "got volTimesStrikes " << volTimesStrikesModel_.size() << std::endl;
    }
}

void AssetModelBuilderBase::buildCacheData(const std::set<Real>& curveTimes,
                                           const std::vector<std::set<std::pair<Real, Real>>>& volTimesStrikes,
                                           std::vector<std::vector<Real>>& curveData,
                                           std::vector<std::vector<Real>>& volData) const {

    for (Size i = 0; i < curveData.size(); ++i) {
        curveData.push_back(std::vector<Real>());
        for (auto t : curveTimes) {
            curveData.back().push_back(allCurves_[i]->discount(t));
        }
    }

    for (Size i = 0; i < volTimesStrikes.size(); ++i) {
        volData.push_back(std::vector<Real>());
        for (auto [t, k] : volTimesStrikes[i]) {
            if (k == Null<Real>())
                k = atmForward(processes_[i]->x0(), processes_[i]->riskFreeRate(), processes_[i]->dividendYield(), t);
            volData.back().push_back(vols_[i]->blackVol(t, k));
        }
    }
}

std::pair<bool, bool> AssetModelBuilderBase::calibrationPointsChanged(const bool updateCache) const {
    std::vector<std::vector<Real>> curveData, curveDataModel;
    std::vector<std::vector<Real>> volData, volDataModel;

    buildCacheData(curveTimes_, volTimesStrikes_, curveData, volData);
    buildCacheData(curveTimesModel_, volTimesStrikesModel_, curveDataModel, volDataModel);

    for (auto const& v : volTimesStrikesModel_) {
        for (auto const& [t, k] : v) {
            std::cout << " volTimesStrikesModel: " << t << "," << k << std::endl;
        }
    }

    return std::make_pair(cache_.hasChanged(std::vector<std::set<Real>>(curveData.size(), curveTimes_), curveData,
                                            volTimesStrikes_, volData, updateCache),
                          cacheModel_.hasChanged(std::vector<std::set<Real>>(curveData.size(), curveTimesModel_),
                                                 curveDataModel, volTimesStrikesModel_, volDataModel, updateCache));
}

} // namespace data
} // namespace ore
