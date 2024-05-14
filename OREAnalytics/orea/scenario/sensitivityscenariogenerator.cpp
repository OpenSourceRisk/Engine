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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/math/comparison.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>

#include <algorithm>
#include <ostream>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

using RFType = RiskFactorKey::KeyType;

SensitivityScenarioGenerator::SensitivityScenarioGenerator(
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensitivityData, const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
    const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
    const QuantLib::ext::shared_ptr<ScenarioFactory>& sensiScenarioFactory, const bool overrideTenors,
    const std::string& sensitivityTemplate, const bool continueOnError,
    const QuantLib::ext::shared_ptr<Scenario>& baseScenarioAbsolute)
    : ShiftScenarioGenerator(baseScenario, simMarketData, simMarket), sensitivityData_(sensitivityData),
      sensiScenarioFactory_(sensiScenarioFactory), sensitivityTemplate_(sensitivityTemplate),
      overrideTenors_(overrideTenors), continueOnError_(continueOnError),
      baseScenarioAbsolute_(baseScenarioAbsolute == nullptr ? baseScenario : baseScenarioAbsolute) {

    QL_REQUIRE(sensitivityData_, "SensitivityScenarioGenerator: sensitivityData is null");

    generateScenarios();
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

bool close(const Real& t_1, const Real& t_2) { return QuantLib::close(t_1, t_2); }

bool vectorEqual(const vector<Real>& v_1, const vector<Real>& v_2) {
    return (v_1.size() == v_2.size() && std::equal(v_1.begin(), v_1.end(), v_2.begin(), close));
}

void SensitivityScenarioGenerator::generateScenarios() {
    Date asof = baseScenario_->asof();

    QL_REQUIRE(sensitivityData_->crossGammaFilter().empty() || sensitivityData_->computeGamma(),
               "SensitivityScenarioGenerator::generateScenarios(): if gamma computation is disabled, the cross gamma "
               "filter must be empty");

    generateDiscountCurveScenarios(true);
    generateDiscountCurveScenarios(false);

    generateIndexCurveScenarios(true);
    generateIndexCurveScenarios(false);

    generateYieldCurveScenarios(true);
    generateYieldCurveScenarios(false);

    if (simMarketData_->simulateFxSpots()) {
        generateFxScenarios(true);
        generateFxScenarios(false);
    }

    generateEquityScenarios(true);
    generateEquityScenarios(false);

    if (simMarketData_->simulateDividendYield()) {
        generateDividendYieldScenarios(true);
        generateDividendYieldScenarios(false);
    }

    generateZeroInflationScenarios(true);
    generateZeroInflationScenarios(false);

    generateYoYInflationScenarios(true);
    generateYoYInflationScenarios(false);

    if (simMarketData_->simulateYoYInflationCapFloorVols()) {
        generateYoYInflationCapFloorVolScenarios(true);
        generateYoYInflationCapFloorVolScenarios(false);
    }

    if (simMarketData_->simulateZeroInflationCapFloorVols()) {
        generateZeroInflationCapFloorVolScenarios(true);
        generateZeroInflationCapFloorVolScenarios(false);
    }

    if (simMarketData_->simulateFXVols()) {
        generateFxVolScenarios(true);
        generateFxVolScenarios(false);
    }

    if (simMarketData_->simulateEquityVols()) {
        generateEquityVolScenarios(true);
        generateEquityVolScenarios(false);
    }

    if (simMarketData_->simulateSwapVols()) {
        generateSwaptionVolScenarios(true);
        generateSwaptionVolScenarios(false);
    }

    if (simMarketData_->simulateYieldVols()) {
        generateYieldVolScenarios(true);
        generateYieldVolScenarios(false);
    }

    if (simMarketData_->simulateCapFloorVols()) {
        generateCapFloorVolScenarios(true);
        generateCapFloorVolScenarios(false);
    }

    if (simMarketData_->simulateSurvivalProbabilities()) {
        generateSurvivalProbabilityScenarios(true);
        generateSurvivalProbabilityScenarios(false);
    }

    if (simMarketData_->simulateCdsVols()) {
        generateCdsVolScenarios(true);
        generateCdsVolScenarios(false);
    }

    if (simMarketData_->simulateBaseCorrelations()) {
        generateBaseCorrelationScenarios(true);
        generateBaseCorrelationScenarios(false);
    }

    if (simMarketData_->commodityCurveSimulate()) {
        generateCommodityCurveScenarios(true);
        generateCommodityCurveScenarios(false);
    }

    if (simMarketData_->commodityVolSimulate()) {
        generateCommodityVolScenarios(true);
        generateCommodityVolScenarios(false);
    }

    if (simMarketData_->securitySpreadsSimulate()) {
        generateSecuritySpreadScenarios(true);
        generateSecuritySpreadScenarios(false);
    }

    if (simMarketData_->simulateCorrelations()) {
        generateCorrelationScenarios(true);
        generateCorrelationScenarios(false);
    }

    // fill keyToFactor and factorToKey maps from scenario descriptions

    DLOG("Fill maps linking factors with RiskFactorKeys");
    keyToFactor_.clear();
    factorToKey_.clear();
    for (Size i = 0; i < scenarioDescriptions_.size(); ++i) {
        RiskFactorKey key = scenarioDescriptions_[i].key1();
        string factor = scenarioDescriptions_[i].factor1();
        keyToFactor_[key] = factor;
        factorToKey_[factor] = key;
        DLOG("KeyToFactor map: " << key << " to " << factor);
    }

    // add simultaneous up-moves in two risk factors for cross gamma calculation

    for (Size i = 0; i < scenarios_.size(); ++i) {
        ScenarioDescription iDesc = scenarioDescriptions_[i];
        if (iDesc.type() != ScenarioDescription::Type::Up)
            continue;
        string iKeyName = iDesc.keyName1();

        // check if iKey matches filter
        if (find_if(sensitivityData_->crossGammaFilter().begin(), sensitivityData_->crossGammaFilter().end(),
                    findFactor(iKeyName)) == sensitivityData_->crossGammaFilter().end())
            continue;

        for (Size j = i + 1; j < scenarios_.size(); ++j) {
            ScenarioDescription jDesc = scenarioDescriptions_[j];
            if (jDesc.type() != ScenarioDescription::Type::Up)
                continue;
            string jKeyName = jDesc.keyName1();

            // check if jKey matches filter
            if (find_if(sensitivityData_->crossGammaFilter().begin(), sensitivityData_->crossGammaFilter().end(),
                        findPair(iKeyName, jKeyName)) == sensitivityData_->crossGammaFilter().end())
                continue;

            // build cross scenario
            QuantLib::ext::shared_ptr<Scenario> crossScenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            for (auto const& k : baseScenario_->keys()) {
                Real v1 = scenarios_[i]->get(k);
                Real v2 = scenarios_[j]->get(k);
                Real b = baseScenario_->get(k);
                if (!close_enough(v1, b) || !close_enough(v2, b))
                    // this is correct for both absolute and relative shifts
                    crossScenario->add(k, v1 + v2 - b);
            }

            scenarioDescriptions_.push_back(ScenarioDescription(iDesc, jDesc));
            crossScenario->label(to_string(scenarioDescriptions_.back()));
            scenarios_.push_back(crossScenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << crossScenario->label() << " created");
        }
    }

    LOG("sensitivity scenario generator finished generating scenarios.");
}

namespace {
bool tryGetBaseScenarioValue(const QuantLib::ext::shared_ptr<Scenario> baseScenario, const RiskFactorKey& key, Real& value,
                             const bool continueOnError) {
    try {
        value = baseScenario->get(key);
        return true;
    } catch (const std::exception& e) {
        if (continueOnError) {
            ALOG("skip scenario generation for key " << key << ": " << e.what());
        } else {
            QL_FAIL(e.what());
        }
    }
    return false;
}
} // namespace

ShiftType SensitivityScenarioGenerator::getShiftType(SensitivityScenarioData::ShiftData& data) const {
    if (auto it = data.keyedShiftType.find(sensitivityTemplate_); it != data.keyedShiftType.end())
        return it->second;
    return data.shiftType;
}

Real SensitivityScenarioGenerator::getShiftSize(SensitivityScenarioData::ShiftData& data) const {
    if (auto it = data.keyedShiftSize.find(sensitivityTemplate_); it != data.keyedShiftSize.end())
        return it->second;
    return data.shiftSize;
}

ShiftScheme SensitivityScenarioGenerator::getShiftScheme(SensitivityScenarioData::ShiftData& data) const {
    if (auto it = data.keyedShiftScheme.find(sensitivityTemplate_); it != data.keyedShiftScheme.end())
        return it->second;
    return data.shiftScheme;
}

bool SensitivityScenarioGenerator::isScenarioRelevant(bool up, SensitivityScenarioData::ShiftData& data) const {
    ShiftScheme scheme = getShiftScheme(data);
    return sensitivityData_->computeGamma() || (up && (scheme == ShiftScheme::Forward)) ||
           (!up && scheme == ShiftScheme::Backward) || scheme == ShiftScheme::Central;
}

void SensitivityScenarioGenerator::storeShiftData(const RiskFactorKey& key, const Real rate, const Real newRate) {
    if (shiftSizes_.find(key) == shiftSizes_.end()) {
        shiftSizes_[key] = std::abs(newRate - rate);
        baseValues_[key] = rate;
    }
}

void SensitivityScenarioGenerator::generateFxScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer FX risk factors than listed in the market
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
    for (auto sensi_fx : sensitivityData_->fxShiftData()) {
        string foreign = sensi_fx.first.substr(0, 3);
        string domestic = sensi_fx.first.substr(3);
        QL_REQUIRE((domestic == baseCcy) || (foreign == baseCcy),
                   "SensitivityScenarioGenerator does not support cross FX pairs("
                       << sensi_fx.first << ", but base currency is " << baseCcy << ")");
    }
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_fx : simMarketData_->fxCcyPairs()) {
        if (sensitivityData_->fxShiftData().find(sim_fx) == sensitivityData_->fxShiftData().end()) {
            WLOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (auto sensi_fx : sensitivityData_->fxShiftData()) {
        string ccypair = sensi_fx.first; // foreign + domestic;
        SensitivityScenarioData::SpotShiftData data = sensi_fx.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType type = getShiftType(data);
        Real size = (up ? 1.0 : -1.0) * getShiftSize(data);
        bool relShift = (type == ShiftType::Relative);

        Real rate;
        RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
        if (!tryGetBaseScenarioValue(baseScenarioAbsolute_, key, rate, continueOnError_))
            continue;

        QuantLib::ext::shared_ptr<Scenario> scenario =
            sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(key, sensitivityData_->useSpreadedTermStructures() ? newRate / rate : newRate);

        storeShiftData(key, rate, newRate);


        scenarios_.push_back(scenario);
        scenarioDescriptions_.push_back(fxScenarioDescription(ccypair, up, getShiftScheme(data)));
        scenario->label(to_string(scenarioDescriptions_.back()));
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newRate);
    }
    DLOG("FX scenarios done");
}

void SensitivityScenarioGenerator::generateEquityScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some equities in simmarket are excluded from the sensitivities list
    for (auto sim_equity : simMarketData_->equityNames()) {
        if (sensitivityData_->equityShiftData().find(sim_equity) == sensitivityData_->equityShiftData().end()) {
            WLOG("Equity " << sim_equity << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (auto e : sensitivityData_->equityShiftData()) {
        string equity = e.first;
        SensitivityScenarioData::SpotShiftData data = e.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType type = getShiftType(data);
        Real size = up ? getShiftSize(data) : -1.0 * getShiftSize(data);
        bool relShift = (type == ShiftType::Relative);

        Real rate;
        RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
        if (!tryGetBaseScenarioValue(baseScenarioAbsolute_, key, rate, continueOnError_))
            continue;

        QuantLib::ext::shared_ptr<Scenario> scenario =
            sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        // Real newRate = up ? rate * (1.0 + getShiftSize(data)) : rate * (1.0 - getShiftSize(data));
        scenario->add(key, sensitivityData_->useSpreadedTermStructures() ? newRate / rate : newRate);

        storeShiftData(key, rate, newRate);

        scenarios_.push_back(scenario);
        scenarioDescriptions_.push_back(equityScenarioDescription(equity, up, getShiftScheme(data)));
        scenario->label(to_string(scenarioDescriptions_.back()));
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newRate);
    }
    DLOG("Equity scenarios done");
}

namespace {
void checkShiftTenors(const std::vector<Period>& effective, const std::vector<Period>& config,
                      const std::string& curveLabel, bool continueOnError = false) {
    if (effective.size() != config.size()) {
        string message = "mismatch between effective shift tenors (" + std::to_string(effective.size()) +
                         ") and configured shift tenors (" + std::to_string(config.size()) + ") for " + curveLabel;
        ALOG(message);
        for (auto const& p : effective)
            ALOG("effective tenor: " << p);
        for (auto const& p : config)
            ALOG("config   tenor: " << p);
        if (!continueOnError)
            QL_FAIL(message);
    }
}
} // namespace

void SensitivityScenarioGenerator::generateDiscountCurveScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_ccy : simMarketData_->ccys()) {
        if (sensitivityData_->discountCurveShiftData().find(sim_ccy) ==
            sensitivityData_->discountCurveShiftData().end()) {
            WLOG("Currency " << sim_ccy << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto c : sensitivityData_->discountCurveShiftData()) {
        string ccy = c.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->yieldCurveTenors(ccy).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for discount curve " << ccy << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->discountCurve(ccy)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for discount curve " << ccy << ", using default A365");
        }

        Real quote = 0.0;
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(ccy)[j];
            times[j] = dc.yearFraction(asof, d);

            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, quote, continueOnError_);
            zeros[j] = -std::log(quote) / times[j];
        }
        if (!valid)
            continue;

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(ccy)
                                              ? simMarketData_->yieldCurveTenors(ccy)
                                              : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "Discount Curve " + ccy, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());
            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                RiskFactorKey key(RFType::DiscountCurve, ccy, k);
                // FIXME why do we have that here, but not in generateIndexCurveScenarios?
                if (!close_enough(shiftedZeros[k], zeros[k])) {
                    Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                    if (sensitivityData_->useSpreadedTermStructures()) {
                        Real discount = exp(-zeros[k] * times[k]);
                        scenario->add(key, shiftedDiscount / discount);
                    } else {
                        scenario->add(key, shiftedDiscount);
                    }
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, zeros[k], shiftedZeros[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(discountScenarioDescription(ccy, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    DLOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateIndexCurveScenarios(bool up) {
    Date asof = baseScenario_->asof();

    for (auto sim_idx : simMarketData_->indices()) {
        if (sensitivityData_->indexCurveShiftData().find(sim_idx) == sensitivityData_->indexCurveShiftData().end()) {
            WLOG("Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto idx : sensitivityData_->indexCurveShiftData()) {
        string indexName = idx.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->yieldCurveTenors(indexName).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for index curve " << indexName << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        SensitivityScenarioData::CurveShiftData data = *idx.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->iborIndex(indexName)->forwardingTermStructure()->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for index " << indexName << ", using default A365");
        }

        Real quote = 0.0;
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(indexName)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, quote, continueOnError_);
            zeros[j] = -std::log(quote) / times[j];
        }
        if (!valid)
            continue;

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(indexName)
                                              ? simMarketData_->yieldCurveTenors(indexName)
                                              : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "Index Curve " + indexName, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Index shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                RiskFactorKey key(RFType::IndexCurve, indexName, k);

                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    Real discount = exp(-zeros[k] * times[k]);
                    scenario->add(key, shiftedDiscount / discount);
                } else {
                    scenario->add(key, shiftedDiscount);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, zeros[k], shiftedZeros[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(indexScenarioDescription(indexName, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    DLOG("Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYieldCurveScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer yield curves than listed in the market
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim_yc : simMarketData_->yieldCurveNames()) {
        if (sensitivityData_->yieldCurveShiftData().find(sim_yc) == sensitivityData_->yieldCurveShiftData().end()) {
            WLOG("Yield Curve " << sim_yc << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto y : sensitivityData_->yieldCurveShiftData()) {
        string name = y.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->yieldCurveTenors(name).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for yield curve " << name << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = *y.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->yieldCurve(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for yield curve " << name << ", using default A365");
        }

        Real quote = 0.0;
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, quote, continueOnError_);
            zeros[j] = -std::log(quote) / times[j];
        }
        if (!valid)
            continue;

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(name)
                                                     ? simMarketData_->yieldCurveTenors(name)
                                                     : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "Yield Curve " + name, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                RiskFactorKey key(RFType::YieldCurve, name, k);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    Real discount = exp(-zeros[k] * times[k]);
                    scenario->add(key, shiftedDiscount / discount);
                } else {
                    scenario->add(key, shiftedDiscount);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, zeros[k], shiftedZeros[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(yieldScenarioDescription(name, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    DLOG("Yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateDividendYieldScenarios(bool up) {
    Date asof = baseScenario_->asof();

    // We can choose to shift fewer yield curves than listed in the market
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim : simMarketData_->equityNames()) {
        if (sensitivityData_->dividendYieldShiftData().find(sim) == sensitivityData_->dividendYieldShiftData().end()) {
            WLOG("Equity " << sim << " in simmarket is not included in dividend yield sensitivity analysis");
        }
    }

    for (auto d : sensitivityData_->dividendYieldShiftData()) {
        string name = d.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->equityDividendTenors(name).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for div yield " << name << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = *d.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->equityDividendCurve(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for dividend yield curve " << name
                                                                                            << ", using default A365");
        }

        Real quote = 0.0;
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->equityDividendTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::DividendYield, name, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, quote, continueOnError_);
            zeros[j] = -std::log(quote) / times[j];
        }
        if (!valid)
            continue;

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasEquityDividendTenors(name)
                                                     ? simMarketData_->equityDividendTenors(name)
                                                     : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "Dividend Yield " + name, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                RiskFactorKey key(RFType::DividendYield, name, k);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    Real discount = exp(-zeros[k] * times[k]);
                    scenario->add(key, shiftedDiscount / discount);
                } else {
                    scenario->add(key, shiftedDiscount);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, zeros[k], shiftedZeros[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(dividendYieldScenarioDescription(name, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    DLOG("Dividend yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateFxVolScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some FX vol pairs in simmarket are excluded from the list
    for (auto sim_fx : simMarketData_->fxVolCcyPairs()) {
        if (sensitivityData_->fxVolShiftData().find(sim_fx) == sensitivityData_->fxVolShiftData().end()) {
            WLOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto f : sensitivityData_->fxVolShiftData()) {
        string ccyPair = f.first;
        QL_REQUIRE(ccyPair.length() == 6, "invalid ccy pair length");

        Size n_fxvol_exp;
        try {
            n_fxvol_exp = simMarketData_->fxVolExpiries(ccyPair).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for fx vol " << ccyPair << ": " << e.what());
            continue;
        }
        std::vector<Real> times(n_fxvol_exp);
        Size n_fxvol_strikes;
        vector<Real> vol_strikes;
        if (!simMarketData_->fxVolIsSurface(ccyPair)) {
            vol_strikes = {0.0};
            n_fxvol_strikes = 1;
        } else if (simMarketData_->fxUseMoneyness(ccyPair)) {
            n_fxvol_strikes = simMarketData_->fxVolMoneyness(ccyPair).size();
            vol_strikes = simMarketData_->fxVolMoneyness(ccyPair);
        } else {
            n_fxvol_strikes = simMarketData_->fxVolStdDevs(ccyPair).size();
            vol_strikes = simMarketData_->fxVolStdDevs(ccyPair);
        }
        vector<vector<Real>> values(n_fxvol_exp, vector<Real>(n_fxvol_strikes, 0.0));

        // buffer for shifted zero curves
        vector<vector<Real>> shiftedValues(n_fxvol_exp, vector<Real>(n_fxvol_strikes, 0.0));

        SensitivityScenarioData::VolShiftData data = f.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Real> shiftStrikes = data.shiftStrikes;
        std::vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->fxVol(ccyPair)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for fx vol surface " << ccyPair
                                                                                      << ", using default A365");
        }
        bool valid = true;
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = asof + simMarketData_->fxVolExpiries(ccyPair)[j];
            times[j] = dc.yearFraction(asof, d);
            for (Size k = 0; k < n_fxvol_strikes; k++) {
                Size idx = k * n_fxvol_exp + j;
                RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccyPair, idx);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, values[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes) && (vectorEqual(vol_strikes, shiftStrikes) ||
                                                                 (vol_strikes.size() == 1 && shiftStrikes.size() == 1));

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            for (Size strikeBucket = 0; strikeBucket < shiftStrikes.size(); ++strikeBucket) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(j, strikeBucket, shiftSize, up, shiftType, shiftTimes, shiftStrikes, times, vol_strikes,
                           values, shiftedValues, true);

                for (Size k = 0; k < n_fxvol_strikes; ++k) {
                    for (Size l = 0; l < n_fxvol_exp; ++l) {
                        Size idx = k * n_fxvol_exp + l;
                        RiskFactorKey key(RFType::FXVolatility, ccyPair, idx);

                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedValues[l][k] - values[l][k]);
                        } else {
                            scenario->add(key, shiftedValues[l][k]);
                        }

                        // Possibly store valid shift size
                        if (validShiftSize && j == l && strikeBucket == k) {
                            storeShiftData(key, values[l][k], shiftedValues[l][k]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    fxVolScenarioDescription(ccyPair, j, strikeBucket, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("FX vol scenarios done");
}

void SensitivityScenarioGenerator::generateEquityVolScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if an Equity in simmarket are excluded from the simulation list
    for (auto sim_equity : simMarketData_->equityVolNames()) {
        if (sensitivityData_->equityVolShiftData().find(sim_equity) == sensitivityData_->equityVolShiftData().end()) {
            WLOG("Equity " << sim_equity << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto e : sensitivityData_->equityVolShiftData()) {
        string equity = e.first;
        SensitivityScenarioData::VolShiftData data = e.second;
        if (!isScenarioRelevant(up, data))
            continue;

        Size n_eqvol_exp;
        try {
            n_eqvol_exp = simMarketData_->equityVolExpiries(equity).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for eq vol " << equity << ": " << e.what());
            continue;
        }
        Size n_eqvol_strikes;
        vector<Real> vol_strikes;
        if (!simMarketData_->equityVolIsSurface(equity)) {
            vol_strikes = {0.0};
            n_eqvol_strikes = 1;
        } else if (simMarketData_->equityUseMoneyness(equity)) {
            vol_strikes = simMarketData_->equityVolMoneyness(equity);
            n_eqvol_strikes = simMarketData_->equityVolMoneyness(equity).size();
        } else {
            vol_strikes = simMarketData_->equityVolStandardDevs(equity);
            n_eqvol_strikes = simMarketData_->equityVolStandardDevs(equity).size();
        }

        // [strike] x [expiry]
        vector<vector<Real>> values(n_eqvol_strikes, vector<Real>(n_eqvol_exp, 0.0));
        vector<Real> times(n_eqvol_exp);

        // buffer for shifted vols
        vector<vector<Real>> shiftedValues(n_eqvol_strikes, vector<Real>(n_eqvol_exp, 0.0));

        ShiftType shiftType = getShiftType(data);
        vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Real> shiftStrikes = data.shiftStrikes;
        vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Equity vol shift tenors not specified");
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->equityVol(equity)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for equity vol surface " << equity
                                                                                          << ", using default A365");
        }
        bool valid = true;
        for (Size j = 0; j < n_eqvol_exp; ++j) {
            Date d = asof + simMarketData_->equityVolExpiries(equity)[j];
            times[j] = dc.yearFraction(asof, d);
            for (Size k = 0; k < n_eqvol_strikes; k++) {
                Size idx = k * n_eqvol_exp + j;
                RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, idx);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, values[k][j], continueOnError_);
            }
        }
        if (!valid)
            continue;

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        }

        // Can we store a valid shift size?
        // Will only work currently if simulation market has a single strike
        bool validShiftSize = vectorEqual(times, shiftTimes);
        validShiftSize = validShiftSize && vectorEqual(vol_strikes, shiftStrikes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            for (Size strikeBucket = 0; strikeBucket < shiftStrikes.size(); ++strikeBucket) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(strikeBucket, j, shiftSize, up, shiftType, shiftStrikes, shiftTimes, vol_strikes, times,
                           values, shiftedValues, true);

                // update the scenario
                for (Size k = 0; k < n_eqvol_strikes; ++k) {
                    for (Size l = 0; l < n_eqvol_exp; l++) {
                        Size idx = k * n_eqvol_exp + l;
                        RiskFactorKey key(RFType::EquityVolatility, equity, idx);

                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedValues[k][l] - values[k][l]);
                        } else {
                            scenario->add(key, shiftedValues[k][l]);
                        }

                        // Possibly store valid shift size
                        if (validShiftSize && j == l && k == strikeBucket) {
                            storeShiftData(key, values[k][l], shiftedValues[k][l]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    equityVolScenarioDescription(equity, j, strikeBucket, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Equity vol scenarios done");
}

void SensitivityScenarioGenerator::generateGenericYieldVolScenarios(bool up, RiskFactorKey::KeyType rfType) {

    Date asof = baseScenario_->asof();

    // set parameters for swaption resp. yield vol scenarios

    bool atmOnly;
    map<string, SensitivityScenarioData::GenericYieldVolShiftData> shiftData;
    function<Size(string)> get_n_term;
    function<Size(string)> get_n_expiry;
    function<vector<Real>(string)> getVolStrikes;
    function<vector<Period>(string)> getVolExpiries;
    function<vector<Period>(string)> getVolTerms;
    function<string(string)> getDayCounter;
    function<ScenarioDescription(string, Size, Size, Size, bool, SensitivityScenarioData::ShiftData&)>
        getScenarioDescription;

    if (rfType == RFType::SwaptionVolatility) {
        atmOnly = simMarketData_->simulateSwapVolATMOnly();
        shiftData = sensitivityData_->swaptionVolShiftData();
        get_n_term = [this](const string& k) { return simMarketData_->swapVolTerms(k).size(); };
        get_n_expiry = [this](const string& k) { return simMarketData_->swapVolExpiries(k).size(); };
        getVolStrikes = [this](const string& k) { return simMarketData_->swapVolStrikeSpreads(k); };
        getVolExpiries = [this](const string& k) { return simMarketData_->swapVolExpiries(k); };
        getVolTerms = [this](const string& k) { return simMarketData_->swapVolTerms(k); };
        getDayCounter = [this](const string& k) {
            try {
                if (auto s = simMarket_.lock()) {
                    return to_string(s->swaptionVol(k)->dayCounter());
                } else {
                    QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
                }
            } catch (const std::exception&) {
                WLOG("Day counter lookup in simulation market failed for swaption vol '" << k
                                                                                         << "', using default A365");
                return std::string("A365F");
            }
        };
        getScenarioDescription = [this](string q, Size n, Size m, Size k, bool up,
                                        SensitivityScenarioData::ShiftData& data) {
            return swaptionVolScenarioDescription(q, n, m, k, up, getShiftScheme(data));
        };
    } else if (rfType == RFType::YieldVolatility) {
        atmOnly = true;
        shiftData = sensitivityData_->yieldVolShiftData();
        get_n_term = [this](const string& k) { return simMarketData_->yieldVolTerms().size(); };
        get_n_expiry = [this](const string& k) { return simMarketData_->yieldVolExpiries().size(); };
        getVolStrikes = [](const string& k) { return vector<Real>({0.0}); };
        getVolExpiries = [this](const string& k) { return simMarketData_->yieldVolExpiries(); };
        getVolTerms = [this](const string& k) { return simMarketData_->yieldVolTerms(); };
        getDayCounter = [this](const string& k) {
            try {
                if (auto s = simMarket_.lock()) {
                    return to_string(s->yieldVol(k)->dayCounter());
                } else {
                    QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
                }
            } catch (const std::exception&) {
                WLOG("Day counter lookup in simulation market failed for swaption vol '" << k
                                                                                         << "', using default A365");
                return std::string("A365F");
            }
        };
        getScenarioDescription = [this](string q, Size n, Size m, Size k, bool up,
                                        SensitivityScenarioData::ShiftData& data) {
            return yieldVolScenarioDescription(q, n, m, up, getShiftScheme(data));
        };
    } else {
        QL_FAIL("SensitivityScenarioGenerator::generateGenericYieldVolScenarios: risk factor type " << rfType
                                                                                                    << " not handled.");
    }

    // generate scenarios
    for (auto s : shiftData) {
        std::string qualifier = s.first;

        Size n_term;
        try {
            n_term = get_n_term(qualifier);
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for general yield vol " << qualifier << ": " << e.what());
            continue;
        }
        Size n_expiry = get_n_expiry(qualifier);

        vector<Real> volExpiryTimes(n_expiry, 0.0);
        vector<Real> volTermTimes(n_term, 0.0);
        Size n_strike = getVolStrikes(qualifier).size();

        vector<vector<vector<Real>>> volData(n_strike, vector<vector<Real>>(n_expiry, vector<Real>(n_term, 0.0)));
        vector<vector<vector<Real>>> shiftedVolData = volData;

        SensitivityScenarioData::GenericYieldVolShiftData data = s.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);

        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        vector<Real> shiftStrikes;
        if (!atmOnly) {
            shiftStrikes = data.shiftStrikes;
            QL_REQUIRE(data.shiftStrikes.size() == n_strike,
                       "number of simulated strikes must equal number of sensitivity strikes");
        } else {
            shiftStrikes = {0.0};
        }

        DayCounter dc = parseDayCounter(getDayCounter(qualifier));

        // cache original vol data
        for (Size j = 0; j < n_expiry; ++j) {
            Date expiry = asof + getVolExpiries(qualifier)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_term; ++j) {
            Date term = asof + getVolTerms(qualifier)[j];
            volTermTimes[j] = dc.yearFraction(asof, term);
        }

        bool valid = true;
        for (Size j = 0; j < n_expiry; ++j) {
            for (Size k = 0; k < n_term; ++k) {
                for (Size l = 0; l < n_strike; ++l) {
                    Size idx = j * n_term * n_strike + k * n_strike + l;
                    RiskFactorKey key(rfType, qualifier, idx);
                    valid = valid &&
                            tryGetBaseScenarioValue(baseScenarioAbsolute_, key, volData[l][j][k], continueOnError_);
                }
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(asof, asof + data.shiftTerms[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(volExpiryTimes, shiftExpiryTimes);
        validShiftSize = validShiftSize && vectorEqual(volTermTimes, shiftTermTimes);
        validShiftSize = validShiftSize && vectorEqual(getVolStrikes(qualifier), shiftStrikes);

        // loop over shift expiries, terms and strikes
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                for (Size l = 0; l < shiftStrikes.size(); ++l) {
                    Size strikeBucket = l;
                    QuantLib::ext::shared_ptr<Scenario> scenario =
                        sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                    // if simulating atm only we shift all strikes otherwise we shift each strike individually
                    Size loopStart = atmOnly ? 0 : l;
                    Size loopEnd = atmOnly ? n_strike : loopStart + 1;

                    for (Size ll = loopStart; ll < loopEnd; ++ll) {
                        applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftTermTimes, volExpiryTimes,
                                   volTermTimes, volData[ll], shiftedVolData[ll], true);
                    }

                    for (Size jj = 0; jj < n_expiry; ++jj) {
                        for (Size kk = 0; kk < n_term; ++kk) {
                            for (Size ll = 0; ll < n_strike; ++ll) {

                                Size idx = jj * n_term * n_strike + kk * n_strike + ll;
                                RiskFactorKey key(rfType, qualifier, idx);

                                if (ll >= loopStart && ll < loopEnd) {
                                    if (sensitivityData_->useSpreadedTermStructures()) {
                                        scenario->add(key, shiftedVolData[ll][jj][kk] - volData[ll][jj][kk]);
                                    } else {
                                        scenario->add(key, shiftedVolData[ll][jj][kk]);
                                    }
                                }

                                // Possibly store valid shift size
                                if (validShiftSize && j == jj && k == kk && l == ll) {
                                    storeShiftData(key, volData[ll][jj][kk], shiftedVolData[ll][jj][kk]);
                                }
                            }
                        }
                    }

                    // add this scenario to the scenario vector
                    scenarios_.push_back(scenario);
                    scenarioDescriptions_.push_back(getScenarioDescription(qualifier, j, k, strikeBucket, up, data));
                    scenario->label(to_string(scenarioDescriptions_.back()));
                    DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                                   << " created for generic yield vol " << qualifier);
                }
            }
        }
    }
}

void SensitivityScenarioGenerator::generateSwaptionVolScenarios(bool up) {
    DLOG("starting swapVol sgen");
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some swaption currencies in simmarket are excluded from the list
    for (auto sim_key : simMarketData_->swapVolKeys()) {
        if (sensitivityData_->swaptionVolShiftData().find(sim_key) == sensitivityData_->swaptionVolShiftData().end()) {
            WLOG("Swaption key " << sim_key << " in simmarket is not included in sensitivities analysis");
        }
    }
    generateGenericYieldVolScenarios(up, RFType::SwaptionVolatility);
    DLOG("Swaption vol scenarios done");
}

void SensitivityScenarioGenerator::generateYieldVolScenarios(bool up) {
    DLOG("starting yieldVol sgen");
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some bond securityId in simmarket are excluded from the list
    for (auto sim_securityId : simMarketData_->yieldVolNames()) {
        if (sensitivityData_->yieldVolShiftData().find(sim_securityId) == sensitivityData_->yieldVolShiftData().end()) {
            WLOG("Bond securityId " << sim_securityId << " in simmarket is not included in sensitivities analysis");
        }
    }
    generateGenericYieldVolScenarios(up, RFType::YieldVolatility);
    DLOG("Yield vol scenarios done");
}

void SensitivityScenarioGenerator::generateCapFloorVolScenarios(bool up) {
    Date asof = baseScenario_->asof();

    // Log an ALERT if some cap currencies in simmarket are excluded from the list
    for (auto sim_cap : simMarketData_->capFloorVolKeys()) {
        if (sensitivityData_->capFloorVolShiftData().find(sim_cap) == sensitivityData_->capFloorVolShiftData().end()) {
            WLOG("CapFloor key " << sim_cap << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto c : sensitivityData_->capFloorVolShiftData()) {
        std::string key = c.first;

        vector<Real> volStrikes;
        try {
            volStrikes = simMarketData_->capFloorVolStrikes(key);
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for cf vol " << key << ": " << e.what());
            continue;
        }
        // Strikes may be empty which indicates that the optionlet structure in the simulation market is an ATM curve
        if (volStrikes.empty()) {
            volStrikes = {0.0};
        }
        Size n_cfvol_strikes = volStrikes.size();

        Size n_cfvol_exp = simMarketData_->capFloorVolExpiries(key).size();
        SensitivityScenarioData::CapFloorVolShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);
        vector<vector<Real>> volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

        std::vector<Period> expiries = overrideTenors_ && simMarketData_->hasCapFloorVolExpiries(key)
                                           ? simMarketData_->capFloorVolExpiries(key)
                                           : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(), "mismatch between effective shift expiries ("
                                                                     << expiries.size() << ") and shift tenors ("
                                                                     << data.shiftExpiries.size());
        vector<Real> shiftExpiryTimes(expiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;
        // Has an ATM shift been configured?
        bool sensiIsAtm = false;
        if (shiftStrikes.size() == 1 && shiftStrikes[0] == 0.0 && data.isRelative) {
            sensiIsAtm = true;
        }

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->capFloorVol(key)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for cap/floor vol surface " << key
                                                                                             << ", using default A365");
        }

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = asof + simMarketData_->capFloorVolExpiries(key)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        bool valid = true;
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Size idx = j * n_cfvol_strikes + k;
                valid = valid &&
                        tryGetBaseScenarioValue(baseScenarioAbsolute_,
                                                RiskFactorKey(RiskFactorKey::KeyType::OptionletVolatility, key, idx),
                                                volData[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + expiries[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(volExpiryTimes, shiftExpiryTimes);
        validShiftSize = validShiftSize && vectorEqual(volStrikes, shiftStrikes);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);

                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                        Size idx = jj * n_cfvol_strikes + kk;
                        RiskFactorKey rfkey(RFType::OptionletVolatility, key, idx);

                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(rfkey, shiftedVolData[jj][kk] - volData[jj][kk]);
                        } else {
                            scenario->add(rfkey, shiftedVolData[jj][kk]);
                        }

                        // Possibly store valid shift size
                        if (validShiftSize && j == jj && k == kk) {
                            storeShiftData(rfkey, volData[jj][kk], shiftedVolData[jj][kk]);
                        }
                    }
                }

                // Give the scenario a label

                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    capFloorVolScenarioDescription(key, j, k, up, sensiIsAtm, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Optionlet vol scenarios done");
}

void SensitivityScenarioGenerator::generateSurvivalProbabilityScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer credit curves than listed in the market
    // Log an ALERT if some names in simmarket are excluded from the list
    for (auto sim_name : simMarketData_->defaultNames()) {
        if (sensitivityData_->creditCurveShiftData().find(sim_name) == sensitivityData_->creditCurveShiftData().end()) {
            WLOG("Credit Name " << sim_name << " in simmarket is not included in sensitivities analysis");
        }
    }
    Size n_ten;

    // original curves' buffer
    std::vector<Real> times;

    for (auto c : sensitivityData_->creditCurveShiftData()) {
        string name = c.first;
        try {
            n_ten = simMarketData_->defaultTenors(name).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for survival curve " << name << ": " << e.what());
            continue;
        }
        std::vector<Real> hazardRates(n_ten); // integrated hazard rates
        times.clear();
        times.resize(n_ten);
        // buffer for shifted survival prob curves
        std::vector<Real> shiftedHazardRates(n_ten);
        SensitivityScenarioData::CurveShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->defaultCurve(name)->curve()->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for default curve " << name << ", using default A365");
        }
        Calendar calendar = parseCalendar(simMarketData_->defaultCurveCalendar(name));

        Real prob = 0.0;
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->defaultTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, prob, continueOnError_);
            // ensure we have a valid value, if prob = 0 we need to avoid nan to generate valid scenarios
            hazardRates[j] = -std::log(std::max(prob, 1E-8)) / times[j];
        }
        if (!valid)
            continue;

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasDefaultTenors(name)
                                              ? simMarketData_->defaultTenors(name)
                                              : data.shiftTenors;

        checkShiftTenors(shiftTenors, data.shiftTenors, "Default Curve " + name, continueOnError_);

        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply averaged hazard rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, hazardRates, times, shiftedHazardRates, true);

            // store shifted survival Prob in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                RiskFactorKey key(RFType::SurvivalProbability, name, k);
                Real shiftedProb = exp(-shiftedHazardRates[k] * times[k]);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    Real prob = exp(-hazardRates[k] * times[k]);
                    scenario->add(key, shiftedProb / prob);
                } else {
                    scenario->add(key, shiftedProb);
                }

                // Possibly store valid shift size
                if (validShiftSize && k == j) {
                    storeShiftData(key, hazardRates[k], shiftedHazardRates[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(survivalProbabilityScenarioDescription(name, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    DLOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateCdsVolScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some swaption currencies in simmarket are excluded from the list
    for (auto sim_name : simMarketData_->cdsVolNames()) {
        if (sensitivityData_->cdsVolShiftData().find(sim_name) == sensitivityData_->cdsVolShiftData().end()) {
            WLOG("CDS name " << sim_name << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_cdsvol_exp = simMarketData_->cdsVolExpiries().size();

    vector<Real> volData(n_cdsvol_exp, 0.0);
    vector<Real> volExpiryTimes(n_cdsvol_exp, 0.0);
    vector<Real> shiftedVolData(n_cdsvol_exp, 0.0);

    for (auto c : sensitivityData_->cdsVolShiftData()) {
        std::string name = c.first;
        SensitivityScenarioData::CdsVolShiftData data = c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);

        vector<Time> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->cdsVol(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for cds vol surface " << name
                                                                                       << ", using default A365");
        }

        // cache original vol data
        for (Size j = 0; j < n_cdsvol_exp; ++j) {
            Date expiry = asof + simMarketData_->cdsVolExpiries()[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        bool valid = true;
        for (Size j = 0; j < n_cdsvol_exp; ++j) {
            RiskFactorKey key(RiskFactorKey::KeyType::CDSVolatility, name, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, volData[j], continueOnError_);
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(volExpiryTimes, shiftExpiryTimes);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            Size strikeBucket = 0; // FIXME
            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            applyShift(j, shiftSize, up, shiftType, shiftExpiryTimes, volData, volExpiryTimes, shiftedVolData, true);
            // add shifted vol data to the scenario
            for (Size jj = 0; jj < n_cdsvol_exp; ++jj) {
                RiskFactorKey key(RFType::CDSVolatility, name, jj);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    scenario->add(key, shiftedVolData[jj] - volData[jj]);
                } else {
                    scenario->add(key, shiftedVolData[jj]);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == jj) {
                    storeShiftData(key, volData[jj], shiftedVolData[jj]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(CdsVolScenarioDescription(name, j, strikeBucket, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    DLOG("CDS vol scenarios done");
}

void SensitivityScenarioGenerator::generateZeroInflationScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketData_->zeroInflationIndices()) {
        if (sensitivityData_->zeroInflationCurveShiftData().find(sim_idx) ==
            sensitivityData_->zeroInflationCurveShiftData().end()) {
            WLOG("Zero Inflation Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto z : sensitivityData_->zeroInflationCurveShiftData()) {
        string indexName = z.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->zeroInflationTenors(indexName).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for zero inflation curve " << indexName << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = *z.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->zeroInflationIndex(indexName)->zeroInflationTermStructure()->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for zero inflation index " << indexName
                                                                                            << ", using default A365");
        }
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->zeroInflationTenors(indexName)[j];
            RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCurve, indexName, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, zeros[j], continueOnError_);
            times[j] = dc.yearFraction(asof, d);
        }
        if (!valid)
            continue;

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasZeroInflationTenors(indexName)
                                              ? simMarketData_->zeroInflationTenors(indexName)
                                              : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "Zero Inflation " + indexName, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "Zero Inflation Index shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                RiskFactorKey key(RFType::ZeroInflationCurve, indexName, k);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    scenario->add(key, shiftedZeros[k] - zeros[k]);
                } else {
                    scenario->add(key, shiftedZeros[k]);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, zeros[k], shiftedZeros[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(zeroInflationScenarioDescription(indexName, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    DLOG("Zero Inflation Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYoYInflationScenarios(bool up) {
    Date asof = baseScenario_->asof();

    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> yoyInfIndexNames;
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketData_->yoyInflationIndices()) {
        if (sensitivityData_->yoyInflationCurveShiftData().find(sim_idx) ==
            sensitivityData_->yoyInflationCurveShiftData().end()) {
            WLOG("YoY Inflation Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto y : sensitivityData_->yoyInflationCurveShiftData()) {
        string indexName = y.first;
        Size n_ten;
        try {
            n_ten = simMarketData_->yoyInflationTenors(indexName).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for yoy inflation curve " << indexName << ": " << e.what());
            continue;
        }
        // original curves' buffer
        std::vector<Real> yoys(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedYoys(n_ten);
        auto itr = sensitivityData_->yoyInflationCurveShiftData().find(indexName);
        QL_REQUIRE(itr != sensitivityData_->yoyInflationCurveShiftData().end(),
                   "yoyinflation CurveShiftData not found for " << indexName);
        SensitivityScenarioData::CurveShiftData data = *y.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->yoyInflationIndex(indexName)->yoyInflationTermStructure()->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for yoy inflation index " << indexName
                                                                                           << ", using default A365");
        }
        bool valid = true;
        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yoyInflationTenors(indexName)[j];
            RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCurve, indexName, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, yoys[j], continueOnError_);
            times[j] = dc.yearFraction(asof, d);
        }
        if (!valid)
            continue;

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYoyInflationTenors(indexName)
                                              ? simMarketData_->yoyInflationTenors(indexName)
                                              : data.shiftTenors;
        checkShiftTenors(shiftTenors, data.shiftTenors, "YoY Inflation " + indexName, continueOnError_);
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j) {
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);
        }
        Real shiftSize = getShiftSize(data);
        QL_REQUIRE(shiftTenors.size() > 0, "YoY Inflation Index shift tenors not specified");

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, yoys, times, shiftedYoys, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                RiskFactorKey key(RFType::YoYInflationCurve, indexName, k);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    scenario->add(key, shiftedYoys[k] - yoys[k]);
                } else {
                    scenario->add(key, shiftedYoys[k]);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, yoys[k], shiftedYoys[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(yoyInflationScenarioDescription(indexName, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                           << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    DLOG("YoY Inflation Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYoYInflationCapFloorVolScenarios(bool up) {
    Date asof = baseScenario_->asof();
    for (auto sim_yoy : simMarketData_->yoyInflationCapFloorVolNames()) {
        if (sensitivityData_->yoyInflationCapFloorVolShiftData().find(sim_yoy) ==
            sensitivityData_->yoyInflationCapFloorVolShiftData().end()) {
            WLOG("Inflation index " << sim_yoy << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto c : sensitivityData_->yoyInflationCapFloorVolShiftData()) {
        std::string name = c.first;
        Size n_yoyvol_strikes;
        try {
            n_yoyvol_strikes = simMarketData_->yoyInflationCapFloorVolStrikes(name).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for yoy inflation cf vol " << name << ": " << e.what());
            continue;
        }
        vector<Real> volStrikes = simMarketData_->yoyInflationCapFloorVolStrikes(name);
        Size n_yoyvol_exp = simMarketData_->yoyInflationCapFloorVolExpiries(name).size();
        SensitivityScenarioData::VolShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);
        vector<vector<Real>> volData(n_yoyvol_exp, vector<Real>(n_yoyvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_yoyvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_yoyvol_exp, vector<Real>(n_yoyvol_strikes, 0.0));

        std::vector<Period> expiries = overrideTenors_ && simMarketData_->hasYoYInflationCapFloorVolExpiries(name)
                                           ? simMarketData_->yoyInflationCapFloorVolExpiries(name)
                                           : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(), "mismatch between effective shift expiries ("
                                                                     << expiries.size() << ") and shift tenors ("
                                                                     << data.shiftExpiries.size());
        vector<Real> shiftExpiryTimes(expiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->yoyCapFloorVol(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for yoy cap/floor vol surface "
                 << name << ", using default A365");
        }

        // cache original vol data
        for (Size j = 0; j < n_yoyvol_exp; ++j) {
            Date expiry = asof + simMarketData_->yoyInflationCapFloorVolExpiries(name)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        bool valid = true;
        for (Size j = 0; j < n_yoyvol_exp; ++j) {
            for (Size k = 0; k < n_yoyvol_strikes; ++k) {
                Size idx = j * n_yoyvol_strikes + k;
                RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, name, idx);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, volData[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + expiries[j]);

        bool validShiftSize = vectorEqual(volExpiryTimes, shiftExpiryTimes);
        validShiftSize = validShiftSize && vectorEqual(volStrikes, shiftStrikes);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);

                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_yoyvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_yoyvol_strikes; ++kk) {
                        Size idx = jj * n_yoyvol_strikes + kk;
                        RiskFactorKey key(RFType::YoYInflationCapFloorVolatility, name, idx);
                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedVolData[jj][kk] - volData[jj][kk]);
                        } else {
                            scenario->add(key, shiftedVolData[jj][kk]);
                        }

                        // Possibly store valid shift size
                        if (validShiftSize && j == jj && k == kk) {
                            storeShiftData(key, volData[jj][kk], shiftedVolData[jj][kk]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    yoyInflationCapFloorVolScenarioDescription(name, j, k, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("YoY inflation optionlet vol scenarios done");
}

void SensitivityScenarioGenerator::generateZeroInflationCapFloorVolScenarios(bool up) {
    Date asof = baseScenario_->asof();
    for (auto sim_zci : simMarketData_->zeroInflationCapFloorVolNames()) {
        if (sensitivityData_->zeroInflationCapFloorVolShiftData().find(sim_zci) ==
            sensitivityData_->zeroInflationCapFloorVolShiftData().end()) {
            WLOG("Inflation index " << sim_zci << " in simmarket is not included in sensitivities analysis");
        }
    }

    for (auto c : sensitivityData_->zeroInflationCapFloorVolShiftData()) {
        std::string name = c.first;
        Size n_strikes;
        try {
            n_strikes = simMarketData_->zeroInflationCapFloorVolStrikes(name).size();
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for zero inflation cf vol " << name << ": " << e.what());
            continue;
        }
        Size n_exp = simMarketData_->zeroInflationCapFloorVolExpiries(name).size();
        vector<Real> volStrikes = simMarketData_->zeroInflationCapFloorVolStrikes(name);
        SensitivityScenarioData::VolShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);
        vector<vector<Real>> volData(n_exp, vector<Real>(n_strikes, 0.0));
        vector<Real> volExpiryTimes(n_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_exp, vector<Real>(n_strikes, 0.0));

        std::vector<Period> expiries = overrideTenors_ && simMarketData_->hasZeroInflationCapFloorVolExpiries(name)
                                           ? simMarketData_->zeroInflationCapFloorVolExpiries(name)
                                           : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(), "mismatch between effective shift expiries ("
                                                                     << expiries.size() << ") and shift tenors ("
                                                                     << data.shiftExpiries.size());
        vector<Real> shiftExpiryTimes(expiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->cpiInflationCapFloorVolatilitySurface(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for cpi cap/floor vol surface "
                 << name << ", using default A365");
        }

        // cache original vol data
        for (Size j = 0; j < n_exp; ++j) {
            Date expiry = asof + simMarketData_->zeroInflationCapFloorVolExpiries(name)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        bool valid = true;
        for (Size j = 0; j < n_exp; ++j) {
            for (Size k = 0; k < n_strikes; ++k) {
                Size idx = j * n_strikes + k;
                RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, name, idx);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, volData[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + expiries[j]);

        bool validShiftSize = vectorEqual(volExpiryTimes, shiftExpiryTimes);
        validShiftSize = validShiftSize && vectorEqual(volStrikes, shiftStrikes);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);

                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_exp; ++jj) {
                    for (Size kk = 0; kk < n_strikes; ++kk) {
                        Size idx = jj * n_strikes + kk;
                        RiskFactorKey key(RFType::ZeroInflationCapFloorVolatility, name, idx);
                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedVolData[jj][kk] - volData[jj][kk]);
                        } else {
                            scenario->add(key, shiftedVolData[jj][kk]);
                        }

                        // Possibly store valid shift size
                        if (validShiftSize && j == jj && k == kk) {
                            storeShiftData(key, volData[jj][kk], shiftedVolData[jj][kk]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    zeroInflationCapFloorVolScenarioDescription(name, j, k, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Zero inflation cap/floor vol scenarios done");
}

void SensitivityScenarioGenerator::generateBaseCorrelationScenarios(bool up) {
    Date asof = baseScenario_->asof();
    // We can choose to shift fewer discount curves than listed in the market
    // Log an ALERT if some names in simmarket are excluded from the list
    for (auto name : simMarketData_->baseCorrelationNames()) {
        if (sensitivityData_->baseCorrelationShiftData().find(name) ==
            sensitivityData_->baseCorrelationShiftData().end()) {
            WLOG("Base Correlation " << name << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_bc_terms = simMarketData_->baseCorrelationTerms().size();
    Size n_bc_levels = simMarketData_->baseCorrelationDetachmentPoints().size();

    vector<vector<Real>> bcData(n_bc_levels, vector<Real>(n_bc_terms, 0.0));
    vector<vector<Real>> shiftedBcData(n_bc_levels, vector<Real>(n_bc_levels, 0.0));
    vector<Real> termTimes(n_bc_terms, 0.0);
    vector<Real> levels = simMarketData_->baseCorrelationDetachmentPoints();

    for (auto b : sensitivityData_->baseCorrelationShiftData()) {
        std::string name = b.first;
        SensitivityScenarioData::BaseCorrelationShiftData data = b.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);

        vector<Real> shiftLevels = data.shiftLossLevels;
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->baseCorrelation(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for base correlation structure "
                 << name << ", using default A365");
        }

        // cache original base correlation data
        for (Size j = 0; j < n_bc_terms; ++j) {
            Date term = asof + simMarketData_->baseCorrelationTerms()[j];
            termTimes[j] = dc.yearFraction(asof, term);
        }
        bool valid = true;
        for (Size j = 0; j < n_bc_levels; ++j) {
            for (Size k = 0; k < n_bc_terms; ++k) {
                RiskFactorKey key(RiskFactorKey::KeyType::BaseCorrelation, name, j);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, bcData[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(asof, asof + data.shiftTerms[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(termTimes, shiftTermTimes);
        validShiftSize = validShiftSize && vectorEqual(levels, shiftLevels);

        // loop over shift levels and terms
        for (Size j = 0; j < shiftLevels.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

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

                        RiskFactorKey key(RFType::BaseCorrelation, name, idx);
                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedBcData[jj][kk] - bcData[jj][kk]);
                        } else {
                            scenario->add(key, shiftedBcData[jj][kk]);
                        }
                        // Possibly store valid shift size
                        if (validShiftSize && j == jj && k == kk) {
                            storeShiftData(key, bcData[jj][kk], shiftedBcData[jj][kk]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(
                    baseCorrelationScenarioDescription(name, j, k, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Base correlation scenarios done");
}

void SensitivityScenarioGenerator::generateCommodityCurveScenarios(bool up) {

    Date asof = baseScenario_->asof();

    // Log an ALERT if some commodity curves in simulation market are not in the list
    for (const string& name : simMarketData_->commodityNames()) {
        if (sensitivityData_->commodityCurveShiftData().find(name) ==
            sensitivityData_->commodityCurveShiftData().end()) {
            ALOG("Commodity " << name
                              << " in simulation market is not "
                                 "included in commodity sensitivity analysis");
        }
    }

    for (auto c : sensitivityData_->commodityCurveShiftData()) {
        string name = c.first;
        // Tenors for this name in simulation market
        vector<Period> simMarketTenors;
        try {
            simMarketTenors = simMarketData_->commodityCurveTenors(name);
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for comm curve " << name << ": " << e.what());
            continue;
        }
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->commodityPriceCurve(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for commodity price curve " << name
                                                                                             << ", using default A365");
        }

        vector<Real> times(simMarketTenors.size());
        vector<Real> basePrices(times.size());
        vector<Real> shiftedPrices(times.size());

        // Get the base prices for this name from the base scenario
        bool valid = true;
        for (Size j = 0; j < times.size(); ++j) {
            times[j] = dc.yearFraction(asof, asof + simMarketTenors[j]);
            RiskFactorKey key(RiskFactorKey::KeyType::CommodityCurve, name, j);
            valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, basePrices[j], continueOnError_);
        }
        if (!valid)
            continue;

        // Get the sensitivity data for this name
        SensitivityScenarioData::CurveShiftData data = *c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);

        // Get the times at which we want to apply the shifts
        QL_REQUIRE(!data.shiftTenors.empty(), "Commodity curve shift tenors have not been given");
        vector<Time> shiftTimes(data.shiftTenors.size());
        for (Size j = 0; j < data.shiftTenors.size(); ++j) {
            shiftTimes[j] = dc.yearFraction(asof, asof + data.shiftTenors[j]);
        }

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);

        // Generate the scenarios for each shift
        for (Size j = 0; j < data.shiftTenors.size(); ++j) {

            QuantLib::ext::shared_ptr<Scenario> scenario =
                sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

            // Apply shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, basePrices, times, shiftedPrices, true);

            // store shifted commodity price curve in the scenario
            for (Size k = 0; k < times.size(); ++k) {
                RiskFactorKey key(RFType::CommodityCurve, name, k);
                if (sensitivityData_->useSpreadedTermStructures()) {
                    scenario->add(key, shiftedPrices[k] - basePrices[k]);
                } else {
                    scenario->add(key, shiftedPrices[k]);
                }

                // Possibly store valid shift size
                if (validShiftSize && j == k) {
                    storeShiftData(key, basePrices[k], shiftedPrices[k]);
                }
            }

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            scenarioDescriptions_.push_back(commodityCurveScenarioDescription(name, j, up, getShiftScheme(data)));
            scenario->label(to_string(scenarioDescriptions_.back()));
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    DLOG("Commodity curve scenarios done");
}

void SensitivityScenarioGenerator::generateCommodityVolScenarios(bool up) {

    // Log an ALERT if some commodity vol names in simulation market are not in the list
    for (const string& name : simMarketData_->commodityVolNames()) {
        if (sensitivityData_->commodityVolShiftData().find(name) == sensitivityData_->commodityVolShiftData().end()) {
            ALOG("Commodity volatility " << name
                                         << " in simulation market is not "
                                            "included in commodity sensitivity analysis");
        }
    }

    // Loop over each commodity and create volatility scenario
    Date asof = baseScenario_->asof();
    for (auto c : sensitivityData_->commodityVolShiftData()) {
        string name = c.first;
        // Simulation market data for the current name
        vector<Period> expiries;
        try {
            expiries = simMarketData_->commodityVolExpiries(name);
        } catch (const std::exception& e) {
            ALOG("skip scenario generation for comm vol " << name << ": " << e.what());
            continue;
        }
        const vector<Real>& moneyness = simMarketData_->commodityVolMoneyness(name);
        QL_REQUIRE(!expiries.empty(), "Sim market commodity volatility expiries have not been specified for " << name);
        QL_REQUIRE(!moneyness.empty(), "Sim market commodity volatility moneyness has not been specified for " << name);
        // Store base scenario volatilities, strike x expiry
        vector<vector<Real>> baseValues(moneyness.size(), vector<Real>(expiries.size()));
        // Time to each expiry
        vector<Time> times(expiries.size());
        // Store shifted scenario volatilities
        vector<vector<Real>> shiftedValues = baseValues;

        SensitivityScenarioData::VolShiftData sd = c.second;
        if (!isScenarioRelevant(up, sd))
            continue;
        QL_REQUIRE(!sd.shiftExpiries.empty(), "commodity volatility shift tenors must be specified");

        ShiftType shiftType = getShiftType(sd);
        vector<Time> shiftTimes(sd.shiftExpiries.size());
        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->commodityVolatility(name)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for commodity vol surface " << name
                                                                                             << ", using default A365");
        }

        // Get the base scenario volatility values
        bool valid = true;
        for (Size j = 0; j < expiries.size(); j++) {
            times[j] = dc.yearFraction(asof, asof + expiries[j]);
            for (Size i = 0; i < moneyness.size(); i++) {
                RiskFactorKey key(RiskFactorKey::KeyType::CommodityVolatility, name, i * expiries.size() + j);
                valid =
                    valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, baseValues[i][j], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // Store the shift expiry times
        for (Size sj = 0; sj < sd.shiftExpiries.size(); ++sj) {
            shiftTimes[sj] = dc.yearFraction(asof, asof + sd.shiftExpiries[sj]);
        }

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(times, shiftTimes);
        validShiftSize = validShiftSize && vectorEqual(moneyness, sd.shiftStrikes);

        // Loop and apply scenarios
        for (Size sj = 0; sj < sd.shiftExpiries.size(); ++sj) {
            for (Size si = 0; si < sd.shiftStrikes.size(); ++si) {

                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(si, sj, getShiftSize(sd), up, shiftType, sd.shiftStrikes, shiftTimes, moneyness, times,
                           baseValues, shiftedValues, true);

                Size counter = 0;
                for (Size i = 0; i < moneyness.size(); i++) {
                    for (Size j = 0; j < expiries.size(); ++j) {
                        RiskFactorKey key(RFType::CommodityVolatility, name, counter++);
                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedValues[i][j] - baseValues[i][j]);
                        } else {
                            scenario->add(key, shiftedValues[i][j]);
                        }
                        // Possibly store valid shift size
                        if (validShiftSize && si == i && sj == j) {
                            storeShiftData(key, baseValues[i][j], shiftedValues[i][j]);
                        }
                    }
                }

                // Give the scenario a label

                // Add the final scenario to the scenario vector
                scenarioDescriptions_.push_back(commodityVolScenarioDescription(name, sj, si, up, getShiftScheme(sd)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                scenarios_.push_back(scenario);
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Commodity volatility scenarios done");
}

void SensitivityScenarioGenerator::generateCorrelationScenarios(bool up) {
    Date asof = baseScenario_->asof();
    for (auto sim_cap : simMarketData_->correlationPairs()) {
        if (sensitivityData_->correlationShiftData().find(sim_cap) == sensitivityData_->correlationShiftData().end()) {
            WLOG("Correlation " << sim_cap << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_c_strikes = simMarketData_->correlationStrikes().size();
    vector<Real> corrStrikes = simMarketData_->correlationStrikes();

    for (auto c : sensitivityData_->correlationShiftData()) {
        std::string label = c.first;
        std::vector<std::string> tokens = ore::data::getCorrelationTokens(label);
        std::pair<string, string> pair(tokens[0], tokens[1]);
        Size n_c_exp = simMarketData_->correlationExpiries().size();
        SensitivityScenarioData::VolShiftData data = c.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType shiftType = getShiftType(data);
        Real shiftSize = getShiftSize(data);
        vector<vector<Real>> corrData(n_c_exp, vector<Real>(n_c_strikes, 0.0));
        vector<Real> corrExpiryTimes(n_c_exp, 0.0);
        vector<vector<Real>> shiftedCorrData(n_c_exp, vector<Real>(n_c_strikes, 0.0));

        std::vector<Period> expiries = overrideTenors_ ? simMarketData_->correlationExpiries() : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(), "mismatch between effective shift expiries ("
                                                                     << expiries.size() << ") and shift tenors ("
                                                                     << data.shiftExpiries.size());
        vector<Real> shiftExpiryTimes(expiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->correlationCurve(pair.first, pair.second)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for correlation curve "
                 << pair.first << " - " << pair.second << ", using default A365");
        }

        // cache original vol data
        for (Size j = 0; j < n_c_exp; ++j) {
            Date expiry = asof + simMarketData_->correlationExpiries()[j];
            corrExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        bool valid = true;
        for (Size j = 0; j < n_c_exp; ++j) {
            for (Size k = 0; k < n_c_strikes; ++k) {
                Size idx = j * n_c_strikes + k;
                RiskFactorKey key(RiskFactorKey::KeyType::Correlation, label, idx);
                valid = valid && tryGetBaseScenarioValue(baseScenarioAbsolute_, key, corrData[j][k], continueOnError_);
            }
        }
        if (!valid)
            continue;

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + expiries[j]);

        // Can we store a valid shift size?
        bool validShiftSize = vectorEqual(corrExpiryTimes, shiftExpiryTimes);
        validShiftSize = validShiftSize && vectorEqual(corrStrikes, shiftStrikes);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                QuantLib::ext::shared_ptr<Scenario> scenario =
                    sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, corrExpiryTimes, corrStrikes,
                           corrData, shiftedCorrData, true);

                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_c_exp; ++jj) {
                    for (Size kk = 0; kk < n_c_strikes; ++kk) {
                        Size idx = jj * n_c_strikes + kk;
                        RiskFactorKey key(RFType::Correlation, label, idx);

                        if (shiftedCorrData[jj][kk] > 1) {
                            shiftedCorrData[jj][kk] = 1;
                        } else if (shiftedCorrData[jj][kk] < -1) {
                            shiftedCorrData[jj][kk] = -1;
                        }

                        if (sensitivityData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedCorrData[jj][kk] - corrData[jj][kk]);
                        } else {
                            scenario->add(key, shiftedCorrData[jj][kk]);
                        }
                        // Possibly store valid shift size
                        if (validShiftSize && j == jj && k == kk) {
                            storeShiftData(key, corrData[jj][kk], shiftedCorrData[jj][kk]);
                        }
                    }
                }

                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                scenarioDescriptions_.push_back(correlationScenarioDescription(label, j, k, up, getShiftScheme(data)));
                scenario->label(to_string(scenarioDescriptions_.back()));
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    DLOG("Correlation scenarios done");
}

void SensitivityScenarioGenerator::generateSecuritySpreadScenarios(bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    Date asof = baseScenario_->asof();
    // Log an ALERT if some equities in simmarket are excluded from the sensitivities list
    for (auto sim_security : simMarketData_->securities()) {
        if (sensitivityData_->securityShiftData().find(sim_security) == sensitivityData_->securityShiftData().end()) {
            WLOG("Security " << sim_security << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (auto s : sensitivityData_->securityShiftData()) {
        string bond = s.first;
        SensitivityScenarioData::SpotShiftData data = s.second;
        if (!isScenarioRelevant(up, data))
            continue;
        ShiftType type = getShiftType(data);
        Real size = up ? getShiftSize(data) : -1.0 * getShiftSize(data);
        bool relShift = (type == ShiftType::Relative);

        QuantLib::ext::shared_ptr<Scenario> scenario =
            sensiScenarioFactory_->buildScenario(asof, !sensitivityData_->useSpreadedTermStructures());

        RiskFactorKey key(RiskFactorKey::KeyType::SecuritySpread, bond);
        Real base_spread;
        if (!tryGetBaseScenarioValue(baseScenarioAbsolute_, key, base_spread, continueOnError_))
            continue;
        Real newSpread = relShift ? base_spread * (1.0 + size) : (base_spread + size);
        // Real newRate = up ? rate * (1.0 + getShiftSize(data)) : rate * (1.0 - getShiftSize(data));
        scenario->add(key, sensitivityData_->useSpreadedTermStructures() ? newSpread - base_spread : newSpread);

        storeShiftData(key, base_spread, newSpread);

        scenarios_.push_back(scenario);
        scenarioDescriptions_.push_back(securitySpreadScenarioDescription(bond, up, getShiftScheme(data)));
        scenario->label(to_string(scenarioDescriptions_.back()));
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newSpread);
    }
    DLOG("Security scenarios done");
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::fxScenarioDescription(string ccypair, bool up, ShiftScheme shiftScheme) {
    RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spot");
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::equityScenarioDescription(string equity, bool up, ShiftScheme shiftScheme) {
    RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spot");
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::dividendYieldScenarioDescription(string name, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::discountScenarioDescription(string ccy, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::indexScenarioDescription(string index, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yieldScenarioDescription(string name, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::fxVolScenarioDescription(string ccypair, Size expiryBucket, Size strikeBucket, bool up,
                                                       ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->fxVolShiftData().find(ccypair) != sensitivityData_->fxVolShiftData().end(),
               "currency pair " << ccypair << " not found in fx vol shift data");
    SensitivityScenarioData::VolShiftData data = sensitivityData_->fxVolShiftData()[ccypair];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 ||
        close_enough(data.shiftStrikes[strikeBucket], 0)) { // shiftStrikes defaults to {0.00}
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::equityVolScenarioDescription(string equity, Size expiryBucket, Size strikeBucket, bool up,
                                                           ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::swaptionVolScenarioDescription(string ccy, Size expiryBucket, Size termBucket,
                                                             Size strikeBucket, bool up, ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->swaptionVolShiftData().find(ccy) != sensitivityData_->swaptionVolShiftData().end(),
               "currency " << ccy << " not found in swaption vol shift data");
    SensitivityScenarioData::GenericYieldVolShiftData data = sensitivityData_->swaptionVolShiftData()[ccy];
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yieldVolScenarioDescription(string securityId, Size expiryBucket, Size termBucket,
                                                          bool up, ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->yieldVolShiftData().find(securityId) != sensitivityData_->yieldVolShiftData().end(),
               "currency " << securityId << " not found in yield vol shift data");
    SensitivityScenarioData::GenericYieldVolShiftData data = sensitivityData_->yieldVolShiftData()[securityId];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(termBucket < data.shiftTerms.size(), "term bucket " << termBucket << " out of range");
    Size index =
        expiryBucket * data.shiftStrikes.size() * data.shiftTerms.size() + termBucket * data.shiftStrikes.size();
    RiskFactorKey key(RiskFactorKey::KeyType::YieldVolatility, securityId, index);
    std::ostringstream o;
    o << data.shiftExpiries[expiryBucket] << "/" << data.shiftTerms[termBucket] << "/ATM";
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::capFloorVolScenarioDescription(string ccy, Size expiryBucket, Size strikeBucket, bool up,
                                                             bool isAtm, ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->capFloorVolShiftData().find(ccy) != sensitivityData_->capFloorVolShiftData().end(),
               "currency " << ccy << " not found in cap/floor vol shift data");
    SensitivityScenarioData::CapFloorVolShiftData data = *sensitivityData_->capFloorVolShiftData()[ccy];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    Size index = expiryBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::OptionletVolatility, ccy, index);
    std::ostringstream o;
    o << data.shiftExpiries[expiryBucket] << "/";
    if (isAtm) {
        o << "ATM";
    } else {
        o << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    }
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, o.str());
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::survivalProbabilityScenarioDescription(string name, Size bucket, bool up,
                                                                     ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::CdsVolScenarioDescription(string name, Size expiryBucket, Size strikeBucket, bool up,
                                                        ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::zeroInflationScenarioDescription(string index, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yoyInflationScenarioDescription(string index, Size bucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yoyInflationCapFloorVolScenarioDescription(string name, Size expiryBucket,
                                                                         Size strikeBucket, bool up,
                                                                         ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->yoyInflationCapFloorVolShiftData().find(name) !=
                   sensitivityData_->yoyInflationCapFloorVolShiftData().end(),
               "index " << name << " not found in yoy cap/floor vol shift data");
    SensitivityScenarioData::CapFloorVolShiftData data = *sensitivityData_->yoyInflationCapFloorVolShiftData()[name];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    Size index = expiryBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, name, index);
    std::ostringstream o;
    // Currently CapFloorVolShiftData must have a collection of absolute strikes for yoy inflation cap/floor vols
    o << data.shiftExpiries[expiryBucket] << "/" << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::zeroInflationCapFloorVolScenarioDescription(string name, Size expiryBucket,
                                                                          Size strikeBucket, bool up,
                                                                          ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->zeroInflationCapFloorVolShiftData().find(name) !=
                   sensitivityData_->zeroInflationCapFloorVolShiftData().end(),
               "currency " << name << " not found in zero inflation cap/floor vol shift data");
    SensitivityScenarioData::VolShiftData data = *sensitivityData_->zeroInflationCapFloorVolShiftData()[name];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    Size index = expiryBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, name, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        o << data.shiftExpiries[expiryBucket] << "/" << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::baseCorrelationScenarioDescription(string indexName, Size lossLevelBucket,
                                                                 Size termBucket, bool up, ShiftScheme shiftScheme) {
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
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::commodityCurveScenarioDescription(const string& commodityName, Size bucket, bool up,
                                                                ShiftScheme shiftScheme) {

    QL_REQUIRE(sensitivityData_->commodityCurveShiftData().count(commodityName) > 0,
               "Name " << commodityName << " not found in commodity curve shift data");
    vector<Period>& shiftTenors = sensitivityData_->commodityCurveShiftData()[commodityName]->shiftTenors;
    QL_REQUIRE(bucket < shiftTenors.size(), "bucket " << bucket << " out of commodity curve bucket range");

    RiskFactorKey key(RiskFactorKey::KeyType::CommodityCurve, commodityName, bucket);
    ostringstream oss;
    oss << shiftTenors[bucket];
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return ScenarioDescription(type, key, oss.str());
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::commodityVolScenarioDescription(const string& commodityName, Size expiryBucket,
                                                              Size strikeBucket, bool up, ShiftScheme shiftScheme) {

    QL_REQUIRE(sensitivityData_->commodityVolShiftData().count(commodityName) > 0,
               "commodity " << commodityName << " not found in commodity vol shift data");

    SensitivityScenarioData::VolShiftData data = sensitivityData_->commodityVolShiftData()[commodityName];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::CommodityVolatility, commodityName, index);
    ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 1.0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << data.shiftExpiries[expiryBucket] << "/" << data.shiftStrikes[strikeBucket];
    }
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return ScenarioDescription(type, key, o.str());
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::correlationScenarioDescription(string pair, Size expiryBucket, Size strikeBucket, bool up,
                                                             ShiftScheme shiftScheme) {
    QL_REQUIRE(sensitivityData_->correlationShiftData().find(pair) != sensitivityData_->correlationShiftData().end(),
               "pair " << pair << " not found in correlation shift data");
    SensitivityScenarioData::VolShiftData data = sensitivityData_->correlationShiftData()[pair];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    // Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    Size index = expiryBucket * data.shiftStrikes.size() + strikeBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::Correlation, pair, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0 || close_enough(data.shiftStrikes[strikeBucket], 0)) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        o << data.shiftExpiries[expiryBucket] << "/" << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::securitySpreadScenarioDescription(string bond, bool up, ShiftScheme shiftScheme) {
    RiskFactorKey key(RiskFactorKey::KeyType::SecuritySpread, bond);
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spread");
    shiftSchemes_[key] = shiftScheme;
    storeShiftData(key, 0.0, 0.0); // default, only used if not popoulated before
    return desc;
}

} // namespace analytics
} // namespace ore
