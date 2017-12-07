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
#include <ql/time/calendars/target.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

StressScenarioGenerator::StressScenarioGenerator(const boost::shared_ptr<StressTestScenarioData>& stressData,
                                                 const boost::shared_ptr<Scenario>& baseScenario,
                                                 const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData)
    : ShiftScenarioGenerator(baseScenario, simMarketData), stressData_(stressData) {
    QL_REQUIRE(stressData_ != NULL, "StressScenarioGenerator: stressData is null");
}

void StressScenarioGenerator::generateScenarios(const boost::shared_ptr<ScenarioFactory>& stressScenarioFactory) {
    Date asof = baseScenario_->asof();
    for (Size i = 0; i < stressData_->data().size(); ++i) {
        StressTestScenarioData::StressTestData data = stressData_->data().at(i);
        boost::shared_ptr<Scenario> scenario = stressScenarioFactory->buildScenario(asof, data.label);

        addFxShifts(data, scenario);
        addEquityShifts(data, scenario);
        addDiscountCurveShifts(data, scenario);
        addIndexCurveShifts(data, scenario);
        addYieldCurveShifts(data, scenario);
        if (simMarketData_->simulateFXVols())
            addFxVolShifts(data, scenario);
        if (simMarketData_->simulateEquityVols())
            addEquityVolShifts(data, scenario);
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

        StressTestScenarioData::SpotShiftData data = d.second;
        ShiftType type = parseShiftType(data.shiftType);
        bool relShift = (type == ShiftType::Relative);
        // QL_REQUIRE(type == ShiftType::Relative, "FX scenario type must be relative");
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
        Real rate = scenario->get(key);
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::FXSpot, ccypair), newRate);
    }
    LOG("FX scenarios done");
}

void StressScenarioGenerator::addEquityShifts(StressTestScenarioData::StressTestData& std,
                                              boost::shared_ptr<Scenario>& scenario) {
    for (auto d : std.equityShifts) {
        string equity = d.first;
        StressTestScenarioData::SpotShiftData data = d.second;
        ShiftType type = parseShiftType(data.shiftType);
        bool relShift = (type == ShiftType::Relative);
        // QL_REQUIRE(type == ShiftType::Relative, "FX scenario type must be relative");
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
        Real rate = baseScenario_->get(key);

        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::EquitySpot, equity), newRate);
    }
    LOG("Equity scenarios done");
}

void StressScenarioGenerator::addDiscountCurveShifts(StressTestScenarioData::StressTestData& std,
                                                     boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

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
        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(ccy));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(ccy)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        // apply zero rate shift at tenor point j
        for (Size j = 0; j < shiftTenors.size(); ++j)
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);

        // store shifted discount curve in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(RiskFactorKey(RiskFactorKey::KeyType::DiscountCurve, ccy, k), shiftedDiscount);
        }
    }
    LOG("Discount curve stress scenarios done");
}

void StressScenarioGenerator::addIndexCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

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
        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(indexName));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(indexName)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Index curve shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j)
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);

        // store shifted discount curve for this index in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(RiskFactorKey(RiskFactorKey::KeyType::IndexCurve, indexName, k), shiftedDiscount);
        }
    }
    LOG("Index curve scenarios done");
}

void StressScenarioGenerator::addYieldCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

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
        DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(name));

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, j);
            Real quote = baseScenario_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Yield curve shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        for (Size j = 0; j < shiftTenors.size(); ++j) {
            // DLOG("apply yield curve shift " << shifts[j] << " to curve " << name << " at tenor " << shiftTenors[j]
            //                                 << ", time " << shiftTimes[j]);
            // apply zero rate shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, zeros, times, shiftedZeros, j == 0 ? true : false);
        }

        // store shifted discount curve in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
            scenario->add(RiskFactorKey(RiskFactorKey::KeyType::YieldCurve, name, k), shiftedDiscount);
            // DLOG("yield scenario " << name << ", " << k << ", " << shiftedZeros[k] << " " << zeros[k] << " "
            //                        << shiftedZeros[k] - zeros[k]);
        }
    } // end of shift curve tenors
    LOG("Yield curve scenarios done");
}

void StressScenarioGenerator::addFxVolShifts(StressTestScenarioData::StressTestData& std,
                                             boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();

    std::vector<Real> values(n_fxvol_exp);
    std::vector<Real> times(n_fxvol_exp);

    // buffer for shifted zero curves
    std::vector<Real> shiftedValues(n_fxvol_exp);

    for (auto d : std.fxVolShifts) {
        string ccypair = d.first;
        LOG("Apply stress scenario to fx vol structure " << ccypair);

        StressTestScenarioData::VolShiftData data = d.second;

        DayCounter dc = parseDayCounter(simMarketData_->fxVolDayCounter(ccypair));
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = asof + simMarketData_->fxVolExpiries()[j];

            RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, j);
            values[j] = baseScenario_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        // FIXME: Apply same shifts to non-ATM vectors if present
        for (Size j = 0; j < shiftTenors.size(); ++j) {
            // apply shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, values, times, shiftedValues, j == 0 ? true : false);
        }

        for (Size k = 0; k < n_fxvol_exp; ++k)
            scenario->add(RiskFactorKey(RiskFactorKey::KeyType::FXVolatility, ccypair, k), shiftedValues[k]);
    }
    LOG("FX vol scenarios done");
}

void StressScenarioGenerator::addEquityVolShifts(StressTestScenarioData::StressTestData& std,
                                                 boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

    Size n_eqvol_exp = simMarketData_->equityVolExpiries().size();

    std::vector<Real> values(n_eqvol_exp);
    std::vector<Real> times(n_eqvol_exp);

    // buffer for shifted zero curves
    std::vector<Real> shiftedValues(n_eqvol_exp);

    for (auto d : std.equityVolShifts) {
        string equity = d.first;
        LOG("Apply stress scenario to equity vol structure " << equity);

        StressTestScenarioData::VolShiftData data = d.second;

        DayCounter dc = parseDayCounter(simMarketData_->equityVolDayCounter(equity));
        for (Size j = 0; j < n_eqvol_exp; ++j) {
            Date d = asof + simMarketData_->equityVolExpiries()[j];

            RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, j);
            values[j] = baseScenario_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        ShiftType shiftType = parseShiftType(data.shiftType);
        std::vector<Period> shiftTenors = data.shiftExpiries;
        std::vector<Time> shiftTimes(shiftTenors.size());
        vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() > 0, "Equity vol shift tenors not specified");
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");

        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        // FIXME: Apply same shifts to non-ATM vectors if present
        for (Size j = 0; j < shiftTenors.size(); ++j) {
            // apply shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, values, times, shiftedValues, j == 0 ? true : false);
        }

        for (Size k = 0; k < n_eqvol_exp; ++k)
            scenario->add(RiskFactorKey(RiskFactorKey::KeyType::EquityVolatility, equity, k), shiftedValues[k]);
    }
    LOG("Equity vol scenarios done");
}

void StressScenarioGenerator::addSwaptionVolShifts(StressTestScenarioData::StressTestData& std,
                                                   boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

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
                Size idx = j * n_swvol_term + k;

                RiskFactorKey key(RiskFactorKey::KeyType::SwaptionVolatility, ccy, idx);
                volData[j][k] = baseScenario_->get(key);
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);
        for (Size j = 0; j < shiftTermTimes.size(); ++j)
            shiftTermTimes[j] = dc.yearFraction(asof, asof + data.shiftTerms[j]);

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
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::SwaptionVolatility, ccy, idx),
                              shiftedVolData[jj][kk]);
            }
        }
    }
    LOG("Swaption vol scenarios done");
}

void StressScenarioGenerator::addCapFloorVolShifts(StressTestScenarioData::StressTestData& std,
                                                   boost::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();

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
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);

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
                scenario->add(RiskFactorKey(RiskFactorKey::KeyType::OptionletVolatility, ccy, idx),
                              shiftedVolData[jj][kk]);
            }
        }
    }
    LOG("Optionlet vol scenarios done");
}
} // namespace analytics
} // namespace ore
