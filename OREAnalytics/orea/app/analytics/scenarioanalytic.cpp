/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/scenario/scenariowriter.hpp>

using namespace ore::analytics;

namespace ore {
namespace analytics {

void ScenarioAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
}

void ScenarioAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("ScenarioAnalytic::runAnalytic called");

    analytic()->buildMarket(loader);

    LOG("Building scenario simulation market for date " << io::iso_date(inputs_->asof()));
    // FIXME: *configurations_.todaysMarketParams uninitialized?
    auto ssm = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, Market::defaultConfiguration,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, true,
        useSpreadedTermStructures_, false, false, *inputs_->iborFallbackConfig());

    setScenarioSimMarket(ssm);
    auto scenario = ssm->baseScenario();
    setScenario(scenario);

    QuantLib::ext::shared_ptr<InMemoryReport> report = QuantLib::ext::make_shared<InMemoryReport>();
    auto sw = ScenarioWriter(nullptr, report);
    sw.writeScenario(scenario, true);
    analytic()->reports()[label()]["scenario"] = report;
}

} // namespace analytics
} // namespace ore
