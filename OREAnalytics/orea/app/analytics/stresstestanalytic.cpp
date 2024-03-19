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

#include <orea/app/analytics/stresstestanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/stresstest.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void StressTestAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = true;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
    setGenerateAdditionalResults(true);
}

void StressTestAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("StressTestAnalytic::runAnalytic called");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());
    QL_REQUIRE(inputs_->portfolio(), "StressTestAnalytic::run: No portfolio loaded.");

    CONSOLEW("StressTestAnalytic: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("StressTestAnalytic: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");
    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
    // This hook allows modifying the portfolio in derived classes before running the analytics below,
    // e.g. to apply SIMM exemptions.
    analytic()->modifyPortfolio();
    CONSOLEW("Risk: Stress Test Report");
    LOG("Stress Test Analysis called");
    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing");
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
    boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
        analytic()->portfolio(), analytic()->market(), marketConfig, inputs_->pricingEngine(),
        inputs_->stressSimMarketParams(), inputs_->stressScenarioData(), *analytic()->configurations().curveConfig,
        *analytic()->configurations().todaysMarketParams, nullptr, inputs_->refDataManager(),
        *inputs_->iborFallbackConfig(), inputs_->continueOnError());
    stressTest->writeReport(report, inputs_->stressThreshold());
    analytic()->reports()[label()]["stress"] = report;
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore
