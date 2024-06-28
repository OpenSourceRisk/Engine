/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>
#include <orea/engine/parstressconverter.hpp>
#include <orea/engine/parstressscenarioconverter.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/app/structuredanalyticswarning.hpp>

namespace ore {
namespace analytics {

namespace{
    //! Build a bi-graph from the parSensitivites and sort the graph by their dependency
std::vector<RiskFactorKey>
sortParRiskFactorByDependency(const ParSensitivityAnalysis::ParContainer& parWithRespectToZero) {

    std::map<RiskFactorKey, std::set<RiskFactorKey>> parToZeroEdges;
    std::map<RiskFactorKey, std::set<RiskFactorKey>> zeroToParEdges;
    std::vector<RiskFactorKey> orderedKeys;
    std::map<RiskFactorKey, size_t> order;
    std::map<RiskFactorKey, std::set<RiskFactorKey>> dependencies;
    for (const auto& [key, value] : parWithRespectToZero) {
        const auto& [parKey, zeroKey] = key;
        if (order.count(parKey) == 0) {
            order[parKey] = 0;
        }
        if (!QuantLib::close_enough(value, 0.0)) {
            parToZeroEdges[parKey].insert(zeroKey);
            if (zeroKey != parKey) {
                order[parKey] = order[parKey] + 1;
                dependencies[zeroKey].insert(parKey);
            }
        }
    }

    std::queue<RiskFactorKey> zeroOrderParKeys;
    for (const auto& [key, n] : order) {
        if (n == 0) {
            zeroOrderParKeys.push(key);
        }
    }

    while (!zeroOrderParKeys.empty()) {
        auto key = zeroOrderParKeys.front();
        zeroOrderParKeys.pop();
        orderedKeys.push_back(key);
        for (const auto& dependentKey : dependencies[key]) {
            order[dependentKey] -= 1;
            if (order[dependentKey] == 0) {
                zeroOrderParKeys.push(dependentKey);
            }
        }
    }
    return orderedKeys;
}

}

ParStressTestConverter::ParStressTestConverter(
    const QuantLib::Date& asof, QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket,
    const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig)
    : asof_(asof), todaysMarketParams_(todaysMarketParams), simMarketParams_(simMarketParams),
      sensiScenarioData_(sensiScenarioData), curveConfigs_(curveConfigs), todaysMarket_(todaysMarket),
      iborFallbackConfig_(iborFallbackConfig) {}

QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> ParStressTestConverter::convertStressScenarioData(
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressTestData) const {
    // Exit early if we have dont have any par stress shifts in any scenario
    if (!stressTestData->hasScenarioWithParShifts()) {
        LOG("ParStressConverter: No scenario with par shifts found, use it as is");
        return stressTestData;
    }
    QuantLib::ext::shared_ptr<StressTestScenarioData> results = QuantLib::ext::make_shared<StressTestScenarioData>();
    results->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
    // Identify all required parInstruments, ignore if a risk factor is a zeroshift in all scenarios
    // The following risk factors arent supported in the par stress
    std::set<RiskFactorKey::KeyType> disabledRiskFactors{
        RiskFactorKey::KeyType::ZeroInflationCurve, RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility,
        RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, RiskFactorKey::KeyType::YoYInflationCurve};

    auto disabled = disabledParRates(stressTestData->withIrCurveParShifts(), stressTestData->withIrCapFloorParShifts(),
                                     stressTestData->withCreditCurveParShifts());
    disabledRiskFactors.insert(disabled.begin(), disabled.end());

    DLOG("ParStressConverter: The following risk factors will be not converted:")
    for (const auto& keyType : disabledRiskFactors) {
        DLOG(keyType);
    }
    LOG("ParStressConverter: Compute Par Sensitivities")
    auto [simMarket, parAnalysis] = computeParSensitivity(disabledRiskFactors);
    // Loop over scenarios
    LOG("ParStressConverter: Build dependency graph of par instruments")
    auto dependencyGraph = sortParRiskFactorByDependency(parAnalysis->parSensitivities());
    ParStressScenarioConverter converter(asof_, dependencyGraph, simMarketParams_, sensiScenarioData_, simMarket,
                                         parAnalysis->parInstruments(), stressTestData->useSpreadedTermStructures());

    for (const auto& scenario : stressTestData->data()) {
        DLOG("ParStressConverter: Scenario" << scenario.label);
        if (scenario.containsParShifts()) {
            try {
                LOG("ParStressConverter: Scenario " << scenario.label << " Convert par shifts to zero shifts");
                auto convertedScenario = converter.convertScenario(scenario);
                results->data().push_back(std::move(convertedScenario));
            } catch (const std::exception& e) {
                StructuredAnalyticsWarningMessage("ParStressConversion", "ScenarioConversionFailed",
                                                  "Skip Scenario " + scenario.label + ", got :" + e.what())
                    .log();
            }
        } else {
            LOG("ParStressConverter: Skip scenario " << scenario.label << ", it contains only zero shifts");
            results->data().push_back(scenario);
        }
    }
    return results;
}

//! Creates a SimMarket, aligns the pillars and strikes of sim and sensitivity scenario market, computes par
//! sensitivites
std::pair<QuantLib::ext::shared_ptr<ScenarioSimMarket>, QuantLib::ext::shared_ptr<ParSensitivityAnalysis>>
ParStressTestConverter::computeParSensitivity(const std::set<RiskFactorKey::KeyType>& typesDisabled) const {
    QL_REQUIRE(simMarketParams_ != nullptr, "computeParSensitivity: simMarketData required to compute par sensitivity before "
                                 "converting par to zero stress test shifts");
    QL_REQUIRE(sensiScenarioData_ != nullptr,
               "computeParSensitivity: sensiScenarioData required to compute par sensitivity before "
               "converting par to zero stress test shifts");
    auto parAnalysis = ext::make_shared<ParSensitivityAnalysis>(asof_, simMarketParams_, *sensiScenarioData_,
                                                                    Market::defaultConfiguration, true, typesDisabled);
    QL_REQUIRE(parAnalysis != nullptr, "ParStressConverter: failed to generate parAnalysis");
    LOG("ParStressConverter: Allign Pillars for par sensitivity analysis");
    parAnalysis->alignPillars();
    // Align Cap Floor Strikes
    if (typesDisabled.count(RiskFactorKey::KeyType::OptionletVolatility) == 0) {
        LOG("ParStressConverter: Allign CapFloor strikes and adjust optionletPillars");
        for (const auto& [index, data] : sensiScenarioData_->capFloorVolShiftData()) {
            simMarketParams_->setCapFloorVolStrikes(index, data->shiftStrikes);
            simMarketParams_->setCapFloorVolAdjustOptionletPillars(true);
        }
    }
    LOG("ParStressConverter: Build SimMarket");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        todaysMarket_, simMarketParams_, Market::defaultConfiguration,
        curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
        todaysMarketParams_ ? *todaysMarketParams_ : ore::data::TodaysMarketParameters(), false,
        sensiScenarioData_->useSpreadedTermStructures(), false, true, *iborFallbackConfig_);
    LOG("ParStressConverter: Build ScenarioGenerator");
    auto scnearioFactory = QuantLib::ext::make_shared<ore::analytics::DeltaScenarioFactory>(simMarket->baseScenario());
    auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
        sensiScenarioData_, simMarket->baseScenario(), simMarketParams_, simMarket, scnearioFactory, true, "", false,
        simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;
    LOG("ParStressConverter: Compute ParInstrumentSensitivities");
    parAnalysis->computeParInstrumentSensitivities(simMarket);
    return {simMarket, parAnalysis};
}

} // namespace analytics

} // namespace ore