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

#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {

//! Stress Test Analysis
/*!
  This class wraps functionality to perform a stress testing analysis for a given portfolio.
  It comprises
  - building the "simulation" market to which sensitivity scenarios are applied
  - building the portfolio linked to this simulation market
  - generating sensitivity scenarios
  - running the scenario "engine" to apply these and compute the NPV impacts of all required shifts
  - fill result structures that can be queried
  - write stress test report to a file

  \ingroup simulation
*/
class StressTest {
public:
    //! Constructor
    StressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
               const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
               const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
               const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
               const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
               const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
               const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
               QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = {},
               const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
               const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
               bool continueOnError = false);

    //! Return set of trades analysed
    const std::set<std::string>& trades() { return trades_; }

    //! Return unique set of factors shifted
    const std::set<std::string>& stressTests() { return labels_; }

    //! Return base NPV by trade, before shift
    const std::map<std::string, Real>& baseNPV() { return baseNPV_; }

    //! Return shifted NPVs by trade and scenario
    const std::map<std::pair<std::string, std::string>, Real>& shiftedNPV() { return shiftedNPV_; }

    //! Return delta NPV by trade and scenario
    const std::map<std::pair<std::string, std::string>, Real>& delta() { return delta_; }

    //! Write NPV by trade/scenario to a file (base and shifted NPVs, delta)
    void writeReport(const QuantLib::ext::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

private:
    // base NPV by trade
    std::map<std::string, Real> baseNPV_;
    // NPV respectively sensitivity by trade and scenario
    std::map<std::pair<string, string>, Real> shiftedNPV_, delta_;
    // scenario labels
    std::set<std::string> labels_, trades_;
};
} // namespace analytics
} // namespace ore
