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
#include <orea/app/analytics/sensitivityanalytic.hpp>
#include <orea/engine/partozeroscenario.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/stresstest.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void StressTestAnalyticImpl::setUpConfigurations() {
    QL_REQUIRE(inputs_->stressScenarioData() != nullptr,
               "StressTestAnalytic: No StressScenarioData found, check input");
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = inputs_->stressScenarioData()->hasParRateScenario();
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->stressSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->stressSensitivityScenarioData();
    setGenerateAdditionalResults(true);
}

void StressTestAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    StressTestAnalytic* stressAnalytic = static_cast<StressTestAnalytic*>(analytic());
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
    boost::shared_ptr<StressTestScenarioData> scenarioData = inputs_->stressScenarioData();
    if (scenarioData->hasParRateScenario()) {
        LOG("Convert PAR rate scenario into zero rate scenario");
        QL_REQUIRE(stressAnalytic->hasDependentAnalytic(stressAnalytic->sensiAnalyticLookupKey),
                   "Internal error, need to build par Conversion analytic");
        // TODO: Check tenors between simulation and sensitivity config matches
        LOG("Stress Test Analytic: Run Par Sensitivity Analysis");
        auto sensiAnalytic = stressAnalytic->dependentAnalytic<SensitivityAnalytic>(stressAnalytic->sensiAnalyticLookupKey);
        sensiAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;
        sensiAnalytic->configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
        sensiAnalytic->runAnalytic(loader);
        LOG("Stress Test Analytic: Par Sensitivity Analysis finished");
        LOG("Convert ParStressScenarios into ZeroStress Scenario");
        scenarioData = convertParScenarioToZeroScenarioData(
            inputs_->asof(), analytic()->market(), analytic()->configurations().simMarketParams, scenarioData,
            inputs_->stressSensitivityScenarioData(), sensiAnalytic->parSensitivities());
        LOG("Finished par to zero scenarios conversion");
    }

    Settings::instance().evaluationDate() = inputs_->asof();
    
    std::string marketConfig = inputs_->marketConfig("pricing");
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
    boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
        analytic()->portfolio(), analytic()->market(), marketConfig, inputs_->pricingEngine(),
        analytic()->configurations().simMarketParams, scenarioData, *analytic()->configurations().curveConfig,
        *analytic()->configurations().todaysMarketParams, nullptr, inputs_->refDataManager(),
        *inputs_->iborFallbackConfig(), inputs_->continueOnError());
    stressTest->writeReport(report, inputs_->stressThreshold());
    analytic()->reports()[label()]["stress"] = report;
    CONSOLE("OK");
}

StressTestAnalytic::StressTestAnalytic(const boost::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<StressTestAnalyticImpl>(inputs), {"STRESS"}, inputs, false, false, false, false) {
    QL_REQUIRE(inputs->stressScenarioData(),
               "StressTestAnalytic: No stress scenariodata found, please check your inputs");
    // Should only be settable for CRIF analytic
    if(inputs->stressScenarioData()->hasParRateScenario()){
        auto sensitivityAnalytic = boost::make_shared<SensitivityAnalytic>(inputs, true, true, false, inputs->stressOptimiseRiskFactors());
        dependentAnalytics_[sensiAnalyticLookupKey] = sensitivityAnalytic;
    }
}
} // namespace analytics
} // namespace ore
