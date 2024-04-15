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

struct TodaysImpliedQuotes {
    std::map<RiskFactorKey, double> baseValues;
    std::map<RiskFactorKey, double> targetValues;
    std::map<RiskFactorKey, double> zeroValues;
    std::map<RiskFactorKey, double> zeroBaseScenarioValue;
    //! ZeroFactor to time
    std::map<RiskFactorKey, double> times;
};

void addTargetValueToResults(TodaysImpliedQuotes& results, const RiskFactorKey& key,
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
    Real fairParRate = results.baseValues[key];
    Real shiftedRate = fairParRate;
    if (data.shiftType == ShiftType::Absolute) {
        shiftedRate += shifts[i];
    } else {
        shiftedRate *= (1.0 + shifts[i]);
    }
    results.targetValues[key] = shiftedRate;
}

void addZeroRateToResults(TodaysImpliedQuotes& results, const RiskFactorKey& key, const QuantLib::Date& asof,
                          const boost::shared_ptr<YieldTermStructure>& ts,
                          const std::vector<QuantLib::Period>& simulationTenors) {
    size_t i = key.index;
    double t = ts->dayCounter().yearFraction(asof, asof + simulationTenors[i]);
    results.times[key] = t;
    results.zeroValues[key] = ts->discount(t);
}

TodaysImpliedQuotes getTodaysImpliedQuotes(const Date& asof, const boost::shared_ptr<ScenarioSimMarket>& market,
                                           const StressTestScenarioData::StressTestData& stressScenario,
                                           const std::map<RiskFactorKey, boost::shared_ptr<Instrument>>& parHelpers,
                                           const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters) {

    TodaysImpliedQuotes results;
    for (const auto& [key, helper] : parHelpers) {
        double fairRate = impliedQuote(helper);
        results.zeroBaseScenarioValue[key] = market->baseScenarioAbsolute()->get(key);
        results.baseValues[key] = fairRate;
        results.targetValues[key] = fairRate;
        if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
            const std::string& ccy = key.name;
            addZeroRateToResults(results, key, asof, *market->discountCurve(ccy),
                                 simMarketParameters->yieldCurveTenors(ccy));

            if (auto it = stressScenario.discountCurveShifts.find(ccy);
                it != stressScenario.discountCurveShifts.end()) {
                addTargetValueToResults(results, key, it->second, simMarketParameters->yieldCurveTenors(ccy));
            }
        } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
            const std::string& indexName = key.name;
            addZeroRateToResults(results, key, asof, *market->iborIndex(indexName)->forwardingTermStructure(),
                                 simMarketParameters->yieldCurveTenors(indexName));
            if (auto it = stressScenario.indexCurveShifts.find(indexName);
                it != stressScenario.indexCurveShifts.end()) {
                addTargetValueToResults(results, key, it->second, simMarketParameters->yieldCurveTenors(indexName));
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
            double delta = x[i];
            trialScenario->add(zeroKeys[i], delta);
        }
        simMarket->applyScenario(trialScenario);
        vector<double> mse;
        for (size_t i = 0; i < parKeys.size(); ++i) {
            double fairParRate = impliedQuote(parHelpers.at(parKeys[i]));
            mse.push_back((goal[i] - fairParRate) * 1e6);
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
        boost::shared_ptr<StressTestScenarioData> convertedScenarios = boost::make_shared<StressTestScenarioData>();
        for (const StressTestScenarioData::StressTestData& scenario : stressTestData->data()) {

            std::cout << "Convert scenario " << scenario.label << std::endl;
            StressTestScenarioData::StressTestData upDatedScenario = scenario;
            bool irCurveParScenario = scenario.irCurveParShifts;
            if (irCurveParScenario) {

                for (const auto& data : upDatedScenario.discountCurveShifts) {
                    std::cout << data.first << std::endl;
                    for (const auto& tenors : data.second.shiftTenors) {
                        std::cout << tenors << std::endl;
                    }
                }

                std::cout << scenario.irCurveParShifts << std::endl;
                // Rate shifts

                // auto n_tenors = simMarketParameters->yieldVolTerms().size();
                if (irCurveParScenario || scenario.irCapFloorParShifts || scenario.creditCurveParShifts) {
                    // Perform ParSensiAnalysis
                    // We have bipart graph with two maps
                    std::map<RiskFactorKey, std::set<RiskFactorKey>> parToZeroEdges;
                    std::map<RiskFactorKey, std::set<RiskFactorKey>> zeroToParEdges;

                    for (const auto& [key, value] : parSensitivities) {
                        const auto& [parKey, zeroKey] = key;
                        if (!QuantLib::close_enough(value, 0.0)) {
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
                            connectedComponents.push_back(connectedParRates);
                        }
                    }
                    std::cout << "Found " << connectedComponents.size() << " connected components" << std::endl;

                    auto targetParRates =
                        getTodaysImpliedQuotes(asof, simMarket, scenario, instruments.parHelpers_, simMarketParameters);

                    size_t i = 0;
                    map<RiskFactorKey, double> solutions;
                    for (const auto& component : connectedComponents) {
                        std::cout << i << "th componentent with size " << component.size() << std::endl;
                        LOG(i++ << "th componentent with size " << component.size());

                        std::vector<RiskFactorKey> parKeys;
                        std::vector<double> goal;
                        std::set<RiskFactorKey> zeroRatesSet;

                        for (const auto& parKey : component) {
                            LOG("Par Key " << parKey << "Fair Par Rate " << targetParRates.baseValues[parKey]
                                           << " Target " << targetParRates.targetValues[parKey]);
                            goal.push_back(targetParRates.targetValues[parKey]);
                            parKeys.push_back(parKey);
                            zeroRatesSet.insert(parToZeroEdges[parKey].begin(), parToZeroEdges[parKey].end());
                        }
                        std::vector<RiskFactorKey> zeroKeys(zeroRatesSet.begin(), zeroRatesSet.end());
                        LOG("All relevant zeroKeys");
                        simMarket->reset();
                        for (const auto& z : zeroKeys) {
                            LOG("Zero Key " << z << "Base Scenario Value " << targetParRates.zeroValues[z] << " "
                                            << targetParRates.zeroBaseScenarioValue[z] << " "
                                            << targetParRates.times[z]);
                        }

                        QuantLib::Array initialGuess(zeroKeys.size(), 1.0);

                        PositiveConstraint noConstraint;
                        LevenbergMarquardt lm;
                        EndCriteria endCriteria(100, 10, 1e-8, 1e-8, 1e-8);
                        TargetFunction target(simMarket, goal, parKeys, zeroKeys, instruments.parHelpers_);
                        Problem problem(target, noConstraint, initialGuess);
                        lm.minimize(problem, endCriteria);
                        auto solution = problem.currentValue();

                        std::cout << "Found solution " << problem.functionValue() << std::endl;
                        std::cout << "Start Looping " << std::endl;
                        for (size_t i = 0; i < zeroKeys.size(); ++i) {
                            std::cout << i << " " << zeroKeys[i] << " " << solution[i] << std::endl;
                            if (solutions.count(zeroKeys[i]) > 0) {
                                std::cout << "Duplicate Key, the components arent disjunct" << std::endl;
                            }
                            solutions[zeroKeys[i]] = solution[i];
                        }
                        std::cout << "Looped over all " << std::endl;
                        for (size_t i = 0; i < zeroKeys.size(); ++i) {
                            const auto key = zeroKeys[i];
                            double zeroShift = 0;
                            if (!stressTestData->useSpreadedTermStructures()) {
                                zeroShift = -std::log(solution[i] / targetParRates.zeroBaseScenarioValue[key]) /
                                            targetParRates.times[key];
                            } else {
                                zeroShift = -std::log(solution[i]) / targetParRates.times[key];
                            }
                            std::cout << i << " " << zeroKeys[i] << " " << solution[i] << " " << zeroShift << std::endl;

                            if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
                                if (upDatedScenario.discountCurveShifts.count(key.name) == 0) {
                                    StressTestScenarioData::CurveShiftData newData;
                                    newData.shiftType = ShiftType::Absolute;
                                    newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
                                    newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
                                    newData.shifts[key.index] = zeroShift;
                                    upDatedScenario.discountCurveShifts.insert({key.name, newData});
                                } else {
                                    upDatedScenario.discountCurveShifts.at(key.name).shifts[key.index] = zeroShift;
                                }
                            } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
                                if (upDatedScenario.indexCurveShifts.count(key.name) == 0) {
                                    StressTestScenarioData::CurveShiftData newData;
                                    newData.shiftType = ShiftType::Absolute;
                                    newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
                                    newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
                                    newData.shifts[key.index] = zeroShift;
                                    upDatedScenario.indexCurveShifts.insert({key.name, newData});
                                } else {
                                    upDatedScenario.indexCurveShifts.at(key.name).shifts[key.index] = zeroShift;
                                }
                            }
                        }
                        // Convert result into a zero shift
                    }
                    std::cout << "Finales Scenario " << std::endl;
                    simMarket->reset();
                    auto targetScenario = simMarket->baseScenario()->clone();
                    for (const auto& [key, zeroValue] : solutions) {
                        std::cout << "Add Scenario " << key << " : " << zeroValue << std::endl;
                        targetScenario->add(key, zeroValue);
                    }
                    simMarket->applyScenario(targetScenario);
                    std::cout << "key;fairrate;target;error" << std::endl;
                    for (const auto& [key, parHelper] : instruments.parHelpers_) {
                        double tgt = targetParRates.targetValues[key];
                        double rate = impliedQuote(parHelper);
                        std::cout << key << ";" << rate << ";" << tgt << ";" << tgt - rate << std::endl;
                    }
                    // Build ScenarioSimMarket

                    // Build Scenario

                    // Build TargetFunction

                    // Optimization
                    std::cout << "Spreaded Termstructures new Data " << stressTestData->useSpreadedTermStructures()
                              << std::endl;
                    std::cout << "Spreaded Termstructures new Data " << convertedScenarios->useSpreadedTermStructures()
                              << std::endl;
                    // Compute Target Par Rate
                    // Modify ZeroRates to match Par-Rate
                    std::cout << "finished scenario " << scenario.label << std::endl;

                    upDatedScenario.irCurveParShifts = false;
                    upDatedScenario.creditCurveParShifts = false;
                    upDatedScenario.irCapFloorParShifts = false;
                    
                }
            }
            convertedScenarios->data().push_back(upDatedScenario);
        }
        convertedScenarios->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
        convertedScenarios->toFile("./Input/convertedStressTest.xml");
        return convertedScenarios;
    }
};

} // namespace analytics
} // namespace ore
