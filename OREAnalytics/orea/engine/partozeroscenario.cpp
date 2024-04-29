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

QuantLib::Period getYieldCurvePeriod(const RiskFactorKey& rfKey,
                                     const boost::shared_ptr<ScenarioSimMarketParameters>& params) {
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
        tenor = params->defaultTenors(rfKey.name)[rfKey.index];
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
    case RiskFactorKey::KeyType::DiscountCurve: {
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
    case RiskFactorKey::KeyType::IndexCurve: {
        auto it = stressScenario.indexCurveShifts.find(key.name);
        if (it != stressScenario.indexCurveShifts.end()) {
            shifts = &(it->second.shifts);
        }
        break;
    }
    case RiskFactorKey::KeyType::SurvivalProbability: {
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

double getCapFloorStressShift(const RiskFactorKey& key, const StressTestScenarioData::StressTestData& stressScenario,
                              const boost::shared_ptr<ScenarioSimMarketParameters>& params) {
    double shift = 0.0;
    auto it = stressScenario.capVolShifts.find(key.name);
    if (it != stressScenario.capVolShifts.end()) {
        const auto& cfData = it->second;
        size_t nStrikes = params->capFloorVolStrikes(key.name).size();
        size_t n = key.index;
        size_t tenorId = n / nStrikes;
        size_t strikeId = n % nStrikes;
        const Period& tenor = cfData.shiftExpiries[tenorId];
        if (cfData.shiftStrikes.empty()) {
            shift = cfData.shifts.at(tenor).front();
        } else {
            shift = cfData.shifts.at(tenor)[strikeId];
        }
    }
    return shift;
}

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
                                        const RiskFactorKey& key, const double zeroShift,
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
    } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        if (stressScenario.capVolShifts.count(key.name) == 0) {
            StressTestScenarioData::CapFloorVolShiftData newData;
            newData.shiftType = ShiftType::Absolute;
            newData.shiftExpiries = simMarketParameters->capFloorVolExpiries(key.name);
            newData.shiftStrikes = simMarketParameters->capFloorVolStrikes(key.name);
            for (size_t i = 0; i < newData.shiftExpiries.size(); ++i) {
                newData.shifts[newData.shiftExpiries[i]] = std::vector<double>(newData.shiftStrikes.size(), 0.0);
            }
            const size_t nStrikes = newData.shiftStrikes.size();
            const size_t expiryId = key.index / nStrikes;
            const size_t strikeId = key.index % nStrikes;
            newData.shifts[newData.shiftExpiries[expiryId]][strikeId] = zeroShift;
            stressScenario.capVolShifts.insert({key.name, newData});
        } else {
            StressTestScenarioData::CapFloorVolShiftData& newData = stressScenario.capVolShifts.at(key.name);
            const size_t nStrikes = newData.shiftStrikes.size();
            const size_t expiryId = key.index / nStrikes;
            const size_t strikeId = key.index % nStrikes;
            newData.shifts[newData.shiftExpiries[expiryId]][strikeId] = zeroShift;
        }
    }
}

ore::analytics::StressTestScenarioData::StressTestData
convertScenario(const ore::analytics::StressTestScenarioData::StressTestData& parStressScenario,
                const QuantLib::Date& asof, const ParSensitivityInstrumentBuilder::Instruments& instruments,
                const SimpleRiskFactorGraph& sensiGraph,
                const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParameters,
                const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket, bool useSpreadedTermstructure) {
    LOG("ParToZeroScenario: converting parshifts to zero shifts in scenario " << parStressScenario.label);
    // Copy scenario
    StressTestScenarioData::StressTestData zeroStressScenario = parStressScenario;

    std::set<RiskFactorKey::KeyType> excludedParRates;

    if (parStressScenario.irCapFloorParShifts) {
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
    std::vector<RiskFactorKey> capFloorRiskFactors;
    std::vector<RiskFactorKey> curveRiskFactors;
    std::map<RiskFactorKey, double> fairRates;
    std::map<RiskFactorKey, double> baseScenarioValues;
    std::map<RiskFactorKey, double> targets;
    // Optimize IR curves and Credit curves
    for (const auto& component : connectedComponents) {
        if(excludedParRates.count(component.keytype) == 1){
            DLOG("Skip " << component << ", since " << component.keytype << " are zero shifts");
        } else if (component.keytype == RiskFactorKey::KeyType::DiscountCurve ||
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
            const double fairRate = impliedCapVolatility(component, instruments);
            const double target = fairRate + getCapFloorStressShift(component, parStressScenario, simMarketParameters);
            const double baseScenarioValue = simMarket->baseScenario()->get(component);
            fairRates[component] = fairRate;
            targets[component] = target;
            baseScenarioValues[component] = baseScenarioValue;
            DLOG(component << " Implied Vol" << fairRate << " target Vol: " << target << "baseValue "
                           << baseScenarioValue);
            capFloorRiskFactors.push_back(component);
        }
    }

    for (const auto& component : curveRiskFactors) {
        LOG("Find zero shift for parRate " << component);
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
    simMarket->reset();
    simMarket->applyScenario(trialScenario);
    DLOG("key;fairrate;target;error");
    for (const auto& [key, parHelper] : instruments.parHelpers_) {
        double tgt = targets[key];
        double rate = impliedQuote(parHelper);
        DLOG(key << ";" << rate << ";" << tgt << ";" << tgt - rate);
    }
    simMarket->reset();

    for (const auto& component : capFloorRiskFactors) {
        LOG("Find zero shift for capFloor " << component);
        const double target = targets[component];
        const double fairRate = fairRates[component];
        const double baseScenarioValue = baseScenarioValues[component];
        DLOG("Par Key " << component << "Fair Par Rate " << fairRate << " Target " << target);
        DLOG("Zero Key " << component << " " << baseScenarioValue);

        auto targetFunction = [&target, &instruments, &component, &trialScenario, &simMarket](double x) {
            trialScenario->add(component, x);
            simMarket->applyScenario(trialScenario);
            return (impliedCapVolatility(component, instruments) - target);
        };

        double targetVol;
        Brent brent;
        try {
            targetVol = brent.solve(targetFunction, 1e-8, 0, -simMarket->baseScenarioAbsolute()->get(component),
                                    4 * simMarket->baseScenarioAbsolute()->get(component));
            trialScenario->add(component, targetVol);
        } catch (std::exception& e) {
            trialScenario->add(component, simMarket->baseScenario()->get(component));
        }
        simMarket->applyScenario(trialScenario);
        double zeroShift = 0.0;
        if (!useSpreadedTermstructure) {
            zeroShift = targetVol - baseScenarioValue;
        } else {
            zeroShift = targetVol;
        }
        DLOG("key;solution;zeroBaseValue;time;shift");
        DLOG(component << ";" << targetVol << ";" << baseScenarioValue << ";"
                       << ";" << zeroShift);
        updateTargetStressTestScenarioData(zeroStressScenario, component, zeroShift, simMarketParameters);
        // Convert result into a zero shift
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
    const std::map<std::pair<ore::analytics::RiskFactorKey, ore::analytics::RiskFactorKey>, double>& parSensitivities,
    const ParSensitivityInstrumentBuilder::Instruments& instruments) {

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
                    LOG(shiftData.shiftTenors[i] << ";" << it->second->shiftTenors[i] << ";"
                                                 << simMarketParameters->yieldCurveTenors(name)[i]);
                    QL_REQUIRE(shiftData.shiftTenors[i] == it->second->shiftTenors[i], "");
                }
            }
            for (const auto& [name, shiftData] : scenario.yieldCurveShifts) {
                auto it = sensiData->yieldCurveShiftData().find(name);
                QL_REQUIRE(it != sensiData->yieldCurveShiftData().end(), "Couldnt find discountCurveSensiShiftData");
                QL_REQUIRE(shiftData.shiftTenors.size() == it->second->shiftTenors.size(), "Mismatch of sizes");
                LOG("Debug discount shifts " << name);
                LOG("StressShiftTenor;SensiShiftTenor;SimulationShiftTenor");
                for (size_t i = 0; i < shiftData.shiftTenors.size(); ++i) {
                    LOG(shiftData.shiftTenors[i] << ";" << it->second->shiftTenors[i] << ";"
                                                 << simMarketParameters->yieldCurveTenors(name)[i]);
                    QL_REQUIRE(shiftData.shiftTenors[i] == it->second->shiftTenors[i], "");
                }
            }
        }
    }
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