/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file engine/multithreadedvaluationengine.hpp
    \brief multi-threaded valuation engine
    \ingroup engine
*/

#pragma once

#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/loader.hpp>

namespace ore {
namespace analytics {

class MultiThreadedValuationEngine : public ore::data::ProgressReporter {
public:
    /* if no cube factories are given, we create default ones as follows
       - cubeFactory          : creates DoublePrecisionInMemoryCube
       - nettingSetCubeFactory: creates nullptr
       - cptyCubeFactory:       creates nullptr */
    MultiThreadedValuationEngine(
        const QuantLib::Size nThreads, const QuantLib::Date& today,
        const QuantLib::ext::shared_ptr<ore::analytics::DateGrid>& dateGrid, const QuantLib::Size nSamples,
        const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& scenarioGenerator,
        const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
        const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
        const std::string& configuration,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
        const bool useSpreadedTermStructures = false, const bool cacheSimData = false,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& scenarioFilter =
            QuantLib::ext::make_shared<ore::analytics::ScenarioFilter>(),
        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig(),
        const bool handlePseudoCurrenciesTodaysMarket = true, const bool handlePseudoCurrenciesSimMarket = true,
        const bool recalibrateModels = true,
        const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(
            const QuantLib::Date&, const std::set<std::string>&, const std::vector<QuantLib::Date>&,
            const QuantLib::Size)>& cubeFactory = {},
        const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(
            const QuantLib::Date&, const std::vector<QuantLib::Date>&, const QuantLib::Size)>& nettingSetCubeFactory =
            {},
        const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(
            const QuantLib::Date&, const std::set<std::string>&, const std::vector<QuantLib::Date>&,
            const QuantLib::Size)>& cptyCubeFactory = {},
        const std::string& context = "unspecified",
        const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& offSetScenario = nullptr);

    // can be optionally called to set the agg scen data (which is done in the ssm for single-threaded runs)
    void setAggregationScenarioData(const QuantLib::ext::shared_ptr<AggregationScenarioData>& aggregationScenarioData);

    /* analoguous to buildCube() in the single-threaded engine, results are retrieved using below constructors
       if no cptyCalculators is given a function returning an empty vector of calculators will be returned */
    void
    buildCube(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
              const std::function<std::vector<QuantLib::ext::shared_ptr<ore::analytics::ValuationCalculator>>()>& calculators,
              const std::function<std::vector<QuantLib::ext::shared_ptr<ore::analytics::CounterpartyCalculator>>()>&
                  cptyCalculators = {},
              bool mporStickyDate = true, bool dryRun = false);

    // result output cubes (mini-cubes, one per thread)
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> outputCubes() const { return miniCubes_; }

    // result netting cubes (might be null, if nettingSetCubeFactory is returning null)
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> outputNettingSetCubes() const {
        return miniNettingSetCubes_;
    }

    // result cpty cubes (might be null, if cptyCubeFactory is returning null)
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> outputCptyCubes() const { return miniCptyCubes_; }

private:
    QuantLib::Size nThreads_;
    QuantLib::Date today_;
    QuantLib::ext::shared_ptr<ore::data::DateGrid> dateGrid_;
    QuantLib::Size nSamples_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator> scenarioGenerator_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    std::string configuration_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData_;
    bool useSpreadedTermStructures_;
    bool cacheSimData_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter> scenarioFilter_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    ore::data::IborFallbackConfig iborFallbackConfig_;
    bool handlePseudoCurrenciesTodaysMarket_;
    bool handlePseudoCurrenciesSimMarket_;
    bool recalibrateModels_;
    std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                             const std::vector<QuantLib::Date>&, const QuantLib::Size)>
        cubeFactory_;
    std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::vector<QuantLib::Date>&,
                                                             const QuantLib::Size)>
        nettingSetCubeFactory_;
    std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                             const std::vector<QuantLib::Date>&, const QuantLib::Size)>
        cptyCubeFactory_;
    std::string context_;
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> offsetScenario_;
    QuantLib::ext::shared_ptr<AggregationScenarioData>
            aggregationScenarioData_;
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> miniCubes_;
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> miniNettingSetCubes_;
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> miniCptyCubes_;
};

} // namespace analytics
} // namespace ore
