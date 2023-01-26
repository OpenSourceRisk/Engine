/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/scenario/deltascenario.hpp>
#include <orea/scenario/scenariosimmarketplus.hpp>

#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

void ScenarioSimMarketPlus::applyScenario(const boost::shared_ptr<ore::analytics::Scenario>& scenario) {

    bool missingPoint = false;
    auto deltaScenario = boost::dynamic_pointer_cast<DeltaScenario>(scenario);

    /*! our assumption is that either all or none of the scenarios we apply are
      delta scenarios! */

    if (deltaScenario != nullptr) {
        for (auto const& key : diffToBaseKeys_) {
            auto it = simData_.find(key);
            if (it != simData_.end()) {
                it->second->setValue(baseScenario_->get(key));
            }
        }
        diffToBaseKeys_.clear();
        auto delta = deltaScenario->delta();
        for (auto const& key : delta->keys()) {
            auto it = simData_.find(key);
            if (it == simData_.end()) {
                ALOG("simulation data point missing for key " << key);
                missingPoint = true;
            } else {
                if (filter_->allow(key)) {
                    it->second->setValue(delta->get(key));
                    diffToBaseKeys_.insert(key);
                }
            }
        }
        QL_REQUIRE(!missingPoint, "simulation data points missing from scenario, exit.");
        asof_ = scenario->asof();
    } else {
        ScenarioSimMarket::applyScenario(scenario);
    }
}
} // namespace analytics
} // namespace ore
