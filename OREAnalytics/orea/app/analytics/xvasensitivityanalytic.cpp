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

#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/analytics/xvasensitivityanalytic.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/utilities.hpp>
namespace ore {
namespace analytics {

XvaSensitivityAnalyticImpl::XvaSensitivityAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaSensitivityAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                             const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA_SENSITIVITY analytic.");

    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "XvaSensitivityAnalytic::run: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    // build t0, sim market, stress scenario generator

    CONSOLEW("XVA_SENSI: Build T0 and Sim Markets and Stress Scenario Generator");

    analytic()->buildMarket(loader);

    LOG("XvaSensitivityAnalytic: Build SimMarket")
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), analytic()->configurations().sensiScenarioData->useSpreadedTermStructures(), false,
        false, *inputs_->iborFallbackConfig(), true);

    LOG("XvaSensitivityAnalytic: Build SensitivityScenarioGenerator")
    
    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
        analytic()->configurations().sensiScenarioData, baseScenario, analytic()->configurations().simMarketParams,
        simMarket, scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    CONSOLE("OK");

    // generate the stress scenarios and run dependent xva analytic under each of them

    CONSOLE("XVA_SENSI: Running sensi scenarios");

    // run stress test
    LOG("Run XVA Sensitivity")
    runSensitivity(scenarioGenerator, loader);

    LOG("Running XVA Sensitivity analytic finished.");
}

void XvaSensitivityAnalyticImpl::runSensitivity(
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {

    std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> xvaReports;
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        auto desc = scenarioGenerator->scenarioDescriptions()[i];
        const auto label = scenario->label();
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> descReport =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>();

        double shiftSize1 = 0.0;
        auto itShiftSize1 = scenarioGenerator->shiftSizes().find(desc.key1());
        if (itShiftSize1 != scenarioGenerator->shiftSizes().end()){
            shiftSize1 = (itShiftSize1->second);
        }
        double shiftSize2 = 0.0;
        auto itShiftSize2 = scenarioGenerator->shiftSizes().find(desc.key2());
        if (itShiftSize2 != scenarioGenerator->shiftSizes().end()) {
            shiftSize2 = (itShiftSize2->second);
        }
        descReport->addColumn("Type", string());
        descReport->addColumn("IsPar", string());
        descReport->addColumn("Factor_1", string());
        descReport->addColumn("ShiftSize_1", double(), 8);
        descReport->addColumn("Factor_2", string());
        descReport->addColumn("ShiftSize_2", double(), 8);
        descReport->addColumn("Currency", string());
        descReport->next();
        descReport->add(ore::data::to_string(desc.type()));
        descReport->add("false");
        descReport->add(desc.factor1());
        descReport->add(shiftSize1);
        descReport->add(desc.factor2());
        descReport->add(shiftSize2);
        descReport->add(inputs_->baseCurrency());
        descReport->end();
        
        try {
            DLOG("Calculate XVA for scenario " << label);
            CONSOLE("XVA_SENSITIVITY: Apply scenario " << label);
            auto newAnalytic = ext::make_shared<XvaAnalytic>(
                inputs_, (label == "BASE" ? nullptr : scenario),
                (label == "BASE" ? nullptr : analytic()->configurations().simMarketParams));
            CONSOLE("XVA_SENSITIVITY: Calculate Exposure and XVA")
            newAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});
            // Collect exposure and xva reports
            for (auto& [name, rpt] : newAnalytic->reports()["XVA"]) {
                // add scenario column to report and copy it, concat it later
                if (boost::starts_with(name, "exposure") || boost::starts_with(name, "xva")) {
                    DLOG("Save and extend report " << name);
                    xvaReports[name].push_back(addColumnsToExisitingReport(descReport, rpt));
                }
            }
            // writeCubes(label, newAnalytic);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaSensitivity", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }
    for (auto& [name, reports] : xvaReports) {
        auto report = concatenateReports(reports);
        if (report != nullptr) {
            analytic()->reports()[label()][name] = report;
        }
    }
}

void XvaSensitivityAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->xvaSensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->xvaSensiScenarioData();
}

XvaSensitivityAnalytic::XvaSensitivityAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaSensitivityAnalyticImpl>(inputs), {"XVA_SENSITIVITY"}, inputs, true, false, false,
               false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

} // namespace analytics
} // namespace ore
