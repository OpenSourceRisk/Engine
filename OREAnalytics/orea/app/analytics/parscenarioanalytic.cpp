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

#include <orea/app/analytics/parscenarioanalytic.hpp>
#include <orea/scenario/scenariowriter.hpp>

using namespace ore::analytics;

namespace ore {
namespace analytics {

void ParScenarioAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
}

void ParScenarioAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("ParScenarioAnalytic::runAnalytic called");

    analytic()->buildMarket(loader);

    LOG("Building scenario simulation market for date " << io::iso_date(asof_));
    



}

} // namespace analytics
} // namespace ore
