/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file engine/stresstest.hpp
    \brief  perform a stress testing analysis for a given portfolio.
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>
#include <ored/report/inmemoryreport.hpp>

#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {

//! Stress Test Analysis
/*!
  This function wraps functionality to perform a stress testing analysis for a given portfolio.
  It comprises
  - building the "simulation" market to which sensitivity scenarios are applied
  - building the portfolio linked to this simulation market
  - generating sensitivity scenarios
  - running the scenario "engine" to apply these and compute the NPV (CF) impacts of all required shifts
  - write results to reports
*/

void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory = nullptr,
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                   const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                   bool continueOnError = false, const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr);

void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                   const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                   bool continueOnError = false, const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr);


void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const Date& asof,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket, const string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData, const string& baseCcy,
                   const QuantLib::ext::shared_ptr<ShiftScenarioGenerator>& scenarioGenerator,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                   const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                   bool continueOnError = false, const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr);

} // namespace analytics
} // namespace ore
