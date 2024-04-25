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

//! Build a bi-graph from the parSensitivites and search for connectedComponents

class SimpleRiskFactorGraph {
public:
    SimpleRiskFactorGraph(const ParSensitivityAnalysis::ParContainer& parWithRespectToZero) {

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
    }

    const std::vector<RiskFactorKey>& orderedRiskFactors() const { return orderedKeys; }

private:
    std::map<RiskFactorKey, std::set<RiskFactorKey>> parToZeroEdges;
    std::map<RiskFactorKey, std::set<RiskFactorKey>> zeroToParEdges;
    std::vector<RiskFactorKey> orderedKeys;
};


double computeTargetRate(const double fairRate, const double shift, const ShiftType shiftType) {
    double shiftedRate = fairRate;
    if (shiftType == ShiftType::Absolute) {
        shiftedRate += shift;
    } else {
        shiftedRate *= (1.0 + shift);
    }
    return shiftedRate;
}

QuantLib::Period getYieldCurvePeriod(const RiskFactorKey& rfKey, const boost::shared_ptr<ScenarioSimMarketParameters>& params){
    QL_REQUIRE(rfKey.index < params->yieldCurveTenors(rfKey.name).size(), "Please align pillars, internal error");
    return params->yieldCurveTenors(rfKey.name)[rfKey.index];
}

double computeMaturityTimeFromRiskFactor(const QuantLib::Date& asof, const RiskFactorKey& rfKey,
                           const boost::shared_ptr<ScenarioSimMarket>& simMarket,
                           const boost::shared_ptr<ScenarioSimMarketParameters>& params) {
    boost::shared_ptr<QuantLib::TermStructure> ts;
    QuantLib::Period tenor;
    switch (rfKey.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:
        ts = *simMarket->discountCurve(rfKey.name);
        tenor = getYieldCurvePeriod(rfKey, params);
        break;
    case RiskFactorKey::KeyType::YieldCurve:
        ts = *simMarket->yieldCurve(rfKey.name);
        tenor = getYieldCurvePeriod(rfKey, params);
        break;
    case RiskFactorKey::KeyType::IndexCurve:
        ts = *simMarket->iborIndex(rfKey.name)->forwardingTermStructure();
        tenor = getYieldCurvePeriod(rfKey, params);
        break;
    case RiskFactorKey::KeyType::SurvivalProbability:
        ts = *simMarket->defaultCurve(rfKey.name)->curve();
        QL_REQUIRE(rfKey.index < params->defaultTenors(rfKey.name).size(), "Please align pillars, internal error");
        tenor =  params->defaultTenors(rfKey.name)[rfKey.index];
        break;
    default:
        QL_FAIL("ParStressScenario to ZeroConversion: Unsupported riskfactor, can not compute time to maturity "
                "from curve");
    }
    return ts->dayCounter().yearFraction(asof, asof + tenor);
}

//! Compute the
double getCurveStressShift(const RiskFactorKey& key, const StressTestScenarioData::StressTestData& stressScenario) {
    const std::vector<double>* shifts = nullptr;
    double shift = 0.0;
    switch (key.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:{
        auto it = stressScenario.discountCurveShifts.find(key.name);
        if (it != stressScenario.discountCurveShifts.end()) {
            shifts = &(it->second.shifts);
        }
        break;
    }
    case RiskFactorKey::KeyType::YieldCurve: {
        auto it = stressScenario.yieldCurveShifts.find(key.name);
        if (it != stressScenario.yieldCurveShifts.end()) {
            shifts = &(it->second.shifts);
        }
        break;
    }
    case RiskFactorKey::KeyType::IndexCurve:{
        auto it = stressScenario.indexCurveShifts.find(key.name);
        if (it != stressScenario.indexCurveShifts.end()) {
            shifts = &(it->second.shifts);
        }
        break;
    }
    case RiskFactorKey::KeyType::SurvivalProbability:{
        auto it = stressScenario.survivalProbabilityShifts.find(key.name);
        if (it != stressScenario.survivalProbabilityShifts.end()) {
            shifts = &(it->second.shifts);
        }
        break;
    }
    default:
        QL_FAIL("ParStressScenario to ZeroConversion: Unsupported riskfactor, can not compute time to maturity "
                "from curve");
    }
    if (shifts != nullptr && key.index < shifts->size()) {
        shift = shifts->at(key.index);
    }
    return shift;
}

struct TargetFunctionCapFloor : public QuantLib::CostFunction {

    TargetFunctionCapFloor(const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket, const vector<double>& goal,
                           const vector<RiskFactorKey>& parKeys, const vector<RiskFactorKey>& zeroKeys,
                           const ParSensitivityInstrumentBuilder::Instruments& parInstruments,
                           const ext::shared_ptr<Scenario>& scenario)
        : simMarket(simMarket), goal(goal), parKeys(parKeys), zeroKeys(zeroKeys), parInstruments_(parInstruments),
          trialScenario_(scenario->clone()) {}

    QuantLib::Array values(const QuantLib::Array& x) const override {
        simMarket->reset();
        for (size_t i = 0; i < zeroKeys.size(); ++i) {
            double delta = x[i];
            trialScenario_->add(zeroKeys[i], delta);
        }
        simMarket->applyScenario(trialScenario_);
        vector<double> error;
        for (size_t i = 0; i < parKeys.size(); ++i) {
            const auto& key = parKeys[i];
            if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
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

    const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket;
    const vector<double>& goal;
    const vector<RiskFactorKey>& parKeys;
    const vector<RiskFactorKey>& zeroKeys;
    const ParSensitivityInstrumentBuilder::Instruments& parInstruments_;
    ext::shared_ptr<Scenario> trialScenario_;
};
;

double impliedCapVolatility(const RiskFactorKey& key, const ParSensitivityInstrumentBuilder::Instruments& instruments) {
    QL_REQUIRE(instruments.parCaps_.count(key) == 1, "Can not convert capFloor par shifts to zero Vols");
    QL_REQUIRE(instruments.parCapsYts_.count(key) > 0, "getTodaysAndTargetQuotes: no cap yts found for key " << key);
    QL_REQUIRE(instruments.parCapsVts_.count(key) > 0, "getTodaysAndTargetQuotes: no cap vts found for key " << key);
    const auto cap = instruments.parCaps_.at(key);
    Real price = cap->NPV();
    Volatility parVol = impliedVolatility(*cap, price, instruments.parCapsYts_.at(key), 0.01,
                                          instruments.parCapsVts_.at(key)->volatilityType(),
                                          instruments.parCapsVts_.at(key)->displacement());
    return parVol;
}

void updateTargetStressTestScenarioData(StressTestScenarioData::StressTestData& stressScenario, 
                                        const RiskFactorKey& key,
                                        const double zeroShift,
                                        const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParameters) {
    if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
        if (stressScenario.discountCurveShifts.count(key.name) == 0) {
            StressTestScenarioData::CurveShiftData newData;
            newData.shiftType = ShiftType::Absolute;
            newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
            newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
            newData.shifts[key.index] = zeroShift;
            stressScenario.discountCurveShifts.insert({key.name, newData});
        } else {
            stressScenario.discountCurveShifts.at(key.name).shifts[key.index] = zeroShift;
        }
    } else if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
        if (stressScenario.indexCurveShifts.count(key.name) == 0) {
            StressTestScenarioData::CurveShiftData newData;
            newData.shiftType = ShiftType::Absolute;
            newData.shiftTenors = simMarketParameters->yieldCurveTenors(key.name);
            newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
            newData.shifts[key.index] = zeroShift;
            stressScenario.indexCurveShifts.insert({key.name, newData});
        } else {
            stressScenario.indexCurveShifts.at(key.name).shifts[key.index] = zeroShift;
        }
    } else if (key.keytype == RiskFactorKey::KeyType::SurvivalProbability) {
        if (stressScenario.survivalProbabilityShifts.count(key.name) == 0) {
            StressTestScenarioData::CurveShiftData newData;
            newData.shiftType = ShiftType::Absolute;
            newData.shiftTenors = simMarketParameters->defaultTenors(key.name);
            newData.shifts = std::vector<double>(newData.shiftTenors.size(), 0.0);
            newData.shifts[key.index] = zeroShift;
            stressScenario.survivalProbabilityShifts.insert({key.name, newData});
        } else {
            stressScenario.survivalProbabilityShifts.at(key.name).shifts[key.index] = zeroShift;
        }
    }
}

ore::analytics::StressTestScenarioData::StressTestData
    convertScenario(const ore::analytics::StressTestScenarioData::StressTestData& parStressScenario, const QuantLib::Date& asof,
                    const ParSensitivityInstrumentBuilder::Instruments& instruments,
                    const SimpleRiskFactorGraph& sensiGraph,
                    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParameters,
                    const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                    bool useSpreadedTermstructure) {
    LOG("ParToZeroScenario: converting parshifts to zero shifts in scenario " << parStressScenario.label);
    // Copy scenario
    StressTestScenarioData::StressTestData zeroStressScenario = parStressScenario;

    std::set<RiskFactorKey::KeyType> excludedParRates;

    if (parStressScenario.irCapFloorParShifts) {
        std::cout << "Remove Caplet shifts " << std::endl;
        zeroStressScenario.capVolShifts.clear();
    } else {
        excludedParRates.insert(RiskFactorKey::KeyType::OptionletVolatility);
    }
    if (parStressScenario.creditCurveParShifts) {
        zeroStressScenario.survivalProbabilityShifts.clear();
    } else {
        excludedParRates.insert(RiskFactorKey::KeyType::SurvivalProbability);
    }
    if (parStressScenario.irCurveParShifts) {
        zeroStressScenario.discountCurveShifts.clear();
        zeroStressScenario.indexCurveShifts.clear();
        zeroStressScenario.yieldCurveShifts.clear();
    } else {
        excludedParRates.insert(RiskFactorKey::KeyType::DiscountCurve);
        excludedParRates.insert(RiskFactorKey::KeyType::YieldCurve);
        excludedParRates.insert(RiskFactorKey::KeyType::IndexCurve);
    }

    zeroStressScenario.label = parStressScenario.label;
    simMarket->reset();

    const auto& connectedComponents = sensiGraph.orderedRiskFactors();
    DLOG("ParToZeroScenario: Found " << connectedComponents.size() << " connected components");
    auto trialScenario = simMarket->baseScenario()->clone();
    std::map<std::string, std::vector<RiskFactorKey>> capFloorRiskFactors;
    std::vector<RiskFactorKey> curveRiskFactors;
    std::map<RiskFactorKey, double> fairRates;
    std::map<RiskFactorKey, double> baseScenarioValues;
    std::map<RiskFactorKey, double> targets;
    // Optimize IR curves and Credit curves
    for (const auto& component : connectedComponents) {
        if (component.keytype == RiskFactorKey::KeyType::DiscountCurve ||
            component.keytype == RiskFactorKey::KeyType::YieldCurve ||
            component.keytype == RiskFactorKey::KeyType::IndexCurve ||
            component.keytype == RiskFactorKey::KeyType::SurvivalProbability) {
            curveRiskFactors.push_back(component);
            auto parInstrument = instruments.parHelpers_.at(component);
            const double fairRate = impliedQuote(parInstrument);
            const double target = fairRate + getCurveStressShift(component, parStressScenario);
            const double baseScenarioValue = simMarket->baseScenario()->get(component);
            fairRates[component] = fairRate;
            targets[component] = target;
            baseScenarioValues[component] = baseScenarioValue;
        } else if (component.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
            // Collet Optionlet Volatilties and process later
            capFloorRiskFactors[component.name].push_back(component);
            
        }
    }

    for (const auto& component : curveRiskFactors) {
            LOG("Find zero shift for parRate " << component);
            // std::cout << "Find zero shift for parRate " << component << std::endl;
            auto parInstrument = instruments.parHelpers_.at(component);
            const double ttm = computeMaturityTimeFromRiskFactor(asof, component, simMarket, simMarketParameters);
            const double target = targets[component];
            const double fairRate = fairRates[component];
            const double baseScenarioValue = baseScenarioValues[component];
            DLOG("Par Key " << component << "Fair Par Rate " << fairRate << " Target " << target);
            DLOG("Zero Key " << component << " " << baseScenarioValue << " " << ttm);
            auto targetFunction = [&target, &parInstrument, &component, &trialScenario, &simMarket](double x) {
                trialScenario->add(component, x);
                simMarket->applyScenario(trialScenario);
                return (impliedQuote(parInstrument) - target) * 1e6;
            };
            double targetDf;
            Brent brent;
            try {
                targetDf = brent.solve(targetFunction, 1e-8, 1.0, 1e-8, 10.0);
                trialScenario->add(component, targetDf);
            } catch (const std::exception& e) {
                trialScenario->add(component, simMarket->baseScenario()->get(component));
                std::cout << " Error: " << component << " : " << e.what();
            }

            LOG("ParToZeroScenario: Mean-squared-error: " << targetFunction(targetDf));

            double zeroShift = getCurveStressShift(component, parStressScenario);
            if (!useSpreadedTermstructure) {
                zeroShift = -std::log(targetDf / baseScenarioValue) / ttm;
            } else {
                zeroShift = -std::log(targetDf) / ttm;
            }

            DLOG("zerokey;solution;zeroBaseValue;time;shift");
            DLOG(component << ";" << targetDf << ";" << baseScenarioValue << ";" << ttm << ";" << zeroShift);
            updateTargetStressTestScenarioData(zeroStressScenario, component, zeroShift, simMarketParameters);
            // Convert result into a zero shift
        }
        /*
        // Optimize OptionletSurface seperately after the yield curves are set, we have one scenario is applied to all
        strikes; for (const auto& [surfaceName, rfKeys] : capFloorRiskFactors) {

            auto capFloorVolShiftDataIt = sensiData->capFloorVolShiftData().find(surfaceName);
            // We have 1 stress scenario for all strikes


            if (capFloorVolShiftDataIt != sensiData->capFloorVolShiftData().end()) {

                auto cfSensiData = capFloorVolShiftDataIt->second;
                const size_t nStrikes = cfSensiData->shiftStrikes.size();
                std::map<RiskFactorKey, std::vector<RiskFactorKey>> stressToSensiKeys;
                std::vector<double> goal;
                for (size_t i = 0; i < cfSensiData->shiftExpiries.size(); ++i) {
                    RiskFactorKey stressKey(RiskFactorKey::KeyType::OptionletVolatility, surfaceName, i);
                    for (size_t j = 0; j < nStrikes; ++j) {
                        auto index = i * nStrikes + j;
                        RiskFactorKey simMarketKey(RiskFactorKey::KeyType::OptionletVolatility, surfaceName, index);
                        QL_REQUIRE(std::find(rfKeys.begin(), rfKeys.end(), simMarketKey) != rfKeys.end(), "Check
        config"); stressToSensiKeys[stressKey].push_back(simMarketKey);


                        goal.push_back();
                        results.targetParQuote[simMarketKey] = parVol;
                            if (auto it = scenario.capVolShifts.find(stressKey.name);

                        }
                }
                }



            } else {
                // TODO warning
                std::cout << "Warnung couldnt find volshiftdata for surface " << surfaceName << " Please check your
        config."
                          << std::endl;
            }
        }*/
        simMarket->reset();
        simMarket->applyScenario(trialScenario);
        DLOG("key;fairrate;target;error");
        for (const auto& [key, parHelper] : instruments.parHelpers_) {
            double tgt = targets[key];
            double rate = impliedQuote(parHelper);
            DLOG(key << ";" << rate << ";" << tgt << ";" << tgt - rate);
        }
        LOG("Finished Scenario conversion");
        zeroStressScenario.irCurveParShifts = false;
        zeroStressScenario.creditCurveParShifts = false;
        zeroStressScenario.irCapFloorParShifts = false;
        return zeroStressScenario;
    }

QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> convertParScenarioToZeroScenarioData(
    const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParameters,
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressTestData,
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiData,
    const std::map<std::pair<ore::analytics::RiskFactorKey, ore::analytics::RiskFactorKey>, double>& parSensitivities) {

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
   
    LOG("ParToZeroScenario: Build ParInstruments");
    ParSensitivityInstrumentBuilder::Instruments instruments;
    ParSensitivityInstrumentBuilder().createParInstruments(
        instruments, asof, simMarketParameters, *sensiData, {},
        {RiskFactorKey::KeyType::DiscountCurve, RiskFactorKey::KeyType::YieldCurve, RiskFactorKey::KeyType::IndexCurve, RiskFactorKey::KeyType::SurvivalProbability},
        {}, false, Market::defaultConfiguration, simMarket);

    simMarket->reset();

    DLOG("Build sensitivity graph")
    SimpleRiskFactorGraph sensiGraph(parSensitivities);

    DLOG("ParToZeroScenario: Begin Stress Scenarios conversion");
    QuantLib::ext::shared_ptr<StressTestScenarioData> results = QuantLib::ext::make_shared<StressTestScenarioData>();
    results->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
    for (const StressTestScenarioData::StressTestData& scenario : stressTestData->data()) {
        if (!scenario.containsParShifts()) {
            LOG("ParToZeroScenario: SKIP stress scenario " << scenario.label << ". It doesn't contain any par shifts");
            results->data().push_back(scenario);
        } else {
            results->data().push_back(convertScenario(scenario, asof, instruments, sensiGraph, simMarketParameters,
                                                      simMarket, stressTestData->useSpreadedTermStructures()));
        }
    }
    results->useSpreadedTermStructures() = stressTestData->useSpreadedTermStructures();
    results->toFile("./stressTest_zero.xml");
    return results;
}

} // namespace analytics

} // namespace ore