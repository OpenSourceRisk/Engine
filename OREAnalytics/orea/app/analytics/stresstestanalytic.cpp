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

#include <orea/app/analytics/stresstestanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/partozeroscenario.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void StressTestAnalyticImpl::setUpConfigurations() {
    const auto stressData =  inputs_->stressScenarioData();
    analytic()->configurations().simulationConfigRequired = true;
    if (stressData != nullptr) {
        analytic()->configurations().sensitivityConfigRequired = stressData->hasScenarioWithParShifts();
    } else {
        analytic()->configurations().sensitivityConfigRequired = false;
    }
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->stressSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->stressSensitivityScenarioData();
    setGenerateAdditionalResults(true);
}

void StressTestAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("StressTestAnalytic::runAnalytic called");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());
    QL_REQUIRE(inputs_->portfolio(), "StressTestAnalytic::run: No portfolio loaded.");

    CONSOLEW("StressTestAnalytic: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("StressTestAnalytic: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");
    QuantLib::ext::shared_ptr<InMemoryReport> report = QuantLib::ext::make_shared<InMemoryReport>();
    // This hook allows modifying the portfolio in derived classes before running the analytics below,
    // e.g. to apply SIMM exemptions.
    analytic()->modifyPortfolio();
    CONSOLEW("Risk: Stress Test Report");
    LOG("Stress Test Analysis called");
    QuantLib::ext::shared_ptr<StressTestScenarioData> scenarioData = inputs_->stressScenarioData();
    if (scenarioData != nullptr && scenarioData->hasScenarioWithParShifts()) {
        LOG("Found par rate scenarios, convert them to zero rate scenarios");
        auto sensiScenarioData = analytic()->configurations().sensiScenarioData;
        auto simMarketParam = analytic()->configurations().simMarketParams;
        auto curveConfigs = analytic()->configurations().curveConfig;
        auto todaysMarketParams = analytic()->configurations().todaysMarketParams;

        // Build simmarket with a dummy scenario generator
        QL_REQUIRE(sensiScenarioData != nullptr,
                   "Dont have sensitivity scenario data for stresstest, can not convert scenarios, check input");

        set<RiskFactorKey::KeyType> typesDisabled{};
        if(!scenarioData->withIrCurveParShifts()){
            typesDisabled.insert(RiskFactorKey::KeyType::DiscountCurve);
            typesDisabled.insert(RiskFactorKey::KeyType::YieldCurve);
            typesDisabled.insert(RiskFactorKey::KeyType::IndexCurve);
        }
        if (!scenarioData->withCreditCurveParShifts()) {
            typesDisabled.insert(RiskFactorKey::KeyType::SurvivalProbability);
        }
        if (!scenarioData->withIrCapFloorParShifts()) {
            typesDisabled.insert(RiskFactorKey::KeyType::OptionletVolatility);
        }

        auto parAnalysis = ext::make_shared<ParSensitivityAnalysis>(inputs_->asof(), simMarketParam, *sensiScenarioData,
                                                                    Market::defaultConfiguration, true, typesDisabled);

        LOG("Stresstest analytic - align pillars (for the par conversion)");
        parAnalysis->alignPillars();
        // Align Cap Floor Strikes
        if(typesDisabled.count(RiskFactorKey::KeyType::OptionletVolatility)== 0){
            for (const auto& [index, data] : analytic()->configurations().sensiScenarioData->capFloorVolShiftData()) {
                analytic()->configurations().simMarketParams->setCapFloorVolStrikes(index, data->shiftStrikes);
                analytic()->configurations().simMarketParams->setCapFloorVolAdjustOptionletPillars(true);
            }
        }
        // Build Simmarket
        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), simMarketParam, Market::defaultConfiguration,
            curveConfigs ? *curveConfigs : ore::data::CurveConfigurations(),
            todaysMarketParams ? *todaysMarketParams : ore::data::TodaysMarketParameters(), false,
            sensiScenarioData->useSpreadedTermStructures(), false, true, *inputs_->iborFallbackConfig());
        // Build ScenarioGenerator
        auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
            analytic()->configurations().sensiScenarioData, simMarket->baseScenario(),
            analytic()->configurations().simMarketParams, simMarket,
            QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket->baseScenario()), true, "", false,
            simMarket->baseScenarioAbsolute());
        simMarket->scenarioGenerator() = scenarioGenerator;
        // Compute ParSensitivities
        parAnalysis->computeParInstrumentSensitivities(simMarket);
        LOG("Stress Test Analytic: Par Sensitivity Analysis finished");
        LOG("Convert ParStressScenarios into ZeroStress Scenario");
        
        scenarioData = convertParScenarioToZeroScenarioData(
            inputs_->asof(), simMarket, analytic()->configurations().simMarketParams, scenarioData,
            inputs_->stressSensitivityScenarioData(), parAnalysis->parSensitivities(), parAnalysis->parInstruments());
        
        LOG("Finished par to zero scenarios conversion");
    }

    Settings::instance().evaluationDate() = inputs_->asof();

    std::string marketConfig = inputs_->marketConfig("pricing");
    std::vector<QuantLib::ext::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
    std::vector<QuantLib::ext::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
    QuantLib::ext::shared_ptr<StressTest> stressTest = QuantLib::ext::make_shared<StressTest>(
        analytic()->portfolio(), analytic()->market(), marketConfig, inputs_->pricingEngine(),
        analytic()->configurations().simMarketParams, scenarioData, *analytic()->configurations().curveConfig,
        *analytic()->configurations().todaysMarketParams, nullptr, inputs_->refDataManager(),
        *inputs_->iborFallbackConfig(), inputs_->continueOnError());
    stressTest->writeReport(report, inputs_->stressThreshold());
    analytic()->reports()[label()]["stress"] = report;
    CONSOLE("OK");
}

StressTestAnalytic::StressTestAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<StressTestAnalyticImpl>(inputs), {"STRESS"}, inputs, false, false, false, false) {}
} // namespace analytics
} // namespace ore
