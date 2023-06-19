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

#include <orea/app/analytics/parconversionanalytic.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void ParConversionAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->parConversionSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->parConversionScenarioData();
    analytic()->configurations().engineData = inputs_->parConversionPricingEngine();
}

void ParConversionAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                            const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("ParConversionAnalytic::runAnalytic called");

    analytic()->buildMarket(loader, false);
        
    auto parConversionAnalytic = static_cast<ParConversionAnalytic*>(analytic());

    auto zeroSensis = parConversionAnalytic->loadZeroSensitivities();

    for (const auto& [id, sensis] : zeroSensis) {
        for (const auto& zero : sensis) {
            std::cout << id << "|" << zero.riskFactor << "|" << zero.delta << std::endl;
        }
    }
}

} // namespace analytics
} // namespace ore
