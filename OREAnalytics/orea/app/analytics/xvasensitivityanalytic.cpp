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

XvaSensitivityAnalyticImpl::XvaResults::XvaResults(const QuantLib::ext::shared_ptr<InMemoryReport>& xvaReport) {
    size_t tradeIdColumn = xvaReport->columnPosition("TradeId");
    size_t nettingSetIdColumn = xvaReport->columnPosition("NettingSetId");
    size_t cvaColumn = xvaReport->columnPosition("CVA");
    size_t dvaColumn = xvaReport->columnPosition("DVA");
    size_t fbaColumn = xvaReport->columnPosition("FBA");
    size_t fcaColumn = xvaReport->columnPosition("FCA");

    for (size_t i = 0; i < xvaReport->rows(); ++i) {
        const std::string& tradeId = boost::get<std::string>(xvaReport->data(tradeIdColumn, i));
        const std::string& nettingset = boost::get<std::string>(xvaReport->data(nettingSetIdColumn, i));
        const double cva = boost::get<double>(xvaReport->data(cvaColumn, i));
        const double dva = boost::get<double>(xvaReport->data(dvaColumn, i));
        const double fba = boost::get<double>(xvaReport->data(fbaColumn, i));
        const double fca = boost::get<double>(xvaReport->data(fcaColumn, i));
        data_[{nettingset, tradeId}] = {cva, dva, fba, fca};
    }
}

bool operator<(const XvaSensitivityAnalyticImpl::XvaResults::Key& lhs,
               const XvaSensitivityAnalyticImpl::XvaResults::Key& rhs) {
    return std::tie(lhs.nettingSetId, lhs.tradeId) < std::tie(rhs.nettingSetId, rhs.tradeId);
}

XvaSensitivityAnalyticImpl::XvaSensitivityAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaSensitivityAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                             const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA_SENSITIVITY analytic.");

    SavedSettings settings;

    optional<bool> localIncTodaysCashFlows = inputs_->exposureIncludeTodaysCashFlows();
    Settings::instance().includeTodaysCashFlows() = localIncTodaysCashFlows;
    LOG("Simulation IncludeTodaysCashFlows is defined: " << (localIncTodaysCashFlows ? "true" : "false"));
    if (localIncTodaysCashFlows) {
        LOG("Exposure IncludeTodaysCashFlows is set to " << (*localIncTodaysCashFlows ? "true" : "false"));
    }

    bool localIncRefDateEvents = inputs_->exposureIncludeReferenceDateEvents();
    Settings::instance().includeReferenceDateEvents() = localIncRefDateEvents;
    LOG("Simulation IncludeReferenceDateEvents is set to " << (localIncRefDateEvents ? "true" : "false"));

    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "XvaSensitivityAnalytic::run: No portfolio loaded.");

    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    void runParAnalysis();
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

    std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>> xvaReports;
    std::map<std::string, XvaResults> xvaResults;
    std::map<std::string, size_t> scenarioIdx;

    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        auto desc = scenarioGenerator->scenarioDescriptions()[i];
        const auto label = scenario->label();
        scenarioIdx[label] = i;
        try {
            DLOG("Calculate XVA for scenario " << label);
            CONSOLE("XVA_SENSITIVITY: Apply scenario " << label);
            auto newAnalytic =
                ext::make_shared<XvaAnalytic>(inputs_, scenario, analytic()->configurations().simMarketParams);
            CONSOLE("XVA_SENSITIVITY: Calculate Exposure and XVA")
            newAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});
            // Collect exposure and xva reports
            for (auto& [name, rpt] : newAnalytic->reports()["XVA"]) {
                // add scenario column to report and copy it, concat it later
                if (boost::starts_with(name, "exposure") || boost::starts_with(name, "xva")) {
                    xvaReports[name][label] = rpt;
                    if (name == "xva") {
                        xvaResults[label] = XvaResults(rpt);
                    }
                }
            }
            createDetailReport(scenarioGenerator, scenarioIdx, xvaReports);
            createZeroSensiReport(scenarioGenerator, scenarioIdx, xvaResults);

        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaSensitivity", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }

    // Create CVA, DVA, FBA, FCA zero sensi reports;

    // Run Par Sensitivity Analysis for each of the reports
}

void XvaSensitivityAnalyticImpl::createDetailReport(
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const std::map<std::string, size_t>& scenarioIdx,
    const std::map<std::string, std::map<std::string, ext::shared_ptr<InMemoryReport>>>& xvaReports) {
    for (auto& [reportName, reports] : xvaReports) {
        std::vector<ext::shared_ptr<InMemoryReport>> extendedReports;
        for (const auto& [scenarioName, rpt] : reports) {
            QuantLib::ext::shared_ptr<ore::data::InMemoryReport> descReport =
                QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
            auto desc = scenarioGenerator->scenarioDescriptions()[scenarioIdx.at(scenarioName)];
            double shiftSize1 = 0.0;
            auto itShiftSize1 = scenarioGenerator->shiftSizes().find(desc.key1());
            if (itShiftSize1 != scenarioGenerator->shiftSizes().end()) {
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
            extendedReports.push_back(addColumnsToExisitingReport(descReport, rpt));
        }
        auto report = concatenateReports(extendedReports);
        if (report != nullptr) {
            analytic()->reports()[label()][reportName] = report;
        }
    }
}

void XvaSensitivityAnalyticImpl::createZeroSensiReport(
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const std::map<std::string, size_t>& scenarioIdx,
    const std::map<std::string, XvaSensitivityAnalyticImpl::XvaResults>& xvaResults) {
    auto baseIt = xvaResults.find("BASE");
    QL_REQUIRE(baseIt != xvaResults.end(), "No base scenario found");
    auto base = baseIt->second;
    auto cvaReport = createEmptySensitivityReport();
    for (const auto& [scenarioLabel, results] : xvaResults) {
        if (scenarioLabel == "BASE") {
            continue;
        }
        for (const auto& [baseKey, baseValues] : base.data()) {
            auto scenarioIt = results.data().find(baseKey);
            if (scenarioIt != results.data().end()) {
                auto baseCVA = baseValues.cva;
                auto scenarioCVA = scenarioIt->second.cva;
                auto delta = scenarioCVA - baseCVA;
                addZeroSensitivityToReport(cvaReport, scenarioLabel, scenarioGenerator, scenarioIdx,
                                           baseKey.nettingSetId, baseKey.tradeId, baseCVA, delta);

            } else {
                // todo handle missing case
            }
        }
        for (const auto& [scenarioKey, scenarioValues] : results.data()) {
            auto baseValueIt = base.data().find(scenarioKey);
            if (baseValueIt == base.data().end()) {
                // todo error handling scenario key but no base key
            }
        }
    }
    cvaReport->end();
    analytic()->reports()[label()]["cva_zero_sensitivity"] = cvaReport;
}

ext::shared_ptr<InMemoryReport> XvaSensitivityAnalyticImpl::createEmptySensitivityReport() {
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());

    report->addColumn("NettingSet", string());
    report->addColumn("TradeId", string());
    report->addColumn("IsPar", string());
    report->addColumn("Factor_1", string());
    report->addColumn("ShiftSize_1", double(), 6);
    report->addColumn("Factor_2", string());
    report->addColumn("ShiftSize_2", double(), 6);
    report->addColumn("Currency", string());
    report->addColumn("BaseValue", double(), 2);
    report->addColumn("Delta", double(), 2);
    report->addColumn("Gamma", double(), 2);
    return report;
}

void XvaSensitivityAnalyticImpl::addZeroSensitivityToReport(
    ext::shared_ptr<InMemoryReport>& report, const std::string& scenarioLabel,
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const std::map<std::string, size_t>& scenarioIdx, const std::string& nettingSetId, const std::string& tradeId,
    double baseValue, double delta) {
    auto desc = scenarioGenerator->scenarioDescriptions()[scenarioIdx.at(scenarioLabel)];
    double shiftSize1 = 0.0;
    auto itShiftSize1 = scenarioGenerator->shiftSizes().find(desc.key1());
    if (itShiftSize1 != scenarioGenerator->shiftSizes().end()) {
        shiftSize1 = (itShiftSize1->second);
    }
    double shiftSize2 = 0.0;
    auto itShiftSize2 = scenarioGenerator->shiftSizes().find(desc.key2());
    if (itShiftSize2 != scenarioGenerator->shiftSizes().end()) {
        shiftSize2 = (itShiftSize2->second);
    }

    report->next();
    report->add(nettingSetId);
    report->add(tradeId);
    report->add("false");
    report->add(desc.factor1());
    report->add(shiftSize1);
    report->add(desc.factor2());
    report->add(shiftSize2);
    report->add(inputs_->baseCurrency());
    report->add(baseValue);
    report->add(delta);
    report->add(0.0);
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
