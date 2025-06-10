/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/correlationanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

using namespace ore::data;
using namespace boost::filesystem;
using namespace QuantLib::ext;

namespace ore {
namespace analytics {

void CorrelationAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes) {

    MEM_LOG;
    LOG("Running Correlation");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    LOG("CORRELATION: Build Market");
    CONSOLEW("Risk: Build Market for Correlation");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Risk: Build Portfolio for Correlation");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    analytic()->enrichIndexFixings(analytic()->portfolio());

    setCorrelationReport(loader);

    QL_REQUIRE(correlationReport_, "No Var Report created");

    LOG("Call VaR calculation");
    CONSOLEW("Risk: VaR Calculation");
    ext::shared_ptr<MarketRiskReport::Reports> reports = ext::make_shared<MarketRiskReport::Reports>();
    QuantLib::ext::shared_ptr<InMemoryReport> correlationReport =
        QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    reports->add(correlationReport);

    //addAdditionalReports(reports);

    correlationReport_->calculate(reports);
    CONSOLE("OK");

    analytic()->addReport(label_, "correlation", correlationReport);

    LOG("Correlation completed");
    MEM_LOG;
}

void CorrelationAnalyticImpl::setCorrelationReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    LOG("Build trade to portfolio id mapping");

    QuantLib::ext::shared_ptr<SensitivityStream> ss = sensiStream(loader);

    LOG("Build Correlation calculator");
    if (inputs_->covarianceData().size() > 0) {
        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, nullptr, 0.01, inputs_->covarianceData());

        correlationReport_ = ext::make_shared<CorrelationReport>(
            inputs_->correlationMethod(),
            inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(),
            boost::none, nullptr, std::move(sensiArgs), nullptr, inputs_->varBreakDown());
    } else {
        TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(inputs_->benchmarkVarPeriod(), &parseDate),
                                      inputs_->mporDays(), inputs_->mporCalendar());

        QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
        if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
            adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

        auto scenarios = buildHistoricalScenarioGenerator(
            inputs_->scenarioReader(), adjFactors, benchmarkVarPeriod, inputs_->mporCalendar(), inputs_->mporDays(),
            analytic()->configurations().simMarketParams, analytic()->configurations().todaysMarketParams,
            inputs_->mporOverlappingPeriods());

        if (inputs_->outputHistoricalScenarios())
            ReportWriter().writeHistoricalScenarios(
                scenarios->scenarioLoader(), QuantLib::ext::make_shared<CSVFileReport>(path(inputs_->resultsPath() / "backtest_histscenrios.csv").string(),
                                                 ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString()));

        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
            *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false,
            false, false, *inputs_->iborFallbackConfig());
        simMarket->scenarioGenerator() = scenarios;
        scenarios->baseScenario() = simMarket->baseScenario();

        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator =
            QuantLib::ext::make_shared<ScenarioShiftCalculator>(analytic()->configurations().sensiScenarioData,
                                                                analytic()->configurations().simMarketParams);

        std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator, 0.01, inputs_->covarianceData());

        correlationReport_ = ext::make_shared<CorrelationReport>(
            inputs_->correlationMethod(),
            inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(), boost::none, scenarios,
            std::move(sensiArgs), nullptr, inputs_->varBreakDown());
    }

}

} //namespace analytics
} // namespace ore