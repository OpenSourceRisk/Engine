/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/lexical_cast.hpp>
#include <boost/timer.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/scenarioengine.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/report/csvreport.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>

#include <iomanip>
#include <iostream>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

namespace {

Real impliedQuote(const boost::shared_ptr<Instrument>& i) {
    if (boost::dynamic_pointer_cast<VanillaSwap>(i))
        return boost::dynamic_pointer_cast<VanillaSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<Deposit>(i))
        return boost::dynamic_pointer_cast<Deposit>(i)->fairRate();
    if (boost::dynamic_pointer_cast<ForwardRateAgreement>(i))
        return boost::dynamic_pointer_cast<ForwardRateAgreement>(i)->forwardRate();
    if (boost::dynamic_pointer_cast<OvernightIndexedSwap>(i))
        return boost::dynamic_pointer_cast<OvernightIndexedSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<CrossCcyBasisSwap>(i))
        return boost::dynamic_pointer_cast<CrossCcyBasisSwap>(i)->fairPaySpread();
    if (boost::dynamic_pointer_cast<FxForward>(i))
        return boost::dynamic_pointer_cast<FxForward>(i)->fairForwardRate().rate();
    QL_FAIL("SensitivityAnalysis: impliedQuote: unknown instrument");
}

} // anonymous namespace

ParSensitivityAnalysis::ParSensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                                               const boost::shared_ptr<ore::data::Market>& market,
                                               const string& marketConfiguration,
                                               const boost::shared_ptr<ore::data::EngineData>& engineData,
                                               const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                               const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                               const Conventions& conventions)
    : SensitivityAnalysis(portfolio, market, marketConfiguration, engineData, simMarketData, sensitivityData,
                          conventions) {
    if (sensitivityData_->parConversion()) {
        LOG("Run par sensitivity conversion");
        parDeltaConversion();
        LOG("Par sensitivity done");
    } else {
        LOG("Skip par sensitivity conversion");
    }
}

void ParSensitivityAnalysis::parDeltaConversion() {

    /****************************************************************
     * Discount curve instrument fair rate sensitivity to zero shifts
     * Index curve instrument fair rate sensitivity to zero shifts
     * Cap/Floor flat vol sensitivity to optionlet vol shifts
     *
     * Step 1:
     * - Apply the base scenario
     * - Build instruments and cache fair base rates/vols
     */
    LOG("Cache base scenario par rates and flat vols");

    scenarioGenerator_->reset();
    simMarket_->update(asof_);

    map<RiskFactorKey, boost::shared_ptr<Instrument> > parHelpers;
    map<RiskFactorKey, Real> parRatesBase;

    // Discount curve instruments
    string baseCcy = simMarketData_->baseCcy(); // FIXME
    for (Size i = 0; i < sensitivityData_->discountCurrencies().size(); ++i) {
        string ccy = sensitivityData_->discountCurrencies()[i];
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->discountCurveShiftData()[ccy];
        Size n_ten = data.shiftTenors.size();
        QL_REQUIRE(data.parInstruments.size() == n_ten,
                   "number of tenors does not match number of discount curve par instruments");
        for (Size j = 0; j < n_ten; ++j) {
            RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
            Period term = data.shiftTenors[j];
            string instType = data.parInstruments[j];
            map<string, string> conventionsMap = data.parInstrumentConventions;
            QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions_.get(conventionsMap[instType]);
            string indexName = ""; // if empty, it will be picked from conventions
            if (instType == "IRS")
                parHelpers[key] = makeSwap(ccy, indexName, term, convention, true);
            else if (instType == "DEP")
                parHelpers[key] = makeDeposit(ccy, indexName, term, convention, true);
            else if (instType == "FRA")
                parHelpers[key] = makeFRA(ccy, indexName, term, convention, true);
            else if (instType == "OIS")
                parHelpers[key] = makeOIS(ccy, indexName, term, convention, true);
            else if (instType == "XBS")
                parHelpers[key] = makeCrossCcyBasisSwap(baseCcy, ccy, term, convention);
            else if (instType == "FXF")
                parHelpers[key] = makeFxForward(baseCcy, ccy, term, convention);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
            parRatesBase[key] = impliedQuote(parHelpers[key]);
            LOG("Par instrument for discount curve, ccy " << ccy << " tenor " << j << ", type " << instType
                                                          << ", base rate " << setprecision(4) << parRatesBase[key]);
        }
    }

    // Index curve instruments
    for (Size i = 0; i < sensitivityData_->indexNames().size(); ++i) {
        string indexName = sensitivityData_->indexNames()[i];
        SensitivityScenarioData::CurveShiftData data = sensitivityData_->indexCurveShiftData()[indexName];
        Size n_ten = data.shiftTenors.size();
        QL_REQUIRE(data.parInstruments.size() == n_ten,
                   "number of tenors does not match number of discount curve par instruments");
        vector<string> tokens;
        boost::split(tokens, indexName, boost::is_any_of("-"));
        QL_REQUIRE(tokens.size() >= 2, "index name " << indexName << " unexpected");
        string ccy = tokens[0];
        QL_REQUIRE(ccy.length() == 3, "currency token not recognised");
        for (Size j = 0; j < n_ten; ++j) {
            RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
            Period term = data.shiftTenors[j];
            string instType = data.parInstruments[j];
            map<string, string> conventionsMap = data.parInstrumentConventions;
            QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions_.get(conventionsMap[instType]);
            if (instType == "IRS")
                parHelpers[key] = makeSwap(ccy, indexName, term, convention, false);
            else if (instType == "DEP")
                parHelpers[key] = makeDeposit(ccy, indexName, term, convention, false);
            else if (instType == "FRA")
                parHelpers[key] = makeFRA(ccy, indexName, term, convention, false);
            else if (instType == "OIS")
                parHelpers[key] = makeOIS(ccy, indexName, term, convention, false);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversin not recognised");
            parRatesBase[key] = impliedQuote(parHelpers[key]);
            LOG("Par instrument for index " << indexName << " ccy " << ccy << " tenor " << j << " base rate "
                                            << setprecision(4) << parRatesBase[key]);
        }
    }

    // Caps/Floors
    map<RiskFactorKey, boost::shared_ptr<CapFloor> > parCaps;
    map<RiskFactorKey, Real> parCapVols;
    for (Size i = 0; i < sensitivityData_->capFloorVolCurrencies().size(); ++i) {
        string ccy = sensitivityData_->capFloorVolCurrencies()[i];
        SensitivityScenarioData::CapFloorVolShiftData data = sensitivityData_->capFloorVolShiftData()[ccy];
        string indexName = data.indexName;
        Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
        Handle<OptionletVolatilityStructure> ovs = simMarket_->capFloorVol(ccy, marketConfiguration_);
        Size n_strikes = data.shiftStrikes.size();
        Size n_expiries = data.shiftExpiries.size();
        for (Size j = 0; j < n_strikes; ++j) {
            Real strike = data.shiftStrikes[j];
            pair<string, Size> key(ccy, j);
            for (Size k = 0; k < n_expiries; ++k) {
                RiskFactorKey key(RiskFactorKey::KeyType::OptionletVolatility, ccy, k * n_strikes + j);
                Period term = data.shiftExpiries[k];
                parCaps[key] = makeCapFloor(ccy, indexName, term, strike);
                Real price = parCaps[key]->NPV();
                parCapVols[key] = impliedVolatility(*parCaps[key], price, yts, 0.01, // initial guess
                                                    ovs->volatilityType(), ovs->displacement());
                LOG("Par cap/floor ccy " << ccy << " strike " << j << " expiry " << k << " base vol " << setprecision(4)
                                         << parCapVols[key]);
            }
        }
    }
    LOG("Caching base scenario par rates and flat vols done");

    /****************************************************************
     * Discount curve instrument fair rate sensitivity to zero shifts
     * Index curve instrument fair rate sensitivity to zero shifts
     * Cap/Floor flat vol sensitivity to optionlet vol shifts
     *
     * Step 2:
     * - Apply all single up-shift scenarios,
     * - Compute respective fair par rates and flat vols
     * - Compute par rate / flat vol sensitivities
     */
    LOG("Compute par rate and flat vol sensitivities");

    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator_->scenarioDescriptions();
    QL_REQUIRE(desc.size() == scenarioGenerator_->samples(),
               "descriptions size " << desc.size() << " does not match samples " << scenarioGenerator_->samples());

    std::set<string> parFactors;
    for (Size i = 1; i < scenarioGenerator_->samples(); ++i) {
        string label = scenarioGenerator_->scenarios()[i]->label();

        simMarket_->update(asof_);

        // use single "UP" shift scenarios only
        if (desc[i].type() != ShiftScenarioGenerator::ScenarioDescription::Type::Up)
            continue;

        string factor = desc[i].factor1();

        // par rate sensi to yield shifts
        if (desc[i].key1().keytype == RiskFactorKey::KeyType::DiscountCurve ||
            desc[i].key1().keytype == RiskFactorKey::KeyType::IndexCurve) {

            parFactors.insert(factor);

            // discount curves
            for (Size j = 0; j < simMarketData_->ccys().size(); ++j) {
                string ccy = simMarketData_->ccys()[j];
                // FIXME: Assumption of sensitivity within currency only, relax this?
                if (factor.find(ccy) == string::npos)
                    continue;
                SensitivityScenarioData::CurveShiftData data = sensitivityData_->discountCurveShiftData()[ccy];
                Size n_ten = data.shiftTenors.size();
                for (Size k = 0; k < n_ten; ++k) {
                    RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, k);
                    Real fair = impliedQuote(parHelpers[key]);
                    Real base = parRatesBase[key];
                    std::pair<RiskFactorKey, RiskFactorKey> sensiKey(key, desc[i].key1());
                    parSensi_[sensiKey] = (fair - base) / data.shiftSize;
                }
            }

            // index curves
            for (Size j = 0; j < simMarketData_->indices().size(); ++j) {
                string indexName = simMarketData_->indices()[j];
                string indexCurrency = sensitivityData_->getIndexCurrency(indexName);
                // FIXME: Assumption of sensitivity within currency only
                if (factor.find(indexCurrency) == string::npos)
                    continue;
                SensitivityScenarioData::CurveShiftData data = sensitivityData_->indexCurveShiftData()[indexName];
                Size n_ten = data.shiftTenors.size();
                for (Size k = 0; k < n_ten; ++k) {
                    RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, k);
                    Real fair = impliedQuote(parHelpers[key]);
                    Real base = parRatesBase[key];
                    std::pair<RiskFactorKey, RiskFactorKey> sensiKey(key, desc[i].key1());
                    parSensi_[sensiKey] = (fair - base) / data.shiftSize;
                }
            }
        }

        // flat cap/floor vol sensitivity to yield shifts and optionlet vol shifts
        if (desc[i].key1().keytype == RiskFactorKey::KeyType::DiscountCurve ||
            desc[i].key1().keytype == RiskFactorKey::KeyType::IndexCurve ||
            desc[i].key1().keytype == RiskFactorKey::KeyType::OptionletVolatility) {

            parFactors.insert(factor);

            // caps/floors
            for (Size ii = 0; ii < simMarketData_->capFloorVolCcys().size(); ++ii) {
                string ccy = simMarketData_->capFloorVolCcys()[ii];
                // FIXME: Assumption of sensitivity within currency only
                if (factor.find(ccy) == string::npos)
                    continue;
                Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
                Handle<OptionletVolatilityStructure> ovs = simMarket_->capFloorVol(ccy, marketConfiguration_);
                SensitivityScenarioData::CapFloorVolShiftData data = sensitivityData_->capFloorVolShiftData()[ccy];
                Size n_strikes = data.shiftStrikes.size();
                Size n_expiries = data.shiftExpiries.size();
                for (Size j = 0; j < n_strikes; ++j) {
                    for (Size k = 0; k < n_expiries; ++k) {
                        RiskFactorKey key(RiskFactorKey::KeyType::OptionletVolatility, ccy, k * n_strikes + j);
                        Real price = parCaps[key]->NPV();
                        Real fair = impliedVolatility(*parCaps[key], price, yts, 0.01, // initial guess
                                                      ovs->volatilityType(), ovs->displacement());
                        Real base = parCapVols[key];
                        std::pair<RiskFactorKey, RiskFactorKey> sensiKey(key, desc[i].key1());
                        parSensi_[sensiKey] = (fair - base) / data.shiftSize;
                        if (parSensi_[sensiKey] != 0.0)
                            LOG("CapFloorVol Sensi " << sensiKey.first << " w.r.t. " << sensiKey.second << " "
                                                     << setprecision(6) << parSensi_[sensiKey]);
                    }
                }
            }
        }
    } // end of loop over samples

    LOG("Computing par rate and flat vol sensitivities done");

    // Build Jacobi matrix and convert sensitivities
    ParSensitivityConverter jacobi(sensitivityData_, delta_, parSensi_, parFactors, scenarioGenerator_->keyToFactor());
    parDelta_ = jacobi.parDelta();
}

boost::shared_ptr<Instrument> ParSensitivityAnalysis::makeSwap(string ccy, string indexName, Period term,
                                                               const boost::shared_ptr<Convention>& conventions,
                                                               bool singleCurve) {
    boost::shared_ptr<IRSwapConvention> conv = boost::dynamic_pointer_cast<IRSwapConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected IRSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index =
        singleCurve ? simMarket_->iborIndex(name)->clone(simMarket_->discountCurve(ccy, marketConfiguration_))
                    : *simMarket_->iborIndex(name, marketConfiguration_);
    // LOG("Make Swap for ccy " << ccy << " index " << name);
    boost::shared_ptr<VanillaSwap> helper = MakeVanillaSwap(term, index, 0.0, 0 * Days)
                                                .withSettlementDays(index->fixingDays())
                                                .withFixedLegDayCount(conv->fixedDayCounter())
                                                .withFixedLegTenor(Period(conv->fixedFrequency()))
                                                .withFixedLegConvention(conv->fixedConvention())
                                                .withFixedLegTerminationDateConvention(conv->fixedConvention())
                                                .withFixedLegCalendar(conv->fixedCalendar())
                                                .withFloatingLegCalendar(conv->fixedCalendar());
    boost::shared_ptr<PricingEngine> swapEngine =
        boost::make_shared<DiscountingSwapEngine>(simMarket_->discountCurve(ccy, marketConfiguration_));
    helper->setPricingEngine(swapEngine);
    return helper;
}

boost::shared_ptr<Instrument> ParSensitivityAnalysis::makeDeposit(string ccy, string indexName, Period term,
                                                                  const boost::shared_ptr<Convention>& conventions,
                                                                  bool singleCurve) {
    boost::shared_ptr<DepositConvention> conv = boost::dynamic_pointer_cast<DepositConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected DepositConvention");
    string name = indexName;
    // if empty, use conventions and append term
    if (name == "") {
        ostringstream o;
        o << conv->index() << "-" << term;
        name = o.str();
        boost::to_upper(name);
        LOG("Deposit index name = " << name);
    }
    boost::shared_ptr<IborIndex> index = simMarket_->iborIndex(name, marketConfiguration_).currentLink();
    auto helper = boost::make_shared<Deposit>(1.0, 0.0, term, index->fixingDays(), index->fixingCalendar(),
                                              index->businessDayConvention(), index->endOfMonth(), index->dayCounter(),
                                              asof_, true, 0 * Days);
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> depositEngine = boost::make_shared<DepositEngine>(engineYts);
    helper->setPricingEngine(depositEngine);
    if (singleCurve)
        engineYts.linkTo(*simMarket_->discountCurve(ccy, marketConfiguration_));
    else
        engineYts.linkTo(*index->forwardingTermStructure());
    return helper;
}

boost::shared_ptr<Instrument> ParSensitivityAnalysis::makeFRA(string ccy, string indexName, Period term,
                                                              const boost::shared_ptr<Convention>& conventions,
                                                              bool singleCurve) {
    boost::shared_ptr<FraConvention> conv = boost::dynamic_pointer_cast<FraConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected FraConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index =
        singleCurve ? simMarket_->iborIndex(name)->clone(simMarket_->discountCurve(ccy, marketConfiguration_))
                    : *simMarket_->iborIndex(name, marketConfiguration_);
    QL_REQUIRE(term.units() == Months, "term unit must be Months");
    QL_REQUIRE(index->tenor().units() == Months, "index tenor unit must be Months");
    QL_REQUIRE(term.length() > index->tenor().length(), "term must be larger than index tenor");
    Date valueDate = index->valueDate(asof_);
    Date maturityDate = index->maturityDate(asof_);
    Handle<YieldTermStructure> ytsTmp;
    if (singleCurve)
        ytsTmp = simMarket_->discountCurve(ccy, marketConfiguration_);
    else
        ytsTmp = index->forwardingTermStructure();
    auto helper =
        boost::make_shared<ForwardRateAgreement>(valueDate, maturityDate, Position::Long, 0.0, 1.0, index, ytsTmp);
    return helper;
}

boost::shared_ptr<Instrument> ParSensitivityAnalysis::makeOIS(string ccy, string indexName, Period term,
                                                              const boost::shared_ptr<Convention>& conventions,
                                                              bool singleCurve) {
    boost::shared_ptr<OisConvention> conv = boost::dynamic_pointer_cast<OisConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected OisConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = simMarket_->iborIndex(name, marketConfiguration_).currentLink();
    boost::shared_ptr<OvernightIndex> overnightIndexTmp = boost::dynamic_pointer_cast<OvernightIndex>(index);
    boost::shared_ptr<OvernightIndex> overnightIndex =
        singleCurve ? boost::dynamic_pointer_cast<OvernightIndex>(
                          overnightIndexTmp->clone(simMarket_->discountCurve(ccy, marketConfiguration_)))
                    : overnightIndexTmp;
    boost::shared_ptr<OvernightIndexedSwap> helper = MakeOIS(term, overnightIndex, Null<Rate>(), 0 * Days);
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> swapEngine =
        boost::make_shared<DiscountingSwapEngine>(simMarket_->discountCurve(ccy, marketConfiguration_));
    helper->setPricingEngine(swapEngine);
    return helper;
}

boost::shared_ptr<CapFloor> ParSensitivityAnalysis::makeCapFloor(string ccy, string indexName, Period term,
                                                                 Real strike) {
    // conventions not needed here, index is sufficient
    Date today = Settings::instance().evaluationDate();
    Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
    boost::shared_ptr<IborIndex> index = simMarket_->iborIndex(indexName, marketConfiguration_).currentLink();
    Date start = index->fixingCalendar().adjust(today + index->fixingDays(), Following); // FIXME: Convention
    Date end = start + term;
    Schedule schedule = MakeSchedule().from(start).to(end).withTenor(index->tenor());
    Leg leg = IborLeg(schedule, index).withNotionals(1.0);
    boost::shared_ptr<CapFloor> tmpCapFloor = boost::make_shared<CapFloor>(CapFloor::Cap, leg, vector<Rate>(1, 0.03));
    Real atmRate = tmpCapFloor->atmRate(**yts);
    Real rate = strike == Null<Real>() ? atmRate : strike;
    CapFloor::Type type = strike == Null<Real>() || strike >= atmRate ? CapFloor::Cap : CapFloor::Floor;
    boost::shared_ptr<CapFloor> capFloor = boost::make_shared<CapFloor>(type, leg, vector<Rate>(1, rate));
    Handle<OptionletVolatilityStructure> ovs = simMarket_->capFloorVol(ccy, marketConfiguration_);
    QL_REQUIRE(!ovs.empty(), "caplet volatility structure not found for currency " << ccy);
    switch (ovs->volatilityType()) {
    case ShiftedLognormal:
        capFloor->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(yts, ovs, ovs->displacement()));
        break;
    case Normal:
        capFloor->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(yts, ovs));
        break;
    default:
        QL_FAIL("Caplet volatility type, " << ovs->volatilityType() << ", not covered");
        break;
    }
    return capFloor;
}

boost::shared_ptr<Instrument>
ParSensitivityAnalysis::makeCrossCcyBasisSwap(string baseCcy, string ccy, Period term,
                                              const boost::shared_ptr<Convention>& conventions) {
    boost::shared_ptr<CrossCcyBasisSwapConvention> conv =
        boost::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected CrossCcyBasisSwapConvention");
    QL_REQUIRE(baseCcy == conv->flatIndex()->currency().code() || baseCcy == conv->spreadIndex()->currency().code(),
               "base currency " << baseCcy << " not covered by convention " << conv->id());
    QL_REQUIRE(ccy == conv->flatIndex()->currency().code() || ccy == conv->spreadIndex()->currency().code(),
               "currency " << ccy << " not covered by convention " << conv->id());
    string baseIndexName, indexName;
    if (baseCcy == conv->flatIndex()->currency().code()) {
        baseIndexName = conv->flatIndexName();
        indexName = conv->spreadIndexName();
    } else {
        baseIndexName = conv->spreadIndexName();
        indexName = conv->flatIndexName();
    }
    Currency baseCurrency = parseCurrency(baseCcy);
    Currency currency = parseCurrency(ccy);
    Handle<IborIndex> baseIndex = simMarket_->iborIndex(baseIndexName, marketConfiguration_);
    Handle<IborIndex> index = simMarket_->iborIndex(indexName, marketConfiguration_);
    Handle<YieldTermStructure> baseDiscountCurve = simMarket_->discountCurve(baseCcy, marketConfiguration_);
    Handle<YieldTermStructure> discountCurve = simMarket_->discountCurve(ccy, marketConfiguration_);
    Handle<Quote> fxSpot =
        simMarket_->fxSpot(ccy + baseCcy, marketConfiguration_); // multiplicative conversion into base ccy
    Date today = Settings::instance().evaluationDate();
    Date start = conv->settlementCalendar().adjust(today + conv->settlementDays(), conv->rollConvention());
    Date end = start + term;
    Schedule baseSchedule = MakeSchedule().from(start).to(end).withTenor(baseIndex->tenor());
    Schedule schedule = MakeSchedule().from(start).to(end).withTenor(index->tenor());
    Real baseNotional = 1.0;
    Real notional = 1.0 / fxSpot->value();
    // LOG("Make Cross Ccy Swap for base ccy " << baseCcy << " currency " << ccy);
    // Set up first leg as spread leg, second as flat leg
    boost::shared_ptr<CrossCcyBasisSwap> helper;
    if (baseCcy == conv->spreadIndex()->currency().code())
        helper = boost::make_shared<CrossCcyBasisSwap>(baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0,
                                                       notional, currency, schedule, *index, 0.0);
    else
        helper = boost::make_shared<CrossCcyBasisSwap>(notional, currency, schedule, *index, 0.0, baseNotional,
                                                       baseCurrency, baseSchedule, *baseIndex, 0.0);

    boost::shared_ptr<PricingEngine> swapEngine =
        boost::make_shared<CrossCcySwapEngine>(baseCurrency, baseDiscountCurve, currency, discountCurve, fxSpot);
    helper->setPricingEngine(swapEngine);
    return helper;
}

boost::shared_ptr<Instrument> ParSensitivityAnalysis::makeFxForward(string baseCcy, string ccy, Period term,
                                                                    const boost::shared_ptr<Convention>& conventions) {
    boost::shared_ptr<FXConvention> conv = boost::dynamic_pointer_cast<FXConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected FXConvention");
    QL_REQUIRE(baseCcy == conv->sourceCurrency().code() || baseCcy == conv->targetCurrency().code(),
               "base currency " << baseCcy << " not covered by convention " << conv->id());
    QL_REQUIRE(ccy == conv->targetCurrency().code() || ccy == conv->targetCurrency().code(),
               "currency " << ccy << " not covered by convention " << conv->id());
    Currency baseCurrency = parseCurrency(baseCcy);
    Currency currency = parseCurrency(ccy);
    Handle<YieldTermStructure> baseDiscountCurve = simMarket_->discountCurve(baseCcy, marketConfiguration_);
    Handle<YieldTermStructure> discountCurve = simMarket_->discountCurve(ccy, marketConfiguration_);
    Handle<Quote> fxSpot =
        simMarket_->fxSpot(ccy + baseCcy, marketConfiguration_); // multiplicative conversion into base ccy
    Date today = Settings::instance().evaluationDate();
    Date maturity = today + term; // FIXME: Adjustment
    Real baseNotional = 1.0;
    Real notional = 1.0 / fxSpot->value();
    boost::shared_ptr<FxForward> helper =
        boost::make_shared<FxForward>(baseNotional, baseCurrency, notional, currency, maturity, true);

    boost::shared_ptr<PricingEngine> engine = boost::make_shared<DiscountingFxForwardEngine>(
        baseCurrency, baseDiscountCurve, currency, discountCurve, fxSpot);
    helper->setPricingEngine(engine);
    return helper;
}

void ParSensitivityConverter::buildJacobiMatrix() {
    parKeySet_.clear();
    rawKeySet_.clear();
    for (auto d : parSensi_) {
        RiskFactorKey parKey = d.first.first;
        RiskFactorKey rawKey = d.first.second;
        parKeySet_.insert(parKey);
        rawKeySet_.insert(rawKey);
    }

    Size n_par = parKeySet_.size();
    Size n_raw = rawKeySet_.size();
    jacobi_ = Matrix(n_par, n_raw, 0.0);
    LOG("Jacobi matrix dimension " << n_par << " x " << n_raw);

    Size i = 0;
    for (auto p : parKeySet_) {
        Size j = 0;
        for (auto r : rawKeySet_) {
            std::pair<RiskFactorKey, RiskFactorKey> key(p, r);
            if (parSensi_.find(key) == parSensi_.end())
                jacobi_[i][j] = 0.0;
            else
                jacobi_[i][j] = parSensi_[key];
            // LOG("matrix entry " << i << " " << j << " " << key.first << " " << key.second << " " << jacobi_[i][j]);
            j++;
        }
        i++;
    }

    LOG("Invert Jacobi matrix");

    jacobiInverse_ = inverse(jacobi_);

    LOG("Inverse Jacobi done");
}

void ParSensitivityConverter::convertSensitivity() {
    LOG("Start sensitivity conversion");

    // ensure matching size order of par factors and raw keys
    QL_REQUIRE(parFactors_.size() == rawKeySet_.size(), "factor/key size mismatch: " << parFactors_.size() << " vs "
                                                                                     << rawKeySet_.size());
    // unique set of trade IDs
    set<string> trades;
    for (auto d : delta_) {
        string trade = d.first.first;
        trades.insert(trade);
    }

    for (auto t : trades) {
        Array deltaArray(parFactors_.size(), 0.0);
        Size i = 0;
        for (auto k : rawKeySet_) {
            pair<string, string> p(t, keyToFactor_[k]);
            if (delta_.find(p) != delta_.end())
                deltaArray[i] = delta_[p];
            i++;
            // LOG("delta " << f << " " << deltaArray[i]);
        }
        Array parDeltaArray = transpose(jacobiInverse_) * deltaArray;
        i = 0;
        for (auto k : rawKeySet_) {
            if (parDeltaArray[i] != 0.0) {
                pair<string, string> p(t, keyToFactor_[k]);
                parDelta_[p] = parDeltaArray[i];
                // LOG("par delta " << f << " " << deltaArray[i] << " " << parDeltaArray[i]);
            }
            i++;
        }
    }

    LOG("sensitivity conversion done");
}

ImpliedCapFloorVolHelper::ImpliedCapFloorVolHelper(VolatilityType type, const CapFloor& cap,
                                                   const Handle<YieldTermStructure>& discountCurve, Real targetValue,
                                                   Real displacement)
    : discountCurve_(discountCurve), targetValue_(targetValue) {
    // set an implausible value, so that calculation is forced
    // at first ImpliedCapFloorVolHelper::operator()(Volatility x) call
    vol_ = boost::shared_ptr<SimpleQuote>(new SimpleQuote(-1));
    Handle<Quote> h(vol_);
    if (type == ShiftedLognormal)
        engine_ = boost::shared_ptr<PricingEngine>(
            new BlackCapFloorEngine(discountCurve_, h, Actual365Fixed(), displacement));
    else if (type == Normal)
        engine_ = boost::shared_ptr<PricingEngine>(new BachelierCapFloorEngine(discountCurve_, h, Actual365Fixed()));
    else
        QL_FAIL("volatility type " << type << " not implemented");
    cap.setupArguments(engine_->getArguments());

    results_ = dynamic_cast<const Instrument::results*>(engine_->getResults());
}

Real ImpliedCapFloorVolHelper::operator()(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    return results_->value - targetValue_;
}

Real ImpliedCapFloorVolHelper::derivative(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    std::map<std::string, boost::any>::const_iterator vega_ = results_->additionalResults.find("vega");
    QL_REQUIRE(vega_ != results_->additionalResults.end(), "vega not provided");
    return boost::any_cast<Real>(vega_->second);
}

Volatility impliedVolatility(const CapFloor& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                             Volatility guess, VolatilityType type, Real displacement, Real accuracy,
                             Natural maxEvaluations, Volatility minVol, Volatility maxVol) {
    QL_REQUIRE(!cap.isExpired(), "instrument expired");
    ImpliedCapFloorVolHelper f(type, cap, d, targetValue, displacement);
    NewtonSafe solver;
    solver.setMaxEvaluations(maxEvaluations);
    return solver.solve(f, accuracy, guess, minVol, maxVol);
}
}
}
