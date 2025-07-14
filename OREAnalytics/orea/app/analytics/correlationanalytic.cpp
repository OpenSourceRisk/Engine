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

void CorrelationAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    if (inputs_->covarianceData().size() == 0) {
        analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
        analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    }
}

void CorrelationAnalyticImpl::buildDependencies() {
    //auto sensiAnalytic =
    //    AnalyticFactory::instance().build("SENSITIVITY", inputs_, analytic()->analyticsManager(), false);
    //if (sensiAnalytic.second)
    //    addDependentAnalytic(sensiLookupKey, sensiAnalytic.second);

}

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

    QL_REQUIRE(correlationReport_, "No Correlation Report created");

    LOG("Call Correlation calculation");
    CONSOLEW("Risk: Correlation Calculation");
    ext::shared_ptr<Report> reports = QuantLib::ext::make_shared<CSVFileReport>(
        path(inputs_->resultsPath() / "correlation.csv").string(), ',', false, inputs_->csvQuoteChar(),
        inputs_->reportNaString());
    QuantLib::ext::shared_ptr<InMemoryReport> correlationReport =
        QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    correlationReport_->calculate(correlationReport);
    correlationReport->end();

    CONSOLE("OK");
    analytic()->addReport(label_, "correlation", correlationReport);

    LOG("Correlation completed");
    MEM_LOG;
}

void CorrelationAnalyticImpl::setCorrelationReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    LOG("Build trade to portfolio id mapping");

    QuantLib::ext::shared_ptr<SensitivityStream> ss = sensiStream(loader);
    
    LOG("Build Correlation calculator");
    if (inputs_->correlationData().size() > 0) {
        TimePeriod period({inputs_->asof(), inputs_->mporDate()});
        /*std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
            std::make_unique<MarketRiskReport::SensiRunArgs>(ss, nullptr, 0.01, inputs_->covarianceData());

        correlationReport_ = ext::make_shared<CorrelationReport>(
            inputs_->correlationMethod(), inputs_->baseCurrency(), analytic()->portfolio(),
            inputs_->portfolioFilter(), period, nullptr, std::move(sensiArgs));*/
    } else {
        TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(inputs_->benchmarkVarPeriod(), &parseDate),
                                      inputs_->mporDays(), inputs_->mporCalendar());

        QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
        if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
            adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

        auto defaultReturnConfig = QuantLib::ext::make_shared<ReturnConfiguration>();
        
        auto scenarios = buildHistoricalScenarioGenerator(
            inputs_->scenarioReader(), adjFactors, benchmarkVarPeriod, inputs_->mporCalendar(), inputs_->mporDays(),
            analytic()->configurations().simMarketParams, analytic()->configurations().todaysMarketParams,
            defaultReturnConfig, inputs_->mporOverlappingPeriods());

        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
            *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false,
            false, false, *inputs_->iborFallbackConfig());
        simMarket->scenarioGenerator() = scenarios;
        scenarios->baseScenario() = simMarket->baseScenario();

        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator =
            QuantLib::ext::make_shared<ScenarioShiftCalculator>(analytic()->configurations().sensiScenarioData,
                                                                analytic()->configurations().simMarketParams);
        
        correlationReport_ = ext::make_shared<CorrelationReport>(inputs_->scenarioReader(),
            inputs_->correlationMethod(), benchmarkVarPeriod, scenarios, shiftCalculator);
    }

}

} //namespace analytics
} // namespace ore