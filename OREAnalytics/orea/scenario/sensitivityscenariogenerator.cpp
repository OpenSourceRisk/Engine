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

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

SensitivityScenarioGenerator::SensitivityScenarioGenerator(boost::shared_ptr<ScenarioFactory> scenarioFactory,
                                                           boost::shared_ptr<SensitivityScenarioData> sensitivityData,
                                                           boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                                                           Date today, boost::shared_ptr<ore::data::Market> initMarket,
                                                           const std::string& configuration)
    : ShiftScenarioGenerator(scenarioFactory, simMarketData, today, initMarket, configuration),
      sensitivityData_(sensitivityData) {
    QL_REQUIRE(sensitivityData != NULL, "SensitivityScenarioGenerator: sensitivityData is null");

    generateDiscountCurveScenarios(true, initMarket);
    generateDiscountCurveScenarios(false, initMarket);

    generateIndexCurveScenarios(true, initMarket);
    generateIndexCurveScenarios(false, initMarket);

    generateYieldCurveScenarios(true, initMarket);
    generateYieldCurveScenarios(false, initMarket);

    generateFxScenarios(true, initMarket);
    generateFxScenarios(false, initMarket);

    if (simMarketData_->simulateFXVols()) {
        generateFxVolScenarios(true, initMarket);
        generateFxVolScenarios(false, initMarket);
    }

    if (simMarketData_->simulateSwapVols()) {
        generateSwaptionVolScenarios(true, initMarket);
        generateSwaptionVolScenarios(false, initMarket);
    }

    if (simMarketData_->simulateCapFloorVols()) {
        generateCapFloorVolScenarios(true, initMarket);
        generateCapFloorVolScenarios(false, initMarket);
    }

    // add simultaneous up-moves in two risk factors for cross gamma calculation
    vector<RiskFactorKey> keys = baseScenario_->keys();
    Size index = scenarios_.size();
    for (Size i = 0; i < index; ++i) {
        for (Size j = i + 1; j < index; ++j) {
            if (i == j || scenarios_[i]->label().find("/UP") == string::npos ||
                scenarios_[j]->label().find("/UP") == string::npos)
                continue;
            // filter desired cross shift combinations
            bool match = false;
            for (Size k = 0; k < sensitivityData_->crossGammaFilter().size(); ++k) {
                if ((scenarios_[i]->label().find(sensitivityData_->crossGammaFilter()[k].first) != string::npos &&
                     scenarios_[j]->label().find(sensitivityData_->crossGammaFilter()[k].second) != string::npos) ||
                    (scenarios_[i]->label().find(sensitivityData_->crossGammaFilter()[k].second) != string::npos &&
                     scenarios_[j]->label().find(sensitivityData_->crossGammaFilter()[k].first) != string::npos)) {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
            string crossLabel = "CROSS:" + scenarios_[i]->label() + ":" + scenarios_[j]->label();
            boost::shared_ptr<Scenario> crossScenario = scenarioFactory_->buildScenario(today_, crossLabel);
            for (Size k = 0; k < keys.size(); ++k) {
                Real baseValue = baseScenario_->get(keys[k]);
                Real iValue = scenarios_[i]->get(keys[k]);
                Real jValue = scenarios_[j]->get(keys[k]);
                crossScenario->add(keys[k], iValue + jValue - baseValue);
            }
            scenarios_.push_back(crossScenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << crossScenario->label() << " created");
        }
    }

    LOG("sensitivity scenario generator initialised");
}

void SensitivityScenarioGenerator::generateFxScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->fxCcyPairs().size() > 0)
        fxCcyPairs_ = sensitivityData_->fxCcyPairs();
    else
        fxCcyPairs_ = simMarketData_->fxCcyPairs();

    for (Size k = 0; k < fxCcyPairs_.size(); k++) {
        string ccypair = fxCcyPairs_[k]; // foreign + domestic;
        SensitivityScenarioData::FxShiftData data = sensitivityData_->fxShiftData()[ccypair];
        ShiftType type = parseShiftType(data.shiftType);
        QL_REQUIRE(type == SensitivityScenarioGenerator::ShiftType::Relative, "FX scenario type must be relative");

        string label = sensitivityData_->fxShiftScenarioLabel(ccypair, up);
        boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
        Real rate = market->fxSpot(ccypair, configuration_)->value();
        Real newRate = up ? rate * (1.0 + data.shiftSize) : rate * (1.0 - data.shiftSize);
        scenario->add(getFxKey(ccypair), newRate);
        // add remaining unshifted data from cache for a complete scenario
        addCacheTo(scenario);
        scenarios_.push_back(scenario);
        LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                      << " created: " << newRate);
    }
    LOG("FX scenarios done");
}

void SensitivityScenarioGenerator::generateDiscountCurveScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->discountCurrencies().size() > 0)
        discountCurrencies_ = sensitivityData_->discountCurrencies();
    else
        discountCurrencies_ = simMarketData_->ccys();

    Size n_ccy = discountCurrencies_.size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();

    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    // buffer for shifted zero curves
    std::vector<Real> shiftedZeros(n_ten);

    for (Size i = 0; i < n_ccy; ++i) {
        string ccy = discountCurrencies_[i];
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->discountCurveShiftData()[ccy];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = market->discountCurve(ccy, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors()[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            string label = sensitivityData_->discountShiftScenarioLabel(ccy, j, up);
            boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getDiscountKey(ccy, k), shiftedDiscount);
            }

            // add remaining unshifted data from cache for a complete scenario
            addCacheTo(scenario);

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateIndexCurveScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->indexNames().size() > 0)
        indexNames_ = sensitivityData_->indexNames();
    else
        indexNames_ = simMarketData_->indices();

    Size n_indices = indexNames_.size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();

    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    // buffer for shifted zero curves
    std::vector<Real> shiftedZeros(n_ten);

    for (Size i = 0; i < n_indices; ++i) {
        string indexName = indexNames_[i];
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->indexCurveShiftData()[indexName];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<IborIndex> iborIndex = market->iborIndex(indexName, configuration_);
        Handle<YieldTermStructure> ts = iborIndex->forwardingTermStructure();
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors()[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Index shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            string label = sensitivityData_->indexShiftScenarioLabel(indexName, j, up);
            boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve for this index in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getIndexKey(indexName, k), shiftedDiscount);
            }

            // add remaining unshifted data from cache for a complete scenario
            addCacheTo(scenario);

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label()
                                          << " created for indexName " << indexName);

        } // end of shift curve tenors
    }
    LOG("Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateYieldCurveScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer yield curves than listed in the market
    if (sensitivityData_->yieldCurveNames().size() > 0)
        yieldCurveNames_ = sensitivityData_->yieldCurveNames();
    else
        yieldCurveNames_ = simMarketData_->yieldCurveNames();

    Size n_curves = yieldCurveNames_.size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();

    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    // buffer for shifted zero curves
    std::vector<Real> shiftedZeros(n_ten);

    for (Size i = 0; i < n_curves; ++i) {
        string name = yieldCurveNames_[i];
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->yieldCurveShiftData()[name];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = market->yieldCurve(name, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors()[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");

        for (Size j = 0; j < shiftTenors.size(); ++j) {

            string label = sensitivityData_->yieldShiftScenarioLabel(name, j, up);
            boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

            // apply zero rate shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, zeros, times, shiftedZeros, true);

            // store shifted discount curve in the scenario
            for (Size k = 0; k < n_ten; ++k) {
                Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
                scenario->add(getYieldKey(name, k), shiftedDiscount);
            }

            // add remaining unshifted data from cache for a complete scenario
            addCacheTo(scenario);

            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");

        } // end of shift curve tenors
    }
    LOG("Yield curve scenarios done");
}

void SensitivityScenarioGenerator::generateFxVolScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->fxVolCcyPairs().size() > 0)
        fxVolCcyPairs_ = sensitivityData_->fxVolCcyPairs();
    else
        fxVolCcyPairs_ = simMarketData_->fxVolCcyPairs();

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
        QL_REQUIRE(shiftDataMap.find(ccypair) != shiftDataMap.end(), "ccy pair " << ccypair
                                                                                 << " not found in FxVolShiftData");
        SensitivityScenarioData::FxVolShiftData data = shiftDataMap[ccypair];
        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        Real shiftSize = data.shiftSize;
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");

        Handle<BlackVolTermStructure> ts = market->fxVol(ccypair, configuration_);
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
            string label = sensitivityData_->fxVolShiftScenarioLabel(ccypair, j, strikeBucket, up);
            boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
            // apply shift at tenor point j
            applyShift(j, shiftSize, up, shiftType, shiftTimes, values, times, shiftedValues, true);
            for (Size k = 0; k < n_fxvol_exp; ++k)
                scenario->add(getFxVolKey(ccypair, k), shiftedValues[k]);

            // add remaining unshifted data from cache for a complete scenario
            addCacheTo(scenario);
            // add this scenario to the scenario vector
            scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("FX vol scenarios done");
}

void SensitivityScenarioGenerator::generateSwaptionVolScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->swaptionVolCurrencies().size() > 0)
        swaptionVolCurrencies_ = sensitivityData_->swaptionVolCurrencies();
    else
        swaptionVolCurrencies_ = simMarketData_->swapVolCcys();

    Size n_swvol_ccy = swaptionVolCurrencies_.size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();

    vector<vector<Real> > volData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));
    vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
    vector<Real> volTermTimes(n_swvol_term, 0.0);
    vector<vector<Real> > shiftedVolData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));

    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = swaptionVolCurrencies_[i];
        SensitivityScenarioData::SwaptionVolShiftData data = sensitivityData_->swaptionVolShiftData()[ccy];
        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;

        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

        Handle<SwaptionVolatilityStructure> ts = market->swaptionVol(ccy, configuration_);
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
                string label = sensitivityData_->swaptionVolShiftScenarioLabel(ccy, j, k, strikeBucket, up);
                boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftTermTimes, volExpiryTimes,
                           volTermTimes, volData, shiftedVolData, true);
                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_swvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_swvol_term; ++kk) {
                        Size idx = jj * n_swvol_term + kk;
                        scenario->add(getSwaptionVolKey(ccy, idx), shiftedVolData[jj][kk]);
                    }
                }
                // add remaining unshifted data from cache for a complete scenario
                addCacheTo(scenario);
                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    LOG("Swaption vol scenarios done");
}

void SensitivityScenarioGenerator::generateCapFloorVolScenarios(bool up, boost::shared_ptr<Market> market) {
    // We can choose to shift fewer discount curves than listed in the market
    if (sensitivityData_->capFloorVolCurrencies().size() > 0)
        capFloorVolCurrencies_ = sensitivityData_->capFloorVolCurrencies();
    else
        capFloorVolCurrencies_ = simMarketData_->capFloorVolCcys();

    Size n_cfvol_ccy = capFloorVolCurrencies_.size();
    Size n_cfvol_strikes = simMarketData_->capFloorVolStrikes().size();
    Size n_cfvol_exp = simMarketData_->capFloorVolExpiries().size();

    vector<vector<Real> > volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
    vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
    vector<Real> volStrikes = simMarketData_->capFloorVolStrikes();
    vector<vector<Real> > shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

    for (Size i = 0; i < n_cfvol_ccy; ++i) {
        std::string ccy = capFloorVolCurrencies_[i];
        SensitivityScenarioData::CapFloorVolShiftData data = sensitivityData_->capFloorVolShiftData()[ccy];

        ShiftType shiftType = parseShiftType(data.shiftType);
        Real shiftSize = data.shiftSize;
        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes;

        Handle<OptionletVolatilityStructure> ts = market->capFloorVol(ccy, configuration_);
        DayCounter dc = ts->dayCounter();

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = today_ + simMarketData_->capFloorVolExpiries()[j];
            volExpiryTimes[j] = dc.yearFraction(today_, expiry);
        }
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Period expiry = simMarketData_->capFloorVolExpiries()[j];
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Real strike = simMarketData_->capFloorVolStrikes()[k];
                Real vol = ts->volatility(expiry, strike);
                volData[j][k] = vol;
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(today_, today_ + data.shiftExpiries[j]);

        // loop over shift expiries and terms
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                string label = sensitivityData_->capFloorVolShiftScenarioLabel(ccy, j, k, up);
                boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
                applyShift(j, k, shiftSize, up, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, true);
                // add shifted vol data to the scenario
                for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
                    for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                        Size idx = jj * n_cfvol_strikes + kk;
                        scenario->add(getOptionletVolKey(ccy, idx), shiftedVolData[jj][kk]);
                    }
                }
                // add remaining unshifted data from cache for a complete scenario
                addCacheTo(scenario);
                // add this scenario to the scenario vector
                scenarios_.push_back(scenario);
                LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
            }
        }
    }
    LOG("Optionlet vol scenarios done");
}
}
}
