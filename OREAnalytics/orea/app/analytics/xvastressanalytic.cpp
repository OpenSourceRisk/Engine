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

#include <orea/app/analytics/xvastressanalytic.hpp>

#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
namespace ore {
namespace analytics {

auto buildNettingSetReport() {
    auto report = QuantLib::ext::make_shared<InMemoryReport>();
    report->addColumn("Scenario", string());
    report->addColumn("NettingSet", string());

    report->addColumn("BaseCVA", double(), 6);
    report->addColumn("CVA", double(), 6);
    report->addColumn("StressCVA", double(), 6);

    report->addColumn("BaseDVA", double(), 6);
    report->addColumn("DVA", double(), 6);
    report->addColumn("StressDVA", double(), 6);
    return report;
}

void addResultsToNettingSetReport(const std::string& label,
                                  QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report,
                                  const std::map<std::string, XvaResult>& stressResults,
                                  const std::map<std::string, XvaResult>& baseResults) {

    for (const auto& [nid, r] : stressResults) {

        if (auto baseIt = baseResults.find(nid); baseIt != baseResults.end()) {
            report->next();
            report->add(label);
            report->add(nid);
            report->add(r.cva);
            report->add(baseIt->second.cva);
            report->add(r.cva - baseIt->second.cva);
            report->add(r.dva);
            report->add(baseIt->second.dva);
            report->add(r.dva - baseIt->second.dva);
        }
    }
}

QuantLib::ext::shared_ptr<ore::data::InMemoryReport>
extendReport(const std::string& label, const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report) {
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> newReport = ext::make_shared<ore::data::InMemoryReport>();
    if (report != nullptr) {
        newReport->addColumn("Scenario", string());
        for (size_t i = 0; i < report->columns(); i++) {
            newReport->addColumn(report->header(i), report->columnType(i), report->columnPrecision(i));
        }
        for (size_t row = 0; row < report->rows(); row++) {
            newReport->next();
            newReport->add(label);
            for (size_t col = 0; col < report->columns(); col++) {
                newReport->add(report->data(col)[row]);
            }
        }
        newReport->end();
    }
    return newReport;
}

void XvaStressAnalyticImpl::writeCubes(const std::string& label,
                                       const QuantLib::ext::shared_ptr<XvaAnalytic>& xvaAnalytic) {
    if (!inputs_->xvaStressWriteCubes() || xvaAnalytic == nullptr) {
        return;
    }

    if (inputs_->rawCubeOutput()) {
        DLOG("Write raw cube under scenario " << label);
        // analytic()->reports()["XVA_STRESS"]["rawcube_" + label] = xvaAnalytic->reports()["XVA"]["rawcube"];
        xvaAnalytic->reports()["XVA"]["rawcube"]->toFile(inputs_->resultsPath().string() + "/rawcube_" + label +
                                                         ".csv");
    }

    if (inputs_->netCubeOutput()) {
        DLOG("Write raw cube under scenario " << label);
        // analytic()->reports()["XVA_STRESS"]["netcube_" + label] = xvaAnalytic->reports()["XVA"]["netcube"];
        xvaAnalytic->reports()["XVA"]["netcube"]->toFile(inputs_->resultsPath().string() + "/netcube_" + label +
                                                         ".csv");
    }

    if (inputs_->writeCube()) {
        const auto& cubes = xvaAnalytic->npvCubes()["XVA"];
        for (const auto& [name, cube] : cubes) {
            DLOG("Write cube under scenario " << name << " for scenario" << label);
            // analytic()->npvCubes()["XVA_STRESS"][name + "_" + label] = cube;
            NPVCubeWithMetaData r;
            r.cube = cube;
            if (name == "cube") {
                // store meta data together with npv cube
                r.scenarioGeneratorData = inputs_->scenarioGeneratorData();
                r.storeFlows = inputs_->storeFlows();
                r.storeCreditStateNPVs = inputs_->storeCreditStateNPVs();
            }
            saveCube(inputs_->resultsPath().string() + "/" + name + "_" + label + ".csv.gz", r);
        }
    }

    if (inputs_->writeScenarios()) {
        DLOG("Write scenario report under scenario " << label);
        // analytic()->reports()["XVA_STRESS"]["scenario" + label] = xvaAnalytic->reports()["XVA"]["scenario"];
        xvaAnalytic->reports()["XVA"]["scenario"]->toFile(inputs_->resultsPath().string() + "/scenario" + label +
                                                          ".csv");
    }
}

XvaStressAnalyticImpl::XvaStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaStressAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA Stress analytic.");

    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "XvaStressAnalytic::run: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    // build t0, sim market, stress scenario generator

    CONSOLEW("XVA_STRESS: Build T0 and Sim Markets and Stress Scenario Generator");

    analytic()->buildMarket(loader);

    LOG("XVA Stress: Build SimMarket and StressTestScenarioGenerator")
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), inputs_->xvaStressSimMarketParams(), marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), inputs_->xvaStressScenarioData()->useSpreadedTermStructures(), false, false,
        *inputs_->iborFallbackConfig(), true);

    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<StressScenarioGenerator>(
        inputs_->xvaStressScenarioData(), baseScenario, inputs_->xvaStressSimMarketParams(), simMarket, scenarioFactory,
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

void XvaStressAnalyticImpl::runStressTest(const QuantLib::ext::shared_ptr<StressScenarioGenerator>& scenarioGenerator,
                                          const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {

    std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> xvaReports;
    std::map<std::string, std::map<std::string, XvaResult>> nettingSetXvaResults;
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        const std::string& label = scenario != nullptr ? scenario->label() : std::string();
        try {
            DLOG("Calculate XVA for scenario " << label);
            CONSOLE("XVA_STRESS: Apply scenario " << label);
            auto newAnalytic = ext::make_shared<XvaAnalytic>(inputs_, (label == "BASE" ? nullptr : scenario));
            CONSOLE("XVA_STRESS: Calculate Exposure and XVA")
            newAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});
            nettingSetXvaResults[label] = newAnalytic->nettingSetResults();
            // Collect exposure and xva reports
            for (auto& [name, rpt] : newAnalytic->reports()["XVA"]) {
                // add scenario column to report and copy it, concat it later
                if (boost::starts_with(name, "exposure") || boost::starts_with(name, "xva")) {
                    DLOG("Save and extend report " << name);
                    xvaReports[name].push_back(extendReport(label, rpt));
                }
            }
            writeCubes(label, newAnalytic);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaStress", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }
    writeStressTestReport(nettingSetXvaResults);
    concatReports(xvaReports);
}

void XvaStressAnalyticImpl::concatReports(
    const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>& xvaReports) {
    DLOG("Concat exposure and xva reports");
    for (auto& [name, reports] : xvaReports) {
        if (!reports.empty()) {
            DLOG("Concat report " << name);
            auto firstReport = reports.front();
            for (size_t i = 1; i < reports.size(); i++) {
                if (reports[i] != nullptr) {
                    firstReport->add(*reports[i]);
                }
            }
            analytic()->reports()["XVA_STRESS"][name] = firstReport;
        }
    }
}

void XvaStressAnalyticImpl::writeStressTestReport(
    const std::map<std::string, std::map<std::string, XvaResult>>& xvaResults) {
    auto it = xvaResults.find("BASE");
    if (it != xvaResults.end()) {
        auto nettingSetReport = buildNettingSetReport();
        for (const auto& [label, results] : xvaResults) {
            addResultsToNettingSetReport(label, nettingSetReport, results, it->second);
        }
        nettingSetReport->end();
        analytic()->reports()["XVA_STRESS"]["xva_stress"] = nettingSetReport;
    } else {
        StructuredAnalyticsWarningMessage("XvaStressTest", "Internal Error", "Missing base scenario").log();
    }
}

void XvaStressAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->xvaStressSimMarketParams();
}

XvaStressAnalytic::XvaStressAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaStressAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, false, false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

} // namespace analytics
} // namespace ore
