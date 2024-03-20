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
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>
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

std::map<RiskFactorKey, double>
buildTargetValues(const StressTestScenarioData::StressTestData& stressScenario,
                  const std::map<RiskFactorKey, boost::shared_ptr<Instrument>>& parHelpers,
                  const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters) {
    std::map<RiskFactorKey, double> results;
    for (const auto& [ccy, data] : stressScenario.discountCurveShifts) {
        DayCounter dc;

        auto simTenors = simMarketParameters->yieldCurveTenors(ccy);
        size_t nTenors = simTenors.size();
        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");
        QL_REQUIRE(shiftTenors.size() == nTenors, "Discount shift tenors not specified");
        std::vector<Real> shifts = data.shifts;

        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        for (size_t i = 0; i < nTenors; ++i) {
            // How to handle the allign tenor function
            // QL_REQUIRE(shiftTenors[i] == simTenors[i], "Tenor mismatch");
            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, i);
            Real fairParRate = impliedQuote(parHelpers.at(key));
            if (data.shiftType == ShiftType::Absolute) {
                results[key] = fairParRate + shifts[i];
            } else {
                results[key] = fairParRate * (1.0 + shifts[i]);
            }
        }
    }
    return results;
}

struct TargetFunction : public QuantLib::CostFunction {

    TargetFunction(boost::shared_ptr<ScenarioSimMarket>& simMarket, const vector<double>& goal,
                   const vector<RiskFactorKey>& parKeys, const vector<RiskFactorKey>& zeroKeys,
                   const std::map<RiskFactorKey, boost::shared_ptr<Instrument>>& parHelper)
        : simMarket(simMarket), goal(goal), parKeys(parKeys), zeroKeys(zeroKeys), parHelpers(parHelper) {}

    QuantLib::Array values(const QuantLib::Array& x) const override {
        simMarket->reset();
        auto trialScenario = simMarket->baseScenario()->clone();
        for (size_t i = 0; i < zeroKeys.size(); ++i) {
            trialScenario->add(zeroKeys[i], x[i]);
        }
        simMarket->applyScenario(trialScenario);
        vector<double> mse;
        for (size_t i = 0; i < parKeys.size(); ++i) {
            mse.push_back(goal[i] - impliedQuote(parHelpers.at(parKeys[i])));
        }
        return QuantLib::Array(mse.begin(), mse.end());
    }

    boost::shared_ptr<ScenarioSimMarket>& simMarket;
    const vector<double>& goal;
    const vector<RiskFactorKey>& parKeys;
    const vector<RiskFactorKey>& zeroKeys;
    const std::map<RiskFactorKey, boost::shared_ptr<Instrument>>& parHelpers;
};

class ParToZeroScenario {
public:
    ParToZeroScenario() {}

    boost::shared_ptr<ore::analytics::StressTestScenarioData> convertParScenarioToZeroScenarioData(
        const QuantLib::Date& asof, const boost::shared_ptr<Market>& market,
        const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters,
        const boost::shared_ptr<StressTestScenarioData>& stressTestData,
        const boost::shared_ptr<SensitivityScenarioData>& sensiData,
        const ParSensitivityAnalysis::ParContainer& parSensitivities,
        const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
        const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
        const bool continueOnError = false, const bool useSpreadedTermStructures = false,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig()) {
        // Dummy implementation, return input, just test tree decomposition in connected components

        constexpr bool handlePseudoCurrencies = true;
        constexpr bool allowPartialScenarios = false;
        constexpr bool cacheSimData = false;

        // Dont build a scenario generator, we modify the simData directly,

        // Check if we have a IR par scenario
        DLOG("Build Simulation Market");
        boost::shared_ptr<ScenarioSimMarket> simMarket = boost::make_shared<ScenarioSimMarket>(
            market, simMarketParameters, Market::defaultConfiguration, curveConfigs, todaysMarketParams,
            continueOnError, stressTestData->useSpreadedTermStructures(), cacheSimData, allowPartialScenarios,
            iborFallbackConfig, handlePseudoCurrencies);
        // Build Par Instruments
        ParSensitivityInstrumentBuilder::Instruments instruments;
        ParSensitivityInstrumentBuilder().createParInstruments(
            instruments, asof, simMarketParameters, *sensiData, {},
            {RiskFactorKey::KeyType::DiscountCurve, RiskFactorKey::KeyType::YieldCurve,
             RiskFactorKey::KeyType::IndexCurve, RiskFactorKey::KeyType::OptionletVolatility,
             RiskFactorKey::KeyType::SurvivalProbability, RiskFactorKey::KeyType::ZeroInflationCurve,
             RiskFactorKey::KeyType::YoYInflationCurve, RiskFactorKey::KeyType::YoYInflationCapFloorVolatility},
            {}, continueOnError, Market::defaultConfiguration, simMarket);

        simMarket->reset();

        for (const auto& scenario : stressTestData->data()) {
            std::cout << "Convert scenario " << scenario.label << std::endl;
            std::cout << scenario.irCurveParShifts << std::endl;
            // Rate shifts
            bool irCurveParScenario = scenario.irCurveParShifts;
            // auto n_tenors = simMarketParameters->yieldVolTerms().size();
            if (irCurveParScenario || scenario.irCapFloorParShifts || scenario.creditCurveParShifts) {
                // Perform ParSensiAnalysis
                // We have bipart graph with two maps
                std::map<RiskFactorKey, std::set<RiskFactorKey>> parToZeroEdges;
                std::map<RiskFactorKey, std::set<RiskFactorKey>> zeroToParEdges;

                for (const auto& [key, value] : parSensitivities) {
                    const auto& [parKey, zeroKey] = key;
                    if (!QuantLib::close_enough(value, 0.0)) {
                        std::cout << "Par " << parKey << " derivided with respect to zero " << zeroKey << " " << value
                                  << std::endl;
                        parToZeroEdges[parKey].insert(zeroKey);
                        zeroToParEdges[zeroKey].insert(parKey);
                    }
                }

                std::set<RiskFactorKey> parNodeVisited;
                std::set<RiskFactorKey> zeroNodeVisited;
                std::queue<RiskFactorKey> queue;
                std::vector<std::set<RiskFactorKey>> connectedComponents;
                // Collect all connected parQuotes
                for (const auto& [parKey, _] : parToZeroEdges) {
                    std::cout << "Par Node " << parKey << std::endl;
                    std::set<RiskFactorKey> connectedParRates;
                    if (parNodeVisited.count(parKey) == 0) {
                        std::cout << "Is not visited yet, so add it to queue" << std::endl;
                        queue.push(parKey);
                    } else {
                        std::cout << "Skip node, already visited" << std::endl;
                    }
                    while (!queue.empty()) {
                        auto currentNode = queue.front();
                        queue.pop();
                        std::cout << "Visit " << currentNode << std::endl;
                        connectedParRates.insert(currentNode);
                        parNodeVisited.insert(currentNode);
                        // Breadth-first-Search
                        for (const auto& zeroNode : parToZeroEdges[currentNode]) {
                            // If that zeroNode hasn't been visited yet, add all non visited connectedParNotes to the
                            // Queue
                            if (zeroNodeVisited.count(zeroNode) == 0) {
                                std::cout << "visit zero rate " << zeroNode << std::endl;
                                zeroNodeVisited.insert(zeroNode);
                                for (const auto& connectedParNode : zeroToParEdges[zeroNode]) {
                                    if (parNodeVisited.count(connectedParNode) == 0) {
                                        std::cout << "Add connected par node " << connectedParNode << "to queue"
                                                  << std::endl;
                                        queue.push(connectedParNode);
                                    } else {
                                        std::cout << "Skip connected par node " << connectedParNode << std::endl;
                                    }
                                }
                            } else {
                                std::cout << "skip zero rate " << zeroNode << std::endl;
                            }
                        }
                    }
                    if (!connectedParRates.empty()) {
                        connectedComponents.push_back(connectedParRates);
                    }
                }
                std::cout << "Found " << connectedComponents.size() << " connected components" << std::endl;

                auto targetParRates = buildTargetValues(scenario, instruments.parHelpers_, simMarketParameters);

                for (const auto& [key, perHelper] : instruments.parHelpers_) {
                    std::cout << key << " Helper " << perHelper << " " << impliedQuote(perHelper) << " Target "
                              << targetParRates[key] << std::endl;
                }

                size_t i = 0;
                for (const auto& component : connectedComponents) {
                    std::cout << i << "th componentent with size " << component.size() << std::endl;

                    std::vector<RiskFactorKey> parKeys;
                    std::vector<double> goal;
                    std::set<RiskFactorKey> zeroRatesSet;

                    for (const auto& parKey : component) {
                        std::cout << "Par Key " << parKey << std::endl;
                        goal.push_back(targetParRates[parKey]);
                        parKeys.push_back(parKey);
                        zeroRatesSet.insert(parToZeroEdges[parKey].begin(), parToZeroEdges[parKey].end());
                    }
                    std::vector<RiskFactorKey> zeroRates(zeroRatesSet.begin(), zeroRatesSet.end());
                    std::cout << "All relevant zeroKeys" << std::endl;
                    for (const auto& z : zeroRates) {
                        std::cout << "Zero Key " << z << std::endl;
                    }

                    std::vector<double> initialGuess;
                    for (size_t i = 0; i < zeroRates.size(); ++i) {
                        initialGuess.push_back(0.001);
                    }

                }
                // Build ScenarioSimMarket

                // Build Scenario

                // Build TargetFunction

                // Optimization

                // Compute Target Par Rate
                // Modify ZeroRates to match Par-Rate
            }
            std::cout << "finished " << std::endl;
        }

        return stressTestData;
    }
};

} // namespace analytics
} // namespace ore
