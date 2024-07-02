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
#include <orea/engine/parstressconverter.hpp>
#include <orea/engine/parsensitivityutilities.hpp>

using namespace ore::analytics;

namespace ore {
namespace analytics {

void ParScenarioAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
}

void ParScenarioAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("ParScenarioAnalytic::runAnalytic called");

    analytic()->buildMarket(loader);

    LOG("Building scenario simulation market for date " << io::iso_date(analytic()->configurations().asofDate));

    ParStressTestConverter converter(
        analytic()->configurations().asofDate, analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
        analytic()->configurations().sensiScenarioData, analytic()->configurations().curveConfig, analytic()->market(),
        inputs_->iborFallbackConfig());

    auto [simMarket, parSensiAnalysis] = converter.computeParSensitivity({});

    for(const auto& key : simMarket->baseScenarioAbsolute()->keys()){
        switch(key.keytype){
            case RiskFactorKey::KeyType::DiscountCurve:
            case RiskFactorKey::KeyType::YieldCurve:
            case RiskFactorKey::KeyType::IndexCurve:
            case RiskFactorKey::KeyType::SurvivalProbability:
            {
                auto parInstrument = parSensiAnalysis->parInstruments().parHelpers_.find(key);
                QL_REQUIRE(parInstrument != parSensiAnalysis->parInstruments().parHelpers_.end(),
                           "ParScenarioAnalysis: Can not compute parRate, instrument missing for " << key);
                parRates_[key] = impliedQuote(parInstrument->second);
                break;
            }
            case RiskFactorKey::KeyType::OptionletVolatility:
                parRates_[key] = impliedVolatility(key, parSensiAnalysis->parInstruments());
                break;
            default:
                parRates_[key] = simMarket->baseScenarioAbsolute()->get(key);
            }
    }
}

} // namespace analytics
} // namespace ore
