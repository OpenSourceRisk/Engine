/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/varanalytic.hpp>
#include <orea/app/analytics/utilities.hpp>
#include <orea/app/inputparameters.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/historicalsimulationvar.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>
#include <ored/report/inmemoryreport.hpp>

using namespace ore::data;
using namespace std::filesystem;
using namespace QuantLib::ext;

namespace ore {
namespace analytics {

void VarVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {

    vector<string> varAnalytics = {"var", "parametricVar", "historicalSimulationVar"};
        
    inputs->loadParameter<vector<double>>(varQuantiles_, varAnalytics, "quantiles", false, parseListOfRealValues);
    inputs->loadParameter<bool>(varBreakDown_, varAnalytics, "breakdown", false, parseBool);
    inputs->loadParameter<string>(portfolioFilter_, varAnalytics, "portfolioFilter", false);
    inputs->loadParameter<string>(lookbackPeriod_, varAnalytics, vector<string>({"lookbackPeriod", "historicalPeriod"}), false);
    inputs->loadParameter<Size>(horizonDays_, varAnalytics, vector<string>({"horizonDays", "mporDays"}), false, parseInteger);
    inputs->loadParameter<Calendar>(horizonCalendar_, varAnalytics, vector<string>({"horizonCalendar", "mporCalendar"}), false, parseCalendar);
    if (horizonCalendar_.empty())
        horizonCalendar_ = parseCalendar(inputs->setupVariables().baseCurrency_);
    inputs->loadParameter<bool>(horizonOverlappingPeriods_, varAnalytics, vector<string>({"horizonOverlappingPeriods", "mporOverlappingPeriods"}), false, parseBool);
    inputs->loadParameterXML<ScenarioSimMarketParameters>(simMarketParams_, varAnalytics, "simulationConfigFile");
    inputs->loadParameterXML<SensitivityScenarioData>(sensiScenarioData_, varAnalytics, "sensitivityConfigFile");
    
    if (!lookbackPeriod_.empty()) {    
        TimePeriod hsPeriod = totalTimePeriod(vector<string>({lookbackPeriod_}), horizonDays_, horizonCalendar_);
        QL_REQUIRE(hsPeriod.numberOfContiguousParts() == 1,
                   "JSON Body: Historical scenarios period must consist of one contiguous part");
        scenarioReader_ =
            inputs->loadScenarioReader(varAnalytics, vector<string>({"historicalScenarioFile", "scenarioFile"}),
                                       hsPeriod.startDates().front(), hsPeriod.endDates().front());
    }    
    inputs->loadParameter<bool>(outputHistoricalScenarios_, varAnalytics, "outputHistoricalScenarios", false, parseBool);
    inputs->loadParameterXML<ReturnConfiguration>(returnConfiguration_, varAnalytics, "returnConfigFile");
}

void ParametricVarVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    VarVariables::loadVariablesImpl(inputs);
        
    inputs->loadParameter<SalvagingAlgorithm::Type>(varSalvagingAlgorithm_, "parametricVar", "SalvagingAlgorithm", false, parseSalvagingAlgorithmType);
    inputs->loadParameter<string>(varMethod_, "parametricVar", vector<string>({"method", "varMethod"}), false);
    inputs->loadParameter<Size>(mcVarSamples_, "parametricVar", vector<string>({"mcSamples", "varMcSamples"}), false, parseInteger);
    inputs->loadParameter<long>(mcVarSeed_, "parametricVar", vector<string>({"mcSeed", "varMcSeed"}), false, parseInteger);
        
    string covarianceInputFile;
    inputs->loadParameter<string>(covarianceInputFile, "parametricVar", "covarianceInputFile", false);
    if (!covarianceInputFile.empty())
        covarianceData_ =
            loadCorrelationDataFromFile((inputs->setupVariables().inputPath_ / covarianceInputFile).generic_string());

    sensitivityStream_ = inputs->loadSensitivityStream("parametricVar", "sensitivityInputFile");
}

void HistoricalSimulationVarVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    VarVariables::loadVariablesImpl(inputs);

    inputs->loadParameter<bool>(includeExpectedShortfall_, "historicalSimulationVar", "includeExpectedShortfall", false, parseBool);
    inputs->loadParameter<bool>(tradePnL_, "historicalSimulationVar", "tradePnl", false, parseBool);
    inputs->loadParameter<bool>(riskFactorBreakdown_, "historicalSimulationVar", "riskFactorBreakdown", false, parseBool);
}

/***********************************************************************************
 * VAR Analytic: DELTA-VAR, DELTA-GAMMA-NORMAL-VAR, MONTE-CARLO-VAR
 ***********************************************************************************/

void VarAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void VarAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::set<std::string>& runTypes) {
    
    MEM_LOG;
    LOG("Running parametric VaR");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    LOG("VAR: Build Market");
    CONSOLEW("Risk: Build Market for VaR");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Risk: Build Portfolio for VaR");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    setVarReport(loader);
    QL_REQUIRE(varReport_, "No Var Report created");
    
    LOG("Call VaR calculation");
    CONSOLEW("Risk: VaR Calculation");
    ext::shared_ptr<MarketRiskReport::Reports> reports = ext::make_shared<MarketRiskReport::Reports>();
    QuantLib::ext::shared_ptr<InMemoryReport> varReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    reports->add(varReport);

    addAdditionalReports(reports);

    varReport_->calculate(reports);
    CONSOLE("OK");
    
    analytic()->addReport(label_, "var", varReport);

    LOG("VaR completed");
    MEM_LOG;
}

QuantLib::ext::shared_ptr<SensitivityStream>
ParametricVarAnalyticImpl::sensiStream(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    auto varVars = ext::dynamic_pointer_cast<ParametricVarVariables>(inputVariables_);
    return varVars->sensitivityStream_;
};

void ParametricVarAnalyticImpl::setUpConfigurations() {
    VarAnalyticImpl::setUpConfigurations();
    auto varVars = ext::dynamic_pointer_cast<ParametricVarVariables>(inputVariables_);
    if (varVars->covarianceData_.size() == 0) {
        analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
        analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    }
}

void ParametricVarAnalyticImpl::setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    LOG("Build trade to portfolio id mapping");
    auto varVars = ext::dynamic_pointer_cast<ParametricVarVariables>(inputVariables_);
    ParametricVarCalculator::ParametricVarParams varParams(varVars->varMethod_, varVars->mcVarSamples_,
                                                           varVars->mcVarSeed_);

    QuantLib::ext::shared_ptr<SensitivityStream> ss = sensiStream(loader);

    LOG("Build VaR calculator");
    if (varVars->covarianceData_.size() > 0) {
        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, nullptr, 0.01, varVars->covarianceData_);

        varReport_ = ext::make_shared<ParametricVarReport>(
            inputs_->baseCurrency(), analytic()->portfolio(), varVars->portfolioFilter_, varVars->varQuantiles_,
            varParams, varVars->varSalvagingAlgorithm_, QuantLib::ext::nullopt, std::move(sensiArgs),
            varVars->varBreakDown_, inputs_->useAtParCouponsCurves(), inputs_->useAtParCouponsTrades());
    } else {
        QL_REQUIRE(varVars->lookbackPeriod_ != std::string(), "BenchmarkVarPeriod Required.");
        TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(varVars->lookbackPeriod_, &parseDate),
                                      varVars->horizonDays_, varVars->horizonCalendar_);

        QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
        if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
            adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

        auto returnConfig = varVars->returnConfiguration_
                                ? varVars->returnConfiguration_
                                : QuantLib::ext::make_shared<ReturnConfiguration>();

        QL_REQUIRE(varVars->scenarioReader_, "ScenarioReader Required.");

        auto scenarios = buildHistoricalScenarioGenerator(
            varVars->scenarioReader_, adjFactors, benchmarkVarPeriod, varVars->horizonCalendar_, varVars->horizonDays_,
            analytic()->configurations().simMarketParams, analytic()->configurations().todaysMarketParams,
            returnConfig, varVars->horizonOverlappingPeriods_);

        if (varVars->outputHistoricalScenarios_)
            ReportWriter().writeHistoricalScenarios(
                scenarios->scenarioLoader(),
                QuantLib::ext::make_shared<CSVFileReport>(path(inputs_->resultsPath() / "backtest_histscenarios.csv").string(),
                                                  ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString()));

        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
            *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false,
            false, false, inputs_->iborFallbackConfig());
        simMarket->scenarioGenerator() = scenarios;
        scenarios->baseScenario() = simMarket->baseScenario();

        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator = QuantLib::ext::make_shared<ScenarioShiftCalculator>(
            analytic()->configurations().sensiScenarioData, analytic()->configurations().simMarketParams);

        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator, 0.01, varVars->covarianceData_);

        varReport_ = ext::make_shared<ParametricVarReport>(
            inputs_->baseCurrency(), analytic()->portfolio(), varVars->portfolioFilter_, scenarios,
            varVars->varQuantiles_, varParams, varVars->varSalvagingAlgorithm_, benchmarkVarPeriod,
            std::move(sensiArgs), varVars->varBreakDown_);
    }
}

void HistoricalSimulationVarAnalyticImpl::setUpConfigurations() {
    VarAnalyticImpl::setUpConfigurations();
    auto varVars = ext::dynamic_pointer_cast<HistoricalSimulationVarVariables>(inputVariables_);
    analytic()->configurations().simMarketParams = varVars->simMarketParams_;
    // ORE Swig does not handle that yet
    // inputs_->loadParameter<bool>(riskFactorBreakdown_, "historicalSimulationVar", "riskFactorBreakdown", false,
    //                              std::function<bool(const string&)>(parseBool));
    riskFactorBreakdown_ = varVars->riskFactorBreakdown_;
    if(riskFactorBreakdown_){
        allowPartialScenarios_ = true;
    }
}

void HistoricalSimulationVarAnalyticImpl::setVarReport(
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    auto varVars = ext::dynamic_pointer_cast<HistoricalSimulationVarVariables>(inputVariables_);

    LOG("Build VaR calculator");
    QL_REQUIRE(varVars->lookbackPeriod_ != std::string(), "LookbackPeriod Required.");
    TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(varVars->lookbackPeriod_, &parseDate), varVars->horizonDays_,
                                  varVars->horizonCalendar_);
        
    QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
        adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

    auto returnConfig = varVars->returnConfiguration_
                            ? varVars->returnConfiguration_
                            : QuantLib::ext::make_shared<ReturnConfiguration>();

    QL_REQUIRE(varVars->scenarioReader_, "ScenarioReader Required.");

    auto scenarios = buildHistoricalScenarioGenerator(
        varVars->scenarioReader_, adjFactors, benchmarkVarPeriod, varVars->horizonCalendar_, varVars->horizonDays_,
        analytic()->configurations().simMarketParams, analytic()->configurations().todaysMarketParams,
        returnConfig, varVars->horizonOverlappingPeriods_, riskFactorBreakdown_);

    if (varVars->outputHistoricalScenarios_)
        ore::analytics::ReportWriter().writeHistoricalScenarios(
            scenarios->scenarioLoader(),
            QuantLib::ext::make_shared<CSVFileReport>(path(inputs_->resultsPath() / "var_histscenarios.csv").string(), ',',
                                              false, inputs_->csvQuoteChar(), inputs_->reportNaString()));
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false, false,
        allowPartialScenarios_, inputs_->iborFallbackConfig());
    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    std::unique_ptr<MarketRiskReport::FullRevalArgs> fullRevalArgs = std::make_unique<MarketRiskReport::FullRevalArgs>(
        simMarket, inputs_->pricingEngine(), inputs_->refDataManager(), inputs_->iborFallbackConfig());

    std::unique_ptr<MarketRiskReport::MultiThreadArgs> multiThreadsArgs;
    if(inputs_->nThreads()>1)
        multiThreadsArgs = std::make_unique<MarketRiskReport::MultiThreadArgs>(inputs_->nThreads(), inputs_->asof(), analytic()->loader(), 
                                                                                analytic()->configurations().curveConfig, analytic()->configurations().todaysMarketParams,
                                                                                inputs_->marketConfig("simulation"), analytic()->configurations().simMarketParams, "histstimvar-simulation");

    varReport_ = ext::make_shared<HistoricalSimulationVarReport>(
        inputs_->baseCurrency(), analytic()->portfolio(), varVars->portfolioFilter_, varVars->varQuantiles_,
        benchmarkVarPeriod, scenarios, std::move(fullRevalArgs), std::move(multiThreadsArgs), varVars->varBreakDown_,
        varVars->includeExpectedShortfall_, varVars->tradePnL_, riskFactorBreakdown_,
        inputs_->useAtParCouponsCurves(), inputs_->useAtParCouponsTrades());
}

void HistoricalSimulationVarAnalyticImpl::addAdditionalReports(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {

        QuantLib::ext::shared_ptr<InMemoryReport> histPnLReport =
        QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());

        reports->add(histPnLReport);
        analytic()->addReport(label_, "historical_PnL", histPnLReport);

        if(riskFactorBreakdown_){
            QuantLib::ext::shared_ptr<InMemoryReport> histPnLRFReport =
                QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
            reports->add(histPnLRFReport);
            analytic()->addReport(label_, "riskFactor_PnL", histPnLRFReport);
        }
}

} // namespace analytics
} // namespace ore
