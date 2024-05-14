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
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

void PnlExplainAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = true;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
}

void PnlExplainAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes) {
    
    CONSOLEW("PNL Explain: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("PNL Explain: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    auto pnlexplainAnalytic = static_cast<PnlExplainAnalytic*>(analytic());
    QL_REQUIRE(pnlexplainAnalytic, "Analytic must be of type PnlExplainAnalytic");

    auto pnlAnalytic = dependentAnalytic(pnlLookupKey);
    pnlAnalytic->runAnalytic(loader);
    auto pnlReport = pnlAnalytic->reports().at("PNL").at("pnl");

    auto sensiAnalytic = dependentAnalytic(sensiLookupKey);

    // Explicitily set the sensi threshold to 0.0 for sensi analysis, this ensures we get a felta entry for each gamma
    // TODO: Pass the threshold into the sensi analytic
    inputs_->setSensiThreshold(QuantLib::Null<QuantLib::Real>());
    sensiAnalytic->runAnalytic(loader, {"SENSITIVITY"});

    auto sensireport = sensiAnalytic->reports().at("SENSITIVITY").at("sensitivity");
    analytic()->reports()[label_]["sensitivity"] = sensireport;
    ext::shared_ptr<SensitivityStream> ss = ext::make_shared<SensitivityReportStream>(sensiAnalytic->reports().at("SENSITIVITY").at("sensitivity"));
    ss = ext::make_shared<FilteredSensitivityStream>(ss, 1e-6);

    auto sensiReports = sensiAnalytic->reports();

    QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors> adjFactors;
    if (auto adjLoader = QuantLib::ext::dynamic_pointer_cast<AdjustedInMemoryLoader>(loader))
        adjFactors = QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(adjLoader->adjustmentFactors());

    // dates needed for scenarios
    set<Date> pnlDates;
    pnlDates.insert(inputs_->asof());
    pnlDates.insert(inputs_->mporDate());
    TimePeriod period({inputs_->asof(), inputs_->mporDate()});

    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> scenarios;
    if (!inputs_->historicalScenarioReader()) {
        
        auto pnlImpl = static_cast<PnlAnalyticImpl*>(pnlAnalytic->impl().get());
        QL_REQUIRE(pnlImpl, "Impl must of type PNLAnalyticImpl");
        auto t0Scenario = pnlImpl->t0Scenario();
        auto t1Scenario = pnlImpl->t1Scenario();

        analytic()->reports()[label_]["pnl_scenario_t0"] = pnlAnalytic->reports().at("PNL").at("pnl_scenario_t0");
        analytic()->reports()[label_]["pnl_scenario_t1"] = pnlAnalytic->reports().at("PNL").at("pnl_scenario_t1");

        vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>> histScens = {t0Scenario, t1Scenario};

        QuantLib::ext::shared_ptr<HistoricalScenarioLoader> scenarioLoader =
            QuantLib::ext::make_shared<HistoricalScenarioLoader>(histScens, pnlDates);
        
        scenarios = QuantLib::ext::make_shared<HistoricalScenarioGenerator>(
            scenarioLoader, QuantLib::ext::make_shared<SimpleScenarioFactory>(), adjFactors,
            ReturnConfiguration(), "hs_");
    } else {
        auto scenarios = buildHistoricalScenarioGenerator(inputs_->historicalScenarioReader(), adjFactors, pnlDates,
                                                          analytic()->configurations().simMarketParams,
                                                          analytic()->configurations().todaysMarketParams);
    }

    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true, true,
        false, false, *inputs_->iborFallbackConfig());
    simMarket->scenarioGenerator() = scenarios;
    scenarios->baseScenario() = simMarket->baseScenario();

    ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator = ext::make_shared<ScenarioShiftCalculator>(
        analytic()->configurations().sensiScenarioData, analytic()->configurations().simMarketParams);

    std::unique_ptr<MarketRiskReport::SensiRunArgs> sensiArgs =
        std::make_unique<MarketRiskReport::SensiRunArgs>(ss, shiftCalculator);

    auto pnlExplainReport =
        ext::make_shared<PnlExplainReport>(inputs_->baseCurrency(), analytic()->portfolio(), inputs_->portfolioFilter(),
                                           period, pnlReport, scenarios, std::move(sensiArgs), nullptr, nullptr, true);

    LOG("Call PNL Explain calculation");
    CONSOLEW("Risk: PNL Explain Calculation");
    ext::shared_ptr<MarketRiskReport::Reports> reports = ext::make_shared<MarketRiskReport::Reports>();
    QuantLib::ext::shared_ptr<InMemoryReport> pnlExplainOutput = QuantLib::ext::make_shared<InMemoryReport>();
    reports->add(pnlReport);

    pnlExplainReport->calculate(reports);
    analytic()->reports()[label_]["pnl"] = pnlReport;
    analytic()->reports()[label_]["pnl_explain"] = pnlReport;
}

} // namespace analytics
} // namespace ore
