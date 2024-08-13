/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/scenarionpvanalytic.hpp>
#include <orea/scenario/scenariowriter.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

void ScenarioNPVAnalyticImpl::setUpConfigurations() {
    LOG("ScenarioNPVAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->exposureSimMarketParams();
    analytic()->configurations().scenarioGeneratorData = inputs_->scenarioGeneratorData();
}

void ScenarioNPVAnalyticImpl::buildScenarioGenerator(bool continueOnError) {
    auto grid = analytic()->configurations().scenarioGeneratorData->getGrid();
    auto loader = QuantLib::ext::make_shared<SimpleScenarioLoader>(inputs_->scenarioReader());
    auto slg = QuantLib::ext::make_shared<ScenarioLoaderGenerator>(loader, inputs_->asof(), grid->dates(), grid->timeGrid());
    scenarioGenerator_ = slg;
    
    samples_ = slg->scenarioLoader()->samples();

    if (inputs_->writeScenarios()) {
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        analytic()->reports()[label()]["scenario"] = report;
        scenarioGenerator_ = QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void ScenarioNPVAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                          const std::set<std::string>& runTypes) {


    XvaAnalyticImpl::runAnalytic(loader, {"EXPOSURE"});
}

} // namespace analytics
} // namespace ore
