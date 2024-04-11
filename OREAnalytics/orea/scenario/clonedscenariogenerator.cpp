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

ClonedScenarioGenerator::ClonedScenarioGenerator(const boost::shared_ptr<ScenarioGenerator>& scenarioGenerator,
                                                 const std::vector<Date>& dates, const Size nSamples) : dates_(dates) {
    DLOG("Build cloned scenario generator for " << dates.size() << " dates and " << nSamples << " samples.");
    scenarioGenerator->reset();
    scenarios_.resize(nSamples * dates_.size());
    for (Size i = 0; i < nSamples; ++i) {
        for (Size j = 0; j < dates_.size(); ++j) {
            scenarios_[i * dates_.size() + j] = scenarioGenerator->next(dates_[j])->clone();
	}
    }
}

boost::shared_ptr<Scenario> ClonedScenarioGenerator::next(const Date& d) {
    if (d == dates_.front()) { // new path
        nDate_++;
        i_ = 0;
    }
    size_t currentStep = (nDate_ - 1) * dates_.size() + i_;
    QL_REQUIRE(currentStep < scenarios_.size(),
               "ClonedScenarioGenerator::next(" << d << "): no more scenarios stored.");
    if (d == dates_[i_]) {
        i_++;
        return scenarios_[currentStep];
    } else {
        auto it = std::find(dates_.begin(), dates_.end(), d);
        size_t pos = (nDate_ - 1) * dates_.size() + std::distance(dates_.begin(), it);
        std::cout << "ClonedScenarioGenerator::next(" << d << "):" << nDate_ << " " << std::distance(dates_.begin(), it)
                  << std::endl;
        QL_REQUIRE(it != dates_.end(), "ClonedScenarioGenerator::next(" << d << "): invalid date " << d);
        return scenarios_[pos];
    } 
}

void ClonedScenarioGenerator::reset() { 
    i_ = 0;
    nDate_ = 0;
}

} // namespace analytics
} // namespace ore
