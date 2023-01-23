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
    MultiThreadedValuationEngine(
        const QuantLib::Size nThreads, const QuantLib::Date& today,
        const boost::shared_ptr<ore::analytics::DateGrid>& dateGrid, const QuantLib::Size nSamples,
        const boost::shared_ptr<ore::data::Loader>& loader,
        const boost::shared_ptr<ore::analytics::ScenarioGenerator>& scenarioGenerator,
        const boost::shared_ptr<ore::data::EngineData>& engineData,
        const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
        const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
        const std::string& configuration,
        const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
        const bool useSpreadedTermStructures = false, const bool cacheSimData = false,
        const boost::shared_ptr<ore::analytics::ScenarioFilter>& scenarioFilter =
            boost::make_shared<ore::analytics::ScenarioFilter>(),
        const std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
            const boost::shared_ptr<ore::data::ReferenceDataManager>&,
            const boost::shared_ptr<ore::data::TradeFactory>&)>& extraTradeBuilders = {},
        const std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()>& extraEngineBuilders = {},
        const std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()>& extraLegBuilders = {},
        const boost::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig(),
        const bool handlePseudoCurrenciesTodaysMarket = true, const bool handlePseudoCurrenciesSimMarket = true,
        const std::function<boost::shared_ptr<ore::analytics::NPVCube>(
            const QuantLib::Date&, const std::set<std::string>&, const std::vector<QuantLib::Date>&,
            const QuantLib::Size)>& cubeFactory = {},
        const std::string& context = "unspecified");

    /* - no support yet for netting set level results, cpty level results,
       - returns mini-cubes, use JointNPVCube, JointNPVSensiCube etc. to combine them to a single cube */
    std::vector<boost::shared_ptr<ore::analytics::NPVCube>>
    buildCube(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
              const std::function<std::vector<boost::shared_ptr<ore::analytics::ValuationCalculator>>()>& calculators,
              bool mporStickyDate = true, bool dryRun = false);

private:
    QuantLib::Size nThreads_;
    QuantLib::Date today_;
    boost::shared_ptr<ore::data::DateGrid> dateGrid_;
    QuantLib::Size nSamples_;
    boost::shared_ptr<ore::data::Loader> loader_;
    boost::shared_ptr<ore::analytics::ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<ore::data::EngineData> engineData_;
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    std::string configuration_;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData_;
    bool useSpreadedTermStructures_;
    bool cacheSimData_;
    boost::shared_ptr<ore::analytics::ScenarioFilter> scenarioFilter_;
    std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
        const boost::shared_ptr<ore::data::ReferenceDataManager>&, const boost::shared_ptr<ore::data::TradeFactory>&)>
        extraTradeBuilders_;
    std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()> extraEngineBuilders_;
    std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()> extraLegBuilders_;
    boost::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    ore::data::IborFallbackConfig iborFallbackConfig_;
    bool handlePseudoCurrenciesTodaysMarket_;
    bool handlePseudoCurrenciesSimMarket_;
    std::function<boost::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                             const std::vector<QuantLib::Date>&, const QuantLib::Size)>
        cubeFactory_;
    std::string context_;
};

} // namespace analytics
} // namespace ore
