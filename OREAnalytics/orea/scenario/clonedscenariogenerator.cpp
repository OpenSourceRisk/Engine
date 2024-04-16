/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <orea/scenario/clonedscenariogenerator.hpp>

#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

ClonedScenarioGenerator::ClonedScenarioGenerator(const QuantLib::ext::shared_ptr<ScenarioGenerator>& scenarioGenerator,
                                                 const std::vector<Date>& dates, const Size nSamples) {
    DLOG("Build cloned scenario generator for " << dates.size() << " dates and " << nSamples << " samples.");
    for (size_t i = 0; i < dates.size(); ++i) {
        dates_[dates[i]] = i;
    }
    firstDate_ = dates.front();
    scenarioGenerator->reset();
    scenarios_.resize(nSamples * dates_.size());
    for (Size i = 0; i < nSamples; ++i) {
        for (Size j = 0; j < dates.size(); ++j) {
            scenarios_[i * dates.size() + j] = scenarioGenerator->next(dates[j])->clone();
        }
    }
}

QuantLib::ext::shared_ptr<Scenario> ClonedScenarioGenerator::next(const Date& d) {
    if (d == firstDate_) { // new path
        ++nSim_;
    }
    auto stepIdx = dates_.find(d);
    QL_REQUIRE(stepIdx != dates_.end(), "ClonedScenarioGenerator::next(" << d << "): invalid date " << d);
    size_t timePos = stepIdx->second;
    size_t currentStep = (nSim_ - 1) * dates_.size() + timePos;
    QL_REQUIRE(currentStep < scenarios_.size(),
               "ClonedScenarioGenerator::next(" << d << "): no more scenarios stored.");
    return scenarios_[currentStep];
}

void ClonedScenarioGenerator::reset() { 
    nSim_ = 0;
}

} // namespace analytics
} // namespace ore
