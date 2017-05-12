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
#include <ored/utilities/log.hpp>
#include <ostream>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

SensitivityScenarioGenerator::SensitivityScenarioGenerator(
    const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
    const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData, const Date& today,
    const boost::shared_ptr<ore::data::Market>& initMarket, const bool overrideTenors, const std::string& configuration,
    boost::shared_ptr<ScenarioFactory> baseScenarioFactory)
    : ShiftScenarioGenerator(simMarketData, today, initMarket, configuration, baseScenarioFactory),
      sensitivityData_(sensitivityData), overrideTenors_(overrideTenors) {
    QL_REQUIRE(sensitivityData_ != NULL, "SensitivityScenarioGenerator: sensitivityData is null");
}

void SensitivityScenarioGenerator::generateScenarios(const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory) {
    generateDiscountCurveScenarios(sensiScenarioFactory, true);
    generateDiscountCurveScenarios(sensiScenarioFactory, false);

    generateIndexCurveScenarios(sensiScenarioFactory, true);
    generateIndexCurveScenarios(sensiScenarioFactory, false);

    generateYieldCurveScenarios(sensiScenarioFactory, true);
    generateYieldCurveScenarios(sensiScenarioFactory, false);

    generateFxScenarios(sensiScenarioFactory, true);
    generateFxScenarios(sensiScenarioFactory, false);

    if (simMarketData_->simulateFXVols()) {
        generateFxVolScenarios(sensiScenarioFactory, true);
        generateFxVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateSwapVols()) {
        generateSwaptionVolScenarios(sensiScenarioFactory, true);
        generateSwaptionVolScenarios(sensiScenarioFactory, false);
    }

    if (simMarketData_->simulateCapFloorVols()) {
        generateCapFloorVolScenarios(sensiScenarioFactory, true);
        generateCapFloorVolScenarios(sensiScenarioFactory, false);
    }

    // add simultaneous up-moves in two risk factors for cross gamma calculation
    vector<RiskFactorKey> keys = baseScenario_->keys();
    Size index = scenarios_.size();
    for (Size i = 0; i < index; ++i) {
        for (Size j = i + 1; j < index; ++j) {
            if (i == j || scenarioDescriptions_[i].type() != ScenarioDescription::Type::Up ||
                scenarioDescriptions_[j].type() != ScenarioDescription::Type::Up)
                continue;
            // filter desired cross shift combinations
            bool match = false;
            for (Size k = 0; k < sensitivityData_->crossGammaFilter().size(); ++k) {
                if ((scenarioDescriptions_[i].factor1().find(sensitivityData_->crossGammaFilter()[k].first) !=
                         string::npos &&
                     scenarioDescriptions_[j].factor1().find(sensitivityData_->crossGammaFilter()[k].second) !=
                         string::npos) ||
                    (scenarioDescriptions_[i].factor1().find(sensitivityData_->crossGammaFilter()[k].second) !=
                         string::npos &&
                     scenarioDescriptions_[j].factor1().find(sensitivityData_->crossGammaFilter()[k].first) !=
                         string::npos)) {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;

            boost::shared_ptr<Scenario> crossScenario = sensiScenarioFactory->buildScenario(today_);

            for (Size k = 0; k < keys.size(); ++k) {
                Real baseValue = baseScenario_->get(keys[k]);
                Real iValue = scenarios_[i]->get(keys[k]);
                Real jValue = scenarios_[j]->get(keys[k]);
                Real newVal = iValue + jValue - baseValue;
                if (newVal != baseValue)
                    crossScenario->add(keys[k], newVal);
            }
            scenarios_.push_back(crossScenario);
            scenarioDescriptions_.push_back(ScenarioDescription(scenarioDescriptions_[i], scenarioDescriptions_[j]));
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
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> simMarketFxPairs = simMarketData_->fxCcyPairs();
    if (sensitivityData_->fxCcyPairs().size() > 0)
        fxCcyPairs_ = sensitivityData_->fxCcyPairs();
    else
        fxCcyPairs_ = simMarketFxPairs;
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
    for (auto sensi_fx : fxCcyPairs_) {
        string foreign = sensi_fx.substr(0, 3);
        string domestic = sensi_fx.substr(3);
        QL_REQUIRE((domestic == baseCcy) || (foreign == baseCcy),
                   "SensitivityScenarioGenerator does not support cross FX pairs("
                       << sensi_fx << ", but base currency is " << baseCcy << ")");
    }
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_fx : simMarketFxPairs) {
        if (std::find(fxCcyPairs_.begin(), fxCcyPairs_.end(), sim_fx) == fxCcyPairs_.end()) {
            ALOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }
    for (Size k = 0; k < fxCcyPairs_.size(); k++) {
        string ccypair = fxCcyPairs_[k]; // foreign + domestic;
        SensitivityScenarioData::FxShiftData data = sensitivityData_->fxShiftData()[ccypair];
        ShiftType type = parseShiftType(data.shiftType);
        Real size = up ? data.shiftSize : -1.0 * data.shiftSize;
        // QL_REQUIRE(type == SensitivityScenarioGenerator::ShiftType::Relative, "FX scenario type must be relative");
        bool relShift = (type == SensitivityScenarioGenerator::ShiftType::Relative);

        boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

        scenarioDescriptions_.push_back(fxScenarioDescription(ccypair, up));

        Real rate = initMarket_->fxSpot(ccypair, configuration_)->value();
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        // Real newRate = up ? rate * (1.0 + data.shiftSize) : rate * (1.0 - data.shiftSize);
        scenario->add(getFxKey(ccypair), newRate);
        scenarios_.push_back(scenario);
        DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                       << " created: " << newRate);
    }
    LOG("FX scenarios done");
}

void SensitivityScenarioGenerator::generateDiscountCurveScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> simMarketCcys = simMarketData_->ccys();
    if (sensitivityData_->discountCurrencies().size() > 0)
        discountCurrencies_ = sensitivityData_->discountCurrencies();
    else
        discountCurrencies_ = simMarketCcys;
    // Log an ALERT if some currencies in simmarket are excluded from the list
    for (auto sim_ccy : simMarketCcys) {
        if (std::find(discountCurrencies_.begin(), discountCurrencies_.end(), sim_ccy) == discountCurrencies_.end()) {
            ALOG("Currency " << sim_ccy << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_ccy = discountCurrencies_.size();

    for (Size i = 0; i < n_ccy; ++i) {
        string ccy = discountCurrencies_[i];
        Size n_ten = simMarketData_->yieldCurveTenors(ccy).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        SensitivityScenarioData::CurveShiftData data = sensitivityData_->discountCurveShiftData()[ccy];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = initMarket_->discountCurve(ccy, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors(ccy)[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(ccy)
                                              ? simMarketData_->yieldCurveTenors(ccy)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(),
                   "mismatch between effective shift tenors (" << shiftTenors.size() << ") and shift tenors ("
                                                               << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);
            scenarioDescriptions_.push_back(discountScenarioDescription(ccy, j, up));
            DLOG("generate discount curve scenario, ccy " << ccy << ", bucket " << j << ", up " << up << ", desc "
                                                          << scenarioDescriptions_.back().text());

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getDiscountKey(ccy, k), shiftedDiscount);
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
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> simMarketIndices = simMarketData_->indices();
    if (sensitivityData_->indexNames().size() > 0)
        indexNames_ = sensitivityData_->indexNames();
    else
        indexNames_ = simMarketIndices;
    // Log an ALERT if some ibor indices in simmarket are excluded from the list
    for (auto sim_idx : simMarketIndices) {
        if (std::find(indexNames_.begin(), indexNames_.end(), sim_idx) == indexNames_.end()) {
            ALOG("Index " << sim_idx << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_indices = indexNames_.size();

    for (Size i = 0; i < n_indices; ++i) {
        string indexName = indexNames_[i];
        Size n_ten = simMarketData_->yieldCurveTenors(indexName).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->indexCurveShiftData()[indexName];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<IborIndex> iborIndex = initMarket_->iborIndex(indexName, configuration_);
        Handle<YieldTermStructure> ts = iborIndex->forwardingTermStructure();
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors(indexName)[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(indexName)
                                              ? simMarketData_->yieldCurveTenors(indexName)
                                              : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(),
                   "mismatch between effective shift tenors (" << shiftTenors.size() << ") and shift tenors ("
                                                               << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Index shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

            scenarioDescriptions_.push_back(indexScenarioDescription(indexName, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getIndexKey(indexName, k), shiftedDiscount);
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
    // We can choose to shift fewer yield curves than listed in the market
    vector<string> simMarketYieldCurves = simMarketData_->yieldCurveNames();
    if (sensitivityData_->yieldCurveNames().size() > 0)
        yieldCurveNames_ = sensitivityData_->yieldCurveNames();
    else
        yieldCurveNames_ = simMarketYieldCurves;
    // Log an ALERT if some yield curves in simmarket are excluded from the list
    for (auto sim_yc : simMarketYieldCurves) {
        if (std::find(yieldCurveNames_.begin(), yieldCurveNames_.end(), sim_yc) == yieldCurveNames_.end()) {
            ALOG("Yield Curve " << sim_yc << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_curves = yieldCurveNames_.size();

    for (Size i = 0; i < n_curves; ++i) {
        string name = yieldCurveNames_[i];
        Size n_ten = simMarketData_->yieldCurveTenors(name).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->yieldCurveShiftData()[name];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = initMarket_->yieldCurve(name, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors(name)[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        const std::vector<Period>& shiftTenors = overrideTenors_ && simMarketData_->hasYieldCurveTenors(name)
                                                     ? simMarketData_->yieldCurveTenors(name)
                                                     : data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() == data.shiftTenors.size(),
                   "mismatch between effective shift tenors (" << shiftTenors.size() << ") and shift tenors ("
                                                               << data.shiftTenors.size() << ")");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

            scenarioDescriptions_.push_back(yieldScenarioDescription(name, j, up));

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getYieldKey(name, k), shiftedDiscount);
            }
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateFxVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> simMarketFxPairs = simMarketData_->fxVolCcyPairs();
    if (sensitivityData_->fxVolCcyPairs().size() > 0)
        fxVolCcyPairs_ = sensitivityData_->fxVolCcyPairs();
    else
        fxVolCcyPairs_ = simMarketFxPairs;
    // Log an ALERT if some FX vol pairs in simmarket are excluded from the list
    for (auto sim_fx : simMarketFxPairs) {
        if (std::find(fxVolCcyPairs_.begin(), fxVolCcyPairs_.end(), sim_fx) == fxVolCcyPairs_.end()) {
            ALOG("FX pair " << sim_fx << " in simmarket is not included in sensitivities analysis");
        }
    }

    string domestic = simMarketData_->baseCcy();

    Size n_fxvol_pairs = fxVolCcyPairs_.size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();

    std::vector<Real> values(n_fxvol_exp);
    std::vector<Real> times(n_fxvol_exp);

    // buffer for shifted zero curves
    std::vector<Real> shiftedValues(n_fxvol_exp);

    map<string, SensitivityScenarioData::FxVolShiftData> shiftDataMap = sensitivityData_->fxVolShiftData();
    for (Size i = 0; i < n_fxvol_pairs; ++i) {
        string ccypair = fxVolCcyPairs_[i];
        QL_REQUIRE(shiftDataMap.find(ccypair) != shiftDataMap.end(),
                   "ccy pair " << ccypair << " not found in FxVolShiftData");
        SensitivityScenarioData::FxVolShiftData data = shiftDataMap[ccypair];
        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");

        Handle<BlackVolTermStructure> ts = initMarket_->fxVol(ccypair, configuration_);
        DayCounter dc = ts->dayCounter();
        Real strike = 0.0; // FIXME
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = today_ + simMarketData_->fxVolExpiries()[j];
            values[j] = ts->blackVol(d, strike);
            times[j] = dc.yearFraction(today_, d);
        }

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            Size strikeBucket = 0; // FIXME
            boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

            scenarioDescriptions_.push_back(fxVolScenarioDescription(ccypair, j, strikeBucket, up));

            // apply shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, values, times, shiftedValues, true);
            for (Size k = 0; k < n_fxvol_exp; ++k)
                scenario->add(getFxVolKey(ccypair, k), shiftedValues[k]);
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("FX vol scenarios done");
}

void SensitivityScenarioGenerator::generateSwaptionVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    std::vector<string> simMarketCcys = simMarketData_->swapVolCcys();
    if (sensitivityData_->swaptionVolCurrencies().size() > 0)
        swaptionVolCurrencies_ = sensitivityData_->swaptionVolCurrencies();
    else
        swaptionVolCurrencies_ = simMarketCcys;
    // Log an ALERT if some swaption currencies in simmarket are excluded from the list
    for (auto sim_ccy : simMarketCcys) {
        if (std::find(swaptionVolCurrencies_.begin(), swaptionVolCurrencies_.end(), sim_ccy) ==
            swaptionVolCurrencies_.end()) {
            ALOG("Swaption currency " << sim_ccy << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_swvol_ccy = swaptionVolCurrencies_.size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();

    vector<vector<Real>> volData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));
    vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
    vector<Real> volTermTimes(n_swvol_term, 0.0);
    vector<vector<Real>> shiftedVolData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));

    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = swaptionVolCurrencies_[i];
        SensitivityScenarioData::SwaptionVolShiftData data = sensitivityData_->swaptionVolShiftData()[ccy];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;

        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        Handle<SwaptionVolatilityStructure> ts = initMarket_->swaptionVol(ccy, configuration_);
        DayCounter dc = ts->dayCounter();
        Real strike = 0.0; // FIXME

        // cache original vol data
        for (Size j = 0; j < n_swvol_exp; ++j) {
            Date expiry = today_ + simMarketData_->swapVolExpiries()[j];
            volExpiryTimes[j] = dc.yearFraction(today_, expiry);
        }
        for (Size j = 0; j < n_swvol_term; ++j) {
            Date term = today_ + simMarketData_->swapVolTerms()[j];
            volTermTimes[j] = dc.yearFraction(today_, term);
        }
        for (Size j = 0; j < n_swvol_exp; ++j) {
            Period expiry = simMarketData_->swapVolExpiries()[j];
            for (Size k = 0; k < n_swvol_term; ++k) {
                Period term = simMarketData_->swapVolTerms()[k];
                Real swvol = ts->volatility(expiry, term, strike);
                volData[j][k] = swvol;
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(today_, today_ + data.shiftExpiries[j]);
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(today_, today_ + data.shiftTerms[j]);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                Size strikeBucket = 0; // FIXME
                boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

                scenarioDescriptions_.push_back(swaptionVolScenarioDescription(ccy, j, k, strikeBucket, up));

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftTermTimes, volExpiryTimes,
                           volTermTimes, volData, shiftedVolData, true);
                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_swvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_swvol_term; ++kk) {
                        Size idx = jj * n_swvol_term + kk;
                        scenario->add(getSwaptionVolKey(ccy, idx), shiftedVolData[jj][kk]);
                    }
                }
                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                DLOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    LOG("Swaption vol scenarios done");
}

void SensitivityScenarioGenerator::generateCapFloorVolScenarios(
    const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory, bool up) {
    // We can choose to shift fewer discount curves than listed in the market
    vector<string> simMarketCapCcys = simMarketData_->capFloorVolCcys();
    if (sensitivityData_->capFloorVolCurrencies().size() > 0)
        capFloorVolCurrencies_ = sensitivityData_->capFloorVolCurrencies();
    else
        capFloorVolCurrencies_ = simMarketCapCcys;
    // Log an ALERT if some cap currencies in simmarket are excluded from the list
    for (auto sim_cap : simMarketCapCcys) {
        if (std::find(capFloorVolCurrencies_.begin(), capFloorVolCurrencies_.end(), sim_cap) ==
            capFloorVolCurrencies_.end()) {
            ALOG("CapFloor currency " << sim_cap << " in simmarket is not included in sensitivities analysis");
        }
    }

    Size n_cfvol_ccy = capFloorVolCurrencies_.size();
    Size n_cfvol_strikes = simMarketData_->capFloorVolStrikes().size();
    vector<Real> volStrikes = simMarketData_->capFloorVolStrikes();

    for (Size i = 0; i < n_cfvol_ccy; ++i) {
        std::string ccy = capFloorVolCurrencies_[i];
        SensitivityScenarioData::CapFloorVolShiftData data = sensitivityData_->capFloorVolShiftData()[ccy];

        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;
        std::vector<Period> expiries = overrideTenors_ && simMarketData_->hasCapFloorVolExpiries(ccy)
                                           ? simMarketData_->capFloorVolExpiries(ccy)
                                           : data.shiftExpiries;
        QL_REQUIRE(expiries.size() == data.shiftExpiries.size(),
                   "mismatch between effective shift expiries (" << expiries.size() << ") and shift tenors ("
                                                                 << data.shiftExpiries.size());
        Size n_cfvol_exp = expiries.size();
        vector<vector<Real>> volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

        vector<Real> shiftExpiryTimes(n_cfvol_exp, 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        Handle<OptionletVolatilityStructure> ts = initMarket_->capFloorVol(ccy, configuration_);
        DayCounter dc = ts->dayCounter();

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = today_ + expiries[j];
            volExpiryTimes[j] = dc.yearFraction(today_, expiry);
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Real strike = simMarketData_->capFloorVolStrikes()[k];
                Real vol = ts->volatility(expiry, strike);
                volData[j][k] = vol;
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(today_, today_ + expiries[j]);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                boost::shared_ptr<Scenario> scenario = sensiScenarioFactory->buildScenario(today_);

                scenarioDescriptions_.push_back(capFloorVolScenarioDescription(ccy, j, k, up));

                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);
                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                        Size idx = jj * n_cfvol_strikes + kk;
                        scenario->add(getOptionletVolKey(ccy, idx), shiftedVolData[jj][kk]);
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

SensitivityScenarioGenerator::ScenarioDescription SensitivityScenarioGenerator::fxScenarioDescription(string ccypair,
                                                                                                      bool up) {
    RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
    std::ostringstream o;
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, "spot");
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::discountScenarioDescription(string ccy, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->discountCurveShiftData().find(ccy) != sensitivityData_->discountCurveShiftData().end(),
               "currency " << ccy << " not found in discount shift data");
    QL_REQUIRE(bucket < sensitivityData_->discountCurveShiftData()[ccy].shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, bucket);
    std::ostringstream o;
    o << sensitivityData_->discountCurveShiftData()[ccy].shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::indexScenarioDescription(string index, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->indexCurveShiftData().find(index) != sensitivityData_->indexCurveShiftData().end(),
               "currency " << index << " not found in index shift data");
    QL_REQUIRE(bucket < sensitivityData_->indexCurveShiftData()[index].shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, index, bucket);
    std::ostringstream o;
    o << sensitivityData_->indexCurveShiftData()[index].shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::yieldScenarioDescription(string name, Size bucket, bool up) {
    QL_REQUIRE(sensitivityData_->yieldCurveShiftData().find(name) != sensitivityData_->yieldCurveShiftData().end(),
               "currency " << name << " not found in index shift data");
    QL_REQUIRE(bucket < sensitivityData_->yieldCurveShiftData()[name].shiftTenors.size(),
               "bucket " << bucket << " out of range");
    RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, bucket);
    std::ostringstream o;
    o << sensitivityData_->yieldCurveShiftData()[name].shiftTenors[bucket];
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}

SensitivityScenarioGenerator::ScenarioDescription
SensitivityScenarioGenerator::fxVolScenarioDescription(string ccypair, Size expiryBucket, Size strikeBucket, bool up) {
    QL_REQUIRE(sensitivityData_->fxVolShiftData().find(ccypair) != sensitivityData_->fxVolShiftData().end(),
               "currency pair " << ccypair << " not found in fx vol shift data");
    SensitivityScenarioData::FxVolShiftData data = sensitivityData_->fxVolShiftData()[ccypair];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    Size index = strikeBucket * data.shiftExpiries.size() + expiryBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0) {
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
    QL_REQUIRE(data.shiftStrikes.size() == 0, "empty Swaption strike vector expected");
    Size index = strikeBucket * data.shiftExpiries.size() * data.shiftTerms.size() +
                 expiryBucket * data.shiftTerms.size() + termBucket;
    RiskFactorKey key(RiskFactorKey::KeyType::SwaptionVolatility, ccy, index);
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0) {
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
    if (data.shiftStrikes.size() == 0) {
        o << data.shiftExpiries[expiryBucket] << "/ATM";
    } else {
        o << data.shiftExpiries[expiryBucket] << "/" << std::setprecision(4) << data.shiftStrikes[strikeBucket];
    }
    string text = o.str();
    ScenarioDescription::Type type = up ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;
    ScenarioDescription desc(type, key, text);
    return desc;
}
}
}
