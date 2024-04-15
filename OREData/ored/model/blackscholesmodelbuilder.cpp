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

#include <ored/model/blackscholesmodelbuilder.hpp>
#include <ored/model/utilities.hpp>

namespace ore {
namespace data {

BlackScholesModelBuilder::BlackScholesModelBuilder(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const std::string& calibration, const std::vector<std::vector<Real>>& calibrationStrikes)
    : BlackScholesModelBuilderBase(curves, processes, simulationDates, addDates, timeStepsPerYear),
      calibration_(calibration),
      calibrationStrikes_(calibrationStrikes.empty() ? std::vector<std::vector<Real>>(processes.size())
                                                     : calibrationStrikes) {
    QL_REQUIRE(calibrationStrikes_.size() == processes.size(),
               "calibrationStrikes size (" << calibrationStrikes_.size() << ") must match processes size ("
                                           << processes.size() << ")");
}

BlackScholesModelBuilder::BlackScholesModelBuilder(const Handle<YieldTermStructure>& curve,
                                                   const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                                   const std::set<Date>& simulationDates,
                                                   const std::set<Date>& addDates, const Size timeStepsPerYear,
                                                   const std::string& calibration,
                                                   const std::vector<Real>& calibrationStrikes)
    : BlackScholesModelBuilderBase(curve, process, simulationDates, addDates, timeStepsPerYear),
      calibration_(calibration), calibrationStrikes_(1, calibrationStrikes) {}

std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>
BlackScholesModelBuilder::getCalibratedProcesses() const {
    // nothing to do, return original processes
    return processes_;
}

std::vector<std::vector<Real>> BlackScholesModelBuilder::getCurveTimes() const {
    std::vector<Real> timesExt(discretisationTimeGrid_.begin() + 1, discretisationTimeGrid_.end());
    for (auto const& d : addDates_) {
        if (d > curves_.front()->referenceDate()) {
            timesExt.push_back(curves_.front()->timeFromReference(d));
        }
    }
    std::sort(timesExt.begin(), timesExt.end());
    auto it = std::unique(timesExt.begin(), timesExt.end(),
                          [](const Real x, const Real y) { return QuantLib::close_enough(x, y); });
    timesExt.resize(std::distance(timesExt.begin(), it));
    return std::vector<std::vector<Real>>(allCurves_.size(), timesExt);
}

std::vector<std::vector<std::pair<Real, Real>>> BlackScholesModelBuilder::getVolTimesStrikes() const {
    std::vector<std::vector<std::pair<Real, Real>>> volTimesStrikes;
    for (Size i = 0; i < processes_.size(); ++i) {
        Real strike;
        if (calibration_ == "ATM") {
            strike = Null<Real>();
        } else if (calibration_ == "Deal") {
            strike = calibrationStrikes_[i].empty() ? Null<Real>() : calibrationStrikes_[i][0];
        } else {
            QL_FAIL("BlackScholesModelBuilder: calibration '" << calibration_ << "' not known, expected ATM or Deal");
        }
        volTimesStrikes.push_back(std::vector<std::pair<Real, Real>>());
        for (Size j = 1; j < discretisationTimeGrid_.size(); ++j) {
            volTimesStrikes.back().push_back(std::make_pair(discretisationTimeGrid_[j], strike));
        }
    }
    return volTimesStrikes;
}

} // namespace data
} // namespace ore
