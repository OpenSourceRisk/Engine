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
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/utilities/log.hpp>
#include <ql/time/calendars/target.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

namespace {
    template<class StressTestShifts>
    std::vector<Wildcard> wildcardList(
        const map<string, QuantLib::ext::shared_ptr<StressTestShifts>>& shifts) {
        std::vector<Wildcard> wcs;
        for (auto const& shift : shifts) {
            Wildcard wc(shift.first);
            if (wc.hasWildcard()) {
                wcs.push_back(wc);
            }
        }
        return wcs;
    }
}

StressScenarioGenerator::StressScenarioGenerator(const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                                                 const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
                                                 const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                                 const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                                                 const QuantLib::ext::shared_ptr<ScenarioFactory>& stressScenarioFactory,
                                                 const QuantLib::ext::shared_ptr<Scenario>& baseScenarioAbsolute)
    : ShiftScenarioGenerator(baseScenario, simMarketData, simMarket), stressData_(stressData),
      stressScenarioFactory_(stressScenarioFactory),
      baseScenarioAbsolute_(baseScenarioAbsolute == nullptr ? baseScenario : baseScenarioAbsolute) {

    QL_REQUIRE(stressData_, "StressScenarioGenerator: stressData is null");

    generateScenarios();
}

void StressScenarioGenerator::generateScenarios() {
    Date asof = baseScenario_->asof();
    for (Size i = 0; i < stressData_->data().size(); ++i) {
        StressTestScenarioData::StressTestData data = stressData_->data().at(i);
        DLOG("Generate stress scenario #" << i << " '" << data.label << "'");
        QuantLib::ext::shared_ptr<Scenario> scenario =
            stressScenarioFactory_->buildScenario(asof, !stressData_->useSpreadedTermStructures(), false, data.label);

        if (simMarketData_->simulateFxSpots())
            addFxShifts(data, scenario);
        addEquityShifts(data, scenario);
        if (simMarketData_->commodityCurveSimulate())
            addCommodityCurveShifts(data, scenario);
        addDiscountCurveShifts(data, scenario);
        addIndexCurveShifts(data, scenario);
        addYieldCurveShifts(data, scenario);
        if (simMarketData_->simulateFXVols())
            addFxVolShifts(data, scenario);
        if (simMarketData_->simulateEquityVols())
            addEquityVolShifts(data, scenario);
        if (simMarketData_->commodityVolSimulate())
            addCommodityVolShifts(data, scenario);
        if (simMarketData_->simulateSwapVols())
            addSwaptionVolShifts(data, scenario);
        if (simMarketData_->simulateCapFloorVols())
            addCapFloorVolShifts(data, scenario);
        if (simMarketData_->securitySpreadsSimulate())
            addSecuritySpreadShifts(data, scenario);
        if (simMarketData_->simulateRecoveryRates())
            addRecoveryRateShifts(data, scenario);
        if (simMarketData_->simulateSurvivalProbabilities())
            addSurvivalProbabilityShifts(data, scenario);

        scenarios_.push_back(scenario);
    }

    DLOG("stress scenario generator: all scenarios generated.");
}

template<class StressTestShifts>
map<string, QuantLib::ext::shared_ptr<StressTestShifts>>
StressScenarioGenerator::populateShiftData(
    const map<string, QuantLib::ext::shared_ptr<StressTestShifts>>& stressTestShifts,
    const vector<Wildcard>& wildcardKeys,
    RiskFactorKey::KeyType keyType) const {
    if (wildcardKeys.empty()) {
        return stressTestShifts;
    }
    auto data = stressTestShifts;
    const auto& keys = baseScenarioAbsolute_->keys();
    for(const auto& key : keys) {
        if (key.keytype == keyType && data.find(key.name) == data.end()) {
            for (const auto& wildcardKey : wildcardKeys) {
                if (wildcardKey.matches(key.name)) {
                    data[key.name] = stressTestShifts.at(wildcardKey.pattern());
                    break;
                }
            }
        }
    }
    for (const auto& key : wildcardKeys) {
        data.erase(key.pattern());
    }
    return data;
}

void StressScenarioGenerator::addFxShifts(StressTestScenarioData::StressTestData& std,
                                          QuantLib::ext::shared_ptr<Scenario>& scenario) {
    auto& data = std.fxShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::FXSpot);
    }

    for (auto d : data) {
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

        TLOG("Apply stress scenario to fx " << ccypair);

        StressTestScenarioData::SpotShiftData data = *d.second;
        ShiftType type = data.shiftType;
        bool relShift = (type == ShiftType::Relative);
        // QL_REQUIRE(type == ShiftType::Relative, "FX scenario type must be relative");
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::FXSpot, ccypair);
        Real rate = scenario->get(key);
        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::FXSpot, ccypair),
                      stressData_->useSpreadedTermStructures() ? newRate / rate : newRate);
    }
    DLOG("FX scenarios done");
}

void StressScenarioGenerator::addEquityShifts(StressTestScenarioData::StressTestData& std,
                                              QuantLib::ext::shared_ptr<Scenario>& scenario) {
    auto& data = std.equityShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::EquitySpot);
    }

    for(const auto& d : data) {
        string equity = d.first;
        StressTestScenarioData::SpotShiftData data = *d.second;
        ShiftType type = data.shiftType;
        bool relShift = (type == ShiftType::Relative);
        // QL_REQUIRE(type == ShiftType::Relative, "EQ scenario type must be relative");
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::EquitySpot, equity);
        Real rate = baseScenarioAbsolute_->get(key);

        Real newRate = relShift ? rate * (1.0 + size) : (rate + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::EquitySpot, equity),
                      stressData_->useSpreadedTermStructures() ? newRate / rate : newRate);
    }
    DLOG("Equity scenarios done");
}

void StressScenarioGenerator::addCommodityCurveShifts(StressTestScenarioData::StressTestData& std,
    QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.commodityCurveShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::CommodityCurve);
    }

    for (auto d : std.commodityCurveShifts) {
        string commodity = d.first;
        TLOG("Apply stress scenario to commodity curve " << commodity);

        Size n_ten = simMarketData_->commodityCurveTenors(commodity).size();
        // original curves' buffer
        std::vector<Real> basePrices(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted price curves
        std::vector<Real> shiftedPrices(n_ten);

        StressTestScenarioData::CurveShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        DayCounter dc = Actual365Fixed();
        if(auto s = simMarket_.lock()) {
            dc = s->commodityPriceCurve(commodity)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->commodityCurveTenors(commodity)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::CommodityCurve, commodity, j);
            basePrices[j] = baseScenarioAbsolute_->get(key);
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Commodity shift tenors not specified");
        std::vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");
        std::vector<Time> shiftTimes(shiftTenors.size());
        for (Size j = 0; j < shiftTenors.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftTenors[j]);

        // apply price shift at tenor point j
        for (Size j = 0; j < shiftTenors.size(); ++j)
            applyShift(j, shifts[j], true, shiftType, shiftTimes, basePrices, times, shiftedPrices, j == 0 ? true : false);

        // store shifted commodity price curve in the scenario
        for (Size k = 0; k < n_ten; ++k) {
            RiskFactorKey key(RiskFactorKey::KeyType::CommodityCurve, commodity, k);
            if (stressData_->useSpreadedTermStructures()) {
                scenario->add(key, shiftedPrices[k] - basePrices[k]);
            } else {
                scenario->add(key, shiftedPrices[k]);
            }
        }
    }
    DLOG("Commodity curve stress scenarios done");
}

void StressScenarioGenerator::addDiscountCurveShifts(StressTestScenarioData::StressTestData& std,
                                                     QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.discountCurveShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::DiscountCurve);
    }

    for (auto d : data) {
        string ccy = d.first;
        TLOG("Apply stress scenario to discount curve " << ccy);

        Size n_ten = simMarketData_->yieldCurveTenors(ccy).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        //DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(ccy));
	DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->discountCurve(ccy)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(ccy)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
            Real quote = baseScenarioAbsolute_->get(key);
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
            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, k);
            Real shiftedDiscount = std::exp(-shiftedZeros[k] * times[k]);
            if (stressData_->useSpreadedTermStructures()) {
                Real discount = std::exp(-zeros[k] * times[k]);
                scenario->add(key, shiftedDiscount / discount);
            } else {
                scenario->add(key, shiftedDiscount);
            }
        }
    }
    DLOG("Discount curve stress scenarios done");
}

void StressScenarioGenerator::addSurvivalProbabilityShifts(StressTestScenarioData::StressTestData& std,
                                                           QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.survivalProbabilityShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::SurvivalProbability);
    }

    for(const auto& d : data) {
        string name = d.first;
        TLOG("Apply stress scenario to " << name);

        Size n_ten = simMarketData_->defaultTenors(name).size();
        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);
        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        //DayCounter dc = parseDayCounter(simMarketData_->defaultCurveDayCounter(name));
	DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->defaultCurve(name)->curve()->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->defaultTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, j);
            Real quote = baseScenarioAbsolute_->get(key);
            zeros[j] = -std::log(quote) / times[j];
        }

        std::vector<Period> shiftTenors = data.shiftTenors;
        QL_REQUIRE(shiftTenors.size() > 0, "Survival Probability shift tenors not specified");
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
            RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, k);
            Real shiftedSurvivalProbability = std::exp(-shiftedZeros[k] * times[k]);
            if (stressData_->useSpreadedTermStructures()) {
                Real survivalProbability = std::exp(-zeros[k] * times[k]);
                scenario->add(key, shiftedSurvivalProbability / survivalProbability);
            } else {
                scenario->add(key, shiftedSurvivalProbability);
            }
        }
    }
    DLOG("Default Curve stress scenarios done");
}

void StressScenarioGenerator::addIndexCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.indexCurveShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::IndexCurve);
    }

    for (auto d : data) {
        string indexName = d.first;
        TLOG("Apply stress scenario to index curve " << indexName);

        Size n_ten = simMarketData_->yieldCurveTenors(indexName).size();

        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);

        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        //DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(indexName));
	DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->iborIndex(indexName)->forwardingTermStructure()->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(indexName)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
            Real quote = baseScenarioAbsolute_->get(key);
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
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, k);
            Real shiftedDiscount = std::exp(-shiftedZeros[k] * times[k]);
            if (stressData_->useSpreadedTermStructures()) {
                Real discount = std::exp(-zeros[k] * times[k]);
                scenario->add(key, shiftedDiscount / discount);
            } else {
                scenario->add(key, shiftedDiscount);
            }
        }
    }
    DLOG("Index curve scenarios done");
}

void StressScenarioGenerator::addYieldCurveShifts(StressTestScenarioData::StressTestData& std,
                                                  QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.yieldCurveShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::YieldCurve);
    }

    for (auto d : data) {
        string name = d.first;
        TLOG("Apply stress scenario to yield curve " << name);

        Size n_ten = simMarketData_->yieldCurveTenors(name).size();

        // original curves' buffer
        std::vector<Real> zeros(n_ten);
        std::vector<Real> times(n_ten);

        // buffer for shifted zero curves
        std::vector<Real> shiftedZeros(n_ten);

        StressTestScenarioData::CurveShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        //DayCounter dc = parseDayCounter(simMarketData_->yieldCurveDayCounter(name));
	DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->yieldCurve(name)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        for (Size j = 0; j < n_ten; ++j) {
            Date d = asof + simMarketData_->yieldCurveTenors(name)[j];
            times[j] = dc.yearFraction(asof, d);
            RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, j);
            Real quote = baseScenarioAbsolute_->get(key);
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
            RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, name, k);
            Real shiftedDiscount = std::exp(-shiftedZeros[k] * times[k]);
            if (stressData_->useSpreadedTermStructures()) {
                Real discount = std::exp(-zeros[k] * times[k]);
                scenario->add(key, shiftedDiscount / discount);
            } else {
                scenario->add(key, shiftedDiscount);
            }
        }
    } // end of shift curve tenors
    DLOG("Yield curve scenarios done");
}

void StressScenarioGenerator::addFxVolShifts(StressTestScenarioData::StressTestData& std,
                                             QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.fxVolShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::FXVolatility);
    }
    
    for (auto d : data) {
        string ccypair = d.first;
        if(simMarketData_->fxVolIsSurface(ccypair)){
            StructuredConfigurationErrorMessage("Simulation Market", "Fx Volatility",
                                                "Stresstest support only ATM shifts", "Skip stresstest for " + ccypair)
                .log();
            continue;
        }

        TLOG("Apply stress scenario to fx vol structure " << ccypair);

        Size n_fxvol_exp = simMarketData_->fxVolExpiries(ccypair).size();

        std::vector<Real> values(n_fxvol_exp);
        std::vector<Real> times(n_fxvol_exp);

        // buffer for shifted zero curves
        std::vector<Real> shiftedValues(n_fxvol_exp);

        StressTestScenarioData::FXVolShiftData data = *d.second;

        //DayCounter dc = parseDayCounter(simMarketData_->fxVolDayCounter(ccypair));
        DayCounter dc;
        if (auto s = simMarket_.lock()) {
            dc = s->fxVol(ccypair)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }
        for (Size j = 0; j < n_fxvol_exp; ++j) {
            Date d = asof + simMarketData_->fxVolExpiries(ccypair)[j];

            RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, j);
            values[j] = baseScenarioAbsolute_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        ShiftType shiftType = data.shiftType;
        vector<Real> shifts;
        std::vector<Time> shiftTimes;

        if (data.mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Explicit) {
            std::vector<Period> shiftTenors = data.shiftExpiries;
            shifts = data.shifts;
            QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");
            QL_REQUIRE(shiftTenors.size() == shifts.size(), "shift tenor and shift size vectors do not match");

            for (Size j = 0; j < shiftTenors.size(); ++j){
                shiftTimes.push_back(dc.yearFraction(asof, asof + shiftTenors[j]));
            }
        } else if (data.mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Weighted) {
            DLOG("FX Vol Stress Scenario with weighted schema")
            QL_REQUIRE(data.weightTenors.size() == data.weights.size(), "Mismatch between weights and weightTenors");
            QL_REQUIRE(data.shiftExpiries.size() == 1, "Weighting schema 'Weighted' requires exactly one tenor");
            QL_REQUIRE(data.shifts.size() == 1, "Weighting schema 'Weighted' requires exactly one shift");
            Period referenceTenor = data.shiftExpiries.front();
            Real referenceShift = data.shifts.front();
            std::vector<Period> shiftTenors = data.weightTenors;
            auto it = std::find(shiftTenors.begin(), shiftTenors.end(), referenceTenor);
            QL_REQUIRE(it != shiftTenors.end(), "Couldnt find reference weight for shift expiry");
            size_t weightPos = std::distance(shiftTenors.begin(), it);
            double referenceWeight = data.weights[weightPos];
            QL_REQUIRE(referenceWeight > 0.0 && !QuantLib::close_enough(referenceWeight, 0.0),
                       "Only strict positive reference weight is allowed");
            const double weightedRefShift = referenceShift / referenceWeight;
            DLOG("Compute shifts from weights")
            DLOG("j,pillar,time,shift")
            for (Size j = 0; j < shiftTenors.size(); ++j) {
                QL_REQUIRE(data.weights[j] >= 0, "only positive weights for weighted stresstest allowed");
                shiftTimes.push_back(dc.yearFraction(asof, asof + shiftTenors[j]));
                shifts.push_back(weightedRefShift * data.weights[j]);
                DLOG(j << "," << shiftTenors[j] << "," << shiftTimes.back() << "," << shifts.back());
            }
        } else if (data.mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Unadjusted) {
            QL_REQUIRE(data.shiftExpiries.size() == 1, "Weighting schema 'Unadjusted' requires exactly one tenor");
            QL_REQUIRE(data.shifts.size() == 1, "Weighting schema 'Unadjusted' requires exactly one shift");
            Time expiryTime = dc.yearFraction(asof, asof + data.shiftExpiries.front());
            if (expiryTime >= 1e-6) {
                shiftTimes.push_back(expiryTime - 1e-6);
                shiftTimes.push_back(expiryTime);
                shiftTimes.push_back(expiryTime + 1e-6);
                shifts.push_back(0.0);
                shifts.push_back(data.shifts.front());
                shifts.push_back(0.0);
            } else {
                shiftTimes.push_back(expiryTime);
                shiftTimes.push_back(expiryTime + 1e-6);
                shifts.push_back(data.shifts.front());
                shifts.push_back(0.0);
            }
        }
        DLOG("Apply shift with shiftType " << shiftType);
        // FIXME: Apply same shifts to non-ATM vectors if present
        for (Size j = 0; j < shiftTimes.size(); ++j) {
            // apply shift at tenor point j
            applyShift(j, shifts[j], true, shiftType, shiftTimes, values, times, shiftedValues, j == 0 ? true : false);
        }

        DLOG("Output generated fx vol stress test scenario " << std.label << " " << ccypair);
        DLOG("j,rfkey,tenor,time,value,shiftedvalue,shift");


        for (Size k = 0; k < n_fxvol_exp; ++k) {
            RiskFactorKey key(RiskFactorKey::KeyType::FXVolatility, ccypair, k);
            DLOG(k << "," << key << "," << simMarketData_->fxVolExpiries(ccypair)[k] << "," << times[k] << ","
                   << values[k] << "," << shiftedValues[k] << "," << shiftedValues[k] - values[k]);
            if (stressData_->useSpreadedTermStructures()) {
                scenario->add(key, shiftedValues[k] - values[k]);
            } else {
                scenario->add(key, shiftedValues[k]);
            }
        }
    }
    DLOG("FX vol scenarios done");
}

void StressScenarioGenerator::addEquityVolShifts(StressTestScenarioData::StressTestData& std,
                                                 QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.equityVolShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::EquityVolatility);
    }

    for(const auto& d : data) {
        string equity = d.first;
        TLOG("Apply stress scenario to equity vol structure " << equity);
        Size n_eqvol_exp = simMarketData_->equityVolExpiries(equity).size();

        std::vector<Real> values(n_eqvol_exp);
        std::vector<Real> times(n_eqvol_exp);

        // buffer for shifted zero curves
        std::vector<Real> shiftedValues(n_eqvol_exp);

        StressTestScenarioData::VolShiftData data = *d.second;

        //DayCounter dc = parseDayCounter(simMarketData_->equityVolDayCounter(equity));
	    DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->equityVol(equity)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }
        for (Size j = 0; j < n_eqvol_exp; ++j) {
            Date d = asof + simMarketData_->equityVolExpiries(equity)[j];

            RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, j);
            values[j] = baseScenarioAbsolute_->get(key);

            times[j] = dc.yearFraction(asof, d);
        }

        ShiftType shiftType = data.shiftType;
        
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

        for (Size k = 0; k < n_eqvol_exp; ++k) {
            RiskFactorKey key(RiskFactorKey::KeyType::EquityVolatility, equity, k);
            if (stressData_->useSpreadedTermStructures()) {
                scenario->add(key, shiftedValues[k] - values[k]);
            } else {
                scenario->add(key, shiftedValues[k]);
            }
        }
    }
    DLOG("Equity vol scenarios done");
}

void StressScenarioGenerator::addCommodityVolShifts(StressTestScenarioData::StressTestData& std,
    QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.commodityVolShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::CommodityVolatility);
    }

    for (auto d : data) {
        string commodity = d.first;
        TLOG("Apply stress scenario to commodity vol structure " << commodity);
        vector<Period> expiries = simMarketData_->commodityVolExpiries(commodity);
        const vector<Real>& moneyness = simMarketData_->commodityVolMoneyness(commodity);

        // Store base scenario volatilities, strike x expiry
        vector<vector<Real>> baseValues(moneyness.size(), vector<Real>(expiries.size()));
        // Time to each expiry
        vector<Time> times(expiries.size());
        // Store shifted scenario volatilities
        vector<vector<Real>> shiftedValues = baseValues;

        StressTestScenarioData::CommodityVolShiftData data = *d.second;

        DayCounter dc = Actual365Fixed();
        try {
            if (auto s = simMarket_.lock()) {
                dc = s->commodityVolatility(commodity)->dayCounter();
            } else {
                QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
            }
        } catch (const std::exception&) {
            WLOG("Day counter lookup in simulation market failed for commodity vol surface " << commodity
                                                                                             << ", using default A365");
        }
        for (Size j = 0; j < expiries.size(); ++j) {
            Date d = asof + simMarketData_->commodityVolExpiries(commodity)[j];
            times[j] = dc.yearFraction(asof, d);
            for (Size i = 0; i < moneyness.size(); i++) {
                RiskFactorKey key(RiskFactorKey::KeyType::CommodityVolatility, commodity, i * expiries.size() + j);
                baseValues[i][j] = baseScenarioAbsolute_->get(key);
            }
        }

        ShiftType shiftType = data.shiftType;

        std::vector<Period> shiftExpiries = data.shiftExpiries;
        std::vector<Real> shiftMoneyness = data.shiftMoneyness;
        std::vector<Time> shiftTimes(shiftExpiries.size());
        vector<Real> shifts = data.shifts;
        QL_REQUIRE(shiftExpiries.size() > 0, "Commodity vol shift tenors not specified");
        QL_REQUIRE(shiftExpiries.size() == shifts.size(), "shift tenor and shift size vectors do not match");

        for (Size j = 0; j < shiftExpiries.size(); ++j)
            shiftTimes[j] = dc.yearFraction(asof, asof + shiftExpiries[j]);

        for (Size sj = 0; sj < shiftExpiries.size(); ++sj) {
            for (Size si = 0; si < shiftMoneyness.size(); ++si) {
                applyShift(si, sj, shifts[sj], true, shiftType, shiftMoneyness, shiftTimes, moneyness, times, baseValues, shiftedValues, sj == 0 ? true : false);

                Size counter = 0;
                for (Size i = 0; i < moneyness.size(); i++) {
                    for (Size j = 0; j < expiries.size(); ++j) {
                        RiskFactorKey key(RiskFactorKey::KeyType::CommodityVolatility, commodity, counter++);
                        if (stressData_->useSpreadedTermStructures()) {
                            scenario->add(key, shiftedValues[i][j] - baseValues[i][j]);
                        } else {
                            scenario->add(key, shiftedValues[i][j]);
                        }
                    }
                }
            }
        }
    }
    DLOG("Commodity vol scenarios done");
}

void StressScenarioGenerator::addSwaptionVolShifts(StressTestScenarioData::StressTestData& std,
                                                   QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.swaptionVolShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::SwaptionVolatility);
    }

    for (auto d : data) {
        std::string key = d.first;
        TLOG("Apply stress scenario to swaption vol structure '" << key << "'");

        Size n_swvol_term = simMarketData_->swapVolTerms(key).size();
        Size n_swvol_exp = simMarketData_->swapVolExpiries(key).size();
        Size n_swvol_strike = simMarketData_->swapVolStrikeSpreads(key).size();

        vector<vector<Real>> volData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));
        vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
        vector<Real> volTermTimes(n_swvol_term, 0.0);
        vector<vector<Real>> shiftedVolData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));

        StressTestScenarioData::SwaptionVolShiftData data = *d.second;
        ShiftType shiftType = data.shiftType;
        map<pair<Period, Period>, Real> shifts = data.shifts;

        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftTermTimes(data.shiftTerms.size(), 0.0);

	DayCounter dc;
        if(auto s = simMarket_.lock()) {
            dc = s->swaptionVol(key)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        // cache original vol data
        for (Size j = 0; j < n_swvol_exp; ++j) {
            Date expiry = asof + simMarketData_->swapVolExpiries(key)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_swvol_term; ++j) {
            Date term = asof + simMarketData_->swapVolTerms(key)[j];
            volTermTimes[j] = dc.yearFraction(asof, term);
        }
        for (Size j = 0; j < n_swvol_exp; ++j) {
            for (Size k = 0; k < n_swvol_term; ++k) {
                Size idx = j * n_swvol_term + k;

                RiskFactorKey rf(RiskFactorKey::KeyType::SwaptionVolatility, key, idx);
                volData[j][k] = baseScenarioAbsolute_->get(rf);
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
        for (Size ii = 0; ii < n_swvol_strike; ++ii) {
            for (Size jj = 0; jj < n_swvol_exp; ++jj) {
                for (Size kk = 0; kk < n_swvol_term; ++kk) {
                    Size idx = jj * n_swvol_term * n_swvol_strike + kk * n_swvol_strike + ii;
                    RiskFactorKey rfkey(RiskFactorKey::KeyType::SwaptionVolatility, key, idx);
                    if (stressData_->useSpreadedTermStructures()) {
                        scenario->add(rfkey, shiftedVolData[jj][kk] - volData[jj][kk]);
                    } else {
                        scenario->add(rfkey, shiftedVolData[jj][kk]);
                    }
                }
            }
        }
    }
    DLOG("Swaption vol scenarios done");
}

void StressScenarioGenerator::addCapFloorVolShifts(StressTestScenarioData::StressTestData& std,
                                                   QuantLib::ext::shared_ptr<Scenario>& scenario) {
    Date asof = baseScenario_->asof();
    auto& data = std.capVolShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::OptionletVolatility);
    }

    for (auto d : data) {
        std::string key = d.first;
        TLOG("Apply stress scenario to cap/floor vol structure " << key);

        vector<Real> volStrikes = simMarketData_->capFloorVolStrikes(key);
        // Strikes may be empty which indicates that the optionlet structure in the simulation market is an ATM curve
        if (volStrikes.empty()) {
            volStrikes = {0.0};
        }
        Size n_cfvol_strikes = volStrikes.size();

        Size n_cfvol_exp = simMarketData_->capFloorVolExpiries(key).size();
        vector<vector<Real>> volData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));
        vector<Real> volExpiryTimes(n_cfvol_exp, 0.0);
        vector<vector<Real>> shiftedVolData(n_cfvol_exp, vector<Real>(n_cfvol_strikes, 0.0));

        StressTestScenarioData::CapFloorVolShiftData data = *d.second;

        ShiftType shiftType = data.shiftType;
        
        vector<Real> shiftExpiryTimes(data.shiftExpiries.size(), 0.0);
        vector<Real> shiftStrikes = data.shiftStrikes.empty() ? volStrikes : data.shiftStrikes;
        
        vector<vector<Real>> shifts;
        for (size_t i = 0; i < data.shiftExpiries.size(); ++i) {
            const auto tenor = data.shiftExpiries[i];
            if (data.shiftStrikes.empty()){
                const double shift = data.shifts[tenor].front();
                shifts.push_back(std::vector<Real>(volStrikes.size(), shift));
            } else{
                shifts.push_back(data.shifts[tenor]);
            }
        }

        // DayCounter dc = parseDayCounter(simMarketData_->capFloorVolDayCounter(key));
        DayCounter dc;
        if (auto s = simMarket_.lock()) {
            dc = s->capFloorVol(key)->dayCounter();
        } else {
            QL_FAIL("Internal error: could not lock simMarket. Contact dev.");
        }

        // cache original vol data
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Date expiry = asof + simMarketData_->capFloorVolExpiries(key)[j];
            volExpiryTimes[j] = dc.yearFraction(asof, expiry);
        }
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                Size idx = j * n_cfvol_strikes + k;
                volData[j][k] =
                    baseScenarioAbsolute_->get(RiskFactorKey(RiskFactorKey::KeyType::OptionletVolatility, key, idx));
            }
        }

        // cache tenor times
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
            shiftExpiryTimes[j] = dc.yearFraction(asof, asof + data.shiftExpiries[j]);

        // loop over shift expiries, apply same shifts across all strikes
        
        for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
            for (Size k = 0; k < shiftStrikes.size(); ++k) {
                applyShift(j, k, shifts[j][k], true, shiftType, shiftExpiryTimes, shiftStrikes, volExpiryTimes, volStrikes,
                           volData, shiftedVolData, j == 0 && k == 0);
            }
        }

        // add shifted vol data to the scenario
        for (Size jj = 0; jj < n_cfvol_exp; ++jj) {
            for (Size kk = 0; kk < n_cfvol_strikes; ++kk) {
                Size idx = jj * n_cfvol_strikes + kk;
                RiskFactorKey rfkey(RiskFactorKey::KeyType::OptionletVolatility, key, idx);
                if (stressData_->useSpreadedTermStructures()) {
                    scenario->add(rfkey, shiftedVolData[jj][kk] - volData[jj][kk]);
                } else {
                    scenario->add(rfkey, shiftedVolData[jj][kk]);
                }
            }
        }
    }
    DLOG("Optionlet vol scenarios done");
}

void StressScenarioGenerator::addSecuritySpreadShifts(StressTestScenarioData::StressTestData& std,
                                                      QuantLib::ext::shared_ptr<Scenario>& scenario) {
    auto& data = std.securitySpreadShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::SecuritySpread);
    }

    for(const auto& d : data) {
        string bond = d.first;
        TLOG("Apply stress scenario to security spread " << bond);
        StressTestScenarioData::SpotShiftData data = *d.second;
        ShiftType type = data.shiftType;
        bool relShift = (type == ShiftType::Relative);
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::SecuritySpread, bond);
        Real base_spread = baseScenarioAbsolute_->get(key);

        Real newSpread = relShift ? base_spread * (1.0 + size) : (base_spread + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::SecuritySpread, bond),
                      stressData_->useSpreadedTermStructures() ? newSpread - base_spread : newSpread);
    }
    DLOG("Security spread scenarios done");
}

void StressScenarioGenerator::addRecoveryRateShifts(StressTestScenarioData::StressTestData& std,
                                                    QuantLib::ext::shared_ptr<Scenario>& scenario) {
    auto& data = std.recoveryRateShifts;
    auto wildcards = wildcardList(data);
    if (wildcards.size() > 0) {
        data = populateShiftData(data, wildcards, RiskFactorKey::KeyType::RecoveryRate);
    }

    for(const auto& d : data) {
        string isin = d.first;
        TLOG("Apply stress scenario to recovery rate " << isin);
        StressTestScenarioData::SpotShiftData data = *d.second;
        ShiftType type = data.shiftType;
        bool relShift = (type == ShiftType::Relative);
        Real size = data.shiftSize;

        RiskFactorKey key(RiskFactorKey::KeyType::RecoveryRate, isin);
        Real base_recoveryRate = baseScenarioAbsolute_->get(key);
        Real new_recoveryRate = relShift ? base_recoveryRate * (1.0 + size) : (base_recoveryRate + size);
        scenario->add(RiskFactorKey(RiskFactorKey::KeyType::RecoveryRate, isin),
                      stressData_->useSpreadedTermStructures() ? new_recoveryRate - base_recoveryRate
                                                               : new_recoveryRate);
    }
    DLOG("Recovery rate scenarios done");
}

} // namespace analytics
} // namespace ore
