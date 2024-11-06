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
    QL_REQUIRE(xvaReport != nullptr, "Empty xvaReport, can not extract any values");
    QL_REQUIRE(xvaReport->hasHeader("TradeId"), "Expect column 'tradeId' in XVA report.");
    QL_REQUIRE(xvaReport->hasHeader("NettingSetId"), "Expect column 'NettingSetId' in XVA report.");
    QL_REQUIRE(xvaReport->hasHeader("CVA"), "Expect column 'CVA' in XVA report.");
    QL_REQUIRE(xvaReport->hasHeader("DVA"), "Expect column 'DVA' in XVA report.");
    QL_REQUIRE(xvaReport->hasHeader("FBA"), "Expect column 'FBA' in XVA report.");
    QL_REQUIRE(xvaReport->hasHeader("FCA"), "Expect column 'FCA' in XVA report.");

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
        if(!tradeId.empty()){
            tradeIds_.insert(tradeId);
            tradeNettingSetMapping_[tradeId] = nettingset;
        }
        if (tradeId.empty()) {
            nettingSetValueAdjustments_[Adjustment::CVA][nettingset] = cva;
            nettingSetValueAdjustments_[Adjustment::DVA][nettingset] = dva;
            nettingSetValueAdjustments_[Adjustment::FBA][nettingset] = fba;
            nettingSetValueAdjustments_[Adjustment::FCA][nettingset] = fca;
        } else {
            tradeValueAdjustments_[Adjustment::CVA][tradeId] = cva;
            tradeValueAdjustments_[Adjustment::DVA][tradeId] = dva;
            tradeValueAdjustments_[Adjustment::FBA][tradeId] = fba;
            tradeValueAdjustments_[Adjustment::FCA][tradeId] = fca;
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
    std::string marketConfig = inputs_->marketConfig("pricing");
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
    QL_REQUIRE(simMarket != nullptr, "Can not build scenario generator without a valid sim market");
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
    auto zeroCubes = computeZeroXvaSensitivity(loader);
    createZeroReports(zeroCubes);
    if (inputs_->xvaSensiParSensi()) {
        LOG("Run Par Conversion")
        auto parCubes = parConversion(zeroCubes);
        createParReports(parCubes, zeroCubes.tradeNettingSetMap_);
        LOG("Running XVA Sensitivity analytic finished.");
        
    }
}

XvaSensitivityAnalyticImpl::ZeroSensiResults XvaSensitivityAnalyticImpl::computeZeroXvaSensitivity(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    auto simMarket = buildSimMarket(false);
    auto scenarioGenerator = buildScenarioGenerator(simMarket, false);
    std::map<size_t, ext::shared_ptr<XvaResults>> xvaResults;
    computeXvaUnderScenarios(xvaResults, loader, scenarioGenerator);
    // Create sensi cubes from xva simulation results   
    return convertXvaResultsToSensiCubes(xvaResults, scenarioGenerator);
}

XvaSensitivityAnalyticImpl::ZeroSensiResults XvaSensitivityAnalyticImpl::convertXvaResultsToSensiCubes(
    const std::map<size_t, QuantLib::ext::shared_ptr<XvaResults>>& xvaResults,
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator) {
    static const std::vector<XvaResults::Adjustment> adjustments = {
        XvaResults::Adjustment::CVA, XvaResults::Adjustment::DVA, XvaResults::Adjustment::FBA,
        XvaResults::Adjustment::FCA};

    // Initialze cubes
    auto baseIt = xvaResults.find(0);
    QL_REQUIRE(baseIt != xvaResults.end(),
               "XVA Sensitivity Run ended without a base scenario");
    auto& baseResults = baseIt->second;
    QL_REQUIRE(baseResults != nullptr, "XVA Sensitivity can not computed without valid base scenario results");
    // Initializx Zero Cubes
    std::map<XvaResults::Adjustment, ext::shared_ptr<NPVSensiCube>> nettingZeroCubes, tradeZeroCubes;
    
    for (const auto& valueAdjustment : adjustments) {
        nettingZeroCubes[valueAdjustment] = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(
            baseResults->nettingSetIds(), inputs_->asof(), scenarioGenerator->samples());
        tradeZeroCubes[valueAdjustment] = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(
            baseResults->tradeIds(), inputs_->asof(), scenarioGenerator->samples());
    }

    // Populate cubes
    auto populateCube = [](ext::shared_ptr<NPVSensiCube>& cube, const std::string& tradeId, const size_t& scenarioIdx,
                           const std::map<std::string, double>& xvas) {
        auto it = xvas.find(tradeId);
        QL_REQUIRE(it != xvas.end(),
                   "XVA values for trade id " << tradeId << " under scenario " << scenarioIdx << " not found ");
        auto& value = it->second;
        if (scenarioIdx == 0) {
            cube->setT0(value, tradeId);
        }
        cube->set(value, tradeId, scenarioIdx);
    };

    std::map<std::string, std::set<size_t>> tradeHasScenarioError;
    std::map<std::string, std::set<size_t>> nettingSetHasScenarioError;
    std::set<size_t> scenariosWithErrors;

    for (const auto& [i, results] : xvaResults) {
        if (results == nullptr) {
            StructuredAnalyticsErrorMessage(
                "XvaSensitivity", "XVACalc",
                "Error during populating cubes, results for scenario " + to_string(i) + " are missing, removing it from results")
                .log();
            continue;
        }
        for (const auto& tradeId : baseResults->tradeIds()) {
            for (const auto& adjustment : adjustments) {
                try {
                    populateCube(tradeZeroCubes[adjustment], tradeId, i, results->getTradeXVAs(adjustment));
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage(
                        "XvaSensitivity", "XVACalc",
                        string("Error during populating cubes with xva values for trade " + tradeId + ", got ") +
                            e.what() + ". Remove it from results.")
                        .log();
                    tradeHasScenarioError[tradeId].insert(i);
                }
            }
        }
        for (const auto& nettingSetId : baseResults->nettingSetIds()) {
            for (const auto& adjustment : adjustments) {
                try {
                    populateCube(nettingZeroCubes[adjustment], nettingSetId, i, results->getNettingSetXVAs(adjustment));
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("XvaSensitivity", "XVACalc",
                                                    "Error during populating cube with xva values for nettingSet " +
                                                        nettingSetId + " , got " + e.what() +
                                                        ". Remove it from results.")
                        .log();
                    nettingSetHasScenarioError[nettingSetId].insert(i);
                }
            }
        }
    }

    for (const auto& [tradeId, scenarios] : tradeHasScenarioError) {
        for (const auto& adjustment : adjustments) {
            auto idx = tradeZeroCubes[adjustment]->getTradeIndex(tradeId);
            for (const auto& scenarioId : scenarios) {
                if (scenarioId == 0) {
                    // Base senario Error and remove all entries
                    tradeZeroCubes[adjustment]->removeT0(idx);
                    tradeZeroCubes[adjustment]->remove(idx, Null<Size>(), false);
                    break;
                } else {
                    tradeZeroCubes[adjustment]->remove(idx, scenarioId, true);
                }
            }
        }
    }

    for (const auto& [nettingSetId, scenarios] : nettingSetHasScenarioError) {
        for (const auto& adjustment : adjustments) {
            auto nettingCube = nettingZeroCubes[adjustment];
            auto idx = nettingCube->getTradeIndex(nettingSetId);

            for (const auto& scenarioId : scenarios) {
                if (scenarioId == 0) {
                    // Base senario Error and remove all entries
                    nettingCube->removeT0(idx);
                    nettingCube->remove(idx, Null<Size>(), false);
                    break;
                } else {
                    nettingCube->remove(idx, scenarioId, true);
                }
            }
        }
    }
    
    for (const auto& scenarioId : scenariosWithErrors){
        for (const auto& adjustment : adjustments) {
            auto tradeCube = tradeZeroCubes[adjustment];
            for (const auto& [_, idx] : tradeCube->idsAndIndexes()){
                tradeCube->remove(idx, scenarioId, true);
            }
            auto nettingCube = nettingZeroCubes[adjustment];
            for (const auto& [_, idx] : nettingCube->idsAndIndexes()){
                nettingCube->remove(idx, scenarioId, true);
            }
        }
    }
    // Create Results
    ZeroSensiResults results;
    results.tradeNettingSetMap_ = baseResults->tradeNettingSetMapping();
    for (const auto& [valueAdjustment, cube] : nettingZeroCubes) {
        results.nettingCubes_[valueAdjustment] = QuantLib::ext::make_shared<SensitivityCube>(
            cube, scenarioGenerator->scenarioDescriptions(), scenarioGenerator->shiftSizes(),
            scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());
    }
    for (const auto& [valueAdjustment, cube] : tradeZeroCubes) {
        results.tradeCubes_[valueAdjustment] = QuantLib::ext::make_shared<SensitivityCube>(
            cube, scenarioGenerator->scenarioDescriptions(), scenarioGenerator->shiftSizes(),
            scenarioGenerator->shiftSizes(), scenarioGenerator->shiftSchemes());
    }

    return results;
}

void XvaSensitivityAnalyticImpl::computeXvaUnderScenarios(std::map<size_t, ext::shared_ptr<XvaResults>>& xvaResults, const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator) {
    // Used for the raw report
    QL_REQUIRE(scenarioGenerator != nullptr,
               "Internal error: Can not compute XVA sensi without valid scenario generator.");
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
                        xvaResults[i] = ext::make_shared<XvaResults>(rpt);
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

void XvaSensitivityAnalyticImpl::createZeroReports(ZeroSensiResults& xvaZeroSeniCubes){
    for(const auto& [valueAdjustment, cube] : xvaZeroSeniCubes.tradeCubes_){
        auto ssTrade = QuantLib::ext::make_shared<SensitivityCubeStream>(cube, inputs_->baseCurrency());
        auto nettingCube = xvaZeroSeniCubes.nettingCubes_[valueAdjustment];
        auto ssNetting = QuantLib::ext::make_shared<SensitivityCubeStream>(nettingCube, inputs_->baseCurrency());
        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> zeroSensiReport =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString())
            .writeXvaSensitivityReport(*zeroSensiReport, ssTrade, ssNetting, xvaZeroSeniCubes.tradeNettingSetMap_,
                                       inputs_->sensiThreshold());
        analytic()->reports()[label()]["xva_zero_sensitivity_" + to_string(valueAdjustment)] = zeroSensiReport;
    }
}

XvaSensitivityAnalyticImpl::ParSensiResults XvaSensitivityAnalyticImpl::parConversion(ZeroSensiResults& zeroResults) {
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

    if (inputs_->xvaSensiOutputJacobi()){
        QuantLib::ext::shared_ptr<InMemoryReport> jacobiReport =
            QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
        analytic()->reports()[label()]["xva_sensi_jacobi"] = jacobiReport;

        QuantLib::ext::shared_ptr<InMemoryReport> jacobiInverseReport =
            QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        parConverter->writeConversionMatrix(*jacobiInverseReport);
        analytic()->reports()[label()]["xva_sensi_jacobi_inverse"] = jacobiInverseReport;
    }

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

void XvaSensitivityAnalyticImpl::createParReports(ParSensiResults& xvaParSensiCubes, const std::map<std::string, std::string>& tradeNettingSetMap){
    for(const auto& [valueAdjustment, cube] : xvaParSensiCubes.tradeParSensiCube_){
        auto pssTrade = QuantLib::ext::make_shared<ParSensitivityCubeStream>(cube, inputs_->baseCurrency());
        auto nettingCube = xvaParSensiCubes.nettingParSensiCube_[valueAdjustment];
        auto pssNetting = QuantLib::ext::make_shared<ParSensitivityCubeStream>(nettingCube, inputs_->baseCurrency());

        QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report =
            QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString())
            .writeXvaSensitivityReport(*report, pssTrade, pssNetting, tradeNettingSetMap,
                                       inputs_->sensiThreshold());
        analytic()->reports()[label()]["xva_par_sensitivity_" + to_string(valueAdjustment)] = report;
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
