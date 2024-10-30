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
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/scenario/clonescenariofactory.hpp>


#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/utilities.hpp>
namespace ore {
namespace analytics {

XvaResults::XvaResults(const QuantLib::ext::shared_ptr<InMemoryReport>& xvaReport) {
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
        nettingSetIds_.insert(nettingset);
        tradeIds_.insert(tradeId);
        tradeNettingSetMapping_[tradeId] = nettingset;
        if (tradeId.empty()) {
            nettingSetValueAdjustments_[nettingset][Adjustment::CVA] = cva;
            nettingSetValueAdjustments_[nettingset][Adjustment::DVA] = dva;
            nettingSetValueAdjustments_[nettingset][Adjustment::FBA] = fba;
            nettingSetValueAdjustments_[nettingset][Adjustment::FCA] = fca;
        } else {
            tradeValueAdjustments_[tradeId][Adjustment::CVA] = cva;
            tradeValueAdjustments_[tradeId][Adjustment::DVA] = dva;
            tradeValueAdjustments_[tradeId][Adjustment::FBA] = fba;
            tradeValueAdjustments_[tradeId][Adjustment::FCA] = fca;
        }
    }
}

XvaSensitivityAnalyticImpl::XvaSensitivityAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

QuantLib::ext::shared_ptr<ScenarioSimMarket> XvaSensitivityAnalyticImpl::buildSimMarket(bool overrideTenors){
    LOG("XvaSensitivityAnalytic: Build SimMarket")
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), analytic()->configurations().sensiScenarioData->useSpreadedTermStructures(), false,
        overrideTenors, *inputs_->iborFallbackConfig(), true);
    return simMarket;
}

QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> XvaSensitivityAnalyticImpl::buildScenarioGenerator(QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket, bool overrideTenors){
    LOG("XvaSensitivityAnalytic: Build SensitivityScenarioGenerator")
    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
        analytic()->configurations().sensiScenarioData, baseScenario, analytic()->configurations().simMarketParams,

    void runSimulations(std::vector<XvaResults>& results,
                        const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);


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

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    // build t0, sim market, stress scenario generator

    CONSOLEW("XVA_SENSI: Build T0");

    analytic()->buildMarket(loader);

    CONSOLE("OK");

    // generate the stress scenarios and run dependent xva analytic under each of them

    CONSOLE("XVA_SENSI: Running sensi scenarios");

    // run stress test
    LOG("Run XVA Zero Sensitivity Sensitivity")
    auto zeroCubes = runXvaZeroSensitivitySimulation(loader);
    createZeroReports(zeroCubes);
    LOG("Run Par Conversion")
    auto parCubes = parConversion(zeroCubes);
    createParReports(parCubes);
    LOG("Running XVA Sensitivity analytic finished.");
}

ZeroSenisResults XvaSensitivityAnalyticImpl::runXvaZeroSensitivitySimulation(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    // Run Simulations 
    auto simMarket = buildSimMarket(false);
    auto scenarioGenerator = buildScenarioGenerator(simMarket, false);
    std::vector<XvaResults> xvaResults;
    runSimulations(xvaResults, loader, scenarioGenerator);
    // Create sensi from xva simulation results cubes
    return ZeroSenisResults();
}

void XvaSensitivityAnalyticImpl::runSimulations(std::vector<XvaResults>& xvaResults, const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator) {
    // Used for the raw report
    std::map<std::string, std::vector<ext::shared_ptr<InMemoryReport>> xvaReports;
    
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(inputs_->asof());
        auto desc = scenarioGenerator->scenarioDescriptions()[i];
        const auto label = scenario->label();
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
                    xvaReports[name].push_back(rpt);
                    if (name == "xva") {
                        xvaResults.push_back(XvaResults(rpt));
                    }
                }
            }
            createDetailReport(scenarioGenerator, scenarioIdx, xvaReports);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaSensitivity", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }

void XvaSensitivityAnalyticImpl::createDetailReport(
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const std::map<std::string, std::vector<ext::shared_ptr<InMemoryReport>>>& xvaReports) {
    for (auto& [reportName, reports] : xvaReports) {
        std::vector<ext::shared_ptr<InMemoryReport>> extendedReports;
        for (size_t idx = 0; idx < reports.size(); idx++) {
            QuantLib::ext::shared_ptr<ore::data::InMemoryReport> descReport =
                QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
            auto desc = scenarioGenerator->scenarioDescriptions()[idx];
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
            extendedReports.push_back(addColumnsToExisitingReport(descReport, reports[idx]));
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
    std::set<std::string> tradeIds;
    std::set<std::string> nettingSetIds;
    for (const auto& [key, _] : base.data()) {
        tradeIds.insert(key.tradeId);
        nettingSetIds.insert(key.nettingSetId);
    }

    QuantLib::ext::shared_ptr<NPVSensiCube> cvaTradeCube =
        QuantLib::ext::make_shared<DoublePrecisionSensiCube>(tradeIds, inputs_->asof(), scenarioGenerator->samples());

    QuantLib::ext::shared_ptr<NPVSensiCube> cvaNettingSetCube =
        QuantLib::ext::make_shared<DoublePrecisionSensiCube>(nettingSetIds, inputs_->asof(), scenarioGenerator->samples());

    for (const auto& [scenarioLabel, results] : xvaResults) {
        size_t idx = scenarioIdx.at(scenarioLabel);
        
        if (scenarioLabel == "BASE") {
            for(const auto& [baseKey, baseValues] : base.data()){
                if(baseKey.tradeId.empty()){
                    cvaNettingSetCube->setT0(baseValues.cva, baseKey.nettingSetId);
                } else{
                    cvaTradeCube->setT0(baseValues.cva, baseKey.tradeId);
                }
            }
        }

        for (const auto& [baseKey, baseValues] : base.data()) {
            auto scenarioIt = results.data().find(baseKey);
            if (scenarioIt != results.data().end()) {
                auto baseCVA = baseValues.cva;
                auto scenarioCVA = scenarioIt->second.cva;
                auto delta = scenarioCVA - baseCVA;
                if (baseKey.tradeId.empty()) {
                    cvaNettingSetCube->set(scenarioCVA, baseKey.nettingSetId, idx);
                } else {
                    cvaTradeCube->set(scenarioCVA, baseKey.tradeId, idx);
                }
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

    auto cvaTradeSensiCube = QuantLib::ext::make_shared<SensitivityCube>(
        cvaTradeCube, scenarioGenerator->scenarioDescriptions(), scenarioGenerator->shiftSizes(),
        scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());

    auto cvaNettingSetSensiCube = QuantLib::ext::make_shared<SensitivityCube>(cvaNettingSetCube, scenarioGenerator->scenarioDescriptions(),
                                                                      scenarioGenerator->shiftSizes(),
                                                                      scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());


    auto ssTrade = QuantLib::ext::make_shared<SensitivityCubeStream>(cvaTradeSensiCube, inputs_->baseCurrency());
    auto ssNetting = QuantLib::ext::make_shared<SensitivityCubeStream>(cvaNettingSetSensiCube, inputs_->baseCurrency());
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> cvaTradeReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> cvaNettingReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*cvaTradeReport, ssTrade, inputs_->sensiThreshold());
    ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*cvaNettingReport, ssNetting, inputs_->sensiThreshold());
    
    analytic()->reports()[label()]["cva_trade_zero_sensitivity"] = cvaTradeReport;
    analytic()->reports()[label()]["cva_netting_zero_sensitivity"] = cvaNettingReport;
    analytic()->reports()[label()]["cva_zero_sensitivity"] = cvaReport;

    // Par Conversion
    set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
   
    parAnalysis_ = QuantLib::ext::make_shared<ParSensitivityAnalysis>(
        inputs_->asof(), analytic()->configurations().simMarketParams, *analytic()->configurations().sensiScenarioData,
        "", true, typesDisabled);
   
        LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
        parAnalysis_->alignPillars();
   
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    LOG("XvaSensitivityAnalytic: Build SimMarket")
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), analytic()->configurations().sensiScenarioData->useSpreadedTermStructures(), false,
        true, *inputs_->iborFallbackConfig(), true);

    LOG("XvaSensitivityAnalytic: Build SensitivityScenarioGenerator")

    auto baseScenario = simMarket_->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGeneratorParConversion = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
        analytic()->configurations().sensiScenarioData, baseScenario, analytic()->configurations().simMarketParams,
        simMarket_, scenarioFactory, true);
    simMarket_->scenarioGenerator() = scenarioGeneratorParConversion;

    parAnalysis_->computeParInstrumentSensitivities(simMarket_);
    
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis_->parSensitivities(),
                                                            parAnalysis_->shiftSizes());

    auto parCvaTradeCube =
        QuantLib::ext::make_shared<ZeroToParCube>(cvaTradeSensiCube, parConverter, typesDisabled, true);
    QuantLib::ext::shared_ptr<ParSensitivityCubeStream> pss =
        QuantLib::ext::make_shared<ParSensitivityCubeStream>(parCvaTradeCube, inputs_->baseCurrency());
    // If the stream is going to be reused - wrap it into a buffered stream to gain some
    // performance. The cost for this is the memory footpring of the buffer.
    QuantLib::ext::shared_ptr<InMemoryReport> parSensiReport =
        QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
    analytic()->reports()[label()]["par_cva_trade_sensitivity"] = parSensiReport;
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
