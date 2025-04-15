/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/sensitivitystressanalytic.hpp>

#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/parstressconverter.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/utilities.hpp>
namespace ore {
namespace analytics {

SensitivityStressAnalyticImpl::SensitivityStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                             const boost::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>>& scenarios)
    : Analytic::Impl(inputs), stressScenarios_(scenarios.get_value_or(inputs->sensitivityStressScenarioData())) {
    setLabel(LABEL);
}

void SensitivityStressAnalyticImpl::buildDependencies() {
}

void SensitivityStressAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running Sensitivity Stress analytic.");

    SavedSettings settings;
    
    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "SensitivityStressAnalytic::run: No portfolio loaded.");
    
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    // build t0, sim market, stress scenario generator

    CONSOLEW("SENSITIVITY_STRESS: Build T0 and Sim Markets and Stress Scenario Generator");

    analytic()->buildMarket(loader);

    QuantLib::ext::shared_ptr<StressTestScenarioData> scenarioData = stressScenarios_;
    if (scenarioData != nullptr && scenarioData->hasScenarioWithParShifts()) {
        try {
            ParStressTestConverter converter(
                inputs_->asof(), analytic()->configurations().todaysMarketParams,
                analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                analytic()->configurations().curveConfig, analytic()->market(), inputs_->iborFallbackConfig());
            scenarioData = converter.convertStressScenarioData(scenarioData);
            analytic()->stressTests()[label()]["stress_ZeroStressData"] = scenarioData;
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage(label(), "ParConversionFailed", e.what()).log();
        }
    }

    LOG("Sensitivity Stress: Build SimMarket and StressTestScenarioGenerator")
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), scenarioData->useSpreadedTermStructures(), false, false,
        *inputs_->iborFallbackConfig(), true);

    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<StressScenarioGenerator>(
        scenarioData, baseScenario, analytic()->configurations().simMarketParams, simMarket, scenarioFactory,
        simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    CONSOLE("OK");

    // generate the stress scenarios and run dependent sensitivity analytic under each of them

    CONSOLE("SENSITIVITY_STRESS: Running stress scenarios");

    // run stress test
    LOG("Run Sensitivity Stresstest")
    runStressTest(scenarioGenerator, loader);

    LOG("Running Sensitivity Stress analytic finished.");
}

void SensitivityStressAnalyticImpl::runStressTest(const QuantLib::ext::shared_ptr<StressScenarioGenerator>& scenarioGenerator,
                                          const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {

    std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> sensitivityReports;
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        const std::string& label = scenario != nullptr ? scenario->label() : std::string();
        if ((inputs_->sensitivityStressCalcBaseScenario() && label == "BASE") || (label != "BASE")) {
            try {
                DLOG("Calculate Sensitivity for scenario " << label);
                CONSOLE("SENSITIVITY_STRESS: Apply scenario " << label);
                auto newAnalytic = AnalyticFactory::instance().build("SENSITIVITY", inputs_, analytic()->analyticsManager(), false).second;
                newAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;
                newAnalytic->configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
                auto sensitivityImpl = static_cast<PricingAnalyticImpl*>(newAnalytic->impl().get());
                sensitivityImpl->setOffsetScenario(scenario);
                sensitivityImpl->setOffsetSimMarketParams(analytic()->configurations().simMarketParams);

                CONSOLE("SENSITIVITY_STRESS: Calculate Sensitivity")
                newAnalytic->runAnalytic(loader, {"SENSITIVITY"});
                // Collect sensitivity reports
                auto rpts = newAnalytic->reports();
                auto it = rpts.find("SENSITIVITY");
                QL_REQUIRE(it != rpts.end(), "Sensitivity report not found in Sensitivity analytic reports");
                for (auto [name, rpt] : it->second) {
                    // add scenario column to report and copy it, concat it later
                    if (boost::starts_with(name, "sensitivity")) {
                        DLOG("Save and extend report " << name);
                        sensitivityReports[name].push_back(addColumnToExisitingReport("Scenario", label, rpt));
                    }
                }

                // FIXME: If the sensitivity analytic above is a dependent analytic, then we do not have to add this timer,
                // otherwise we have to manually add the SensitivityAnalytic::timer
                analytic()->addTimer("Sensitivity analytic", newAnalytic->getTimer());
            } catch (const std::exception& e) {
                StructuredAnalyticsErrorMessage("SensitivityStress", "SensitivityCalc",
                                                "Error during Sensitivity calc under scenario " + label + ", got " + e.what() +
                                                    ". Skip it")
                    .log();
            }
        }
    }
    concatReports(sensitivityReports);
}

void SensitivityStressAnalyticImpl::concatReports(
    const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>& sensitivityReports) {
    DLOG("Concat sensitivity reports");
    for (auto& [name, reports] : sensitivityReports) {
        auto report = concatenateReports(reports);
        if (report != nullptr) {
            analytic()->addReport(label(), name, report);
        }
    }
}

void SensitivityStressAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensitivityStressSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensitivityStressSensitivityScenarioData();
}

} // namespace analytics
} // namespace ore
