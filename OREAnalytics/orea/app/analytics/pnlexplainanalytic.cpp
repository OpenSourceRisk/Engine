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
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

void PNLExplainAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = true;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
}

void PNLExplainAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes) {
    
    CONSOLEW("PNL Explain: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("PNL Explain: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    auto pnlexplainAnalytic = static_cast<PNLExplainAnalytic*>(analytic());
    QL_REQUIRE(pnlexplainAnalytic, "Analytic must be of type PNLExplainAnalytic");

    auto sensiAnalytic = analytic()->dependentAnalytic("SENSI");
    // Explicitily set the sensi threshold to 0.0 for sensi analysis, this ensures we get a felta entry for each gamma
    // TODO: Pass the threshold into the sensi analytic
    inputs_->setSensiThreshold(QuantLib::Null<QuantLib::Real>());
    sensiAnalytic->runAnalytic(loader, {"SENSITIVITY"});

    auto sensireport = sensiAnalytic->reports().at("SENSITIVITY").at("sensitivity");
    analytic()->reports()[label_]["sensitivity"] = sensireport;
    ext::shared_ptr<SensitivityStream> ss = ext::make_shared<SensitivityReportStream>(sensiAnalytic->reports().at("SENSITIVITY").at("sensitivity"));
    ss = ext::make_shared<FilteredSensitivityStream>(ss, 1e-6);

    auto sensiReports = sensiAnalytic->reports();

    ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
        adjFactors = ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

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
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, true, false,
        false, *inputs_->iborFallbackConfig());
    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator = ext::make_shared<ScenarioShiftCalculator>(
        analytic()->configurations().sensiScenarioData, analytic()->configurations().simMarketParams);

    std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
        std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator);

    auto pnlExplainReport = ext::make_shared<PNLExplainReport>(inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(),
                                           period, scenarios, move(sensiArgs), nullptr, nullptr, true);
        
    LOG("Call PNL Explain calculation");
    CONSOLEW("Risk: PNL Explain Calculation");
    ext::shared_ptr<MarketRiskReport::Reports> reports = ext::make_shared<MarketRiskReport::Reports>();
    QuantLib::ext::shared_ptr<InMemoryReport> pnlReport = QuantLib::ext::make_shared<InMemoryReport>();
    reports->add(pnlReport);

    pnlExplainReport->calculate(reports);
    analytic()->reports()[label_]["pnl_explain"] = pnlReport;
}

} // namespace analytics
} // namespace ore