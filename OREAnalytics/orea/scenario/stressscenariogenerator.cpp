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

#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

StressScenarioGenerator::StressScenarioGenerator(const boost::shared_ptr<StressTestScenarioData>& stressData,
                                                 const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                                 const QuantLib::Date& today,
                                                 const boost::shared_ptr<ore::data::Market>& initMarket,
                                                 const std::string& configuration,
                                                 boost::shared_ptr<ScenarioFactory> baseScenarioFactory)
    : ShiftScenarioGenerator(simMarketData, today, initMarket, configuration, baseScenarioFactory),
      stressData_(stressData) {
    QL_REQUIRE(stressData_ != NULL, "StressScenarioGenerator: stressData is null");
}

void StressScenarioGenerator::generateScenarios(const boost::shared_ptr<ScenarioFactory>& stressScenarioFactory) {
    for (Size i = 0; i < stressData_->data().size(); ++i) {
        StressTestScenarioData::StressTestData data = stressData_->data().at(i);
        boost::shared_ptr<Scenario> scenario = stressScenarioFactory->buildScenario(today_, data.label);

        addFxShifts(data, scenario);
        addDiscountCurveShifts(data, scenario);
        addIndexCurveShifts(data, scenario);
        addYieldCurveShifts(data, scenario);
        if (simMarketData_->simulateFXVols())
            addFxVolShifts(data, scenario);
        if (simMarketData_->simulateSwapVols())
            addSwaptionVolShifts(data, scenario);
        if (simMarketData_->simulateCapFloorVols())
            addCapFloorVolShifts(data, scenario);

        scenarios_.push_back(scenario);
    }

    LOG("stress scenario generator initialised");
}

void StressScenarioGenerator::addFxShifts(StressTestScenarioData::StressTestData& std,
                                          boost::shared_ptr<Scenario>& scenario) {
    for (auto d : std.fxShifts) {
        string ccypair = d.first; // foreign + domestic;

        // Is this too strict?
        // - implemented to avoid cases where input cross FX rates are not consistent
        // - Consider an example (baseCcy = EUR) of a GBPUSD FX trade - two separate routes to pricing
        // - (a) call GBPUSD FX rate from sim market
        // - (b) call GBPEUR and EURUSD FX rates, manually join them to obtain GBPUSD
        // - now, if GBPUSD is an explicit risk factor in sim market, consider what happens
        // - if we bump GBPUSD value and leave other FX rates unchanged (for e.g. a sensitivity analysis)
        // - (a) the value of the trade changes
        // - (b) the value of the GBPUSD trade stays the same
        // in light of the above we restrict the universe of FX pairs that we support here for the time being
        string baseCcy = simMarketData_->baseCcy();
        string foreign = ccypair.substr(0, 3);
        string domestic = ccypair.substr(3);
        QL_REQUIRE((domestic == baseCcy) || (foreign == baseCcy),
                   "SensitivityScenarioGenerator does not support cross FX pairs("
                       << ccypair << ", but base currency is " << baseCcy << ")");

        LOG("Apply stress scenario to fx " << ccypair);

        StressTestScenarioData::FxShiftData data = d.second;
        ShiftType type = parseShiftType(data.shiftType);
        bool relShift = (type == ShiftType::Relative);
        // QL_REQUIRE(type == ShiftType::Relative, "FX scenario type must be relative");
        Real size = data.shiftSize;

        Real rate = initMarket_->fxSpot(ccypair, configuration_)->value();
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(getFxKey(ccypair), newRate);
    }
    LOG("FX scenarios done");
}

void StressScenarioGenerator::addDiscountCurveShifts(StressTestScenarioData::StressTestData& std,
                                                     boost::shared_ptr<Scenario>& scenario) {
    for (auto d : std.discountCurveShifts) {
        string ccy = d.first;
        LOG("Apply stress scenario to discount curve " << ccy);

        Size n_ten = simMarketData_->yieldCurveTenors(ccy).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = d.second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = initMarket_->discountCurve(ccy, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date date = today_ + simMarketData_->yieldCurveTenors(ccy)[j];
            zeros[j] = ts->zeroRate(date, dc, Continuous);
            times[j] = dc.yearFraction(today_, date);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

        // apply zero rate shift at tenor point j
        for (Size j = 0; j < shiftTenors.size(); ++j)
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);

        // store shifted discount curve in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(getDiscountKey(ccy, k), shiftedDiscount);
        }
    }
    LOG("Discount curve stress scenarios done");
}

void StressScenarioGenerator::addIndexCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  boost::shared_ptr<Scenario>& scenario) {
    for (auto d : std.indexCurveShifts) {
        string indexName = d.first;
        LOG("Apply stress scenario to index curve " << indexName);

        Size n_ten = simMarketData_->yieldCurveTenors(indexName).size();

        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);

        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = d.second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<IborIndex> iborIndex = initMarket_->iborIndex(indexName, configuration_);
        Handle<YieldTermStructure> ts = iborIndex->forwardingTermStructure();
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date d = today_ + simMarketData_->yieldCurveTenors(indexName)[j];
            zeros[j] = ts->zeroRate(d, dc, Continuous);
            times[j] = dc.yearFraction(today_, d);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Index curve shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j)
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);

        // store shifted discount curve for this index in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(getIndexKey(indexName, k), shiftedDiscount);
        }
    }
    LOG("Index curve scenarios done");
}

void StressScenarioGenerator::addYieldCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  boost::shared_ptr<Scenario>& scenario) {
    for (auto d : std.yieldCurveShifts) {
        string name = d.first;
        LOG("Apply stress scenario to yield curve " << name);

        Size n_ten = simMarketData_->yieldCurveTenors(name).size();

        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);

        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = d.second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        Handle<YieldTermStructure> ts = initMarket_->yieldCurve(name, configuration_);
        DayCounter dc = ts->dayCounter();
        for (Size j = 0; j < n_ten; ++j) {
            Date date = today_ + simMarketData_->yieldCurveTenors(name)[j];
            zeros[j] = ts->zeroRate(date, dc, Continuous);
            times[j] = dc.yearFraction(today_, date);
            // DLOG("yield curve " << name << ": " << times[j] << " " << zeros[j]);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Yield curve shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            // DLOG("apply yield curve shift " << shifts[j] << " to curve " << name << " at tenor " << shiftTenors[j]
            //                                 << ", time " << shiftTimes[j]);
            // apply zero rate shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);
        }

        // store shifted discount curve in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(getYieldKey(name, k), shiftedDiscount);
            // DLOG("yield scenario " << name << ", " << k << ", " << shiftedZeros[k] << " " << zeros[k] << " "
            //                        << shiftedZeros[k] - zeros[k]);
        }
    } // end of shift curve tenors
    LOG("Yield curve scenarios done");
}

void StressScenarioGenerator::addFxVolShifts(StressTestScenarioData::StressTestData& std,
                                             boost::shared_ptr<Scenario>& scenario) {
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();

    std::vector<Real> values(n_fxvol_exp);
    std::vector<Real> times(n_fxvol_exp);

    // buffer for shifted zero curves
    std::vector<Real> shiftedValues(n_fxvol_exp);

    for (auto d : std.fxVolShifts) {
        string ccypair = d.first;
        LOG("Apply stress scenario to fx vol structure " << ccypair);

        StressTestScenarioData::FxVolShiftData data = d.second;

        Handle<BlackVolTermStructure> ts = initMarket_->fxVol(ccypair, configuration_);
        DayCounter dc = ts->dayCounter();
        Real strike = 0.0; // FIXME
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = today_ + simMarketData_->fxVolExpiries()[j];
            values[j] = ts->blackVol(d, strike);
            times[j] = dc.yearFraction(today_, d);
        }

        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

        // FIXME: Apply same shifts to non-ATM vectors if present
        for (Size j = 0; j < shiftTenors.size(); ++j) {
            // apply shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, values, times, shiftedValues, j == 0 ? true : false);
        }

        for (Size k = 0; k < n_fxvol_exp; ++k)
            scenario->add(getFxVolKey(ccypair, k), shiftedValues[k]);
    }
    LOG("FX vol scenarios done");
}

void StressScenarioGenerator::addSwaptionVolShifts(StressTestScenarioData::StressTestData& std,
                                                   boost::shared_ptr<Scenario>& scenario) {
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();

    vector<vector<Real>> volData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));
    vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
    vector<Real> volTermTimes(n_swvol_term, 0.0);
    vector<vector<Real>> shiftedVolData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));

    for (auto d : std.swaptionVolShifts) {
        std::string ccy = d.first;
        LOG("Apply stress scenario to swaption vol structure " << ccy);

        StressTestScenarioData::SwaptionVolShiftData data = d.second;
        ShiftType shiftType = parseShiftType(data.shiftType);
        map<pair<Period, Period>, Real> shifts = data.shifts;

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
        // FIXME: apply same shifts to all strikes when present
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftTermTimes.size(); ++k) {
                // Size strikeBucket = 0; // FIXME
                Real shift = 0.0;
                pair<Period, Period> key(data.shiftExpiries[j], data.shiftTerms[k]);
                if (shifts.size() == 0)
                    shift = data.parallelShiftSize;
                else {
                    QL_REQUIRE(shifts.find(key) != shifts.end(), "swaption vol shift not found for expiry "
                                                                     << data.shiftExpiries[j] << " and term "
                                                                     << data.shiftTerms[k]);
                    shift = shifts[key];
                }
                applyShift(j, k, shift, true, shiftType, shiftExpiryTimes, shiftTermTimes, volExpiryTimes, volTermTimes,
                           volData, shiftedVolData, j == 0 && k == 0);
            }
        }

        // add shifted vol data to the scenario
        for (Size jj = 0; jj < n_swvol_exp; ++jj) {
            for (Size kk = 0; kk < n_swvol_term; ++kk) {
                Size idx = jj * n_swvol_term + kk;
                scenario->add(getSwaptionVolKey(ccy, idx), shiftedVolData[jj][kk]);
            }
        }
    }
    LOG("Swaption vol scenarios done");
}

void StressScenarioGenerator::addCapFloorVolShifts(StressTestScenarioData::StressTestData& std,
                                                   boost::shared_ptr<Scenario>& scenario) {
    vector<Real> volStrikes = simMarketData_->capFloorVolStrikes();
    Size n_cfvol_strikes = volStrikes.size();

    for (auto d : std.capVolShifts) {
        std::string ccy = d.first;
        LOG("Apply stress scenario to cap/floor vol structure " << ccy);

        Size n_cfvol_exp = simMarketData_->capFloorVolExpiries(ccy).size();
        vector<vector<Real>> volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

        StressTestScenarioData::CapFloorVolShiftData data = d.second;

        ShiftType shiftType = parseShiftType(data.shiftType);
        vector<Real> shifts = data.shifts;
        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);

        Handle<OptionletVolatilityStructure> ts = initMarket_->capFloorVol(ccy, configuration_);
        DayCounter dc = ts->dayCounter();

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = today_ + simMarketData_->capFloorVolExpiries(ccy)[j];
            volExpiryTimes[j] = dc.yearFraction(today_, expiry);
        }
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Period expiry = simMarketData_->capFloorVolExpiries(ccy)[j];
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Real strike = simMarketData_->capFloorVolStrikes()[k];
                Real vol = ts->volatility(expiry, strike);
                volData[j][k] = vol;
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(today_, today_ + data.shiftExpiries[j]);

        // loop over shift expiries, apply same shifts across all strikes
        vector<Real> shiftStrikes = volStrikes;
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            for (Size k = 0; k < shiftStrikes.size(); ++k)
                applyShift(j, k, shifts[j], true, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, j == 0 && k == 0);

        // add shifted vol data to the scenario
        for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
            for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                Size idx = jj * n_cfvol_strikes + kk;
                scenario->add(getOptionletVolKey(ccy, idx), shiftedVolData[jj][kk]);
            }
        }
    }
    LOG("Optionlet vol scenarios done");
}
} // namespace analytics
} // namespace ore
