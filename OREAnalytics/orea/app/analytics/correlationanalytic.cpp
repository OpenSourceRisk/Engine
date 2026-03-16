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
#include <orea/app/analytics/utilities.hpp>
#include <orea/app/inputparameters.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>
#include <ored/report/inmemoryreport.hpp>

using namespace ore::data;
using namespace std::filesystem;
using namespace QuantLib::ext;

namespace ore {
namespace analytics {

void CorrelationVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    vector<string> correlationAnalytics = {"correlation", "xva"};

    inputs->loadParameterXML<ScenarioSimMarketParameters>(simMarketParams_, correlationAnalytics, "marketConfigFile");
    inputs->loadParameterXML<SensitivityScenarioData>(sensiScenarioData_, correlationAnalytics, "sensitivityConfigFile");

    inputs->loadParameter<string>(lookbackPeriod_, correlationAnalytics,
                                  vector<string>({"lookbackPeriod", "historicalPeriod", "benchmarkVarPeriod", "benchmarkPeriod"}),
                                  false);
    inputs->loadParameter<string>(correlationMethod_, correlationAnalytics, vector<string>({"correlationMethod", "correlation_method"}), false);
    inputs->loadParameter<Size>(horizonDays_, correlationAnalytics, vector<string>({"horizonDays", "mporDays"}), false, parseInteger);
    inputs->loadParameter<Calendar>(horizonCalendar_, correlationAnalytics, vector<string>({"horizonCalendar", "mporCalendar"}), false, parseCalendar);
    if (horizonCalendar_.empty())
        horizonCalendar_ = parseCalendar(inputs->setupVariables().baseCurrency_);

    inputs->loadParameter<bool>(horizonOverlappingPeriods_, correlationAnalytics, vector<string>({"horizonOverlappingPeriods", "mporOverlappingPeriods"}), false, parseBool);
    inputs->loadParameter<bool>(allowPartialScenarios_, correlationAnalytics, "allowPartialScenarios", false, parseBool);
    
    TimePeriod hsPeriod = totalTimePeriod(vector<string>({lookbackPeriod_}), horizonDays_, horizonCalendar_);
    QL_REQUIRE(hsPeriod.numberOfContiguousParts() == 1,
               "JSON Body: Historical scenarios period must consist of one contiguous part");
    scenarioReader_ = inputs->loadScenarioReader(correlationAnalytics, vector<string>({"historicalScenarioFile", "scenarioFile"}),
                                                 hsPeriod.startDates().front(),
                                                 hsPeriod.endDates().front());
}

void CorrelationAnalyticImpl::setUpConfigurations() {

    auto corrVars = ext::dynamic_pointer_cast<CorrelationVariables>(inputVariables_);
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().sensiScenarioData = corrVars->sensiScenarioData_;
    analytic()->configurations().simMarketParams = corrVars->simMarketParams_;
}

void CorrelationAnalyticImpl::buildDependencies() { }

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

    CONSOLEW("Risk: Build Parameters for Correlation");
    analytic()->buildPortfolio();
    CONSOLE("OK");

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

    LOG("Build Correlation Calculator");
    auto corrVars = ext::dynamic_pointer_cast<CorrelationVariables>(inputVariables_);
    QL_REQUIRE(corrVars->lookbackPeriod_ != std::string(), "Correlation LookbackPeriod Required.");
    TimePeriod benchmarkVarPeriod(parseListOfValues<Date>(corrVars->lookbackPeriod_, &parseDate),
                                  corrVars->horizonDays_, corrVars->horizonCalendar_);

    QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader)) {
        adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());
    }    

    auto defaultReturnConfig = QuantLib::ext::make_shared<ReturnConfiguration>();

    std::string configuration = inputs_->marketConfig("simulation");
    
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams,
        QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
        *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), false, true,
        corrVars->allowPartialScenarios_, inputs_->iborFallbackConfig(), false, nullptr);
    
    QL_REQUIRE(corrVars->scenarioReader_, "ScenarioReader Required.");
    
    auto scenarios = buildHistoricalScenarioGenerator(
        corrVars->scenarioReader_, adjFactors, benchmarkVarPeriod, corrVars->horizonCalendar_, corrVars->horizonDays_,
        analytic()->configurations().simMarketParams, analytic()->configurations().todaysMarketParams,
        defaultReturnConfig, corrVars->horizonOverlappingPeriods_);

    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator =
            QuantLib::ext::make_shared<ScenarioShiftCalculator>(analytic()->configurations().sensiScenarioData,
                                                                analytic()->configurations().simMarketParams);
        
    correlationReport_ = ext::make_shared<CorrelationReport>(corrVars->scenarioReader_, corrVars->correlationMethod_,
                                                             benchmarkVarPeriod, scenarios, shiftCalculator);
}

} //namespace analytics
} // namespace ore