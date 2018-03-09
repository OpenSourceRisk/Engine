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

#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ostream>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

SensitivityScenarioGenerator::SensitivityScenarioGenerator(
    const boost::shared_ptr<SensitivityScenarioData>& sensitivityData, const boost::shared_ptr<Scenario>& baseScenario,
    const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData, const bool overrideTenors)
    : ShiftScenarioGenerator(baseScenario, simMarketData), sensitivityData_(sensitivityData),
      overrideTenors_(overrideTenors) {
    QL_REQUIRE(sensitivityData_ != NULL, "SensitivityScenarioGenerator: sensitivityData is null");
}

struct findFactor {
    findFactor(const string& factor) : factor_(factor) {}

    string factor_;
    const bool operator()(const std::pair<string, string>& p) const {
        return (p.first == factor_) || (p.second == factor_);
    }
};

struct findPair {
    findPair(const string& first, const string& second) : first_(first), second_(second) {}

    string first_;
    string second_;
    const bool operator()(const std::pair<string, string>& p) const {
        return (p.first == first_ && p.second == second_) || (p.second == first_ && p.first == second_);
    }
};
void SensitivityScenarioGenerator::generateScenarios(const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory) {
    Date asof = baseScenario_->asof();

    generateDiscountCurveScenarios(sensiScenarioFactory, true);
    generateDiscountCurveScenarios(sensiScenarioFactory, false);

    generateIndexCurveScenarios(sensiScenarioFactory, true);
    generateIndexCurveScenarios(sensiScenarioFactory, false);

    generateYieldCurveScenarios(sensiScenarioFactory, true);
    generateYieldCurveScenarios(sensiScenarioFactory, false);

    generateFxScenarios(sensiScenarioFactory, true);
    generateFxScenarios(sensiScenarioFactory, false);

    generateEquityScenarios(sensiScenarioFactory, true);
    generateEquityScenarios(sensiScenarioFactory, false);

    if (simMarketData_->simulateEquityForecastCurve()) {
        generateEquityForecastCurveScenarios(sensiScenarioFactory, true);
        generateEquityForecastCurveScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateDividendYield()) {
        generateDividendYieldScenarios(sensiScenarioFactory, true);
        generateDividendYieldScenarios(sensiScenarioFactory, false);
    }

    generateZeroInflationScenarios(sensiScenarioFactory, true);
    generateZeroInflationScenarios(sensiScenarioFactory, false);

    generateYoYInflationScenarios(sensiScenarioFactory, true);
    generateYoYInflationScenarios(sensiScenarioFactory, false);

    if (simMarketData_->simulateFXVols()) {
        generateFxVolScenarios(sensiScenarioFactory, true);
        generateFxVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateEquityVols()) {
        generateEquityVolScenarios(sensiScenarioFactory, true);
        generateEquityVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateSwapVols()) {
        generateSwaptionVolScenarios(sensiScenarioFactory, true);
        generateSwaptionVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateCapFloorVols()) {
        generateCapFloorVolScenarios(sensiScenarioFactory, true);
        generateCapFloorVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateSurvivalProbabilities()) {
        generateSurvivalProbabilityScenarios(sensiScenarioFactory, true);
        generateSurvivalProbabilityScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateCdsVols()) {
        generateCdsVolScenarios(sensiScenarioFactory, true);
        generateCdsVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateBaseCorrelations()) {
        generateBaseCorrelationScenarios(sensiScenarioFactory, true);
        generateBaseCorrelationScenarios(sensiScenarioFactory, false);
    }

    // add simultaneous up-moves in two risk factors for cross gamma calculation

    // store base scenario values
    vector<RiskFactorKey> keys = baseScenario_->keys();
    vector<Real> baseValues;
    for (auto k : keys) {
        baseValues.push_back(baseScenario_->get(k));
    }

    Size index = scenarios_.size();
    for (Size i = 0; i < index; ++i) {
        ScenarioDescription iDesc = scenarioDescriptions_[i];
        if (iDesc.type() != ScenarioDescription::Type::Up)
            continue;
        string iKeyName = iDesc.keyName1();

        // Check Filters for i
        bool match = (find_if(sensitivityData_->crossGammaFilter().begin(), sensitivityData_->crossGammaFilter().end(),
                              findFactor(iKeyName)) != sensitivityData_->crossGammaFilter().end());

        if (!match)
            continue;

        boost::shared_ptr<Scenario> iScenario = scenarios_[i];
        vector<Real> iValues;
        for (auto k : keys)
            iValues.push_back(iScenario->get(k));

        for (Size j = i + 1; j < index; ++j) {
            ScenarioDescription jDesc = scenarioDescriptions_[j];
            if (jDesc.type() != ScenarioDescription::Type::Up)
                continue;
            // filter desired cross shift combinations
            match = (find_if(sensitivityData_->crossGammaFilter().begin(), sensitivityData_->crossGammaFilter().end(),
                             findPair(iKeyName, jDesc.keyName1())) != sensitivityData_->crossGammaFilter().end());

            if (!match)
                continue;

            boost::shared_ptr<Scenario> crossScenario = sensiScenarioFactory->buildScenario(asof);
            boost::shared_ptr<Scenario> jScenario = scenarios_[j];
            for (Size k = 0; k < keys.size(); k++) {
                Real iValue = iValues[k];
                Real jValue = jScenario->get(keys[k]);
                Real baseValue = baseValues[k];
                if (!close_enough(iValue, baseValue) || !close_enough(jValue, baseValue)) {
                    Real newVal = iValue + jValue - baseValue;
                    crossScenario->add(keys[k], newVal);
                }
            }
            scenarios_.push_back(crossScenario);
            scenarioDescriptions_.push_back(ScenarioDescription(iDesc, jDesc));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << crossScenario->label() << " created");
        }
    }

    // fill keyToFactor and factorToKey maps from scenario descriptions
    LOG("Fill maps linking factors with RiskFactorKeys");
    keyToFactor_.clear();
    factorToKey_.clear();
    for (Size i = 0; i < scenarioDescriptions_.size(); ++i) {
        RiskFactorKey key = scenarioDescriptions_[i].key1();
        string factor = scenarioDescriptions_[i].factor1();
        keyToFactor_[key] = factor;
        factorToKey_[factor] = key;
        LOG("KeyToFactor map: " << key << " to " << factor);
    }

    LOG("sensitivity scenario generator initialised");
}

void SensitivityScenarioGenerator::generateFxScenarios(const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory,
                                                       bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> fxCcyPairs;
    if (sensitivityData_->fxCcyPairs().size() > 0)
        fxCcyPairs = sensitivityData_->fxCcyPairs();
    else
        fxCcyPairs = simMarketData_->fxCcyPairs();
    // Is this too strict?
    // - implemented to avoid cases where input cross FX rates are not consistent
    // - Consider an example (baseCcy = EUR) of a GBPUSD FX trade - two separate routes to pricing
    // - (a) call GBPUSD FX rate from sim market
    // - (b) call GBPEUR and EURUSD FX rates, manually join them to obtain GBPUSD
    // - now, if GBPUSD is an explicit risk factor in sim market, consider what happens
    // - if we bump GBPUSD value and leave other FX rates unchanged (for e.g. a sensitivity analysis)
    // - (a) the value of the trade changes
    // - (b) the value of the GBPUSD trade stays the same
    // - in light of the above we restrict the universe of FX pairs that we support here for the time being
    string baseCcy = simMarketData_->baseCcy();
    for (auto sensi_fx : fxCcyPairs) {
        string foreign = sensi_fx.substr(0, 3);
        string domestic = sensi_fx.substr(3);
        QL_REQUIRE((domestic == baseCcy) || (foreign == baseCcy),
                   "SensitivityScenarioGenerator does not support cross FX pairs("
                       << sensi_fx << ", but base currency is " << baseCcy << ")");
    }
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_fx : simMarketData_->fxCcyPairs()) {
        if (std::find(fxCcyPairs.begin(), fxCcyPairs.end(), sim_fx) == fxCcyPairs.end()) {
            ALOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (Size k = 0; k < fxCcyPairs.size(); k++) {
        string ccypair = fxCcyPairs[k]; // foreign + domestic;
        auto itr = sensitivityData_->fxShiftData().find(ccypair);
        QL_REQUIRE(itr != sensitivityData_->fxShiftData().end(), "fxShiftData not found for " << ccypair);
        SensitivityScenarioData::SpotShiftData data = itr->second;
        ShiftType type = parseShiftType(data.shiftType);
        Real size = up ? data.shiftSize : -1.0 * data.shiftSize;
        // QL_REQUIRE(type == SensitivityScenarioGenerator::ShiftType::Relative, "FX scenario type must be relative");
        bool relShift = (type == SensitivityScenarioGenerator::ShiftType::Relative);

        boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

        scenarioDescriptions_.push_back(fxScenarioDescription(ccypair, up));

        RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
        Real rate = scenario->get(key);
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        // Real newRate = up ? rate * (1.0 + data.shiftSize) : rate * (1.0 - data.shiftSize);
        scenario->add(key, newRate);
        scenarios_.push_back(scenario);
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newRate);
    }
    LOG("FX scenarios done");
}

void SensitivityScenarioGenerator::generateEquityScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    Date asof = baseScenario_->asof();
    std::vector<string> equityNames;
    if (sensitivityData_->equityNames().size() > 0)
        equityNames = sensitivityData_->equityNames();
    else
        equityNames = simMarketData_->equityNames();

    // Log an ALERT if some equities in simmarket are excluded from the sensitivities list
    for (auto sim_equity : simMarketData_->equityNames()) {
        if (std::find(equityNames.begin(), equityNames.end(), sim_equity) == equityNames.end()) {
            ALOG("Equity " << sim_equity << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (Size k = 0; k < equityNames.size(); k++) {
        string equity = equityNames[k];
        auto itr = sensitivityData_->equityShiftData().find(equity);
        QL_REQUIRE(itr != sensitivityData_->equityShiftData().end(), "equityShiftData not found for " << equity);
        SensitivityScenarioData::SpotShiftData data = itr->second;
        ShiftType type = parseShiftType(data.shiftType);
        Real size = up ? data.shiftSize : -1.0 * data.shiftSize;
        bool relShift = (type == SensitivityScenarioGenerator::ShiftType::Relative);

        boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

        scenarioDescriptions_.push_back(equityScenarioDescription(equity, up));
        RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
        Real rate = baseScenario_->get(key);
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        // Real newRate = up ? rate * (1.0 + data.shiftSize) : rate * (1.0 - data.shiftSize);
        scenario->add(key, newRate);
        scenarios_.push_back(scenario);
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newRate);
    }
    LOG("Equity scenarios done");
}

void SensitivityScenarioGenerator::generateDiscountCurveScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> discountCurrencies;
    if (sensitivityData_->discountCurrencies().size() > 0)
        discountCurrencies = sensitivityData_->discountCurrencies();
    else
        discountCurrencies = simMarketData_->ccys();
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_ccy : simMarketData_->ccys()) {
        if (std::find(discountCurrencies.begin(), discountCurrencies.end(), sim_ccy) == discountCurrencies.end()) {
            ALOG("Currency " << sim_ccy << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_ccy = discountCurrencies.size();

    for (Size i = 0; i < n_ccy; ++i) {
        string ccy = discountCurrencies[i];
        Size n_ten = simMarketData_->yieldCurveTenors(ccy).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->discountCurveShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->discountCurveShiftData().end(), "CurveShiftData not found for " << ccy);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(ccy));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(ccy)[j];
            times[j] = dc.yearFraction(asof, d);

            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(ccy)
                                              ? simMarketData_->yieldCurveTenors(ccy)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);
            scenarioDescriptions_.push_back(discountScenarioDescription(ccy, j, up));
            DLOG("generate discount curve scenario, ccy " << ccy << ", bucket " << j << ", up " << up << ", desc "
                                                          << scenarioDescriptions_.back().text());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                if (!close_enough(shiftedZeros[k], zeros[k])) {
                    Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                    scenario->add(RiskFactorKey(RiskFactorKey::KeyType::DiscountCurve, ccy, k), shiftedDiscount);
                }
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateIndexCurveScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();

    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> indexNames;
    if (sensitivityData_->indexNames().size() > 0)
        indexNames = sensitivityData_->indexNames();
    else
        indexNames = simMarketData_->indices();
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketData_->indices()) {
        if (std::find(indexNames.begin(), indexNames.end(), sim_idx) == indexNames.end()) {
            ALOG("Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_indices = indexNames.size();

    for (Size i = 0; i < n_indices; ++i) {
        string indexName = indexNames[i];
        Size n_ten = simMarketData_->yieldCurveTenors(indexName).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->indexCurveShiftData().find(indexName);
        QL_REQUIRE(itr != sensitivityData_->indexCurveShiftData().end(), "CurveShiftData not found for " << indexName);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);

        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(indexName));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(indexName)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(indexName)
                                              ? simMarketData_->yieldCurveTenors(indexName)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Index shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(indexScenarioDescription(indexName, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::IndexCurve, indexName, k), shiftedDiscount);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    LOG("Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYieldCurveScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer yield curves than listed in the market
    vector<string> yieldCurveNames;
    if (sensitivityData_->yieldCurveNames().size() > 0)
        yieldCurveNames = sensitivityData_->yieldCurveNames();
    else
        yieldCurveNames = simMarketData_->yieldCurveNames();
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim_yc : simMarketData_->yieldCurveNames()) {
        if (std::find(yieldCurveNames.begin(), yieldCurveNames.end(), sim_yc) == yieldCurveNames.end()) {
            ALOG("Yield Curve " << sim_yc << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_curves = yieldCurveNames.size();

    for (Size i = 0; i < n_curves; ++i) {
        string name = yieldCurveNames[i];
        Size n_ten = simMarketData_->yieldCurveTenors(name).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->yieldCurveShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->yieldCurveShiftData().end(), "CurveShiftData not found for " << name);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);

        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(name));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(name)
                                                     ? simMarketData_->yieldCurveTenors(name)
                                                     : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(yieldScenarioDescription(name, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::YieldCurve, name, k), shiftedDiscount);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateEquityForecastCurveScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer yield curves than listed in the market
    Date asof = baseScenario_->asof();
    vector<string> equityForecastNames;
    if (sensitivityData_->equityForecastCurveNames().size() > 0)
        equityForecastNames = sensitivityData_->equityForecastCurveNames();
    else
        equityForecastNames = simMarketData_->equityNames();
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim_ec : simMarketData_->equityNames()) {
        if (std::find(equityForecastNames.begin(), equityForecastNames.end(), sim_ec) == equityForecastNames.end()) {
            ALOG("equityForecast Curve " << sim_ec << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_curves = equityForecastNames.size();

    for (Size i = 0; i < n_curves; ++i) {
        string name = equityForecastNames[i];
        Size n_ten = simMarketData_->equityForecastTenors(name).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->equityForecastCurveShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->equityForecastCurveShiftData().end(),
                   "equityForecast CurveShiftData not found for " << name);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);

        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(name));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->equityForecastTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::EquityForecastCurve, name, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasEquityForecastTenors(name)
                                                     ? simMarketData_->equityForecastTenors(name)
                                                     : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(equityForecastCurveScenarioDescription(name, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::EquityForecastCurve, name, k), shiftedDiscount);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Equity forecast curve scenarios done");
}

void SensitivityScenarioGenerator::generateDividendYieldScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();

    // We can choose to shift fewer yield curves than listed in the market
    vector<string> dividendYieldNames;
    if (sensitivityData_->equityNames().size() > 0)
        dividendYieldNames = sensitivityData_->dividendYieldNames();
    else
        dividendYieldNames = simMarketData_->equityNames();
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim : simMarketData_->equityNames()) {
        if (std::find(dividendYieldNames.begin(), dividendYieldNames.end(), sim) == dividendYieldNames.end()) {
            ALOG("Equity " << sim << " in simmarket is not included in dividend yield sensitivity analysis");
        }
    }

    Size n_curves = dividendYieldNames.size();
    for (Size i = 0; i < n_curves; ++i) {
        string name = dividendYieldNames[i];
        Size n_ten = simMarketData_->equityDividendTenors(name).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->dividendYieldShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->dividendYieldShiftData().end(),
                   "dividendYield CurveShiftData not found for " << name);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);

        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(name));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->equityDividendTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::DividendYield, name, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasEquityDividendTenors(name)
                                                     ? simMarketData_->equityDividendTenors(name)
                                                     : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(dividendYieldScenarioDescription(name, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::DividendYield, name, k), shiftedDiscount);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Dividend yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateFxVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> fxVolCcyPairs;
    if (sensitivityData_->fxVolCcyPairs().size() > 0)
        fxVolCcyPairs = sensitivityData_->fxVolCcyPairs();
    else
        fxVolCcyPairs = simMarketData_->fxVolCcyPairs();
    // Log an ALERT if some FX vol pairs in simmarket are excluded from the list
    for (auto sim_fx : simMarketData_->fxVolCcyPairs()) {
        if (std::find(fxVolCcyPairs.begin(), fxVolCcyPairs.end(), sim_fx) == fxVolCcyPairs.end()) {
            ALOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_fxvol_pairs = fxVolCcyPairs.size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();
    Size n_fxvol_strikes = simMarketData_->fxVolMoneyness().size();

    vector<vector<Real>> values(n_fxvol_strikes, vector<Real>(n_fxvol_exp, 0.0));
    std::vector<Real> times(n_fxvol_exp);

    // buffer for shifted zero curves
    vector<vector<Real>> shiftedValues(n_fxvol_strikes, vector<Real>(n_fxvol_exp, 0.0));

    map<string, SensitivityScenarioData::VolShiftData> shiftDataMap = sensitivityData_->fxVolShiftData();
    for (Size i = 0; i < n_fxvol_pairs; ++i) {
        string ccyPair = fxVolCcyPairs[i];
        QL_REQUIRE(ccyPair.length() == 6, "invalid ccy pair length");

        QL_REQUIRE(shiftDataMap.find(ccyPair) != shiftDataMap.end(),
                   "ccy pair " << ccyPair << " not found in VolShiftData");
        SensitivityScenarioData::VolShiftData data = shiftDataMap[ccyPair];
        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");

        DayCounter dc = parseDayCounter(simMarketData_->fxVolDayCounter(ccyPair));
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = asof + simMarketData_->fxVolExpiries()[j];
            times[j] = dc.yearFraction(asof, d);
            for (Size k = 0; k < n_fxvol_strikes; k++) {
                Size idx = k * n_fxvol_exp + j;

                RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccyPair, idx);
                values[k][j] = baseScenario_->get(key);
            }
        }

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            Size strikeBucket = 0; // FIXME
            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(fxVolScenarioDescription(ccyPair, j, strikeBucket, up));

            // apply shift at tenor point j
            for (Size k = 0; k < n_fxvol_strikes; ++k) {
                applyShift(j, shiftSize, up, shiftType, shiftTimes, values[k], times, shiftedValues[k], true);
            }

            for (Size k = 0; k < n_fxvol_strikes; ++k) {
                for (Size l = 0; l < n_fxvol_exp; ++l) {
                    Size idx = k * n_fxvol_exp + l;
                    scenario->add(RiskFactorKey(RiskFactorKey::KeyType::FXVolatility, ccyPair, idx),
                                  shiftedValues[k][l]);
                }
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("FX vol scenarios done");
}

void SensitivityScenarioGenerator::generateEquityVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> equityVolNames;
    if (sensitivityData_->equityVolNames().size() > 0)
        equityVolNames = sensitivityData_->equityVolNames();
    else
        equityVolNames = simMarketData_->equityVolNames();
    // Log an ALERT if an Equity in simmarket are excluded from the simulation list
    for (auto sim_equity : simMarketData_->equityVolNames()) {
        if (std::find(equityVolNames.begin(), equityVolNames.end(), sim_equity) == equityVolNames.end()) {
            ALOG("Equity " << sim_equity << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_eqvol_names = equityVolNames.size();
    Size n_eqvol_exp = simMarketData_->equityVolExpiries().size();
    Size n_eqvol_strikes = simMarketData_->equityVolIsSurface() ? simMarketData_->equityVolMoneyness().size() : 1;

    // [strike] x [expiry]
    vector<vector<Real>> values(n_eqvol_strikes, vector<Real>(n_eqvol_exp, 0.0));
    vector<Real> times(n_eqvol_exp);

    // buffer for shifted vols
    vector<vector<Real>> shiftedValues(n_eqvol_strikes, vector<Real>(n_eqvol_exp, 0.0));

    map<string, SensitivityScenarioData::VolShiftData> shiftDataMap = sensitivityData_->equityVolShiftData();
    for (Size i = 0; i < n_eqvol_names; ++i) {
        string equity = equityVolNames[i];
        QL_REQUIRE(shiftDataMap.find(equity) != shiftDataMap.end(),
                   "equity " << equity << " not found in VolShiftData");
        SensitivityScenarioData::VolShiftData data = shiftDataMap[equity];
        ShiftType shiftType = parseShiftType(data.shiftType);
        vector<Period> shiftTenors = data.shiftExpiries;
        vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Equity vol shift tenors not specified");
        DayCounter dc = parseDayCounter(simMarketData_->equityVolDayCounter(equity));
        for (Size j = 0; j < n_eqvol_exp; ++j) {
            Date d = asof + simMarketData_->equityVolExpiries()[j];
            times[j] = dc.yearFraction(asof, d);
            for (Size k = 0; k < n_eqvol_strikes; k++) {
                Size idx = k * n_eqvol_exp + j;

                RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, idx);
                values[k][j] = baseScenario_->get(key);
            }
        }
        for (Size j = 0; j < shiftTenors.size(); ++j) {
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

            Size strikeBucket = 0; // FIXME
            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(equityVolScenarioDescription(equity, j, strikeBucket, up));

            // apply shift at tenor point j for each strike
            for (Size k = 0; k < n_eqvol_strikes; ++k) {
                applyShift(j, shiftSize, up, shiftType, shiftTimes, values[k], times, shiftedValues[k], true);
            }

            // update the scenario
            for (Size k = 0; k < n_eqvol_strikes; ++k) {
                for (Size l = 0; l < n_eqvol_exp; l++) {
                    Size idx = k * n_eqvol_exp + l;
                    scenario->add(RiskFactorKey(RiskFactorKey::KeyType::EquityVolatility, equity, idx),
                                  shiftedValues[k][l]);
                }
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("Equity vol scenarios done");
}

void SensitivityScenarioGenerator::generateSwaptionVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    LOG("starting swapVol sgen");
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> swaptionVolCurrencies;
    if (sensitivityData_->swaptionVolCurrencies().size() > 0)
        swaptionVolCurrencies = sensitivityData_->swaptionVolCurrencies();
    else
        swaptionVolCurrencies = simMarketData_->swapVolCcys();
    // Log an ALERT if some swaption currencies in simmarket are excluded from the list
    for (auto sim_ccy : simMarketData_->swapVolCcys()) {
        if (std::find(swaptionVolCurrencies.begin(), swaptionVolCurrencies.end(), sim_ccy) ==
            swaptionVolCurrencies.end()) {
            ALOG("Swaption currency " << sim_ccy << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_swvol_ccy = swaptionVolCurrencies.size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();
    Size n_swvol_strike = simMarketData_->swapVolStrikeSpreads().size();

    bool atmOnly_swvol = simMarketData_->simulateSwapVolATMOnly();

    vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
    vector<Real> volTermTimes(n_swvol_term, 0.0);
    vector<vector<vector<Real>>> volData;
    vector<vector<vector<Real>>> shiftedVolData;

    volData.resize(n_swvol_strike, vector<vector<Real>>(n_swvol_exp, vector<Real>(n_swvol_term, 0.0)));
    shiftedVolData.resize(n_swvol_strike, vector<vector<Real>>(n_swvol_exp, vector<Real>(n_swvol_term, 0.0)));

    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = swaptionVolCurrencies[i];
        auto itr = sensitivityData_->swaptionVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->swaptionVolShiftData().end(), "SwaptionVolShiftData not found for " << ccy);
        SensitivityScenarioData::SwaptionVolShiftData data = itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;

        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        vector<Real> shiftStrikes;
        if (!atmOnly_swvol) {
            shiftStrikes = data.shiftStrikes;
            QL_REQUIRE(data.shiftStrikes.size() == n_swvol_strike,
                       "number of simulated strikes must equal number of sensitivity strikes");
        } else {
            shiftStrikes = {0.0};
        }

        DayCounter dc = parseDayCounter(simMarketData_->swapVolDayCounter(ccy));

        // cache original vol data
        for (Size j = 0; j < n_swvol_exp; ++j) {
            Date expiry = asof + simMarketData_->swapVolExpiries()[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_swvol_term; ++j) {
            Date term = asof + simMarketData_->swapVolTerms()[j];
            volTermTimes[j] = dc.yearFraction(asof, term);
        }

        for (Size j = 0; j < n_swvol_exp; ++j) {
            for (Size k = 0; k < n_swvol_term; ++k) {
                for (Size l = 0; l < n_swvol_strike; ++l) {
                    Size idx = j * n_swvol_term * n_swvol_strike + k * n_swvol_strike + l;

                    RiskFactorKey key(RiskFactorKey::KeyType::SwaptionVolatility, ccy, idx);
                    volData[l][j][k] = baseScenario_->get(key);
                }
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(asof, asof + data.shiftTerms[j]);

        // loop over shift expiries, terms and strikes
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                for (Size l = 0; l < shiftStrikes.size(); ++l) {
                    Size strikeBucket = l;
                    boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

                    scenarioDescriptions_.push_back(swaptionVolScenarioDescription(ccy, j, k, strikeBucket, up));

                    // if simulating atm only we shift all strikes otherwise we shift each strike individually
                    Size loopStart = atmOnly_swvol ? 0 : l;
                    Size loopEnd = atmOnly_swvol ? n_swvol_strike : loopStart + 1;

                    LOG("Swap vol looping over " << loopStart << " to " << loopEnd << " for strike "
                                                 << shiftStrikes[l]);
                    for (Size ll = loopStart; ll < loopEnd; ++ll) {
                        applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftTermTimes, volExpiryTimes,
                                   volTermTimes, volData[ll], shiftedVolData[ll], true);
                    }

                    for (Size jj = 0; jj < n_swvol_exp; ++jj) {
                        for (Size kk = 0; kk < n_swvol_term; ++kk) {
                            for (Size ll = 0; ll < n_swvol_strike; ++ll) {
                                Size idx = jj * n_swvol_term * n_swvol_strike + kk * n_swvol_strike + ll;
                                if (ll >= loopStart && ll < loopEnd) {
                                    scenario->add(RiskFactorKey(RiskFactorKey::KeyType::SwaptionVolatility, ccy, idx),
                                                  shiftedVolData[ll][jj][kk]);
                                } else {
                                    scenario->add(RiskFactorKey(RiskFactorKey::KeyType::SwaptionVolatility, ccy, idx),
                                                  volData[ll][jj][kk]);
                                }
                            }
                        }
                    }
                    // add this scenario to the scenario vector
                    scenarios_.push_back(scenario);
                    DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                                   << " created for swaption vol " << ccy);
                }
            }
        }
    }
    LOG("Swaption vol scenarios done");
}

void SensitivityScenarioGenerator::generateCapFloorVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    vector<string> capFloorVolCurrencies;
    if (sensitivityData_->capFloorVolCurrencies().size() > 0)
        capFloorVolCurrencies = sensitivityData_->capFloorVolCurrencies();
    else
        capFloorVolCurrencies = simMarketData_->capFloorVolCcys();
    // Log an ALERT if some cap currencies in simmarket are excluded from the list
    for (auto sim_cap : simMarketData_->capFloorVolCcys()) {
        if (std::find(capFloorVolCurrencies.begin(), capFloorVolCurrencies.end(), sim_cap) ==
            capFloorVolCurrencies.end()) {
            ALOG("CapFloor currency " << sim_cap << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_cfvol_ccy = capFloorVolCurrencies.size();
    Size n_cfvol_strikes = simMarketData_->capFloorVolStrikes().size();
    vector<Real> volStrikes = simMarketData_->capFloorVolStrikes();

    for (Size i = 0; i < n_cfvol_ccy; ++i) {
        std::string ccy = capFloorVolCurrencies[i];
        Size n_cfvol_exp = simMarketData_->capFloorVolExpiries(ccy).size();
        auto itr = sensitivityData_->capFloorVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->capFloorVolShiftData().end(), "SwaptionVolShiftData not found for " << ccy);
        SensitivityScenarioData::CapFloorVolShiftData data = itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;
        vector<vector<Real>> volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

        std::vector<Period> expiries = overrideTenors_ && simMarketData_->hasCapFloorVolExpiries(ccy)
                                           ? simMarketData_->capFloorVolExpiries(ccy)
                                           : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(), "mismatch between effective shift expiries ("
                                                                     << expiries.size() << ") and shift tenors ("
                                                                     << data.shiftExpiries.size());
        vector<Real> shiftExpiryTimes(expiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        DayCounter dc = parseDayCounter(simMarketData_->capFloorVolDayCounter(ccy));

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = asof + simMarketData_->capFloorVolExpiries(ccy)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Size idx = j * n_cfvol_strikes + k;
                RiskFactorKey key(RiskFactorKey::KeyType::OptionletVolatility, ccy, idx);
                volData[j][k] = baseScenario_->get(key);
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + expiries[j]);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

                scenarioDescriptions_.push_back(capFloorVolScenarioDescription(ccy, j, k, up));

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);
                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                        Size idx = jj * n_cfvol_strikes + kk;
                        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::OptionletVolatility, ccy, idx),
                                      shiftedVolData[jj][kk]);
                    }
                }
                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    LOG("Optionlet vol scenarios done");
}

void SensitivityScenarioGenerator::generateSurvivalProbabilityScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer credit curves than listed in the market
    std::vector<string> crNames;

    if (sensitivityData_->creditNames().size() > 0)
        crNames = sensitivityData_->creditNames();
    else
        crNames = simMarketData_->defaultNames();
    // Log an ALERT if some names in simmarket are excluded from the list
    for (auto sim_name : simMarketData_->defaultNames()) {
        if (std::find(crNames.begin(), crNames.end(), sim_name) == crNames.end()) {
            ALOG("Credit Name " << sim_name << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_names = crNames.size();
    Size n_ten;

    // original curves' buffer
    std::vector<Real> times;

    for (Size i = 0; i < n_names; ++i) {
        string name = crNames[i];
        n_ten = simMarketData_->defaultTenors(crNames[i]).size();
        std::vector<Real> hazardRates(n_ten); // integrated hazard rates
        times.clear();
        times.resize(n_ten);
        // buffer for shifted survival prob curves
        std::vector<Real> shiftedHazardRates(n_ten);
        auto itr = sensitivityData_->creditCurveShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->creditCurveShiftData().end(),
                   "credit CurveShiftData not found for " << name);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        DayCounter dc = parseDayCounter(simMarketData_->defaultCurveDayCounter(name));
        Calendar calendar = parseCalendar(simMarketData_->defaultCurveCalendar(name));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->defaultTenors(crNames[i])[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, j);
            Real prob = baseScenario_->get(key);
            hazardRates[j] = -std::log(prob) / times[j];
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasDefaultTenors(name)
                                              ? simMarketData_->defaultTenors(name)
                                              : data.shiftTenors;

        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");

        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);
            scenarioDescriptions_.push_back(survivalProbabilityScenarioDescription(name, j, up));
            LOG("generate survival probability scenario, name " << name << ", bucket " << j << ", up " << up
                                                                << ", desc " << scenarioDescriptions_.back().text());

            // apply averaged hazard rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, hazardRates, times, shiftedHazardRates, true);

            // store shifted survival Prob in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedProb = exp(-shiftedHazardRates[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::SurvivalProbability, name, k), shiftedProb);
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateCdsVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> cdsVolNames;
    if (sensitivityData_->cdsVolNames().size() > 0)
        cdsVolNames = sensitivityData_->cdsVolNames();
    else
        cdsVolNames = simMarketData_->cdsVolNames();
    // Log an ALERT if some swaption currencies in simmarket are excluded from the list
    for (auto sim_name : simMarketData_->cdsVolNames()) {
        if (std::find(cdsVolNames.begin(), cdsVolNames.end(), sim_name) == cdsVolNames.end()) {
            ALOG("CDS name " << sim_name << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_cdsvol_name = cdsVolNames.size();
    Size n_cdsvol_exp = simMarketData_->cdsVolExpiries().size();

    vector<Real> volData(n_cdsvol_exp, 0.0);
    vector<Real> volExpiryTimes(n_cdsvol_exp, 0.0);
    vector<Real> shiftedVolData(n_cdsvol_exp, 0.0);

    for (Size i = 0; i < n_cdsvol_name; ++i) {
        std::string name = cdsVolNames[i];
        auto itr = sensitivityData_->cdsVolShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->cdsVolShiftData().end(), "CdsVolShiftData not found for " << name);
        SensitivityScenarioData::CdsVolShiftData data = itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;

        vector<Time> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);

        DayCounter dc = parseDayCounter(simMarketData_->cdsVolDayCounter(name));

        // cache original vol data
        for (Size j = 0; j < n_cdsvol_exp; ++j) {
            Date expiry = asof + simMarketData_->cdsVolExpiries()[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_cdsvol_exp; ++j) {
            RiskFactorKey key(RiskFactorKey::KeyType::CDSVolatility, name, j);
            volData[j] = baseScenario_->get(key);
        }
        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            Size strikeBucket = 0; // FIXME
            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(CdsVolScenarioDescription(name, j, strikeBucket, up));

            applyShift(j, shiftSize, up, shiftType, shiftExpiryTimes, volData, volExpiryTimes, shiftedVolData, true);
            // add shifted vol data to the scenario
            for (Size jj = 0; jj < n_cdsvol_exp; ++jj) {
                Size idx = jj;
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::CDSVolatility, name, idx), shiftedVolData[jj]);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("CDS vol scenarios done");
}

void SensitivityScenarioGenerator::generateZeroInflationScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> zeroInfIndexNames;
    if (sensitivityData_->zeroInflationIndices().size() > 0)
        zeroInfIndexNames = sensitivityData_->zeroInflationIndices();
    else
        zeroInfIndexNames = simMarketData_->zeroInflationIndices();
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketData_->zeroInflationIndices()) {
        if (std::find(zeroInfIndexNames.begin(), zeroInfIndexNames.end(), sim_idx) == zeroInfIndexNames.end()) {
            ALOG("Zero Inflation Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_indices = zeroInfIndexNames.size();

    for (Size i = 0; i < n_indices; ++i) {
        string indexName = zeroInfIndexNames[i];
        Size n_ten = simMarketData_->zeroInflationTenors(indexName).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        auto itr = sensitivityData_->zeroInflationCurveShiftData().find(indexName);
        QL_REQUIRE(itr != sensitivityData_->zeroInflationCurveShiftData().end(),
                   "zero inflation CurveShiftData not found for " << indexName);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        DayCounter dc = parseDayCounter(simMarketData_->zeroInflationDayCounter(indexName));
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->zeroInflationTenors(indexName)[j];
            RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCurve, indexName, j);
            zeros[j] = baseScenario_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasZeroInflationTenors(indexName)
                                              ? simMarketData_->zeroInflationTenors(indexName)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Zero Inflation Index shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(zeroInflationScenarioDescription(indexName, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                // Real shiftedDiscount = exp(-shiftedYoys[k] * times[k]);
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::ZeroInflationCurve, indexName, k), shiftedZeros[k]);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    LOG("Zero Inflation Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYoYInflationScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();

    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> yoyInfIndexNames;
    if (sensitivityData_->yoyInflationIndices().size() > 0)
        yoyInfIndexNames = sensitivityData_->yoyInflationIndices();
    else
        yoyInfIndexNames = simMarketData_->yoyInflationIndices();
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketData_->yoyInflationIndices()) {
        if (std::find(yoyInfIndexNames.begin(), yoyInfIndexNames.end(), sim_idx) == yoyInfIndexNames.end()) {
            ALOG("YoY Inflation Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_indices = yoyInfIndexNames.size();

    for (Size i = 0; i < n_indices; ++i) {
        string indexName = yoyInfIndexNames[i];
        Size n_ten = simMarketData_->yoyInflationTenors(indexName).size();
        // original curves' buffer
        std::vector<Real> yoys(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedYoys(n_ten);
        auto itr = sensitivityData_->yoyInflationCurveShiftData().find(indexName);
        QL_REQUIRE(itr != sensitivityData_->yoyInflationCurveShiftData().end(),
                   "yoyinflation CurveShiftData not found for " << indexName);
        SensitivityScenarioData::CurveShiftData data = *itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        DayCounter dc = parseDayCounter(simMarketData_->yoyInflationDayCounter(indexName));
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yoyInflationTenors(indexName)[j];
            RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCurve, indexName, j);
            yoys[j] = baseScenario_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYoyInflationTenors(indexName)
                                              ? simMarketData_->yoyInflationTenors(indexName)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(), "mismatch between effective shift tenors ("
                                                                      << shiftTenors.size() << ") and shift tenors ("
                                                                      << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "YoY Inflation Index shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

            scenarioDescriptions_.push_back(yoyInflationScenarioDescription(indexName, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, yoys, times, shiftedYoys, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::YoYInflationCurve, indexName, k), shiftedYoys[k]);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    LOG("YoY Inflation Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateBaseCorrelationScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> baseCorrelationNames;
    if (sensitivityData_->baseCorrelationNames().size() > 0)
        baseCorrelationNames = sensitivityData_->baseCorrelationNames();
    else
        baseCorrelationNames = simMarketData_->baseCorrelationNames();
    // Log an ALERT if some names in simmarket are excluded from the list
    for (auto name : simMarketData_->baseCorrelationNames()) {
        if (std::find(baseCorrelationNames.begin(), baseCorrelationNames.end(), name) == baseCorrelationNames.end()) {
            ALOG("Base Correlation " << name << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_bc_names = baseCorrelationNames.size();
    Size n_bc_terms = simMarketData_->baseCorrelationTerms().size();
    Size n_bc_levels = simMarketData_->baseCorrelationDetachmentPoints().size();

    vector<vector<Real>> bcData(n_bc_levels, vector<Real>(n_bc_terms, 0.0));
    vector<vector<Real>> shiftedBcData(n_bc_levels, vector<Real>(n_bc_levels, 0.0));
    vector<Real> termTimes(n_bc_terms, 0.0);
    vector<Real> levels = simMarketData_->baseCorrelationDetachmentPoints();

    for (Size i = 0; i < n_bc_names; ++i) {
        std::string name = baseCorrelationNames[i];
        auto itr = sensitivityData_->baseCorrelationShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->baseCorrelationShiftData().end(),
                   "BaseCorrelationShiftData not found for " << name);
        SensitivityScenarioData::BaseCorrelationShiftData data = itr->second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;

        vector<Real> shiftLevels = data.shiftLossLevels;
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        DayCounter dc = parseDayCounter(simMarketData_->baseCorrelationDayCounter(name));

        // cache original base correlation data
        for (Size j = 0; j < n_bc_terms; ++j) {
            Date term = asof + simMarketData_->baseCorrelationTerms()[j];
            termTimes[j] = dc.yearFraction(asof, term);
        }
        for (Size j = 0; j < n_bc_levels; ++j) {
            for (Size k = 0; k < n_bc_terms; ++k) {
                RiskFactorKey key(RiskFactorKey::KeyType::BaseCorrelation, name, j);
                bcData[j][k] = baseScenario_->get(key);
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(asof, asof + data.shiftTerms[j]);

        // loop over shift levels and terms
        for (Size j = 0; j < shiftLevels.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(asof);

                scenarioDescriptions_.push_back(baseCorrelationScenarioDescription(name, j, k, up));

                applyShift(j, k, shiftSize, up, shiftType, shiftLevels, shiftTermTimes, levels, termTimes, bcData,
                           shiftedBcData, true);

                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_bc_levels; ++jj) {
                    for (Size kk = 0; kk < n_bc_terms; ++kk) {
                        Size idx = jj * n_bc_terms + kk;
                        if (shiftedBcData[jj][kk] < 0.0) {
                            ALOG("invalid shifted base correlation " << shiftedBcData[jj][kk] << " at lossLevelIndex "
                                                                     << jj << " and termIndex " << kk
                                                                     << " set to zero");
                            shiftedBcData[jj][kk] = 0.0;
                        } else if (shiftedBcData[jj][kk] > 1.0) {
                            ALOG("invalid shifted base correlation " << shiftedBcData[jj][kk] << " at lossLevelIndex "
                                                                     << jj << " and termIndex " << kk
                                                                     << " set to 1 - epsilon");
                            shiftedBcData[jj][kk] = 1.0 - QL_EPSILON;
                        }
                        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::BaseCorrelation, name, idx),
                                      shiftedBcData[jj][kk]);
                    }
                }
                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    LOG("Base correlation scenarios done");
}

SensitivityScenarioGenerator::ScenarioDescription SensitivityScenarioGenerator::fxScenarioDescription(string ccypair,
                                                                                                      bool up) {
    RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spot");
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription SensitivityScenarioGenerator::equityScenarioDescription(string equity,
                                                                                                          bool up) {
    RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spot");
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::dividendYieldScenarioDescription(string name, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->dividendYieldShiftData().find(name) !=
                   sensitivityData_->dividendYieldShiftData().end(),
               "equity " << name << " not found in dividend yield shift data");
    QL_REQUIRE(bucket < sensitivityData_->dividendYieldShiftData()[name]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::DividendYield, name, bucket);
    std::ostringstream o;
    o << sensitivityData_->dividendYieldShiftData()[name]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::discountScenarioDescription(string ccy, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->discountCurveShiftData().find(ccy) != sensitivityData_->discountCurveShiftData().end(),
               "currency " << ccy << " not found in discount shift data");
    QL_REQUIRE(bucket < sensitivityData_->discountCurveShiftData()[ccy]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, bucket);
    std::ostringstream o;
    o << sensitivityData_->discountCurveShiftData()[ccy]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::indexScenarioDescription(string index, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->indexCurveShiftData().find(index) != sensitivityData_->indexCurveShiftData().end(),
               "currency " << index << " not found in index shift data");
    QL_REQUIRE(bucket < sensitivityData_->indexCurveShiftData()[index]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, index, bucket);
    std::ostringstream o;
    o << sensitivityData_->indexCurveShiftData()[index]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yieldScenarioDescription(string name, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->yieldCurveShiftData().find(name) != sensitivityData_->yieldCurveShiftData().end(),
               "currency " << name << " not found in index shift data");
    QL_REQUIRE(bucket < sensitivityData_->yieldCurveShiftData()[name]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, bucket);
    std::ostringstream o;
    o << sensitivityData_->yieldCurveShiftData()[name]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::equityForecastCurveScenarioDescription(string name, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->equityForecastCurveShiftData().find(name) !=
                   sensitivityData_->equityForecastCurveShiftData().end(),
               "equity " << name << " not found in index shift data");
    QL_REQUIRE(bucket < sensitivityData_->equityForecastCurveShiftData()[name]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::EquityForecastCurve, name, bucket);
    std::ostringstream o;
    o << sensitivityData_->equityForecastCurveShiftData()[name]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::fxVolScenarioDescription(string ccypair, Size expiryBucket, Size strikeBucket, bool up) {
    QL_REQUIRE(sensitivityData_->fxVolShiftData().find(ccypair) != sensitivityData_->fxVolShiftData().end(),
               "currency pair " << ccypair << " not found in fx vol shift data");
    SensitivityScenarioData::VolShiftData data = sensitivityData_->fxVolShiftData()[ccypair];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::equityVolScenarioDescription(string equity, Size expiryBucket, Size strikeBucket,
                                                           bool up) {
    QL_REQUIRE(sensitivityData_->equityVolShiftData().find(equity) != sensitivityData_->equityVolShiftData().end(),
               "currency pair " << equity << " not found in fx vol shift data");
    SensitivityScenarioData::VolShiftData data = sensitivityData_->equityVolShiftData()[equity];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::swaptionVolScenarioDescription(string ccy, Size expiryBucket, Size termBucket,
                                                             Size strikeBucket, bool up) {
    QL_REQUIRE(sensitivityData_->swaptionVolShiftData().find(ccy) != sensitivityData_->swaptionVolShiftData().end(),
               "currency " << ccy << " not found in swaption vol shift data");
    SensitivityScenarioData::SwaptionVolShiftData data = sensitivityData_->swaptionVolShiftData()[ccy];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(termBucket < data.shiftTerms.size(), "term bucket " << termBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    Size index = expiryBucket * data.shiftStrikes.size() * data.shiftTerms.size() +
                 termBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::SwaptionVolatility, ccy, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftTerms[termBucket] << "/ATM";
    } else {
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftTerms[termBucket] << "/" << std::setprecision(4)
          << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::capFloorVolScenarioDescription(string ccy, Size expiryBucket, Size strikeBucket,
                                                             bool up) {
    QL_REQUIRE(sensitivityData_->capFloorVolShiftData().find(ccy) != sensitivityData_->capFloorVolShiftData().end(),
               "currency " << ccy << " not found in cap/floor vol shift data");
    SensitivityScenarioData::CapFloorVolShiftData data = sensitivityData_->capFloorVolShiftData()[ccy];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    // Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    Size index = expiryBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::OptionletVolatility, ccy, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        o << data.shiftExpiries[expiryBucket] << "/" << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::survivalProbabilityScenarioDescription(string name, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->creditCurveShiftData().find(name) != sensitivityData_->creditCurveShiftData().end(),
               "Name " << name << " not found in credit shift data");
    QL_REQUIRE(bucket < sensitivityData_->creditCurveShiftData()[name]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, bucket);
    std::ostringstream o;
    o << sensitivityData_->creditCurveShiftData()[name]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::CdsVolScenarioDescription(string name, Size expiryBucket, Size strikeBucket, bool up) {
    QL_REQUIRE(sensitivityData_->cdsVolShiftData().find(name) != sensitivityData_->cdsVolShiftData().end(),
               "name " << name << " not found in swaption name shift data");
    SensitivityScenarioData::CdsVolShiftData data = sensitivityData_->cdsVolShiftData()[name];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::CDSVolatility, name, index);
    std::ostringstream o;
    o << data.shiftExpiries[expiryBucket] << "/"
      << "ATM";
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::zeroInflationScenarioDescription(string index, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->zeroInflationCurveShiftData().find(index) !=
                   sensitivityData_->zeroInflationCurveShiftData().end(),
               "inflation index " << index << " not found in zero inflation index shift data");
    QL_REQUIRE(bucket < sensitivityData_->zeroInflationCurveShiftData()[index]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCurve, index, bucket);
    std::ostringstream o;
    o << sensitivityData_->zeroInflationCurveShiftData()[index]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yoyInflationScenarioDescription(string index, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->yoyInflationCurveShiftData().find(index) !=
                   sensitivityData_->yoyInflationCurveShiftData().end(),
               "yoy inflation index " << index << " not found in zero inflation index shift data");
    QL_REQUIRE(bucket < sensitivityData_->yoyInflationCurveShiftData()[index]->shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCurve, index, bucket);
    std::ostringstream o;
    o << sensitivityData_->yoyInflationCurveShiftData()[index]->shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::baseCorrelationScenarioDescription(string indexName, Size lossLevelBucket,
                                                                 Size termBucket, bool up) {
    QL_REQUIRE(sensitivityData_->baseCorrelationShiftData().find(indexName) !=
                   sensitivityData_->baseCorrelationShiftData().end(),
               "name " << indexName << " not found in base correlation shift data");
    SensitivityScenarioData::BaseCorrelationShiftData data = sensitivityData_->baseCorrelationShiftData()[indexName];
    QL_REQUIRE(termBucket < data.shiftTerms.size(), "term bucket " << termBucket << " out of range");
    QL_REQUIRE(lossLevelBucket < data.shiftLossLevels.size(),
               "loss level bucket " << lossLevelBucket << " out of range");
    Size index = lossLevelBucket * data.shiftTerms.size() + termBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::BaseCorrelation, indexName, index);
    std::ostringstream o;
    o << data.shiftLossLevels[lossLevelBucket] << "/" << data.shiftTerms[termBucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

} // namespace analytics
} // namespace ore
