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

#include <orea/app/analytics/parstressconversionanalytic.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parstressconverter.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void ParStressConversionAnalyticImpl::setUpConfigurations() {
    const auto stressData =  inputs_->parStressScenarioData();
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = false;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->parStressSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->parStressSensitivityScenarioData();
    setGenerateAdditionalResults(true);
}

void ParStressConversionAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("ParStressConversionAnalytic::runAnalytic called");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    CONSOLEW("ParStressConversionAnalytic: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("ParStressConversionAnalytic: Convert ParStressScenario");
    LOG("Par Stress Conversion Analysis called");
    QuantLib::ext::shared_ptr<StressTestScenarioData> scenarioData = inputs_->parStressScenarioData();
    if (scenarioData != nullptr && scenarioData->hasScenarioWithParShifts()) {
        ParStressTestConverter converter(
            inputs_->asof(), analytic()->configurations().todaysMarketParams,
            analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
            analytic()->configurations().curveConfig, analytic()->market(), inputs_->iborFallbackConfig());
        scenarioData = converter.convertStressScenarioData(scenarioData);
        analytic()->stressTests()[label()]["parStress_ZeroStressData"] = scenarioData;
        LOG("Finished par to zero scenarios conversion");
    } 
    CONSOLE("OK");
}

ParStressConversionAnalytic::ParStressConversionAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<ParStressConversionAnalyticImpl>(inputs), {"PARSTRESSCONVERSION"}, inputs, false, false,
               false, false) {}
} // namespace analytics
} // namespace ore
