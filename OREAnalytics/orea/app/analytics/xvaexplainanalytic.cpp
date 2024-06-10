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

#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/analytics/xvaexplainanalytic.hpp>
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


XvaExplainAnalyticImpl::XvaExplainAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
    auto mporAnalytic = AnalyticFactory::instance().build("SCENARIO", inputs);
    if (mporAnalytic.second) {
        auto sai = static_cast<ScenarioAnalyticImpl*>(mporAnalytic.second->impl().get());
        sai->setUseSpreadedTermStructures(true);
        addDependentAnalytic(mporLookupKey, mporAnalytic.second);
    }
}

void XvaExplainAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA Explain analytic.");
    QL_REQUIRE(inputs_->portfolio(), "XvaExplainAnalytic::run: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing");

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    CONSOLEW("XVA_EXPLAIN: Build T0 and Sim Market");

    analytic()->buildMarket(loader);

    ParStressTestConverter converter(
        inputs_->asof(), analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
        analytic()->configurations().sensiScenarioData, analytic()->configurations().curveConfig, analytic()->market(),
        inputs_->iborFallbackConfig());

    auto [simMarket, parSensiAnalysis] = converter.computeParSensitivity({});

    // compute t0 parRates
    std::map<RiskFactorKey, double> parRateT0;
    for(const auto& [key, instrument] : parSensiAnalysis->parInstruments().parHelpers_){
        parRateT0[key] = instrument->NPV();
    }

    // Get Shift scenario for t1, apply it and compute t1;

    auto mporAnalytic = dependentAnalytic(mporLookupKey);
    // FIXME: load mpor date from command line
    mporAnalytic->configurations().asofDate = inputs_->asof() + 1  * Days;
    mporAnalytic->configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    mporAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;

    // Run the mpor analytic to generate the market scenario as of t1
    mporAnalytic->runAnalytic(loader);

    // Set the evaluation date back to t0
    Settings::instance().evaluationDate() = inputs_->asof();
        
    QuantLib::ext::shared_ptr<Scenario> asofBaseScenario = simMarket->baseScenarioAbsolute();
    auto sai = static_cast<ScenarioAnalyticImpl*>(mporAnalytic->impl().get());
    QuantLib::ext::shared_ptr<Scenario> mporBaseScenario = sai->scenarioSimMarket()->baseScenarioAbsolute();




    QuantLib::ext::shared_ptr<StressTestScenarioData> scenarioData = inputs_->XvaExplainScenarioData();
    if (scenarioData != nullptr && scenarioData->hasScenarioWithParShifts()) {
        try {
            
            scenarioData = converter.convertStressScenarioData(scenarioData);
            analytic()->stressTests()[label()]["stress_ZeroStressData"] = scenarioData;
        } catch (const std::exception& e) {
            scenarioData = inputs_->XvaExplainScenarioData();
            StructuredAnalyticsErrorMessage(label(), "ParConversionFailed", e.what()).log();
        }
    }

    LOG("XVA Stress: Build SimMarket and StressTestScenarioGenerator")
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

    // generate the stress scenarios and run dependent xva analytic under each of them

    CONSOLE("XVA_STRESS: Running stress scenarios");

    // run stress test
    LOG("Run XVA Stresstest")
    runStressTest(scenarioGenerator, loader);

    LOG("Running XVA Stress analytic finished.");
}

void XvaExplainAnalyticImpl::runStressTest(const QuantLib::ext::shared_ptr<StressScenarioGenerator>& scenarioGenerator,
                                          const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {

    std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> xvaReports;
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        const std::string& label = scenario != nullptr ? scenario->label() : std::string();
        try {
            DLOG("Calculate XVA for scenario " << label);
            CONSOLE("XVA_STRESS: Apply scenario " << label);
            auto newAnalytic = ext::make_shared<XvaAnalytic>(
                inputs_, (label == "BASE" ? nullptr : scenario),
                (label == "BASE" ? nullptr : analytic()->configurations().simMarketParams));
            CONSOLE("XVA_STRESS: Calculate Exposure and XVA")
            newAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});
            // Collect exposure and xva reports
            for (auto& [name, rpt] : newAnalytic->reports()["XVA"]) {
                // add scenario column to report and copy it, concat it later
                if (boost::starts_with(name, "exposure") || boost::starts_with(name, "xva")) {
                    DLOG("Save and extend report " << name);
                    xvaReports[name].push_back(addColumnToExisitingReport("Scenario", label, rpt));
                }
            }
            writeCubes(label, newAnalytic);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaExplain", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }
    concatReports(xvaReports);
}

void XvaExplainAnalyticImpl::concatReports(
    const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>& xvaReports) {
    DLOG("Concat exposure and xva reports");
    for (auto& [name, reports] : xvaReports) {
        auto report = concatenateReports(reports);
        if (report != nullptr) {
            analytic()->reports()[label()][name] = report;
        }
    }
}

void XvaExplainAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->XvaExplainSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->XvaExplainSensitivityScenarioData();
}

XvaExplainAnalytic::XvaExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaExplainAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, false, false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

} // namespace analytics
} // namespace ore
