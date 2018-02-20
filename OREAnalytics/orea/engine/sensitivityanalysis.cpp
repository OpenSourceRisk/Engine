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

#include <boost/timer.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

SensitivityAnalysis::SensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                                         const boost::shared_ptr<ore::data::Market>& market,
                                         const string& marketConfiguration,
                                         const boost::shared_ptr<ore::data::EngineData>& engineData,
                                         const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                         const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                         const Conventions& conventions, const bool recalibrateModels,
                                         const bool nonShiftedBaseCurrencyConversion)
    : market_(market), marketConfiguration_(marketConfiguration), asof_(market->asofDate()),
      simMarketData_(simMarketData), sensitivityData_(sensitivityData), conventions_(conventions),
      recalibrateModels_(recalibrateModels), overrideTenors_(false),
      nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion), engineData_(engineData),
      portfolio_(portfolio), initialized_(false), computed_(false) {}

std::vector<boost::shared_ptr<ValuationCalculator>> SensitivityAnalysis::buildValuationCalculators() const {
    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    if (nonShiftedBaseCurrencyConversion_) // use "original" FX rates to convert sensi to base currency
        calculators.push_back(boost::make_shared<NPVCalculatorFXT0>(simMarketData_->baseCcy(), market_));
    else // use the scenario FX rate when converting sensi to base currency
        calculators.push_back(boost::make_shared<NPVCalculator>(simMarketData_->baseCcy()));
    return calculators;
}

void SensitivityAnalysis::initialize(boost::shared_ptr<NPVCube>& cube) {
    LOG("Build Sensitivity Scenario Generator and Simulation Market");
    initializeSimMarket();

    LOG("Build Engine Factory and rebuild portfolio");
    boost::shared_ptr<EngineFactory> factory = buildFactory();
    resetPortfolio(factory);
    if (recalibrateModels_)
        modelBuilders_ = factory->modelBuilders();
    else
        modelBuilders_.clear();

    if (!cube) {
        LOG("Build the cube object to store sensitivities");
        initializeCube(cube);
    }

    boost::shared_ptr<vector<ShiftScenarioGenerator::ScenarioDescription>> scenDesc =
        boost::make_shared<vector<ShiftScenarioGenerator::ScenarioDescription>>(
            scenarioGenerator_->scenarioDescriptions());
    sensiCube_ = boost::make_shared<SensitivityCube>(cube, scenDesc, portfolio_);
    initialized_ = true;
}

void SensitivityAnalysis::generateSensitivities() {

    QL_REQUIRE(!initialized_, "unexpected state of SensitivitiesAnalysis object");

    // initialize the helper member objects
    boost::shared_ptr<NPVCube> cube;
    initialize(cube);
    QL_REQUIRE(initialized_, "SensitivitiesAnalysis member objects not correctly initialized");
    boost::shared_ptr<DateGrid> dg = boost::make_shared<DateGrid>("1,0W");
    vector<boost::shared_ptr<ValuationCalculator>> calculators = buildValuationCalculators();
    ValuationEngine engine(asof_, dg, simMarket_, modelBuilders_);
    for (auto const& i : this->progressIndicators())
        engine.registerProgressIndicator(i);
    LOG("Run Sensitivity Scenarios");
    engine.buildCube(portfolio_, cube, calculators);

    computed_ = true;
    LOG("Sensitivity analysis completed");
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();

    for (auto f : sensiCube_->upFactors()) {
        Size idx = f.second;
        RiskFactorKey key = (*scenDesc)[idx].key1();
        factors_[key] = getShiftSize(key);
    }
}

void SensitivityAnalysis::initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact) {
    LOG("Initialise sim market for sensitivity analysis");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(market_, simMarketData_, conventions_, marketConfiguration_);
    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(sensitivityData_, simMarket_->baseScenario(),
                                                                          simMarketData_, overrideTenors_);
    simMarket_->scenarioGenerator() = scenarioGenerator_;
    boost::shared_ptr<Scenario> baseScen = scenarioGenerator_->baseScenario();
    boost::shared_ptr<ScenarioFactory> scenFactory =
        (scenFact != NULL) ? scenFact
                           : boost::make_shared<CloneScenarioFactory>(
                                 baseScen); // needed so that sensi scenarios are consistent with base scenario
    LOG("Generating sensitivity scenarios");
    scenarioGenerator_->generateScenarios(scenFactory);
}

boost::shared_ptr<EngineFactory>
SensitivityAnalysis::buildFactory(const std::vector<boost::shared_ptr<EngineBuilder>> extraBuilders) const {
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration_;
    boost::shared_ptr<EngineFactory> factory =
        boost::make_shared<EngineFactory>(engineData_, simMarket_, configurations);
    if (extraBuilders.size() > 0) {
        for (auto eb : extraBuilders)
            factory->registerBuilder(eb);
    }
    return factory;
}

void SensitivityAnalysis::resetPortfolio(const boost::shared_ptr<EngineFactory>& factory) {
    portfolio_->reset();
    portfolio_->build(factory);
}

void SensitivityAnalysis::initializeCube(boost::shared_ptr<NPVCube>& cube) const {
    cube = boost::make_shared<DoublePrecisionInMemoryCube>(asof_, portfolio_->ids(), vector<Date>(1, asof_),
                                                           scenarioGenerator_->samples());
}

void SensitivityAnalysis::writeScenarioReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {

    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    LOG("writing Scenario report");

    report->addColumn("TradeId", string());
    report->addColumn("Factor", string());
    report->addColumn("Up/Down", string());
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("Scenario NPV", double(), 2);
    report->addColumn("Difference", double(), 2);

    Size ps = portfolio_->size();
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();

    for (Size i = 0; i < ps; ++i) {
        string id = portfolio_->trades()[i]->id();
        Real base = sensiCube_->baseNPV(i);

        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {

            Real npv = sensiCube_->getNPV(i, j);
            Real sensi = npv - base;

            if (fabs(sensi) > outputThreshold) {

                ShiftScenarioGenerator::ScenarioDescription desc = (*scenDesc)[j];
                string type = desc.typeString();
                ostringstream o;
                o << desc.factor1();
                if (desc.factor2() != "")
                    o << ":" << desc.factor2();
                string factor = o.str();

                report->next();
                report->add(id);
                report->add(factor);
                report->add(type);
                report->add(base);
                report->add(npv);
                report->add(sensi);
            }
        }
    }

    report->end();
    LOG("Scenario report finished");
}

void SensitivityAnalysis::writeSensitivityReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {

    LOG("writing Sensitivity report");
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    report->addColumn("TradeId", string());
    report->addColumn("Factor", string());
    report->addColumn("ShiftSize", double(), 6);
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("Delta", double(), 2);
    report->addColumn("Gamma", double(), 2);

    Size ps = portfolio_->size();
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();

    for (Size i = 0; i < ps; ++i) {
        string id = portfolio_->trades()[i]->id();
        Real base = baseNPV(id);

        for (auto f : sensiCube_->upFactors()) {
            string factor = f.first;

            Real d = delta(id, factor);
            Real g = gamma(id, factor);

            if (fabs(d) > outputThreshold || fabs(g) > outputThreshold) {
                Size idx = f.second;
                RiskFactorKey key = (*scenDesc)[idx].key1();
                Real shiftSize = factors_.find(key)->second; // getShiftSize(key);;

                report->next();
                report->add(id);
                report->add(factor);
                report->add(shiftSize);
                report->add(base);
                report->add(d);
                report->add(g);
            }
        }
    }
    report->end();
    LOG("Sensitivity report finished");
}

void SensitivityAnalysis::writeCrossGammaReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {
    LOG("writing CrossGamma report");
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    report->addColumn("TradeId", string());
    report->addColumn("Factor 1", string());
    report->addColumn("ShiftSize1", double(), 6);
    report->addColumn("Factor 2", string());
    report->addColumn("ShiftSize2", double(), 6);
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("CrossGamma", double(), 2);

    Size ps = portfolio_->size();
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();

    for (Size i = 0; i < ps; ++i) {

        Real npv0 = sensiCube_->baseNPV(i);
        string id = portfolio_->trades()[i]->id();

        for (auto f : sensiCube_->crossFactors()) {
            string factor1 = f.first.first;
            string factor2 = f.first.second;

            Real cg = crossGamma(id, factor1, factor2);

            if (fabs(cg) > outputThreshold) {
                Size idx = f.second;
                RiskFactorKey key1 = (*scenDesc)[idx].key1();
                RiskFactorKey key2 = (*scenDesc)[idx].key2();
                Real shiftSize1 = factors_.find(key1)->second; // getShiftSize(key1);
                Real shiftSize2 = factors_.find(key2)->second; // getShiftSize(key2);

                report->next();
                report->add(id);
                report->add(factor1);
                report->add(shiftSize1);
                report->add(factor2);
                report->add(shiftSize2);
                report->add(npv0);
                report->add(cg);
            }
        }
    }
    report->end();
    LOG("crossgamma written");
}

Real SensitivityAnalysis::getShiftSize(const RiskFactorKey& key) const {
    RiskFactorKey::KeyType keytype = key.keytype;
    string keylabel = key.name;
    Real shiftSize = 0.0;
    Real shiftMult = 1.0;
    switch (keytype) {
    case RiskFactorKey::KeyType::FXSpot: {
        auto itr = sensitivityData_->fxShiftData().find(keylabel);
        QL_REQUIRE(itr != sensitivityData_->fxShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            shiftMult = simMarket_->fxSpot(keylabel, marketConfiguration_)->value();
        }
    } break;
    case RiskFactorKey::KeyType::EquitySpot: {
        auto itr = sensitivityData_->equityShiftData().find(keylabel);
        QL_REQUIRE(itr != sensitivityData_->equityShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            shiftMult = simMarket_->equitySpot(keylabel, marketConfiguration_)->value();
        }
    } break;
    case RiskFactorKey::KeyType::DiscountCurve: {
        string ccy = keylabel;
        auto itr = sensitivityData_->discountCurveShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->discountCurveShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::IndexCurve: {
        string idx = keylabel;
        auto itr = sensitivityData_->indexCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensitivityData_->indexCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts =
                simMarket_->iborIndex(idx, marketConfiguration_)->forwardingTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::YieldCurve: {
        string yc = keylabel;
        auto itr = sensitivityData_->yieldCurveShiftData().find(yc);
        QL_REQUIRE(itr != sensitivityData_->yieldCurveShiftData().end(), "shiftData not found for " << yc);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->yieldCurve(yc, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::EquityForecastCurve: {
        string ec = keylabel;
        auto itr = sensitivityData_->equityForecastCurveShiftData().find(ec);
        QL_REQUIRE(itr != sensitivityData_->equityForecastCurveShiftData().end(), "shiftData not found for " << ec);
        shiftSize = itr->second->shiftSize;

        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->equityForecastCurve(ec, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::DividendYield: {
        string eq = keylabel;
        auto itr = sensitivityData_->dividendYieldShiftData().find(eq);
        QL_REQUIRE(itr != sensitivityData_->dividendYieldShiftData().end(), "shiftData not found for " << eq);
        shiftSize = itr->second->shiftSize;

        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> ts = simMarket_->equityDividendCurve(eq, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = ts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::FXVolatility: {
        string pair = keylabel;
        auto itr = sensitivityData_->fxVolShiftData().find(pair);
        QL_REQUIRE(itr != sensitivityData_->fxVolShiftData().end(), "shiftData not found for " << pair);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = sensitivityData_->fxVolShiftData()[pair].shiftStrikes;
            QL_REQUIRE(strikes.size() == 1 && close_enough(strikes[0], 0), "Only ATM FX vols supported");
            Real atmFwd = 0.0; // hardcoded, since only ATM supported
            Size keyIdx = key.index;
            Period p = itr->second.shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->fxVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, atmFwd);
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::EquityVolatility: {
        string pair = keylabel;
        auto itr = sensitivityData_->equityVolShiftData().find(pair);
        QL_REQUIRE(itr != sensitivityData_->equityVolShiftData().end(), "shiftData not found for " << pair);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second.shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->equityVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, Null<Real>());
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::SwaptionVolatility: {
        string ccy = keylabel;
        auto itr = sensitivityData_->swaptionVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->swaptionVolShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second.shiftStrikes;
            vector<Period> tenors = itr->second.shiftTerms;
            vector<Period> expiries = itr->second.shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx / (tenors.size() * strikes.size());
            Period p_exp = expiries[expIdx];
            Size tenIdx = keyIdx % tenors.size();
            Period p_ten = tenors[tenIdx];
            Real strike = Null<Real>(); // for cubes this will be ATM
            Handle<SwaptionVolatilityStructure> vts = simMarket_->swaptionVol(ccy, marketConfiguration_);
            // Time t_exp = vts->dayCounter().yearFraction(asof_, asof_ + p_exp);
            // Time t_ten = vts->dayCounter().yearFraction(asof_, asof_ + p_ten);
            // Real atmVol = vts->volatility(t_exp, t_ten, Null<Real>());
            Real vol = vts->volatility(p_exp, p_ten, strike);
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::OptionletVolatility: {
        string ccy = keylabel;
        auto itr = sensitivityData_->capFloorVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensitivityData_->capFloorVolShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second.shiftStrikes;
            vector<Period> expiries = itr->second.shiftExpiries;
            QL_REQUIRE(strikes.size() > 0, "Only strike capfloor vols supported");
            Size keyIdx = key.index;
            Size expIdx = keyIdx / strikes.size();
            Period p_exp = expiries[expIdx];
            Size strIdx = keyIdx % strikes.size();
            Real strike = strikes[strIdx];
            Handle<OptionletVolatilityStructure> vts = simMarket_->capFloorVol(ccy, marketConfiguration_);
            Time t_exp = vts->dayCounter().yearFraction(asof_, asof_ + p_exp);
            Real vol = vts->volatility(t_exp, strike);
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::CDSVolatility: {
        string name = keylabel;
        auto itr = sensitivityData_->cdsVolShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->cdsVolShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Period> expiries = itr->second.shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx;
            Period p_exp = expiries[expIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->cdsVol(name, marketConfiguration_);
            Time t_exp = vts->dayCounter().yearFraction(asof_, asof_ + p_exp);
            Real strike = 0.0; // FIXME
            Real atmVol = vts->blackVol(t_exp, strike);
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::SurvivalProbability: {
        string name = keylabel;
        auto itr = sensitivityData_->creditCurveShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->creditCurveShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<DefaultProbabilityTermStructure> ts = simMarket_->defaultCurve(name, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real prob = ts->survivalProbability(t);
            shiftMult = -std::log(prob) / t;
        }
    } break;
    case RiskFactorKey::KeyType::BaseCorrelation: {
        string name = keylabel;
        auto itr = sensitivityData_->baseCorrelationShiftData().find(name);
        QL_REQUIRE(itr != sensitivityData_->baseCorrelationShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> lossLevels = itr->second.shiftLossLevels;
            vector<Period> terms = itr->second.shiftTerms;
            Size keyIdx = key.index;
            Size lossLevelIdx = keyIdx / terms.size();
            Real lossLevel = lossLevels[lossLevelIdx];
            Size termIdx = keyIdx % terms.size();
            Period term = terms[termIdx];
            Handle<BilinearBaseCorrelationTermStructure> ts = simMarket_->baseCorrelation(name, marketConfiguration_);
            Real bc = ts->correlation(asof_ + term, lossLevel, true); // extrapolate
            shiftMult = bc;
        }
    } break;
    case RiskFactorKey::KeyType::ZeroInflationCurve: {
        string idx = keylabel;
        auto itr = sensitivityData_->zeroInflationCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensitivityData_->zeroInflationCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<ZeroInflationTermStructure> yts =
                simMarket_->zeroInflationIndex(idx, marketConfiguration_)->zeroInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::YoYInflationCurve: {
        string idx = keylabel;
        auto itr = sensitivityData_->yoyInflationCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensitivityData_->yoyInflationCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = sensitivityData_->yoyInflationCurveShiftData()[idx]->shiftTenors[keyIdx];
            Handle<YoYInflationTermStructure> yts =
                simMarket_->yoyInflationIndex(idx, marketConfiguration_)->yoyInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real yoyRate = yts->yoyRate(t);
            shiftMult = yoyRate;
        }
    } break;
    case RiskFactorKey::KeyType::CommodityCurve: {
        auto it = sensitivityData_->commodityCurveShiftData().find(keylabel);
        QL_REQUIRE(it != sensitivityData_->commodityCurveShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = it->second->shiftSize;
        if (parseShiftType(it->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Period p = it->second->shiftTenors[key.index];
            Handle<PriceTermStructure> priceCurve = simMarket_->commodityPriceCurve(keylabel, marketConfiguration_);
            Time t = priceCurve->dayCounter().yearFraction(asof_, asof_ + p);
            shiftMult = priceCurve->price(t);
        }
    } break;
    default:
        // KeyType::CPIIndex does not get shifted
        QL_FAIL("KeyType not supported yet - " << keytype);
    }
    Real realShift = shiftSize * shiftMult;
    return realShift;
}

Real SensitivityAnalysis::baseNPV(std::string& id) const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return sensiCube_->baseNPV(id);
}

Real SensitivityAnalysis::delta(const std::string& trade, const std::string& factor) const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    Size i = sensiCube_->getTradeIndex(trade);
    Real npv = sensiCube_->upNPV(i, factor);
    Real npv0 = sensiCube_->baseNPV(i);
    return npv - npv0;
}

Real SensitivityAnalysis::gamma(const std::string& trade, const std::string& factor) const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    Size i = sensiCube_->getTradeIndex(trade);
    Real u = sensiCube_->upNPV(i, factor);
    Real d = sensiCube_->downNPV(i, factor);
    Real npv0 = sensiCube_->baseNPV(i);
    return u - 2.0 * npv0 + d;
}

Real SensitivityAnalysis::crossGamma(const std::string& trade, const std::string& factor1,
                                     const std::string& factor2) const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
    Size i = sensiCube_->getTradeIndex(trade);
    Real npv = sensiCube_->crossNPV(i, factor1, factor2);
    Real npv0 = sensiCube_->baseNPV(i);
    Real up1 = sensiCube_->upNPV(i, factor1);
    Real up2 = sensiCube_->upNPV(i, factor2);
    return npv - up1 - up2 + npv0; // f_xy(x,y) * u * v
}
} // namespace analytics
} // namespace ore
