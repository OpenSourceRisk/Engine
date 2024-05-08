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

#include <orea/app/analytics/zerotoparshiftanalytic.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parstressconverter.hpp>
#include <orea/engine/zerotoparshift.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void ZeroToParShiftAnalyticImpl::setUpConfigurations() {
    const auto stressData = inputs_->stressScenarioData();
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = true;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->zeroToParShiftSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->zeroToParShiftSensitivityScenarioData();
    setGenerateAdditionalResults(true);
}

void ZeroToParShiftAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                             const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("StressTestAnalytic::runAnalytic called");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    CONSOLEW("StressTestAnalytic: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    auto market = analytic()->market();
    auto stressData = inputs_->zeroToParShiftScenarioData();
    auto simMarketData = analytic()->configurations().simMarketParams;
    auto curveConfigs = analytic()->configurations().curveConfig;
    auto todaysMarketParam = analytic()->configurations().todaysMarketParams;
    auto sensiScenarioData = analytic()->configurations().sensiScenarioData;
    Date asof = market->asofDate();
    
    // reuse the converter to generate the simmarket and the par instruments, dont need to run full par sensi analysis;
    ParStressTestConverter converter(inputs_->asof(), todaysMarketParam, simMarketData, sensiScenarioData, curveConfigs,
                                     market, inputs_->iborFallbackConfig());

    std::set<RiskFactorKey::KeyType> disabled;
    auto [simMarket, parAnalysis] = converter.computeParSensitivity(disabled);
    auto instruments = parAnalysis->parInstruments();
   
    DLOG("Build Stress Scenario Generator");

    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    QuantLib::ext::shared_ptr<StressScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<StressScenarioGenerator>(stressData, baseScenario, simMarketData, simMarket,
                                                            scenarioFactory, simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    ZeroToParShiftConverter shiftConvert(instruments, simMarket);

    QuantLib::ext::shared_ptr<InMemoryReport> report = QuantLib::ext::make_shared<InMemoryReport>();
    
    report->addColumn("ScenarioLabel", string());
    report->addColumn("ParKey", string());
    report->addColumn("ParShift", double(), 6);

    // Zero to par Converter
    simMarket->reset();
    scenarioGenerator->reset();
    for (size_t i = 0; i < scenarioGenerator->samples(); i++) {
        auto scenario = scenarioGenerator->next(asof);
        auto label = scenario->label();
        auto shifts = shiftConvert.parShifts(scenario);
        for (const auto& [key, shift] : shifts) {
                if (std::abs(shift) > 1e-6) {
                    report->next();
                    report->add(label);
                    report->add(ore::data::to_string(key));
                    report->add(shift);
                }
        }
    }
    analytic()->reports()[label()]["parshifts"] = report;
    CONSOLE("OK");
}

ZeroToParShiftAnalytic::ZeroToParShiftAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<ZeroToParShiftAnalyticImpl>(inputs), {"ZEROTOPARSHIFT"}, inputs, false, false, false,
               false) {}
} // namespace analytics
} // namespace ore
