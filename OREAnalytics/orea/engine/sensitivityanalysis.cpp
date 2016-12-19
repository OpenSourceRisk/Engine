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
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
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
    QL_FAIL("SensitivityAnalysis: impliedQuote: unknown instrument");
}

} // anonymous namespace

SensitivityAnalysis::SensitivityAnalysis(boost::shared_ptr<ore::data::Portfolio>& portfolio,
                                         boost::shared_ptr<ore::data::Market>& market,
                                         const string& marketConfiguration,
                                         const boost::shared_ptr<ore::data::EngineData>& engineData,
                                         boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                         const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                         const Conventions& conventions) {

    LOG("Build Sensitivity Scenario Generator");
    Date asof = market->asofDate();
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        boost::make_shared<SensitivityScenarioGenerator>(scenarioFactory, sensitivityData, simMarketData, asof, market);
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator);

    LOG("Build Simulation Market");
    boost::shared_ptr<ScenarioSimMarket> simMarket =
        boost::make_shared<ScenarioSimMarket>(sgen, market, simMarketData, conventions);

    LOG("Build Engine Factory");
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(engineData, simMarket, configurations);

    LOG("Reset and Build Portfolio");
    portfolio->reset();
    portfolio->build(factory);

    LOG("Build the cube object to store sensitivities");
    boost::shared_ptr<NPVCube> cube = boost::make_shared<DoublePrecisionInMemoryCube>(
        asof, portfolio->ids(), vector<Date>(1, asof), scenarioGenerator->samples());

    LOG("Build Scenario Engine");
    ScenarioEngine engine(asof, simMarket, simMarketData->baseCcy());

    LOG("Run Sensitivity Scenarios");
    // no progress bar here
    engine.buildCube(portfolio, cube);

    string upString = sensitivityData->labelSeparator() + sensitivityData->shiftDirectionLabel(true);
    string downString = sensitivityData->labelSeparator() + sensitivityData->shiftDirectionLabel(false);

    /***********************************************
     * Compute
     * - base NPVs,
     * - NPVs after single factor up shifts,
     * - NPVs after single factor down shifts
     * - deltas, gammas and cross gammas
     */
    baseNPV_.clear();
    for (Size i = 0; i < portfolio->size(); ++i) {

        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        trades_.insert(id);
        baseNPV_[id] = npv0;

        // single shift scenarios: up, down, delta
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            string label = scenarioGenerator->scenarios()[j]->label();
            if (sensitivityData->isSingleShiftScenario(label)) {
                Real npv = cube->get(i, 0, j, 0);
                string factor = sensitivityData->labelToFactor(label);
                pair<string, string> p(id, factor);
                if (sensitivityData->isUpShiftScenario(label)) {
                    upNPV_[p] = npv;
                    delta_[p] = npv - npv0;
                } else if (sensitivityData->isDownShiftScenario(label)) {
                    downNPV_[p] = npv;
                } else
                    continue;
                factors_.insert(factor);
            }
        }

        // double shift scenarios: cross gamma
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            string label = scenarioGenerator->scenarios()[j]->label();
            // select cross scenarios here
            if (sensitivityData->isCrossShiftScenario(label)) {
                Real npv = cube->get(i, 0, j, 0);
                string f1up = sensitivityData->getCrossShiftScenarioLabel(label, 1);
                string f2up = sensitivityData->getCrossShiftScenarioLabel(label, 2);
                QL_REQUIRE(sensitivityData->isUpShiftScenario(f1up), "scenario " << f1up << " not an up shift");
                QL_REQUIRE(sensitivityData->isUpShiftScenario(f2up), "scenario " << f2up << " not an up shift");
                string f1 = sensitivityData->labelToFactor(f1up);
                string f2 = sensitivityData->labelToFactor(f2up);
                std::pair<string, string> p1(id, f1);
                std::pair<string, string> p2(id, f2);
                // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
                Real base = baseNPV_[id];
                Real up1 = upNPV_[p1];
                Real up2 = upNPV_[p2];
                std::tuple<string, string, string> triple(id, f1, f2);
                crossGamma_[triple] = npv - up1 - up2 + base; // f_xy(x,y) * u * v
            }
        }
    }

    // gamma
    for (auto data : upNPV_) {
        pair<string, string> p = data.first;
        Real u = data.second;
        string id = p.first;
        string factor = p.second;
        QL_REQUIRE(baseNPV_.find(id) != baseNPV_.end(), "base NPV not found for trade " << id);
        Real b = baseNPV_[id];
        QL_REQUIRE(downNPV_.find(p) != downNPV_.end(), "down shift result not found for trade " << id << ", factor "
                                                                                                << factor);
        Real d = downNPV_[p];
        // f_x(x) = (f(x+u) - f(x)) / u
        delta_[p] = u - b; // = f_x(x) * u
        // f_xx(x) = (f(x+u) - 2*f(x) + f(x-u)) / u^2
        gamma_[p] = u - 2.0 * b + d; // = f_xx(x) * u^2
    }

    // The remainder below is about converting sensitivity to sensitivity w.r.t. specified par rates and flat vols
    if (!sensitivityData->parConversion())
        return;

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

    scenarioGenerator->reset();
    simMarket->update(asof);

    map<string, vector<boost::shared_ptr<Instrument>>> parHelpers;
    map<string, vector<Real>> parRatesBase;

    // Discount curve instruments
    Size n_ten = sensitivityData->discountShiftTenors().size();
    QL_REQUIRE(sensitivityData->discountParInstruments().size() == n_ten,
               "number of tenors does not match number of discount curve par instruments");
    for (Size i = 0; i < simMarketData->ccys().size(); ++i) {
        string ccy = simMarketData->ccys()[i];
        parHelpers[ccy] = vector<boost::shared_ptr<Instrument>>(n_ten);
        parRatesBase[ccy] = vector<Real>(n_ten);
        for (Size j = 0; j < n_ten; ++j) {
            Period term = sensitivityData->discountShiftTenors()[j];
            string instType = sensitivityData->discountParInstruments()[j];
            pair<string, string> p(ccy, instType);
            map<pair<string, string>, string> conventionsMap = sensitivityData->discountParInstrumentConventions();
            QL_REQUIRE(conventionsMap.find(p) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions.get(conventionsMap[p]);
            string indexName = ""; // if empty, it will be picked from conventions
            if (instType == "IRS")
                parHelpers[ccy][j] = makeSwap(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "DEP")
                parHelpers[ccy][j] = makeDeposit(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "FRA")
                parHelpers[ccy][j] = makeFRA(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "OIS")
                parHelpers[ccy][j] = makeOIS(ccy, indexName, term, simMarket, convention, true);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversin not recognised");
            parRatesBase[ccy][j] = impliedQuote(parHelpers[ccy][j]);
        }
    }

    // Index curve instruments
    QL_REQUIRE(sensitivityData->indexParInstruments().size() == n_ten,
               "number of tenors does not match number of index curve par instruments");
    for (Size i = 0; i < simMarketData->indices().size(); ++i) {
        string indexName = simMarketData->indices()[i];
        vector<string> tokens;
        boost::split(tokens, indexName, boost::is_any_of("-"));
        QL_REQUIRE(tokens.size() >= 2, "index name " << indexName << " unexpected");
        string ccy = tokens[0];
        QL_REQUIRE(ccy.length() == 3, "currency token not recognised");
        parHelpers[indexName] = vector<boost::shared_ptr<Instrument>>(n_ten);
        parRatesBase[indexName] = vector<Real>(n_ten);
        for (Size j = 0; j < n_ten; ++j) {
            Period term = sensitivityData->indexShiftTenors()[j];
            string instType = sensitivityData->indexParInstruments()[j];
            pair<string, string> p(ccy, instType);
            map<pair<string, string>, string> conventionsMap = sensitivityData->indexParInstrumentConventions();
            QL_REQUIRE(conventionsMap.find(p) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions.get(conventionsMap[p]);
            if (instType == "IRS")
                parHelpers[indexName][j] = makeSwap(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "DEP")
                parHelpers[indexName][j] = makeDeposit(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "FRA")
                parHelpers[indexName][j] = makeFRA(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "OIS")
                parHelpers[indexName][j] = makeOIS(ccy, indexName, term, simMarket, convention, false);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversin not recognised");
            parRatesBase[indexName][j] = impliedQuote(parHelpers[indexName][j]);
            // LOG("Par instrument for index " << indexName << " ccy " << ccy << " tenor " << j << " base rate "
            //                                 << setprecision(4) << parRatesBase[indexName][j]);
        }
    }

    // Caps/Floors
    map<pair<string, Size>, vector<boost::shared_ptr<CapFloor>>> parCaps;
    map<pair<string, Size>, vector<Real>> parCapVols;
    Size n_strikes = sensitivityData->capFloorVolShiftStrikes().size();
    Size n_expiries = sensitivityData->capFloorVolShiftExpiries().size();
    map<string, string> indexMap = sensitivityData->capFloorVolIndexMapping();
    for (Size i = 0; i < simMarketData->capFloorVolCcys().size(); ++i) {
        string ccy = simMarketData->capFloorVolCcys()[i];
        QL_REQUIRE(indexMap.find(ccy) != indexMap.end(), "no cap/floor index found in the index map for ccy " << ccy);
        string indexName = indexMap[ccy];
        Handle<YieldTermStructure> yts = simMarket->discountCurve(ccy);
        Handle<OptionletVolatilityStructure> ovs = simMarket->capFloorVol(ccy); // FIXME: configuration
        for (Size j = 0; j < n_strikes; ++j) {
            Real strike = sensitivityData->capFloorVolShiftStrikes()[j];
            pair<string, Size> key(ccy, j);
            parCaps[key] = vector<boost::shared_ptr<CapFloor>>(n_expiries);
            parCapVols[key] = vector<Real>(n_expiries, 0.0);
            for (Size k = 0; k < n_expiries; ++k) {
                Period term = sensitivityData->capFloorVolShiftExpiries()[k];
                parCaps[key][k] = makeCapFloor(ccy, indexName, term, strike, simMarket);
                Real price = parCaps[key][k]->NPV();
                parCapVols[key][k] = impliedVolatility(*parCaps[key][k], price, yts, 0.01, // initial guess
                                                       ovs->volatilityType(), ovs->displacement());
            }
        }
    }
    LOG("Caching base scenario par rates and flat vols done");

    // vector<string> parConversionFactors;
    // for (auto f : factors_) {
    //     if (f.find("YIELD") != string::npos) // || f.find("CAPFLOOR") != string::npos)
    //         parConversionFactors.push_back(f);
    // }

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

    for (Size i = 1; i < scenarioGenerator->samples(); ++i) {
        string label = scenarioGenerator->scenarios()[i]->label();

        simMarket->update(asof);

        // use single "UP" shift scenarios only
        if (!sensitivityData->isSingleShiftScenario(label) || !sensitivityData->isUpShiftScenario(label))
            continue;

        // par rate sensi to yield shifts
        if (sensitivityData->isYieldShiftScenario(label)) {

            // discount curves
            for (Size j = 0; j < simMarketData->ccys().size(); ++j) {
                string ccy = simMarketData->ccys()[j];
                // FIXME: Assumption of sensitivity within currency only
                if (label.find(ccy) == string::npos)
                    continue;
                pair<string, string> key(ccy, sensitivityData->labelToFactor(label));
                parRatesSensi_[key] = vector<Real>(n_ten);
                for (Size k = 0; k < n_ten; ++k) {
                    Real fair = impliedQuote(parHelpers[ccy][k]);
                    Real base = parRatesBase[ccy][k];
                    parRatesSensi_[key][k] = (fair - base) / sensitivityData->discountShiftSize();
                }
            }

            // index curves
            for (Size j = 0; j < simMarketData->indices().size(); ++j) {
                string indexName = simMarketData->indices()[j];
                string indexCurrency = sensitivityData->getIndexCurrency(indexName);
                // FIXME: Assumption of sensitivity within currency only
                if (label.find(indexCurrency) == string::npos)
                    continue;
                pair<string, string> key(indexName, sensitivityData->labelToFactor(label));
                parRatesSensi_[key] = vector<Real>(n_ten);
                for (Size k = 0; k < n_ten; ++k) {
                    Real fair = impliedQuote(parHelpers[indexName][k]);
                    Real base = parRatesBase[indexName][k];
                    parRatesSensi_[key][k] = (fair - base) / sensitivityData->indexShiftSize();
                }
            }
        }

        // flat cap/floor vol sensitivity to yield shifts and optionlet vol shifts
        if (sensitivityData->isYieldShiftScenario(label) || sensitivityData->isCapFloorVolShiftScenario(label)) {

            string factor = sensitivityData->labelToFactor(label);

            // caps/floors
            for (Size ii = 0; ii < simMarketData->capFloorVolCcys().size(); ++ii) {
                string ccy = simMarketData->capFloorVolCcys()[ii];
                // FIXME: Assumption of sensitivity within currency only
                if (label.find(ccy) == string::npos)
                    continue;
                Handle<YieldTermStructure> yts = simMarket->discountCurve(ccy);         // FIXME: configuration
                Handle<OptionletVolatilityStructure> ovs = simMarket->capFloorVol(ccy); // FIXME: configuration
                for (Size j = 0; j < n_strikes; ++j) {
                    tuple<string, Size, string> sensiKey(ccy, j, factor);
                    pair<string, Size> baseKey(ccy, j);
                    flatCapVolSensi_[sensiKey] = vector<Real>(n_expiries, 0.0);
                    for (Size k = 0; k < n_expiries; ++k) {
                        Real price = parCaps[baseKey][k]->NPV();
                        Real fair = impliedVolatility(*parCaps[baseKey][k], price, yts, 0.01, // initial guess
                                                      ovs->volatilityType(), ovs->displacement());
                        Real base = parCapVols[baseKey][k];
                        flatCapVolSensi_[sensiKey][k] = (fair - base) / sensitivityData->capFloorVolShiftSize();
                        if (flatCapVolSensi_[sensiKey][k] != 0.0)
                            LOG("CapFloorVol Sensi " << std::get<0>(sensiKey) << " " << std::get<2>(sensiKey)
                                                     << " strike " << std::get<1>(sensiKey) << " tenor " << k << " = "
                                                     << setprecision(6) << flatCapVolSensi_[sensiKey][k]);
                    }
                }
            }
        }
    } // end of loop over samples

    LOG("Computing par rate and flat vol sensitivities done");

    // Build Jacobi matrix and convert sensitivities
    ParSensitivityConverter jacobi(sensitivityData, delta_, parRatesSensi_, flatCapVolSensi_);
    parDelta_ = jacobi.parDelta();
}

void SensitivityAnalysis::writeScenarioReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Scenario Label" << sep << "Up/Down" << sep << "Base NPV" << sep << "Scenario NPV"
         << sep << "Sensitivity" << endl;
    LOG("Write scenario output to " << fileName);
    for (auto data : upNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold)
            file << id << sep << factor << sep << "Up" << sep << base << sep << npv << sep << sensi << endl;
    }
    for (auto data : downNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold)
            file << id << sep << factor << sep << "Down" << sep << base << sep << npv << sep << sensi << endl;
    }
    file.close();
}

void SensitivityAnalysis::writeSensitivityReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Factor" << sep << "Base NPV" << sep << "Delta*Shift" << sep << "ParDelta*Shift" << sep
         << "Gamma*Shift^2" << sep << "ParGamma*Shift^2" << endl;
    LOG("Write sensitivity output to " << fileName);
    for (auto data : delta_) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string factor = data.first.second;
        Real delta = data.second;
        Real gamma = gamma_[p];
        Real base = baseNPV_[id];
        if (fabs(delta) > outputThreshold || fabs(gamma) > outputThreshold) {
            if (parDelta_.find(p) != parDelta_.end())
                file << id << sep << factor << sep << base << sep << delta << sep << parDelta_[p] << sep << gamma << sep
                     << "N/A" << endl;
            else
                file << id << sep << factor << sep << base << sep << delta << sep << "N/A" << sep << gamma << sep
                     << "N/A" << endl;
        }
    }
    file.close();
}

void SensitivityAnalysis::writeCrossGammaReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Factor 1" << sep << "Factor 2" << sep << "Base NPV" << sep << "CrossGamma*Shift^2"
         << sep << "ParCrossGamma*Shift^2" << endl;
    LOG("Write cross gamma output to " << fileName);
    for (auto data : crossGamma_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        string factor2 = std::get<2>(data.first);
        Real crossGamma = data.second;
        Real base = baseNPV_[id];
        if (fabs(crossGamma) > outputThreshold)
            file << id << sep << factor1 << sep << factor2 << sep << base << sep << crossGamma << sep << "N/A" << endl;
    }
    file.close();
}

void SensitivityAnalysis::writeParRateSensitivityReport(string fileName) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "ParInstrumentType" << sep << "ParCurveName" << sep << "Factor" << sep << "ParSensitivity" << endl;
    LOG("Write sensitivity output to " << fileName);
    for (auto data : parRatesSensi_) {
        pair<string, string> p = data.first;
        string curveName = data.first.first;
        string factor = data.first.second;
        vector<Real> sensi = data.second;
        file << "YieldCurve" << sep << curveName << sep << factor;
        for (Size i = 0; i < sensi.size(); ++i)
            file << sep << sensi[i];
        file << endl;
    }
    for (auto data : flatCapVolSensi_) {
        std::tuple<string, Size, string> p = data.first;
        string curveName = std::get<0>(p);
        string factor = std::get<2>(p);
        Size bucket = std::get<1>(p);
        vector<Real> sensi = data.second;
        file << "CapFloor" << sep << curveName << "_" << bucket << sep << factor;
        for (Size i = 0; i < sensi.size(); ++i)
            file << sep << sensi[i];
        file << endl;
    }
    file.close();
}

boost::shared_ptr<Instrument> SensitivityAnalysis::makeSwap(string ccy, string indexName, Period term,
                                                            const boost::shared_ptr<ore::data::Market>& market,
                                                            const boost::shared_ptr<Convention>& conventions,
                                                            bool singleCurve) {
    boost::shared_ptr<IRSwapConvention> conv = boost::dynamic_pointer_cast<IRSwapConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected IRSwapConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    // LOG("Make Swap for ccy " << ccy << " index " << name);
    boost::shared_ptr<VanillaSwap> helper = MakeVanillaSwap(term, index, 0.0, 0 * Days)
                                                .withSettlementDays(index->fixingDays())
                                                .withFixedLegDayCount(conv->fixedDayCounter())
                                                .withFixedLegTenor(Period(conv->fixedFrequency()))
                                                .withFixedLegConvention(conv->fixedConvention())
                                                .withFixedLegTerminationDateConvention(conv->fixedConvention())
                                                .withFixedLegCalendar(conv->fixedCalendar())
                                                .withFloatingLegCalendar(conv->fixedCalendar());
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(engineYts);
    helper->setPricingEngine(swapEngine);
    if (singleCurve)
        engineYts.linkTo(*yts);
    else
        engineYts.linkTo(*index->forwardingTermStructure());
    return helper;
}

boost::shared_ptr<Instrument> SensitivityAnalysis::makeDeposit(string ccy, string indexName, Period term,
                                                               const boost::shared_ptr<ore::data::Market>& market,
                                                               const boost::shared_ptr<Convention>& conventions,
                                                               bool singleCurve) {
    boost::shared_ptr<DepositConvention> conv = boost::dynamic_pointer_cast<DepositConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected DepositConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName;
    // if empty, use conventions and append term
    if (name == "") {
        ostringstream o;
        o << conv->index() << "-" << term;
        name = o.str();
        boost::to_upper(name);
        LOG("Deposit index name = " << name);
    }
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    auto helper = boost::make_shared<Deposit>(1.0, 0.0, term, index->fixingDays(), index->fixingCalendar(),
                                              index->businessDayConvention(), index->endOfMonth(), index->dayCounter(),
                                              market->asofDate(), true, 0 * Days);
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> depositEngine = boost::make_shared<DepositEngine>(engineYts);
    helper->setPricingEngine(depositEngine);
    if (singleCurve)
        engineYts.linkTo(*yts);
    else
        engineYts.linkTo(*index->forwardingTermStructure());
    return helper;
}

boost::shared_ptr<Instrument> SensitivityAnalysis::makeFRA(string ccy, string indexName, Period term,
                                                           const boost::shared_ptr<ore::data::Market>& market,
                                                           const boost::shared_ptr<Convention>& conventions,
                                                           bool singleCurve) {
    boost::shared_ptr<FraConvention> conv = boost::dynamic_pointer_cast<FraConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected FraConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    QL_REQUIRE(term.units() == Months, "term unit must be Months");
    QL_REQUIRE(index->tenor().units() == Months, "index tenor unit must be Months");
    QL_REQUIRE(term.length() > index->tenor().length(), "term must be larger than index tenor");
    Date asof = market->asofDate();
    Date valueDate = index->valueDate(asof);
    Date maturityDate = index->maturityDate(asof);
    Handle<YieldTermStructure> ytsTmp;
    if (singleCurve)
        ytsTmp = yts;
    else
        ytsTmp = index->forwardingTermStructure();
    auto helper =
        boost::make_shared<ForwardRateAgreement>(valueDate, maturityDate, Position::Long, 0.0, 1.0, index, ytsTmp);
    return helper;
}

boost::shared_ptr<Instrument> SensitivityAnalysis::makeOIS(string ccy, string indexName, Period term,
                                                           const boost::shared_ptr<ore::data::Market>& market,
                                                           const boost::shared_ptr<Convention>& conventions,
                                                           bool singleCurve) {
    boost::shared_ptr<OisConvention> conv = boost::dynamic_pointer_cast<OisConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected OisConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    boost::shared_ptr<OvernightIndex> overnightIndex = boost::dynamic_pointer_cast<OvernightIndex>(index);
    boost::shared_ptr<OvernightIndexedSwap> helper = MakeOIS(term, overnightIndex, Null<Rate>(), 0 * Days);
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(engineYts);
    helper->setPricingEngine(swapEngine);
    if (singleCurve) {
        engineYts.linkTo(*yts);
    } else {
        engineYts.linkTo(*index->forwardingTermStructure());
    }
    return helper;
}

boost::shared_ptr<CapFloor> SensitivityAnalysis::makeCapFloor(string ccy, string indexName, Period term, Real strike,
                                                              const boost::shared_ptr<ore::data::Market>& market) {
    // conventions not needed here, index is sufficient
    Date today = Settings::instance().evaluationDate();
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    boost::shared_ptr<IborIndex> index = market->iborIndex(indexName).currentLink();
    Date start = index->fixingCalendar().adjust(today + index->fixingDays(), Following); // FIXME
    Date end = start + term;
    Schedule schedule = MakeSchedule().from(start).to(end).withTenor(index->tenor());
    Leg leg = IborLeg(schedule, index).withNotionals(1.0);
    boost::shared_ptr<CapFloor> tmpCapFloor = boost::make_shared<CapFloor>(CapFloor::Cap, leg, vector<Rate>(1, 0.03));
    Real atmRate = tmpCapFloor->atmRate(**yts);
    Real rate = strike == Null<Real>() ? atmRate : strike;
    CapFloor::Type type = strike == Null<Real>() || strike >= atmRate ? CapFloor::Cap : CapFloor::Floor;
    boost::shared_ptr<CapFloor> capFloor = boost::make_shared<CapFloor>(type, leg, vector<Rate>(1, rate));
    Handle<OptionletVolatilityStructure> ovs = market->capFloorVol(ccy); // FIXME: configuration
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

void ParSensitivityConverter::buildJacobiMatrix() {
    vector<string> currencies = sensitivityData_->discountCurrencies();
    Size n_discountTenors = sensitivityData_->discountShiftTenors().size();
    vector<string> indices = sensitivityData_->indexNames();
    Size n_indexTenors = sensitivityData_->indexShiftTenors().size();
    vector<string> capCurrencies = sensitivityData_->capFloorVolCurrencies();
    Size n_capTerms = sensitivityData_->capFloorVolShiftExpiries().size();
    Size n_capStrikes = sensitivityData_->capFloorVolShiftStrikes().size();

    // unique set of factors relevant for conversion
    std::set<std::string> factorSet;
    for (auto d : delta_) {
        string factor = d.first.second;
        if (sensitivityData_->isYieldShiftScenario(factor) || sensitivityData_->isCapFloorVolShiftScenario(factor))
            // if (sensitivityData_->isYieldShiftScenario(factor))
            factorSet.insert(factor);
    }

    // Jacobi matrix dimension and allocation
    Size n_shifts = factorSet.size();
    Size n_par = currencies.size() * n_discountTenors + indices.size() * n_indexTenors +
                 capCurrencies.size() * n_capStrikes * n_capTerms;
    // Size n_par = currencies.size() * n_discountTenors + indices.size() * n_indexTenors;
    jacobi_ = Matrix(n_par, n_shifts, 0.0);
    LOG("Jacobi matrix dimension " << n_par << " x " << n_shifts);

    // Derive unique curves (by type, curveName, bucketIndex).
    // Curve identifier is
    // - curveName=ccy for discount curves,
    // - curveName=indexName for index curves
    // - curveName=ccy + strikeBucketIndex for cap vol curves
    std::vector<std::string> curves, types, buckets;
    for (auto f : factorSet) {
        factors_.push_back(f);
        vector<string> tokens;
        boost::split(tokens, f, boost::is_any_of(sensitivityData_->labelSeparator()));
        QL_REQUIRE(tokens.size() >= 3, "more than three tokens expected in " << f);
        string type = tokens[0];
        string curve = tokens[1];
        string bucket = tokens[2]; // third token is always a bucket index, can be converted to Size
        LOG("Conversion factor " << f << " type " << type << " curve " << curve << " bucket " << bucket);
        if (sensitivityData_->isYieldShiftScenario(f)) {
            if (curves.size() == 0 || curve != curves.back() || type != types.back()) {
                types.push_back(type);
                curves.push_back(curve);
                buckets.push_back(bucket);
            }
        } else if (sensitivityData_->isCapFloorVolShiftScenario(f)) {
            if (curves.size() == 0 || curve != curves.back() || type != types.back() || bucket != buckets.back()) {
                types.push_back(type);
                curves.push_back(curve);
                buckets.push_back(bucket);
            }
        }
    }

    LOG("Build Jacobi matrix");
    Size offset = 0;
    for (Size i = 0; i < curves.size(); ++i) {
        string curveName = curves[i];
        string curveType = types[i];
        string bucket = buckets[i];
        Size dim;
        if (curveType == sensitivityData_->discountLabel())
            dim = n_discountTenors;
        else if (curveType == sensitivityData_->indexLabel())
            dim = n_indexTenors;
        else if (curveType == sensitivityData_->capFloorVolLabel())
            dim = n_capTerms;
        else
            QL_FAIL("curve type " << curveType << " not covered");
        LOG("Curve " << i << " type " << curveType << " name " << curveName << " bucket " << bucket << ": dimension "
                     << dim);

        for (Size k = 0; k < n_shifts; ++k) {
            string factor = factors_[k];
            vector<Real> v;
            if (curveType == sensitivityData_->discountLabel() || curveType == sensitivityData_->indexLabel()) {
                pair<string, string> key(curveName, factor);
                if (parRateSensi_.find(key) == parRateSensi_.end())
                    v = vector<Real>(dim, 0.0);
                else
                    v = parRateSensi_[key];
            } else if (curveType == sensitivityData_->capFloorVolLabel()) {
                Size sBucket = boost::lexical_cast<Size>(bucket);
                std::tuple<string, Size, string> key(curveName, sBucket, factor);
                if (flatCapVolSensi_.find(key) == flatCapVolSensi_.end())
                    v = vector<Real>(dim, 0.0);
                else
                    v = flatCapVolSensi_[key];
                // cout << i << " " << curveType << " " << curveName << " " << sBucket << " " << factor << ": ";
                // for (Size l = 0; l < dim; ++l)
                //   cout << v[l] << " ";
                // cout << endl;
            } else
                QL_FAIL("factor " << factor << " not covered");

            for (Size j = 0; j < dim; ++j) {
                jacobi_[offset + j][k] = v[j];
            }
        }
        offset += dim;
    }

    LOG("Jacobi matrix dimension " << jacobi_.rows() << " x " << jacobi_.columns());

    jacobiInverse_ = inverse(jacobi_);

    LOG("Inverse Jacobi done");
}

void ParSensitivityConverter::convertSensitivity() {
    set<string> trades;
    for (auto d : delta_) {
        string trade = d.first.first;
        trades.insert(trade);
    }
    for (auto t : trades) {
        Array deltaArray(factors_.size(), 0.0);
        for (Size i = 0; i < factors_.size(); ++i) {
            pair<string, string> p(t, factors_[i]);
            if (delta_.find(p) != delta_.end())
                deltaArray[i] = delta_[p];
            // LOG("delta " << yieldFactors_[i] << " " << deltaArray[i]);
        }
        Array parDeltaArray = transpose(jacobiInverse_) * deltaArray;
        for (Size i = 0; i < factors_.size(); ++i) {
            if (parDeltaArray[i] != 0.0) {
                pair<string, string> p(t, factors_[i]);
                parDelta_[p] = parDeltaArray[i];
                // LOG("par delta " << factors_[i] << " " << deltaArray[i] << " " << parDeltaArray[i]);
            }
        }
    }
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
