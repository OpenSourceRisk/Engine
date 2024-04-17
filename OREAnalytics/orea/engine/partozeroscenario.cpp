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
#include <orea/engine/partozeroscenario.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/math/optimization/constraint.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>

namespace ore {
namespace analytics {

struct SensitivityBiGraph {
    std::map<RiskFactorKey, std::set<RiskFactorKey>> parToZeroEdges;
    std::map<RiskFactorKey, std::set<RiskFactorKey>> zeroToParEdges;

    explicit SensitivityBiGraph(const ParSensitivityAnalysis::ParContainer& parSensitivities) {
        for (const auto& [key, value] : parSensitivities) {
            const auto& [parKey, zeroKey] = key;
            if (!QuantLib::close_enough(value, 0.0)) {
                parToZeroEdges[parKey].insert(zeroKey);
                zeroToParEdges[zeroKey].insert(parKey);
            }
        }
    }

    std::vector<std::set<RiskFactorKey>> connectedComponents() {
        std::vector<std::set<RiskFactorKey>> results;
        std::set<RiskFactorKey> parNodeVisited;
        std::set<RiskFactorKey> zeroNodeVisited;
        std::queue<RiskFactorKey> queue;

        // Collect all connected parQuotes
        for (const auto& [parKey, _] : parToZeroEdges) {

            std::set<RiskFactorKey> connectedParRates;
            if (parNodeVisited.count(parKey) == 0) {

                queue.push(parKey);
            } else {
            }
            while (!queue.empty()) {
                auto currentNode = queue.front();
                queue.pop();

                connectedParRates.insert(currentNode);
                parNodeVisited.insert(currentNode);
                // Breadth-first-Search
                for (const auto& zeroNode : parToZeroEdges[currentNode]) {
                    // If that zeroNode hasn't been visited yet, add all non visited connectedParNotes to
                    // the Queue
                    if (zeroNodeVisited.count(zeroNode) == 0) {

                        zeroNodeVisited.insert(zeroNode);
                        for (const auto& connectedParNode : zeroToParEdges[zeroNode]) {
                            if (parNodeVisited.count(connectedParNode) == 0) {

                                queue.push(connectedParNode);
                            } else {
                            }
                        }
                    } else {
                    }
                }
            }
            if (!connectedParRates.empty()) {
                results.push_back(connectedParRates);
            }
        }
        return results;
    }
};

struct TodaysImpliedAndTargetQuotes {
    std::map<RiskFactorKey, double> baseParQuote;
    std::map<RiskFactorKey, double> targetParQuote;
    std::map<RiskFactorKey, double> scenarioBaseValue;
    std::map<RiskFactorKey, double> time;
};

//! Compute the
void populateTargetParRate(TodaysImpliedAndTargetQuotes& results, const RiskFactorKey& key,
                           const StressTestScenarioData::CurveShiftData& data,
                           const std::vector<QuantLib::Period>& simulationTenors) {
    std::vector<Period> shiftTenors = data.shiftTenors;
    std::vector<Real> shifts = data.shifts;
    QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");
    QL_REQUIRE(shiftTenors.size() == simulationTenors.size(), "Discount shift doesn't match sim market tenors, got "
                                                                  << simulationTenors.size()
                                                                  << " SimulationMarket Tenors and "
                                                                  << shiftTenors.size() << " ScenarioShiftTenors");
    QL_REQUIRE(simulationTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
    size_t i = key.index;
    Real fairParRate = results.baseParQuote[key];
    Real shiftedRate = fairParRate;
    if (data.shiftType == ShiftType::Absolute) {
        shiftedRate += shifts[i];
    } else {
        shiftedRate *= (1.0 + shifts[i]);
    }
    results.targetParQuote[key] = shiftedRate;
}

void populateTargetParRate(TodaysImpliedAndTargetQuotes& results, const RiskFactorKey& key,
                           const StressTestScenarioData::VolShiftData& data) {
    std::vector<Real> shifts = data.shifts;
    size_t i = key.index;
    Real fairParRate = results.baseParQuote[key];
    Real shiftedRate = fairParRate;
    if (data.shiftType == ShiftType::Absolute) {
        shiftedRate += shifts[i];
    } else {
        shiftedRate *= (1.0 + shifts[i]);
    }
    results.targetParQuote[key] = shiftedRate;
}

void populateRiskFactorTime(TodaysImpliedAndTargetQuotes& results, const RiskFactorKey& key, const QuantLib::Date& asof,
                            const boost::shared_ptr<TermStructure>& ts,
                            const std::vector<QuantLib::Period>& simulationTenors) {
    size_t i = key.index;
    double t = ts->dayCounter().yearFraction(asof, asof + simulationTenors[i]);
    results.time[key] = t;
}

TodaysImpliedAndTargetQuotes
getTodaysAndTargetQuotes(const Date& asof, const boost::shared_ptr<ScenarioSimMarket>& market,
                         const StressTestScenarioData::StressTestData& stressScenario,
                         const ParSensitivityInstrumentBuilder::Instruments& parInstruments,
                         const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters) {

    TodaysImpliedAndTargetQuotes results;

    // Populate Zero Domain BaseValues and Times
    for (const auto& key : market->baseScenarioAbsolute()->keys()) {
        results.scenarioBaseValue[key] = market->baseScenarioAbsolute()->get(key);
        if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
            const std::string& ccy = key.name;
            populateRiskFactorTime(results, key, asof, *market->discountCurve(ccy),
                                   simMarketParameters->yieldCurveTenors(ccy));
        } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
            const std::string& indexName = key.name;
            populateRiskFactorTime(results, key, asof, *market->iborIndex(indexName)->forwardingTermStructure(),
                                   simMarketParameters->yieldCurveTenors(indexName));
        } else if (key.keytype == RiskFactorKey::KeyType::YieldCurve) {
            const std::string& curveName = key.name;
            populateRiskFactorTime(results, key, asof, *market->yieldCurve(curveName),
                                   simMarketParameters->yieldCurveTenors(curveName));
        } else if (key.keytype == RiskFactorKey::KeyType::SurvivalProbability) {
            const std::string& curveName = key.name;
            populateRiskFactorTime(results, key, asof, *market->defaultCurve(curveName)->curve(),
                                   simMarketParameters->defaultTenors(curveName));
        } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
            const std::string& indexName = key.name;
            populateRiskFactorTime(results, key, asof, *market->capFloorVol(indexName),
                                   simMarketParameters->capFloorVolExpiries(indexName));
        }
    }

    // Populate Par Domain Base and Target Value
    for (const auto& [key, helper] : parInstruments.parHelpers_) {
        double fairRate = impliedQuote(helper);
        results.baseParQuote[key] = fairRate;
        results.targetParQuote[key] = fairRate;
        if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
            const std::string& ccy = key.name;
            if (auto it = stressScenario.discountCurveShifts.find(ccy);
                it != stressScenario.discountCurveShifts.end()) {
                populateTargetParRate(results, key, it->second, simMarketParameters->yieldCurveTenors(ccy));
            }
        } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
            const std::string& indexName = key.name;
            if (auto it = stressScenario.indexCurveShifts.find(indexName);
                it != stressScenario.indexCurveShifts.end()) {
                populateTargetParRate(results, key, it->second, simMarketParameters->yieldCurveTenors(indexName));
            }
        } else if (key.keytype == RiskFactorKey::KeyType::YieldCurve) {
            const std::string& curveName = key.name;
            if (auto it = stressScenario.yieldCurveShifts.find(curveName);
                it != stressScenario.yieldCurveShifts.end()) {
                populateTargetParRate(results, key, it->second, simMarketParameters->yieldCurveTenors(curveName));
            }
        } else if (key.keytype == RiskFactorKey::KeyType::SurvivalProbability) {
            const std::string& curveName = key.name;
            if (auto it = stressScenario.survivalProbabilityShifts.find(curveName);
                it != stressScenario.survivalProbabilityShifts.end()) {
                populateTargetParRate(results, key, it->second, simMarketParameters->defaultTenors(curveName));
            }
        }
    }

    for (const auto& [key, cap] : parInstruments.parCaps_) {
        if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
            QL_REQUIRE(parInstruments.parCapsYts_.count(key) > 0,
                       "getTodaysAndTargetQuotes: no cap yts found for key " << key);
            QL_REQUIRE(parInstruments.parCapsVts_.count(key) > 0,
                       "getTodaysAndTargetQuotes: no cap vts found for key " << key);
            Real price = cap->NPV();
            Volatility parVol = impliedVolatility(*cap, price, parInstruments.parCapsYts_.at(key), 0.01,
                                                  parInstruments.parCapsVts_.at(key)->volatilityType(),
                                                  parInstruments.parCapsVts_.at(key)->displacement());
            results.baseParQuote[key] = parVol;
            results.targetParQuote[key] = parVol;
            if (auto it = stressScenario.capVolShifts.find(key.name); it != stressScenario.capVolShifts.end()) {
                populateTargetParRate(results, key, it->second);
            }
        }
    }

    return results;
}

struct TargetFunction : public QuantLib::CostFunction {

    TargetFunction(boost::shared_ptr<ScenarioSimMarket>& simMarket, const vector<double>& goal,
                   const vector<RiskFactorKey>& parKeys, const vector<RiskFactorKey>& zeroKeys,
                   const ParSensitivityInstrumentBuilder::Instruments& parInstruments)
        : simMarket(simMarket), goal(goal), parKeys(parKeys), zeroKeys(zeroKeys), parInstruments_(parInstruments) {}

    QuantLib::Array values(const QuantLib::Array& x) const override {
        simMarket->reset();
        auto trialScenario = simMarket->baseScenario()->clone();

        for (size_t i = 0; i < zeroKeys.size(); ++i) {
            double delta = x[i];
            trialScenario->add(zeroKeys[i], delta);
        }
        simMarket->applyScenario(trialScenario);
        vector<double> error;
        for (size_t i = 0; i < parKeys.size(); ++i) {
            const auto& key = parKeys[i];
            if (key.keytype == RiskFactorKey::KeyType::DiscountCurve ||
                key.keytype == RiskFactorKey::KeyType::IndexCurve) {
                double fairParRate = impliedQuote(parInstruments_.parHelpers_.at(key));
                error.push_back((goal[i] - fairParRate) * 1e6);
            } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
                auto capIt = parInstruments_.parCaps_.find(key);
                QL_REQUIRE(capIt != parInstruments_.parCaps_.end(), "");
                const auto& cap = capIt->second;
                QL_REQUIRE(parInstruments_.parCapsYts_.count(key) > 0,
                           "getTodaysAndTargetQuotes: no cap yts found for key " << key);
                QL_REQUIRE(parInstruments_.parCapsVts_.count(key) > 0,
                           "getTodaysAndTargetQuotes: no cap vts found for key " << key);
                Real price = cap->NPV();
                Volatility parVol = impliedVolatility(*cap, price, parInstruments_.parCapsYts_.at(key), 0.01,
                                                      parInstruments_.parCapsVts_.at(key)->volatilityType(),
                                                      parInstruments_.parCapsVts_.at(key)->displacement());
                error.push_back((goal[i] - parVol) * 1e6);
            }
        }
        return QuantLib::Array(error.begin(), error.end());
    }

    boost::shared_ptr<ScenarioSimMarket>& simMarket;
    const vector<double>& goal;
    const vector<RiskFactorKey>& parKeys;
    const vector<RiskFactorKey>& zeroKeys;
    const ParSensitivityInstrumentBuilder::Instruments& parInstruments_;
};

boost::shared_ptr<ore::analytics::StressTestScenarioData> convertParScenarioToZeroScenarioData(
    const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& market,
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParameters,
    const boost::shared_ptr<ore::analytics::StressTestScenarioData>& stressTestData,
    const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiData,
    const std::map<std::pair<ore::analytics::RiskFactorKey, ore::analytics::RiskFactorKey>, double>& parSensitivities,
    const ore::data::CurveConfigurations& curveConfigs, const ore::data::TodaysMarketParameters& todaysMarketParams,
    const bool continueOnError, const bool useSpreadedTermStructures,
    const ore::data::IborFallbackConfig& iborFallbackConfig) {

    constexpr bool handlePseudoCurrencies = true;
    constexpr bool allowPartialScenarios = true;
    constexpr bool cacheSimData = false;

    // Check that the stress scenario matches the sensitivity scenario data
    for (const auto& scenario : stressTestData->data()) {
        if (scenario.irCurveParShifts) {
            for (const auto& [name, shiftData] : scenario.discountCurveShifts) {
                auto it = sensiData->discountCurveShiftData().find(name);
                QL_REQUIRE(it != sensiData->discountCurveShiftData().end(), "Couldnt find discountCurveSensiShiftData");
                QL_REQUIRE(shiftData.shiftTenors.size() == it->second->shiftTenors.size(), "Mismatch of sizes");
                LOG("Debug discount shifts " << name);
                LOG("StressShiftTenor;SensiShiftTenor;SimulationShiftTenor");
                for (size_t i = 0; i < shiftData.shiftTenors.size(); ++i) {
                    LOG(shiftData.shiftTenors[i] << ";" << it->second->shiftTenors[i] << ";" << simMarketParameters->yieldCurveTenors(name)[i]);
                    QL_REQUIRE(shiftData.shiftTenors[i] == it->second->shiftTenors[i], "");
                }
            }
        }
    }

    // Check if we have a IR par scenario
    LOG("ParToZeroScenario: Build Simulation Market");
    boost::shared_ptr<ScenarioSimMarket> simMarket = boost::make_shared<ScenarioSimMarket>(
        market, simMarketParameters, Market::defaultConfiguration, curveConfigs, todaysMarketParams, continueOnError,
        stressTestData->useSpreadedTermStructures(), cacheSimData, allowPartialScenarios, iborFallbackConfig,
        handlePseudoCurrencies);

    LOG("ParToZeroScenario: Build ParInstruments");
    ParSensitivityInstrumentBuilder::Instruments instruments;
    ParSensitivityInstrumentBuilder().createParInstruments(
        instruments, asof, simMarketParameters, *sensiData, {},
        {RiskFactorKey::KeyType::DiscountCurve, RiskFactorKey::KeyType::YieldCurve, RiskFactorKey::KeyType::IndexCurve,
         RiskFactorKey::KeyType::OptionletVolatility, RiskFactorKey::KeyType::SurvivalProbability},
        {}, continueOnError, Market::defaultConfiguration, simMarket);

    simMarket->reset();
    DLOG("ParToZeroScenario: Begin Stress Scenarios conversion");
    boost::shared_ptr<StressTestScenarioData> results = boost::make_shared<StressTestScenarioData>();
    results->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
    for (const StressTestScenarioData::StressTestData& scenario : stressTestData->data()) {
        if (!scenario.containsParShifts()) {
            LOG("ParToZeroScenario: SKIP stress scenario " << scenario.label << ". It doesn't contain any par shifts");
            results->data().push_back(scenario);
        } else {
            LOG("ParToZeroScenario: converting parshifts to zero shifts in scenario " << scenario.label);
            // Copy scenario
            StressTestScenarioData::StressTestData convertedScenario;
            convertedScenario.label = scenario.label;

            SensitivityBiGraph sensiGraph(parSensitivities);

            auto connectedComponents = sensiGraph.connectedComponents();
            LOG("ParToZeroScenario: Found " << connectedComponents.size() << " connected components");

            auto targetParRates = getTodaysAndTargetQuotes(asof, simMarket, scenario, instruments, simMarketParameters);

            size_t i = 0;
            map<RiskFactorKey, double> solutions;
            for (const auto& component : connectedComponents) {
                std::cout << i << "th componentent with size " << component.size() << std::endl;
                LOG(i++ << "th componentent with size " << component.size());

                std::vector<RiskFactorKey> parKeys;
                std::vector<double> goal;
                std::set<RiskFactorKey> zeroRatesSet;

                for (const auto& parKey : component) {
                    LOG("Par Key " << parKey << "Fair Par Rate " << targetParRates.baseParQuote[parKey] << " Target "
                                   << targetParRates.targetParQuote[parKey]);
                    goal.push_back(targetParRates.targetParQuote[parKey]);
                    parKeys.push_back(parKey);
                    zeroRatesSet.insert(sensiGraph.parToZeroEdges[parKey].begin(),
                                        sensiGraph.parToZeroEdges[parKey].end());
                }
                std::vector<RiskFactorKey> zeroKeys(zeroRatesSet.begin(), zeroRatesSet.end());

                QuantLib::Array initialGuess(zeroKeys.size(), 1.0);

                PositiveConstraint noConstraint;
                LevenbergMarquardt lm;
                EndCriteria endCriteria(1250, 10, 1e-8, 1e-8, 1e-8);
                TargetFunction target(simMarket, goal, parKeys, zeroKeys, instruments);
                Problem problem(target, noConstraint, initialGuess);
                lm.minimize(problem, endCriteria);
                auto solution = problem.currentValue();

                LOG("ParToZeroScenario: Mean-squared-error: " << problem.functionValue());
                for (size_t i = 0; i < zeroKeys.size(); ++i) {
                    if (solutions.count(zeroKeys[i]) > 0) {
                        std::cout << "Duplicate Key, the components arent disjunct" << std::endl;
                    }
                    solutions[zeroKeys[i]] = solution[i];
                }
                LOG("ParToZeroScenario: Calculate zero shift from solution");
                LOG("i;zerokey;solution;zeroBaseValue;time;logDf;shift");
                for (size_t i = 0; i < zeroKeys.size(); ++i) {
                    const auto key = zeroKeys[i];
                    double zeroShift = 0;
                    double discountFactor = solution[i];
                    double time = targetParRates.time[key];
                    double baseValue = targetParRates.scenarioBaseValue[key];
                    if (!stressTestData->useSpreadedTermStructures()) {
                        zeroShift = -std::log(discountFactor / baseValue) / time;
                    } else {
                        zeroShift = -std::log(discountFactor) / time;
                    }
                    LOG(i << ";" << zeroKeys[i] << ";" << discountFactor << ";" << baseValue << ";" << time << ";"
                          << std::log(discountFactor) << ";" << zeroShift);

                    if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
                        if (convertedScenario.discountCurveShifts.count(key.name) == 0) {
                            StressTestScenarioData::CurveShiftData newData;
                            newData.shiftType = ShiftType::Absolute;
                            newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
                            newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
                            newData.shifts[key.index] = zeroShift;
                            convertedScenario.discountCurveShifts.insert({key.name, newData});
                        } else {
                            convertedScenario.discountCurveShifts.at(key.name).shifts[key.index] = zeroShift;
                        }
                    } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
                        if (convertedScenario.indexCurveShifts.count(key.name) == 0) {
                            StressTestScenarioData::CurveShiftData newData;
                            newData.shiftType = ShiftType::Absolute;
                            newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
                            newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
                            newData.shifts[key.index] = zeroShift;
                            convertedScenario.indexCurveShifts.insert({key.name, newData});
                        } else {
                            convertedScenario.indexCurveShifts.at(key.name).shifts[key.index] = zeroShift;
                        }
                    }
                }
                // Convert result into a zero shift
            }
            LOG("DBEUG zero from Par Scenario ");
            simMarket->reset();
            auto targetScenario = simMarket->baseScenario()->clone();
            for (const auto& [key, zeroValue] : solutions) {
                LOG("Add Scenario " << key << " : " << zeroValue);
                targetScenario->add(key, zeroValue);
            }
            simMarket->applyScenario(targetScenario);
            LOG("key;fairrate;target;error");
            for (const auto& [key, parHelper] : instruments.parHelpers_) {
                double tgt = targetParRates.targetParQuote[key];
                double rate = impliedQuote(parHelper);
                LOG(key << ";" << rate << ";" << tgt << ";" << tgt - rate);
            }
            std::cout << "finished scenario " << scenario.label << std::endl;
            convertedScenario.irCurveParShifts = false;
            convertedScenario.creditCurveParShifts = false;
            convertedScenario.irCapFloorParShifts = false;
            results->data().push_back(convertedScenario);
        }
    }
    results->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
    results->toFile("./Output/stressTest_zero.xml");
    return results;
}

} // namespace analytics

} // namespace ore