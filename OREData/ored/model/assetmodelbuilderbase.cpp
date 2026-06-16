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

AssetModelBuilderBase::AssetModelBuilderBase(
    const Handle<YieldTermStructure>& curve, const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const Handle<YieldTermStructure>& baseCurve, const bool observeContinuum,
    const std::function<std::set<Real>(const TimeGrid&)>& curveTimes,
    const std::function<std::vector<std::set<std::pair<Real, Real>>>(const TimeGrid&)>& volTimesStrikes)
    : AssetModelBuilderBase(std::vector<Handle<YieldTermStructure>>{curve},
                            std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>{process},
                            simulationDates, addDates, timeStepsPerYear, baseCurve, observeContinuum) {}

AssetModelBuilderBase::AssetModelBuilderBase(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const Handle<YieldTermStructure>& baseCurve, const bool observeContinuum,
    const std::function<std::set<Real>(const TimeGrid&)>& curveTimes,
    const std::function<std::vector<std::set<std::pair<Real, Real>>>(const TimeGrid&)>& volTimesStrikes)
    : curves_(curves), baseCurve_(baseCurve), processes_(processes), simulationDates_(simulationDates),
      addDates_(addDates), timeStepsPerYear_(timeStepsPerYear), observeContinuum_(observeContinuum),
      curveTimesBase_(curveTimes), volTimesStrikesBase_(volTimesStrikes) {

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
    return (forceCalibration_ || referenceDate_ != curves_.front()->referenceDate() ||
            marketObserver_->hasUpdated(false) || calibrationPointsChanged(false));
}

void AssetModelBuilderBase::newCalcWithoutRecalibration() const { calculate(); }

void AssetModelBuilderBase::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

void AssetModelBuilderBase::setupDatesAndTimes() const {
    Date referenceDate = curves_.front()->referenceDate();
    effectiveSimulationDates_ = std::set<Date>(simulationDates_.lower_bound(referenceDate), simulationDates_.end());
    effectiveSimulationDates_.insert(referenceDate);
    discretisationTimeGrid_ =
        buildTimeGrid(referenceDate, curves_.front()->dayCounter(), simulationDates_, timeStepsPerYear_);
}

void AssetModelBuilderBase::performCalculations() const {
    if (requiresRecalibration()) {

        // update reference date

        referenceDate_ = curves_.front()->referenceDate();

        // these are enhanced with additional points in getCalibratedProcesses() below

        curveTimes_ = curveTimesBase_ ? curveTimesBase_(discretisationTimeGrid_) : std::set<Real>{};
        volTimesStrikes_ = volTimesStrikesBase_ ? volTimesStrikesBase_(discretisationTimeGrid_)
                                                : std::vector<std::set<std::pair<Real, Real>>>(processes_.size());

        for (Size j = 1; j < discretisationTimeGrid_.size(); ++j) {
            curveTimes_.insert(discretisationTimeGrid_[j]);
        }


        // setup model

        model_.linkTo(QuantLib::ext::make_shared<AssetModelWrapper>(processType(), getCalibratedProcesses(),
                                                                    effectiveSimulationDates_, discretisationTimeGrid_,
                                                                    calibrationResults_));

        // update vol and curves cache

        calibrationPointsChanged(true);

        // reset market observer's updated flag

        marketObserver_->hasUpdated(true);
    }
}

void AssetModelBuilderBase::buildCacheData(const std::set<Real>& curveTimes,
                                           const std::vector<std::set<std::pair<Real, Real>>>& volTimesStrikes,
                                           std::vector<std::vector<Real>>& curveData,
                                           std::vector<std::vector<Real>>& volData) const {

    for (Size i = 0; i < allCurves_.size(); ++i) {
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

bool AssetModelBuilderBase::calibrationPointsChanged(const bool updateCache) const {

    if (observeContinuum_)
        return true;

    std::vector<std::vector<Real>> curveData;
    std::vector<std::vector<Real>> volData;

    buildCacheData(curveTimes_, volTimesStrikes_, curveData, volData);

    return cache_.hasChanged(std::vector<std::set<Real>>(curveData.size(), curveTimes_), curveData, volTimesStrikes_,
                               volData, updateCache);
}

} // namespace data
} // namespace ore
