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

#include <ored/model/blackscholesmodelbuilderbase.hpp>
#include <ored/model/utilities.hpp>

namespace ore {
namespace data {

BlackScholesModelBuilderBase::BlackScholesModelBuilderBase(
    const Handle<YieldTermStructure>& curve, const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear)
    : BlackScholesModelBuilderBase(std::vector<Handle<YieldTermStructure>>{curve},
                                   std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>{process},
                                   simulationDates, addDates, timeStepsPerYear) {}

BlackScholesModelBuilderBase::BlackScholesModelBuilderBase(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear)
    : curves_(curves), processes_(processes), simulationDates_(simulationDates), addDates_(addDates),
      timeStepsPerYear_(timeStepsPerYear) {

    QL_REQUIRE(!curves_.empty(), "BlackScholesModelBuilderBase: no curves given");

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();

    for (auto const& c : curves_)
        registerWith(c);

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
    for (auto const& p : processes_) {
        vols_.push_back(p->blackVolatility());
        allCurves_.push_back(p->riskFreeRate());
        allCurves_.push_back(p->dividendYield());
    }
}

BlackScholesModelBuilderBase::BlackScholesModelBuilderBase(
    const Handle<YieldTermStructure>& curve, const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process)
    : BlackScholesModelBuilderBase(curve, process, {}, {}, 1) {}

Handle<BlackScholesModelWrapper> BlackScholesModelBuilderBase::model() const {
    calculate();
    return model_;
}

bool BlackScholesModelBuilderBase::requiresRecalibration() const {
    setupDatesAndTimes();
    return calibrationPointsChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_;
}

void BlackScholesModelBuilderBase::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

void BlackScholesModelBuilderBase::setupDatesAndTimes() const {
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

void BlackScholesModelBuilderBase::performCalculations() const {
    if (requiresRecalibration()) {

        // update vol and curves cache

        calibrationPointsChanged(true);

        // reset market observer's updated flag

        marketObserver_->hasUpdated(true);

        // setup model

        model_.linkTo(QuantLib::ext::make_shared<BlackScholesModelWrapper>(getCalibratedProcesses(), effectiveSimulationDates_,
                                                                   discretisationTimeGrid_));

        // notify model observers
        model_->notifyObservers();
    }
}

bool BlackScholesModelBuilderBase::calibrationPointsChanged(const bool updateCache) const {

    // get times for curves and times / strikes for vols

    std::vector<std::vector<Real>> curveTimes = getCurveTimes();
    std::vector<std::vector<std::pair<Real, Real>>> volTimesStrikes = getVolTimesStrikes();

    // build data

    std::vector<std::vector<Real>> curveData;
    for (Size i = 0; i < curveTimes.size(); ++i) {
        curveData.push_back(std::vector<Real>());
        for (Size j = 0; j < curveTimes[i].size(); ++j) {
            curveData.back().push_back(allCurves_[i]->discount(curveTimes[i][j]));
        }
    }

    std::vector<std::vector<Real>> volData;
    for (Size i = 0; i < volTimesStrikes.size(); ++i) {
        volData.push_back(std::vector<Real>());
        for (Size j = 0; j < volTimesStrikes[i].size(); ++j) {
            Real k = volTimesStrikes[i][j].second;
            // check for null = ATM
            if (k == Null<Real>())
                k = atmForward(processes_[i]->x0(), processes_[i]->riskFreeRate(), processes_[i]->dividendYield(),
                               volTimesStrikes[i][j].first);
            volData.back().push_back(vols_[i]->blackVol(volTimesStrikes[i][j].first, k));
        }
    }

    // check if something has changed
    return cache_.hasChanged(curveTimes, curveData, volTimesStrikes, volData, updateCache);
}

} // namespace data
} // namespace ore
