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

/*! \file orea/engine/partozeroscenario.hpp
    \brief Convert a par stress scenario into a zero stress scenario
    \ingroup engine
*/

#pragma once
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
namespace ore {
namespace analytics {

boost::shared_ptr<ore::analytics::StressTestScenarioData> convertParScenarioToZeroScenarioData(
    const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& market,
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParameters,
    const boost::shared_ptr<ore::analytics::StressTestScenarioData>& stressTestData,
    const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiData,
    const std::map<std::pair<ore::analytics::RiskFactorKey, ore::analytics::RiskFactorKey>, double>& parSensitivities,
    const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
    const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
    const bool continueOnError = false, const bool useSpreadedTermStructures = false,
    const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig());

} // namespace analytics
} // namespace ore
