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


#include <orea/scenario/scenariogenerator.hpp>

namespace ore {
namespace analytics {

QuantLib::ext::shared_ptr<Scenario> ScenarioPathGenerator::next(const Date& d) {
    if (d == dates_.front()) { // new path
        path_ = nextPath();
        pathStep_ = 0;
    }
    QL_REQUIRE(pathStep_ < dates_.size(), "step mismatch");
    if (d == dates_[pathStep_]) {
        return path_[pathStep_++]; // post increment
    } else {
        auto it = std::find(dates_.begin(), dates_.end(), d);
        QL_REQUIRE(it != dates_.end(), "invalid date " << d);
        return path_[std::distance(dates_.begin(), it)];
    }
}

std::vector<QuantLib::ext::shared_ptr<Scenario>> ScenarioLoaderGenerator::nextPath() {
    auto scenarios = scenarioLoader_->getScenarios(i_);
    std::vector<QuantLib::ext::shared_ptr<Scenario>> vs;
    for (const auto& s : scenarios)
        vs.push_back(s.second);
    i_++;
    return vs;
}

} // namespace analytics
} // namespace ore