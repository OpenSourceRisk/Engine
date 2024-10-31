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

ostream& operator<<(std::ostream& os, XvaResults::Adjustment adjustment) {
    if (adjustment == XvaResults::Adjustment::CVA) {
        return os << "cva";
    } else if (adjustment == XvaResults::Adjustment::DVA) {
        return os << "dva";
    } else if (adjustment == XvaResults::Adjustment::FBA) {
        return os << "fba";
    } else if (adjustment == XvaResults::Adjustment::FCA) {
        return os << "fca";
    } else {
        QL_FAIL("Internal error not recognise XvaResults::Adjustment " << static_cast<int>(adjustment));
    }
}

XvaSensitivityAnalyticImpl::XvaSensitivityAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

QuantLib::ext::shared_ptr<ScenarioSimMarket> XvaSensitivityAnalyticImpl::buildSimMarket(bool overrideTenors){
    LOG("XvaSensitivityAnalytic: Build SimMarket")
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), analytic()->configurations().sensiScenarioData->useSpreadedTermStructures(), false,
        overrideTenors, *inputs_->iborFallbackConfig(), true);
    return simMarket;
}

QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>
XvaSensitivityAnalyticImpl::buildScenarioGenerator(QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                                                   bool overrideTenors) {
    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
        analytic()->configurations().sensiScenarioData, baseScenario, analytic()->configurations().simMarketParams,
        simMarket, scenarioFactory, true);
    simMarket->scenarioGenerator() = scenarioGenerator;
    return scenarioGenerator;
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

XvaSensitivityAnalyticImpl::ZeroSenisResults XvaSensitivityAnalyticImpl::runXvaZeroSensitivitySimulation(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    // Run Simulations 
    auto simMarket = buildSimMarket(false);
    auto scenarioGenerator = buildScenarioGenerator(simMarket, false);
    std::vector<ext::shared_ptr<XvaResults>> xvaResults;
    runSimulations(xvaResults, loader, scenarioGenerator);
    // Create sensi cubes from xva simulation results
    QL_REQUIRE(!xvaResults.empty(),
               "XVA Sensitivity Run ended without any results, there should be at least the base scenario");
    
    auto& baseResults = xvaResults.front();
    std::map<XvaResults::Adjustment, ext::shared_ptr<NPVSensiCube>> nettingZeroCubes, tradeZeroCubes;
    for (const auto& valueAdjustment : {XvaResults::Adjustment::CVA, XvaResults::Adjustment::DVA}) {
        nettingZeroCubes[valueAdjustment] = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(baseResults->nettingSetIds(), inputs_->asof(),
                                                                                          scenarioGenerator->samples());
        tradeZeroCubes[valueAdjustment] = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(
            baseResults->tradeIds(), inputs_->asof(), scenarioGenerator->samples());
    }

    for (size_t i = 0; i < xvaResults.size(); ++i) {
        if (i == 0) {
            for (const auto& tradeId : baseResults->tradeIds()) {
                tradeZeroCubes[XvaResults::Adjustment::CVA]->setT0(
                    baseResults->getTradeValueAdjustment(tradeId, XvaResults::Adjustment::CVA), tradeId);
                tradeZeroCubes[XvaResults::Adjustment::DVA]->setT0(
                    baseResults->getTradeValueAdjustment(tradeId, XvaResults::Adjustment::DVA), tradeId);
            }
            for (const auto& nettingSetId : baseResults->nettingSetIds()) {
                nettingZeroCubes[XvaResults::Adjustment::CVA]->setT0(
                    baseResults->getNettingSetValueAdjustment(nettingSetId, XvaResults::Adjustment::CVA), nettingSetId);
                nettingZeroCubes[XvaResults::Adjustment::DVA]->setT0(
                    baseResults->getNettingSetValueAdjustment(nettingSetId, XvaResults::Adjustment::DVA), nettingSetId);
            }
        }
        for (const auto& tradeId : baseResults->tradeIds()) {
            tradeZeroCubes[XvaResults::Adjustment::CVA]->set(
                baseResults->getTradeValueAdjustment(tradeId, XvaResults::Adjustment::CVA), tradeId, i);
            tradeZeroCubes[XvaResults::Adjustment::DVA]->set(
                baseResults->getTradeValueAdjustment(tradeId, XvaResults::Adjustment::DVA), tradeId, i);
        }
        for (const auto& nettingSetId : baseResults->nettingSetIds()) {
            nettingZeroCubes[XvaResults::Adjustment::CVA]->set(
                baseResults->getNettingSetValueAdjustment(nettingSetId, XvaResults::Adjustment::CVA), nettingSetId, i);
            nettingZeroCubes[XvaResults::Adjustment::DVA]->set(
                baseResults->getNettingSetValueAdjustment(nettingSetId, XvaResults::Adjustment::DVA), nettingSetId, i);
        }
    }
    ZeroSenisResults results;

    for(const auto& [valueAdjustment, cube] : nettingZeroCubes){
        results.nettingCubes_[valueAdjustment] = QuantLib::ext::make_shared<SensitivityCube>(
            cube, scenarioGenerator->scenarioDescriptions(), scenarioGenerator->shiftSizes(),
            scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());
    }
    for(const auto& [valueAdjustment, cube] : tradeZeroCubes){
        results.tradeCubes_[valueAdjustment] = QuantLib::ext::make_shared<SensitivityCube>(
            cube, scenarioGenerator->scenarioDescriptions(), scenarioGenerator->shiftSizes(),
            scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());
    }
    
    return results;
}

void XvaSensitivityAnalyticImpl::runSimulations(std::vector<ext::shared_ptr<XvaResults>>& xvaResults, const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator) {
    // Used for the raw report
    std::map<std::string, std::vector<ext::shared_ptr<InMemoryReport>>> xvaReports;
    
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
                        xvaResults.push_back(ext::make_shared<XvaResults>(rpt));
                    }
                }
            }
            createDetailReport(scenarioGenerator, xvaReports);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XvaSensitivity", "XVACalc",
                                            "Error during XVA calc under scenario " + label + ", got " + e.what() +
                                                ". Skip it")
                .log();
        }
    }
}

void XvaSensitivityAnalyticImpl::createZeroReports(ZeroSenisResults& xvaZeroSeniCubes){
    for(const auto& [valueAdjustment, cube] : xvaZeroSeniCubes.tradeCubes_){
        auto ss = QuantLib::ext::make_shared<SensitivityCubeStream>(cube, inputs_->baseCurrency());
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
        analytic()->reports()[label()][to_string(valueAdjustment) + "_trade_zero_sensitivity"] = report;
    }
    for(const auto& [valueAdjustment, cube] : xvaZeroSeniCubes.nettingCubes_){
        auto ss = QuantLib::ext::make_shared<SensitivityCubeStream>(cube, inputs_->baseCurrency());
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
        analytic()->reports()[label()][to_string(valueAdjustment) + "_netting_zero_sensitivity"] = report;
    }
}

XvaSensitivityAnalyticImpl::ParSensiResults XvaSensitivityAnalyticImpl::parConversion(ZeroSenisResults& zeroResults) {
    set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};

    auto parAnalysis = QuantLib::ext::make_shared<ParSensitivityAnalysis>(
        inputs_->asof(), analytic()->configurations().simMarketParams, *analytic()->configurations().sensiScenarioData,
        "", true, typesDisabled);

    LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
    parAnalysis->alignPillars();

    auto simMarket = buildSimMarket(true);
    auto scenarioGenerator = buildScenarioGenerator(simMarket, true);

    parAnalysis->computeParInstrumentSensitivities(simMarket);

    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());

    ParSensiResults results;

    for(const auto& [valueAdjustment, zeroCube] : zeroResults.tradeCubes_){
        results.tradeParSensiCube_[valueAdjustment] =
            QuantLib::ext::make_shared<ZeroToParCube>(zeroCube, parConverter, typesDisabled, true);
    }

    for(const auto& [valueAdjustment, zeroCube] : zeroResults.nettingCubes_){
        results.nettingParSensiCube_[valueAdjustment] =
            QuantLib::ext::make_shared<ZeroToParCube>(zeroCube, parConverter, typesDisabled, true);
    }

    return results;
}

void XvaSensitivityAnalyticImpl::createParReports(ParSensiResults& xvaParSensiCubes){
    for(const auto& [valueAdjustment, cube] : xvaParSensiCubes.tradeCubes_){
        auto pss = QuantLib::ext::make_shared<ParSensitivityCubeStream>(cube, inputs_->baseCurrency());
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, pss, inputs_->sensiThreshold());
        analytic()->reports()[label()][to_string(valueAdjustment) + "_trade_zero_sensitivity"] = report;
    }
    for (const auto& [valueAdjustment, cube] : xvaParSensiCubes.nettingCubes_) {
        auto pss = QuantLib::ext::make_shared<ParSensitivityCubeStream>(cube, inputs_->baseCurrency());
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, pss, inputs_->sensiThreshold());
        analytic()->reports()[label()][to_string(valueAdjustment) + "_netting_zero_sensitivity"] = report;
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
