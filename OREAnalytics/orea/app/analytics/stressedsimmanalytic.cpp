/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <orea/app/analytics/stressedsimmanalytic.hpp>
#include <orea/app/inputparameters.hpp>

#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/simmanalytic.hpp>

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/parstressconverter.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/report/utilities.hpp>

namespace ore {
namespace analytics {

void StressedSimmVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    LOG("Loading StressedSimmVariables");
    inputs->loadParameterXML<StressTestScenarioData>(stressedSimmScenarioData_, "stressedSimm",
                                                     "stressedSimmScenarioData", true);
    LOG("StressedSimmVariables loaded");
    LOG("StressedSimmVariables::stressedSimmScenarioData_ has " << (stressedSimmScenarioData_ ? "data" : "no data"));
}

StressedSimmAnalyticImpl::StressedSimmAnalyticImpl(
    const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs, QuantLib::ext::make_shared<StressedSimmVariables>()) {
    LOG("Constructing ore::StressedSimmAnalyticImpl");
    setLabel(LABEL);
}

void StressedSimmAnalyticImpl::setUpConfigurations() {
    LOG("ore::StressedSimmAnalyticImpl::setUpConfigurations called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();

    QL_REQUIRE(analytic()->configurations().simMarketParams, "StressedSimmAnalytic: simMarketParams not set");
    QL_REQUIRE(analytic()->configurations().sensiScenarioData, "StressedSimmAnalytic: sensiScenarioData not set");
    QL_REQUIRE(analytic()->configurations().todaysMarketParams, "StressedSimmAnalytic: todaysMarketParams not set");
}

void StressedSimmAnalyticImpl::buildDependencies() {}

void StressedSimmAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                           const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running Stressed SIMM analytic.");

    SavedSettings settings;

    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "StressedSimmAnalytic::run: No portfolio loaded.");

    QL_REQUIRE(inputs_->crif() == nullptr || inputs_->crif()->empty(),
               "StressedSimmAnalytic does not support CRIF input");
    

    CONSOLEW("STRESS_SIMM: Build T0 and Sim Markets and Stress Scenario Generator");

    analytic()->buildMarket(loader);
    DLOG("StressSIMM: Market built");
    QuantLib::ext::shared_ptr<StressTestScenarioData> scenarioData =
        QuantLib::ext::static_pointer_cast<StressedSimmVariables>(inputVariables_)->stressedSimmScenarioData_;

    QL_REQUIRE(scenarioData != nullptr, "StressedSimmAnalytic requires stress scenario data");
    QL_REQUIRE(scenarioData->useSpreadedTermStructures(),
               "StressedSimmAnalytic only supports spreaded term structures for now");
    
    // Need to align pillars before building the stress scenarios
    
    const set<RiskFactorKey::KeyType>& typesDisabled =
        analytic()->configurations().sensiScenarioData->parConversionExcludes();
    
    auto parAnalysis = QuantLib::ext::make_shared<ParSensitivityAnalysis>(
        inputs_->asof(), analytic()->configurations().simMarketParams, *analytic()->configurations().sensiScenarioData,
        "", true, typesDisabled);
    parAnalysis->alignPillars();
    // Convert par stress scenarios
    if (scenarioData != nullptr && scenarioData->hasScenarioWithParShifts()) {
        try {
            QuantLib::ext::shared_ptr<InMemoryReport> parScenarioReport =
                QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
            ParStressTestConverter converter(
                inputs_->asof(), analytic()->configurations().todaysMarketParams,
                analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                analytic()->configurations().curveConfig, analytic()->market(), inputs_->iborFallbackConfig());
            scenarioData = converter.convertStressScenarioData(scenarioData, parScenarioReport);
            analytic()->stressTests()[label()]["stress_ZeroStressData"] = scenarioData;
            if (parScenarioReport->rows() > 0) {
                analytic()->addReport(label(), "stress_scenario_par_rates", parScenarioReport);
            }
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage(label(), "ParConversionFailed", e.what()).log();
        }
    }
    
    std::string marketConfig = inputs_->marketConfig("pricing");

    LOG("STRESS_SIMM: Build SimMarket and StressTestScenarioGenerator")
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), scenarioData->useSpreadedTermStructures(), false, false,
        inputs_->iborFallbackConfig(), true);

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

void StressedSimmAnalyticImpl::runStressTest(
    const QuantLib::ext::shared_ptr<StressScenarioGenerator>& scenarioGenerator,
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    

    std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> simmReports;
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        const std::string& label = scenario != nullptr ? scenario->label() : std::string();
        try {
            DLOG("Calculate SIMM for scenario " << label);
            CONSOLE("SIMM_STRESS: Apply scenario " << label);
            auto newAnalytic = ore::analytics::AnalyticFactory::instance().build("SIMM", inputs_, analytic()->analyticsManager(), false).second;
            newAnalytic->setUp();
            newAnalytic->configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
            newAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;
            for( const auto& da : newAnalytic->allDependentAnalytics()){
                DLOG("Set up dependent analytic " << da->label());
                da->setUp();
            }
            DLOG("SIMM analytic built for scenario " << label);
            DLOG("SIMM analytic initialised for scenario " << label);
            auto simmAnalytic = QuantLib::ext::dynamic_pointer_cast<WithOffsetScenario>(newAnalytic);
            simmAnalytic->setOffsetScenario(scenario, analytic()->configurations().simMarketParams);
            DLOG("SIMM analytic configured for scenario " << label);
            CONSOLE("SIMM_STRESS: Calculate SIMM")
            newAnalytic->runAnalytic(loader, {"SIMM"});
            // Collect SIMM reports
            auto rpts = newAnalytic->reports();
            auto it = rpts.find("SIMM");
            QL_REQUIRE(it != rpts.end(), "SIMM report not found in SIMM analytic reports");
            for (auto [name, rpt] : it->second) {
                // add scenario column to report and copy it, concat it later
                DLOG("found report " << name << " for scenario " << label);
                if (name == "simm") {
                    DLOG("Save and extend report " << name);
                    simmReports["stressed_simm"].push_back(addColumnToExisitingReport("Scenario", label, rpt));
                }
            }

            // FIXME: If the sensitivity analytic above is a dependent analytic, then we do not have to add this
            // timer, otherwise we have to manually add the SensitivityAnalytic::timer
            analytic()->addTimer("Sensitivity analytic", newAnalytic->getTimer());
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("StressedSimm", "StressedSIMM",
                                            "Error during StressedSIMM calc under scenario " + label + ", got " +
                                                e.what() + ". Skip it")
                .log();
        }
    }
    concatReports(simmReports);
}

void StressedSimmAnalyticImpl::concatReports(
    const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>&
        simmReports) {
    DLOG("Concat SIMM reports");
    for (auto& [name, reports] : simmReports) {
        auto report = concatenateReports(reports);
        if (report != nullptr) {
            analytic()->addReport(label(), name, report);
        }
    }
}

} // namespace analytics
} // namespace ore
