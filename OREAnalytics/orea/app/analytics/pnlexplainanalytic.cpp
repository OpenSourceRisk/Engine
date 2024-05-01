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

#include <orea/app/analytics/pnlexplainanalytic.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/engine/pnlexplainreport.hpp>
#include <orea/engine/sensitivityreportstream.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

void PNLExplainAnalyticImpl::setUpConfigurations() {
}

void PNLExplainAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes) {
    
    CONSOLEW("Pricing: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    auto pnlexplainAnalytic = static_cast<PNLExplainAnalytic*>(analytic());
    QL_REQUIRE(pnlexplainAnalytic, "Analytic must be of type PNLExplainAnalytic");

    auto sensiAnalytic = analytic()->dependentAnalytic("SENSI");
    sensiAnalytic->runAnalytic(loader, {"SENSITIVITY"});
    auto ss = ext::make_shared<SensitivityReportStream>(
        sensiAnalytic->reports().at("SENSITIVITY").at("sensitivity"));

    auto sensiReports = sensiAnalytic->reports();

    boost::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = boost::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
        adjFactors = boost::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

    // dates needed for scenarios
    set<Date> pnlDates;
    pnlDates.insert(inputs_->asof());
    pnlDates.insert(inputs_->pnlDate());
    TimePeriod period({inputs_->asof(), inputs_->pnlDate()});

    auto scenarios = buildHistoricalScenarioGenerator(
        inputs_->historicalScenarioReader(), adjFactors, pnlDates, analytic()->configurations().simMarketParams,
        analytic()->configurations().todaysMarketParams);

    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, false, false,
        false, *inputs_->iborFallbackConfig());
    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    boost::shared_ptr<ScenarioShiftCalculator> shiftCalculator = boost::make_shared<ScenarioShiftCalculator>(
        analytic()->configurations().sensiScenarioData, analytic()->configurations().simMarketParams);

    std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
        std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator);

    auto pnlExplainReport = ext::make_shared<PNLExplainReport>(inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(),
                                           period, scenarios, move(sensiArgs), nullptr, nullptr, true);
        
    LOG("Call VaR calculation");
    CONSOLEW("Risk: VaR Calculation");
    ext::shared_ptr<MarketRiskReport::Reports> reports = ext::make_shared<MarketRiskReport::Reports>();
    QuantLib::ext::shared_ptr<InMemoryReport> pnlReport = QuantLib::ext::make_shared<InMemoryReport>();
    reports->add(pnlReport);

    pnlExplainReport->calculate(reports);
    analytic()->reports()[label_]["pnl"] = pnlReport;
}

} // namespace analytics
} // namespace ore