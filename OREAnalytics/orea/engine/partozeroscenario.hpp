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
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>

namespace ore {
namespace analytics {

class ParToZeroScenario{
public:
    ParToZeroScenario();
    
    boost::shared_ptr<ore::analytics::StressTestScenarioData> convertParScenarioToZeroScenarioData(
        const boost::shared_ptr<Market>& market, boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters,
        boost::shared_ptr<StressTestScenarioData>& stressTestData, boost::shared_ptr<SensitivityScenarioData> sensiData,
        const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
        const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
        const bool continueOnError = false, const bool useSpreadedTermStructures = false,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig()) {
        // Build ScenarioSimMarket
        constexpr bool handlePseudoCurrencies = true;
        constexpr bool allowPartialScenarios = false;
        constexpr bool cacheSimData = false;
        auto simMarket = boost::make_shared<ScenarioSimMarket>(
            market, simMarketParameters, curveConfigs, todaysMarketParams, continueOnError, useSpreadedTermStructures,
            cacheSimData, allowPartialScenarios, iborFallbackConfig, handlePseudoCurrencies);
        // Dont build a scenario generator, we modify the simData directly,
        
        // Check if we have a IR par scenario
        
        for (const auto& scenario : stressTestData->data()) {
            // Rate shifts
            bool irCurveParScenario = false;
            for(const auto& [ccy, shiftData]: scenario.discountCurveShifts){
                irCurveParScenario = irCurveParScenario || shiftData.isParShift;
                auto n_tenors = simMarketParameters->yieldVolTerms().size();
                QL_REQUIRE(!irCurveParScenario || shiftData.isParShift,
                           "all ir curve scenarios have to be par or zero.");
                QL_REQUIRE(shiftData.shiftTenors.size() == n_tenors,
                           "mismatch of tenors between simulation and stresstest");
                QL_REQUIRE(sensiData->discountCurveShiftData().at(ccy)->shiftTenors.size() == n_tenors,
                           "mismatch of tenors between sensitivity and stresstest");
            }
            for (const auto& [ccy, shiftData] : scenario.indexCurveShifts) {
                irCurveParScenario = irCurveParScenario || shiftData.isParShift;
                QL_REQUIRE(!irCurveParScenario || shiftData.isParShift,
                           "all ir curve scenarios have to be par or zero.");
            }
            for (const auto& [ccy, shiftData] : scenario.yieldCurveShifts) {
                irCurveParScenario = irCurveParScenario || shiftData.isParShift;
                QL_REQUIRE(!irCurveParScenario || shiftData.isParShift,
                           "all ir curve scenarios have to be par or zero.");
            }
            if(irCurveParScenario){
                // Perform ParSensiAnalysis
                

                // Compute Target Par Rate
                // Modify ZeroRates to match Par-Rate
            }
        }

        // Rates
    }
}

} // namespace analytics
} // namespace ore
