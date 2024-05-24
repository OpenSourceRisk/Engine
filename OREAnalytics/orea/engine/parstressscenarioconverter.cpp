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

#include <orea/engine/parsensitivityutilities.hpp>
#include <orea/engine/parstressscenarioconverter.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>

namespace ore {
namespace analytics {

// Utility functions
namespace {

const static std::set<RiskFactorKey::KeyType> supportedCurveShiftTypes = {
    RiskFactorKey::KeyType::DiscountCurve, RiskFactorKey::KeyType::YieldCurve, RiskFactorKey::KeyType::IndexCurve,
    RiskFactorKey::KeyType::SurvivalProbability};


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

//! Creates a copy from the parStressScenario and delete all par shifts  but keeps all zeroShifts
StressTestScenarioData::StressTestData
removeParShiftsCopy(const StressTestScenarioData::StressTestData& parStressScenario) {
    StressTestScenarioData::StressTestData zeroStressScenario = parStressScenario;
    if (parStressScenario.irCapFloorParShifts) {
        zeroStressScenario.capVolShifts.clear();
    }
    if (parStressScenario.creditCurveParShifts) {
        zeroStressScenario.survivalProbabilityShifts.clear();
    }
    if (parStressScenario.irCurveParShifts) {
        zeroStressScenario.discountCurveShifts.clear();
        zeroStressScenario.indexCurveShifts.clear();
        zeroStressScenario.yieldCurveShifts.clear();
    }
    zeroStressScenario.irCapFloorParShifts = false;
    zeroStressScenario.irCurveParShifts = false;
    zeroStressScenario.creditCurveParShifts = false;
    return zeroStressScenario;
}
} // namespace

std::set<RiskFactorKey::KeyType> disabledParRates(bool irCurveParRates, bool irCapFloorParRates, bool creditParRates) {
    set<RiskFactorKey::KeyType> disabledParRates;
    if (!irCurveParRates) {
        disabledParRates.insert(RiskFactorKey::KeyType::DiscountCurve);
        disabledParRates.insert(RiskFactorKey::KeyType::YieldCurve);
        disabledParRates.insert(RiskFactorKey::KeyType::IndexCurve);
    }
    if (!irCapFloorParRates) {
        disabledParRates.insert(RiskFactorKey::KeyType::OptionletVolatility);
    }
    if (!creditParRates) {
        disabledParRates.insert(RiskFactorKey::KeyType::SurvivalProbability);
    }
    return disabledParRates;
}

//! Checks the the tenors for curves in a stresstest scenario are alligned with par sensitivity config
bool checkCurveShiftData(const std::string& name, const StressTestScenarioData::CurveShiftData& stressShiftData,
                         const std::map<std::string, QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& sensiData) {
    auto it = sensiData.find(name);
    if (it == sensiData.end()) {
        StructuredConfigurationWarningMessage(
            "StressScenario", name, "Par Shift to zero conversion",
            "no par sensitivity scenario found. Please add par sensi config").log();
        return false;
    }
    auto parShiftData = ext::dynamic_pointer_cast<SensitivityScenarioData::CurveShiftParData>(it->second);
    if (parShiftData == nullptr) {
        StructuredConfigurationWarningMessage("StressScenario", name, "Par Shift to zero conversion",
                                              "no par sensitivity scenario found. Please add par sensi config")
            .log();
        return false;
    }
    const size_t nParShifts = parShiftData->shiftTenors.size();
    const size_t nStressShifts = stressShiftData.shiftTenors.size();
    if (nParShifts != nStressShifts) {
        StructuredConfigurationWarningMessage(
            "StressScenario", name, "Par Shift to zero conversion",
            "mismatch between tenors, we have " + to_string(nParShifts) + " parInstruments defined but " +
                to_string(nStressShifts) +
                " shifts in the scenario. Please align pillars of stress test and par sensi config")
            .log();
        return false;
    }
    for (size_t i = 0; i < nParShifts; ++i){
        if (parShiftData->shiftTenors[i] != stressShiftData.shiftTenors[i]){
            StructuredConfigurationWarningMessage("StressScenario", name, "Par Shift to zero conversion",
                                                  "tenors are not aligned, " + to_string(i) + " par Pillar is " +
                                                      to_string(parShiftData->shiftTenors[i]) +
                                                      " vs stress shift piller " +
                                                      to_string(stressShiftData.shiftTenors[i]) +
                                                      ". Please align pillars of stress test and par sensi config")
                .log();
            return false;
        }
    }
    return true;
}


//! Checks the the strikes and expiries of cap floors in stresstest scenario are alligned with par sensitivity config
bool checkCapFloorShiftData(const std::string& name, const StressTestScenarioData::CapFloorVolShiftData& stressShiftData,
                         const std::map<std::string, QuantLib::ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& sensiData){

    auto it = sensiData.find(name);
    if (it == sensiData.end()) {
        StructuredConfigurationWarningMessage(
            "StressScenario", name, "Par Shift to zero conversion",
            "no par cap floor sensitivity scenario found. Please add par sensi config")
            .log();
        return false;
    }
    auto parShiftData = ext::dynamic_pointer_cast<SensitivityScenarioData::CapFloorVolShiftParData>(it->second);
    if (parShiftData == nullptr) {
        StructuredConfigurationWarningMessage(
            "StressScenario", name, "Par Shift to zero conversion",
            "no par cap floor sensitivity scenario found. Please add par sensi config")
            .log();
        return false;
    }
    const size_t nParShifts = parShiftData->shiftExpiries.size();
    const size_t nStressShifts = stressShiftData.shiftExpiries.size();
    if (nParShifts != nStressShifts) {
        StructuredConfigurationWarningMessage(
            "StressScenario", name, "Par Shift to zero conversion",
            "mismatch between capFloor expiries, we have " + to_string(nParShifts) + " parInstruments defined but " +
                to_string(nStressShifts) +
                " shifts in the scenario. Please align pillars of stress test and par sensi config")
            .log();
        return false;
    }
    for (size_t i = 0; i < nParShifts; ++i) {
        if (parShiftData->shiftExpiries[i] != stressShiftData.shiftExpiries[i]) {
            StructuredConfigurationWarningMessage(
                "StressScenario", name, "Par Shift to zero conversion",
                "CapFloor expiries are not aligned, " + to_string(i) + " CapFloor Pillar is " +
                    to_string(parShiftData->shiftExpiries[i]) + " vs stress shift piller " +
                    to_string(stressShiftData.shiftExpiries[i]) +
                    ". Please align pillars of stress test and par sensi config")
                .log();
            return false;
        }
    }
    if(!stressShiftData.shiftStrikes.empty()){
        const size_t nParStrikes = parShiftData->shiftStrikes.size();
        const size_t nSressStrikes = stressShiftData.shiftStrikes.size();

        if (nParStrikes != nSressStrikes) {
            StructuredConfigurationWarningMessage(
                "StressScenario", name, "Par Shift to zero conversion",
                "mismatch between capFloor strikes, we have " + to_string(nParStrikes) + " par strikes defined but " +
                    to_string(nSressStrikes) +
                    " strikes in the scenario. Please align strikes of stress test and par sensi config")
                .log();
            return false;
        }
        for (size_t i = 0; i < nParStrikes; ++i) {
            if (parShiftData->shiftStrikes[i] != stressShiftData.shiftStrikes[i]) {
                StructuredConfigurationWarningMessage(
                    "StressScenario", name, "Par Shift to zero conversion",
                    "CapFloor expiries are not aligned, " + to_string(i) + " CapFloor strike is " +
                        to_string(parShiftData->shiftStrikes[i]) + " vs stress shift strike " +
                        to_string(stressShiftData.shiftStrikes[i]) +
                        ". Please align strikes of stress test and par sensi config")
                    .log();
                return false;
            }
        }
    }
    return true;
}

//! Check if the scenarios defines a shift for each par rate defined in the sensitivityScenarioData;
bool ParStressScenarioConverter::scenarioCanBeConverted(
    const StressTestScenarioData::StressTestData& parStressScenario) const {
    DLOG("Check if the par stresstest scenario is compatible with the parInstruments");
    bool result = true;
    if (parStressScenario.irCurveParShifts) {
        // We require that shifts between stress scenario and sensi scenario are aligned
        for (const auto& [ccy, curveShifts] : parStressScenario.discountCurveShifts) {
            DLOG("Check if pillars between stress test and sensi config are alligned for discount curve " << ccy);
            result = result && checkCurveShiftData(ccy, curveShifts, sensiScenarioData_->discountCurveShiftData());
        }

        for (const auto& [indexName, curveShifts] : parStressScenario.indexCurveShifts) {
            DLOG("Check if pillars between stress test and sensi config are alligned for index curve " << indexName);
            result = result && checkCurveShiftData(indexName, curveShifts, sensiScenarioData_->indexCurveShiftData());
        }

        for (const auto& [curveName, curveShifts] : parStressScenario.yieldCurveShifts) {
            DLOG("Check if pillars between stress test and sensi config are alligned for yield curve " << curveName);
            result = result && checkCurveShiftData(curveName, curveShifts, sensiScenarioData_->yieldCurveShiftData());
        }
    }

    if (parStressScenario.creditCurveParShifts) {
        for (const auto& [curveName, curveShifts] : parStressScenario.survivalProbabilityShifts) {
            DLOG("Check if pillars between stress test and sensi config are alligned for credit curve " << curveName);
            result = result && checkCurveShiftData(curveName, curveShifts, sensiScenarioData_->creditCurveShiftData());
        }
    }

    if (parStressScenario.irCapFloorParShifts) {
        for (const auto& [capSurfaceName, capShifts] : parStressScenario.capVolShifts) {
            DLOG("Check if pillars and strikes between stress test and sensi config are alligned for cap floor surface "
                 << capSurfaceName);
            result =
                result && checkCapFloorShiftData(capSurfaceName, capShifts, sensiScenarioData_->capFloorVolShiftData());
        }
    }

    return result;
}

ParStressScenarioConverter::ParStressScenarioConverter(
    const QuantLib::Date& asof, const std::vector<RiskFactorKey>& sortedParInstrumentRiskFactorKeys,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
    const ore::analytics::ParSensitivityInstrumentBuilder::Instruments& parInstruments, bool useSpreadedTermStructure)
    : asof_(asof), sortedParInstrumentRiskFactorKeys_(sortedParInstrumentRiskFactorKeys),
      simMarketParams_(simMarketParams), sensiScenarioData_(sensiScenarioData), simMarket_(simMarket),
      parInstruments_(parInstruments), useSpreadedTermStructure_(useSpreadedTermStructure) {}

ore::analytics::StressTestScenarioData::StressTestData
ParStressScenarioConverter::convertScenario(const StressTestScenarioData::StressTestData& parStressScenario) const {

    if (!parStressScenario.containsParShifts()) {
        return parStressScenario;
    }

    if (!scenarioCanBeConverted(parStressScenario)) {
        WLOG("Can not convert scenario " << parStressScenario.label << " Skip it and apply all shifts as zero shifts.");
        return parStressScenario;
    }

    simMarket_->reset();

    LOG("ParStressConverter: Scenario " << parStressScenario.label << " has IR Curve Par Shifts = "
                                        << ore::data::to_string(parStressScenario.irCurveParShifts));

    LOG("ParStressConverter: Scenario " << parStressScenario.label << " has CapFloor Par Shifts = "
                                        << ore::data::to_string(parStressScenario.irCapFloorParShifts));

    LOG("ParStressConverter: Scenario " << parStressScenario.label << " has Credit Par Shifts = "
                                        << ore::data::to_string(parStressScenario.creditCurveParShifts));

    std::set<RiskFactorKey::KeyType> excludedParRates =
        disabledParRates(parStressScenario.irCurveParShifts, parStressScenario.irCapFloorParShifts,
                         parStressScenario.creditCurveParShifts);

    LOG("ParStressConverter: Copy scenario and remove parShifts from scenario");
    auto zeroStressScenario = removeParShiftsCopy(parStressScenario);
    DLOG("ParStressConverter: Clone base scenario");
    auto zeroSimMarketScenario = simMarket_->baseScenario()->clone();

    // Derive t0 (unshifted) fair Rates
    std::vector<RiskFactorKey> relevantParKeys;
    std::map<RiskFactorKey, double> fairRates;
    std::map<RiskFactorKey, double> baseScenarioValues;
    std::map<RiskFactorKey, double> targets;
    LOG("ParStressConverter: Compute fair rate and target rate for all ParInstruments");
    for (const auto& rfKey : sortedParInstrumentRiskFactorKeys_) {
        if (excludedParRates.count(rfKey.keytype) == 0 &&
            (supportedCurveShiftTypes.count(rfKey.keytype) == 1 ||
             rfKey.keytype == RiskFactorKey::KeyType::OptionletVolatility)) {
            relevantParKeys.push_back(rfKey);
            const double fairRate = impliedParRate(rfKey);
            // TODO get shift type;
            const double target =
                computeTargetRate(fairRate, getStressShift(rfKey, parStressScenario), ShiftType::Absolute);
            const double baseScenarioValue = simMarket_->baseScenario()->get(rfKey);
            const double baseScenarioAbsoluteValue = simMarket_->baseScenarioAbsolute()->get(rfKey);
            DLOG("ParStressConverter:  ParInstrument "
                 << rfKey << ", fair rate = " << fairRate << ", target rate = " << target << ", baseScenarioValue  = "
                 << baseScenarioValue << ", baseScenarioAbsoluteValue = " << baseScenarioAbsoluteValue);
            fairRates[rfKey] = fairRate;
            targets[rfKey] = target;
            baseScenarioValues[rfKey] = baseScenarioValue;
        } else {
            DLOG("Skip parInsturment " << rfKey << " the shifts for this risk factor type are in zero domain.");
        }
    }
    // Optimize IR curves and Credit curves
    LOG("ParStressConverter: Imply zero shifts");
    std::map<RiskFactorKey, double> shifts;
    for (const auto& rfKey : relevantParKeys) {
        DLOG("Imply zero shifts for parInstrument " << rfKey);
        const double target = targets[rfKey];
        auto targetFunction = [this, &target, &rfKey, &zeroSimMarketScenario](double x) {
            zeroSimMarketScenario->add(rfKey, x);
            simMarket_->applyScenario(zeroSimMarketScenario);
            return (impliedParRate(rfKey) - target) * 1e6;
        };
        double targetValue;
        Brent brent;
        try {
            DLOG("ParStressConverter: Try to imply zero rate" << rfKey << " with bounds << " << lowerBound(rfKey) << ","
                                                              << upperBound(rfKey));
            targetValue =
                brent.solve(targetFunction, accuracy_, baseScenarioValues[rfKey], lowerBound(rfKey), upperBound(rfKey));
        } catch (const std::exception& e) {
            ALOG("ParStressConverter: Couldn't find a solution to imply a zero rate for parRate " << rfKey << ", got "
                                                                                                  << e.what());
            targetValue = simMarket_->baseScenario()->get(rfKey);
        }
        zeroSimMarketScenario->add(rfKey, targetValue);
        shifts[rfKey] = shiftsSizeForScenario(rfKey, targetValue, baseScenarioValues[rfKey]);
        updateTargetStressTestScenarioData(zeroStressScenario, rfKey, shifts[rfKey]);
    }
    simMarket_->applyScenario(zeroSimMarketScenario);
    DLOG("ParStressConverter: Implied Scenario");
    DLOG("parInstrument;fairRate;targetFairRate;zeroBaseValue;shift");
    for (const auto& rfKey : relevantParKeys) {
        DLOG(rfKey << ";" << impliedParRate(rfKey) << ";" << targets[rfKey] << ";" << baseScenarioValues[rfKey] << ";"
                   << shifts[rfKey]);
    }
    simMarket_->reset();
    return zeroStressScenario;
}

double ParStressScenarioConverter::maturityTime(const RiskFactorKey& rfKey) const {
    boost::shared_ptr<QuantLib::TermStructure> ts;
    QuantLib::Period tenor;
    switch (rfKey.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:
        ts = *simMarket_->discountCurve(rfKey.name);
        tenor = getYieldCurvePeriod(rfKey, simMarketParams_);
        break;
    case RiskFactorKey::KeyType::YieldCurve:
        ts = *simMarket_->yieldCurve(rfKey.name);
        tenor = getYieldCurvePeriod(rfKey, simMarketParams_);
        break;
    case RiskFactorKey::KeyType::IndexCurve:
        ts = *simMarket_->iborIndex(rfKey.name)->forwardingTermStructure();
        tenor = getYieldCurvePeriod(rfKey, simMarketParams_);
        break;
    case RiskFactorKey::KeyType::SurvivalProbability:
        ts = *simMarket_->defaultCurve(rfKey.name)->curve();
        QL_REQUIRE(rfKey.index < simMarketParams_->defaultTenors(rfKey.name).size(),
                   "Please align pillars, internal error");
        tenor = simMarketParams_->defaultTenors(rfKey.name)[rfKey.index];
        break;
    case RiskFactorKey::KeyType::OptionletVolatility: {
        ts = *simMarket_->capFloorVol(rfKey.name);
        auto [tenorId, strikeId] = getCapFloorTenorAndStrikeIds(rfKey);
        QL_REQUIRE(
            simMarketParams_->capFloorVolExpiries(rfKey.name).size() > tenorId,
            "Internal Error: ParStressScenarioConversion, simmarket and par sensitivity instruments are not aligned.");
        tenor = simMarketParams_->capFloorVolExpiries(rfKey.name)[tenorId];
    }
    default:
        QL_FAIL("ParStressScenario to ZeroConversion: Unsupported riskfactor, can not compute time to maturity "
                "from curve");
    }
    return ts->dayCounter().yearFraction(asof_, asof_ + tenor);
}

std::pair<size_t, size_t> ParStressScenarioConverter::getCapFloorTenorAndStrikeIds(const RiskFactorKey& rfKey) const {
    size_t nStrikes = simMarketParams_->capFloorVolStrikes(rfKey.name).size();
    size_t n = rfKey.index;
    size_t tenorId = n / nStrikes;
    size_t strikeId = n % nStrikes;
    return {tenorId, strikeId};
}

double ParStressScenarioConverter::shiftsSizeForScenario(const RiskFactorKey rfKey, double targetValue,
                                                         double baseValue) const {
    DLOG("compute shift for" << rfKey << " targetZeroValue " << targetValue << " baseValue " << baseValue);
    double shift = 0.0;
    switch (rfKey.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:
    case RiskFactorKey::KeyType::YieldCurve:
    case RiskFactorKey::KeyType::IndexCurve:
    case RiskFactorKey::KeyType::SurvivalProbability: {
        double ttm = maturityTime(rfKey);
        DLOG("TTM " << ttm);
        if (!useSpreadedTermStructure_) {
            shift = -std::log(targetValue / baseValue) / ttm;
        } else {
            shift = -std::log(targetValue) / ttm;
        }
        DLOG("Shift = " << shift);
        break;
    }
    case RiskFactorKey::KeyType::OptionletVolatility:
        if (!useSpreadedTermStructure_) {
            shift = targetValue - baseValue;
        } else {
            shift = targetValue;
        };
        break;
    default:
        QL_FAIL("ShiftSizeForScenario: Unsupported par instruments type " << rfKey.keytype);
    }
    return shift;
}

double ParStressScenarioConverter::impliedParRate(const RiskFactorKey& key) const {
    if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return impliedVolatility(key, parInstruments_);
    } else if (supportedCurveShiftTypes.count(key.keytype) == 1) {
        auto parInstIt = parInstruments_.parHelpers_.find(key);
        QL_REQUIRE(parInstIt != parInstruments_.parHelpers_.end(),
                   "Internal error, trying to compute parRate but havent build parRateHelper");
        return impliedQuote(parInstIt->second);
    } else {
        QL_FAIL("Unsupported parRate");
    }
}

double ParStressScenarioConverter::getStressShift(const RiskFactorKey& key,
                                                  const StressTestScenarioData::StressTestData& stressScenario) const {
    if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return getCapFloorStressShift(key, stressScenario, simMarketParams_);
    } else {
        return getCurveStressShift(key, stressScenario);
    }
}

void ParStressScenarioConverter::updateTargetStressTestScenarioData(
    StressTestScenarioData::StressTestData& stressScenario, const RiskFactorKey& key, const double zeroShift) const {
    if (key.keytype == RiskFactorKey::KeyType::DiscountCurve) {
        if (stressScenario.discountCurveShifts.count(key.name) == 0) {
            StressTestScenarioData::CurveShiftData newData;
            newData.shiftType = ShiftType::Absolute;
            newData.shiftTenors = simMarketParams_->yieldCurveTenors(key.name);
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
            newData.shiftTenors = simMarketParams_->yieldCurveTenors(key.name);
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
            newData.shiftTenors = simMarketParams_->defaultTenors(key.name);
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
            newData.shiftExpiries = simMarketParams_->capFloorVolExpiries(key.name);
            newData.shiftStrikes = simMarketParams_->capFloorVolStrikes(key.name);
            for (size_t i = 0; i < newData.shiftExpiries.size(); ++i) {
                newData.shifts[newData.shiftExpiries[i]] = std::vector<double>(newData.shiftStrikes.size(), 0.0);
            }
            const auto& [expiryId, strikeId] = getCapFloorTenorAndStrikeIds(key);
            newData.shifts[newData.shiftExpiries[expiryId]][strikeId] = zeroShift;
            stressScenario.capVolShifts.insert({key.name, newData});
        } else {
            StressTestScenarioData::CapFloorVolShiftData& newData = stressScenario.capVolShifts.at(key.name);
            const auto& [expiryId, strikeId] = getCapFloorTenorAndStrikeIds(key);
            newData.shifts[newData.shiftExpiries[expiryId]][strikeId] = zeroShift;
        }
    }
}

double ParStressScenarioConverter::lowerBound(const RiskFactorKey key) const {
    if (useSpreadedTermStructure_ && key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return minVol_ - simMarket_->baseScenarioAbsolute()->get(key);
    } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return minVol_;
    } else if (useSpreadedTermStructure_ && (key.keytype == RiskFactorKey::KeyType::DiscountCurve ||
                                             key.keytype == RiskFactorKey::KeyType::YieldCurve ||
                                             key.keytype == RiskFactorKey::KeyType::IndexCurve ||
                                             key.keytype == RiskFactorKey::KeyType::SurvivalProbability)) {
        return minDiscountFactor_ / simMarket_->baseScenarioAbsolute()->get(key);
    } else {
        return minDiscountFactor_;
    }
}

double ParStressScenarioConverter::upperBound(const RiskFactorKey key) const {
    if (useSpreadedTermStructure_ && key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return maxVol_ * simMarket_->baseScenarioAbsolute()->get(key);
    } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        return maxVol_ * simMarket_->baseScenarioAbsolute()->get(key);
    } else if (useSpreadedTermStructure_ && (key.keytype == RiskFactorKey::KeyType::DiscountCurve ||
                                             key.keytype == RiskFactorKey::KeyType::YieldCurve ||
                                             key.keytype == RiskFactorKey::KeyType::IndexCurve ||
                                             key.keytype == RiskFactorKey::KeyType::SurvivalProbability)) {
        return maxDiscountFactor_ / simMarket_->baseScenarioAbsolute()->get(key);
    } else {
        return maxDiscountFactor_;
    }
}
} // namespace analytics
} // namespace ore