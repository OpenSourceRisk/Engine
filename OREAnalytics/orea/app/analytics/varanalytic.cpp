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
#include <orea/app/reportwriter.hpp>
#include <orea/engine/historicalsimulationvar.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

using namespace ore::data;
using namespace boost::filesystem;
using namespace QuantLib::ext;

namespace ore {
namespace analytics {

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
    QuantLib::ext::shared_ptr<InMemoryReport> varReport = QuantLib::ext::make_shared<InMemoryReport>();
    reports->add(varReport);

    varReport_->calculate(reports);
    CONSOLE("OK");
        
    analytic()->reports()[label_]["var"] = varReport;

    LOG("VaR completed");
    MEM_LOG;
}

void ParametricVarAnalyticImpl::setUpConfigurations() {
    VarAnalyticImpl::setUpConfigurations();
    if (inputs_->covarianceData().size() == 0) {
        analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
        analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    }
}

void ParametricVarAnalyticImpl::setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    LOG("Build trade to portfolio id mapping");
    ParametricVarCalculator::ParametricVarParams varParams(inputs_->varMethod(), inputs_->mcVarSamples(),
                                                           inputs_->mcVarSeed());

    QuantLib::ext::shared_ptr<SensitivityStream> ss = sensiStream(loader);

    LOG("Build VaR calculator");
    if (inputs_->covarianceData().size() > 0) {
        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, nullptr, 0.01, inputs_->covarianceData());

        varReport_ = ext::make_shared<ParametricVarReport>(
            inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(), inputs_->varQuantiles(),
            varParams,
            inputs_->salvageCovariance(), boost::none, std::move(sensiArgs), inputs_->varBreakDown());
    } else {
        TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(inputs_->benchmarkVarPeriod(), &parseDate),
                                      inputs_->mporDays(), inputs_->mporCalendar());

        QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
        if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
            adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

        auto scenarios = buildHistoricalScenarioGenerator(inputs_->historicalScenarioReader(), adjFactors,
            benchmarkVarPeriod, inputs_->mporCalendar(), inputs_->mporDays(), analytic()->configurations().simMarketParams,
            analytic()->configurations().todaysMarketParams, inputs_->mporOverlappingPeriods());

        if (inputs_->outputHistoricalScenarios())
            ReportWriter().writeHistoricalScenarios(
                scenarios->scenarioLoader(),
                QuantLib::ext::make_shared<CSVFileReport>(path(inputs_->resultsPath() / "backtest_histscenrios.csv").string(),
                                                  ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString()));

        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
            *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false,
            false, false, *inputs_->iborFallbackConfig());
        simMarket->scenarioGenerator() = scenarios;
        scenarios->baseScenario() = simMarket->baseScenario();

        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator = QuantLib::ext::make_shared<ScenarioShiftCalculator>(
            analytic()->configurations().sensiScenarioData, analytic()->configurations().simMarketParams);

        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator, 0.01, inputs_->covarianceData());

        varReport_ = ext::make_shared<ParametricVarReport>(
            inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(), scenarios,
            inputs_->varQuantiles(), varParams,
            inputs_->salvageCovariance(), benchmarkVarPeriod, std::move(sensiArgs), inputs_->varBreakDown());
    }
}

void HistoricalSimulationVarAnalyticImpl::setUpConfigurations() {
    VarAnalyticImpl::setUpConfigurations();
    analytic()->configurations().simMarketParams = inputs_->histVarSimMarketParams();
}

void HistoricalSimulationVarAnalyticImpl::setVarReport(
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {

    LOG("Build VaR calculator");
    TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(inputs_->benchmarkVarPeriod(), &parseDate), inputs_->mporDays(), inputs_->mporCalendar());
        
    QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
        adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());
        
    auto scenarios =
        buildHistoricalScenarioGenerator(inputs_->historicalScenarioReader(), adjFactors, benchmarkVarPeriod, inputs_->mporCalendar(),
        inputs_->mporDays(), analytic()->configurations().simMarketParams,
        analytic()->configurations().todaysMarketParams, inputs_->mporOverlappingPeriods());
    
    if (inputs_->outputHistoricalScenarios())
        ore::analytics::ReportWriter().writeHistoricalScenarios(
            scenarios->scenarioLoader(),
            QuantLib::ext::make_shared<CSVFileReport>(path(inputs_->resultsPath() / "var_histscenarios.csv").string(), ',',
                                              false, inputs_->csvQuoteChar(), inputs_->reportNaString()));

    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false, false,
        false, *inputs_->iborFallbackConfig());
    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    std::unique_ptr<MarketRiskReport::FullRevalArgs> fullRevalArgs = std::make_unique<MarketRiskReport::FullRevalArgs>(
        simMarket, inputs_->pricingEngine(), inputs_->refDataManager(), *inputs_->iborFallbackConfig());

    varReport_ = ext::make_shared<HistoricalSimulationVarReport>(
        inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(), 
        inputs_->varQuantiles(), benchmarkVarPeriod, scenarios, std::move(fullRevalArgs), inputs_->varBreakDown());

}

} // namespace analytics
} // namespace ore
