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
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <ored/utilities/log.hpp>
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
      recalibrateModels_(recalibrateModels), overrideTenors_(false), nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion),
      engineData_(engineData), portfolio_(portfolio), initialized_(false), computed_(false) {}

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

    collectResultsFromCube(cube);
    computed_ = true;
    LOG("Sensitivity analysis completed");
}

void SensitivityAnalysis::initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact) {
    LOG("Initialise sim market for sensitivity analysis");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(market_, simMarketData_, conventions_, marketConfiguration_);
    scenarioGenerator_ =
        boost::make_shared<SensitivityScenarioGenerator>(sensitivityData_, simMarket_, simMarketData_, overrideTenors_);
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

void SensitivityAnalysis::collectResultsFromCube(const boost::shared_ptr<NPVCube>& cube) {

    /***********************************************
     * Collect results
     * - base NPVs,
     * - NPVs after single factor up shifts,
     * - NPVs after single factor down shifts
     * - deltas, gammas and cross gammas
     */
    baseNPV_.clear();
    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator_->scenarioDescriptions();
    QL_REQUIRE(desc.size() == scenarioGenerator_->samples(),
               "descriptions size " << desc.size() << " does not match samples " << scenarioGenerator_->samples());
    for (Size i = 0; i < portfolio_->size(); ++i) {

        Real npv0 = cube->getT0(i, 0);
        string id = portfolio_->trades()[i]->id();
        trades_.insert(id);
        baseNPV_[id] = npv0;

        // single shift scenarios: up, down, delta
        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {
            // LOG("scenario description " << j << ": " << desc[j].text());
            if (desc[j].type() == ShiftScenarioGenerator::ScenarioDescription::Type::Up ||
                desc[j].type() == ShiftScenarioGenerator::ScenarioDescription::Type::Down) {
                Real npv = cube->get(i, 0, j, 0);
                string factor = desc[j].factor1();
                pair<string, string> p(id, factor);
                if (desc[j].type() == ShiftScenarioGenerator::ScenarioDescription::Type::Up) {
                    LOG("up npv stored for id " << id << ", factor " << factor << ", npv " << npv);
                    upNPV_[p] = npv;
                    delta_[p] = npv - npv0;
                } else if (desc[j].type() == ShiftScenarioGenerator::ScenarioDescription::Type::Down) {
                    downNPV_[p] = npv;
                } else
                    continue;
                storeFactorShifts(desc[j]);
            }
        }

        // double shift scenarios: cross gamma
        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {
            // select cross scenarios here
            if (desc[j].type() == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
                Real npv = cube->get(i, 0, j, 0);
                string f1 = desc[j].factor1();
                string f2 = desc[j].factor2();
                std::pair<string, string> p1(id, f1);
                std::pair<string, string> p2(id, f2);
                // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
                Real base = baseNPV_[id];
                Real up1 = upNPV_[p1];
                Real up2 = upNPV_[p2];
                std::tuple<string, string, string> triple(id, f1, f2);
                crossNPV_[triple] = npv;
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
        QL_REQUIRE(downNPV_.find(p) != downNPV_.end(),
                   "down shift result not found for trade " << id << ", factor " << factor);
        Real d = downNPV_[p];
        // f_x(x) = (f(x+u) - f(x)) / u
        delta_[p] = u - b;           // = f_x(x) * u
                                     // f_xx(x) = (f(x+u) - 2*f(x) + f(x-u)) / u^2
        gamma_[p] = u - 2.0 * b + d; // = f_xx(x) * u^2
    }
}

void SensitivityAnalysis::writeScenarioReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {

    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    report->addColumn("TradeId", string());
    report->addColumn("Factor", string());
    report->addColumn("Up/Down", string());
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("Scenario NPV", double(), 2);
    report->addColumn("Difference", double(), 2);

    for (auto data : upNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold) {
            report->next();
            report->add(id);
            report->add(factor);
            report->add("Up");
            report->add(base);
            report->add(npv);
            report->add(sensi);
        }
    }

    for (auto data : downNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold) {
            report->next();
            report->add(id);
            report->add(factor);
            report->add("Down");
            report->add(base);
            report->add(npv);
            report->add(sensi);
        }
    }

    for (auto data : crossNPV_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        string factor2 = std::get<2>(data.first);
        ostringstream o;
        o << factor1 << ":" << factor2;
        string factor = o.str();
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold) {
            report->next();
            report->add(id);
            report->add(factor);
            report->add("Cross");
            report->add(base);
            report->add(npv);
            report->add(sensi);
        }
    }

    report->end();
}

void SensitivityAnalysis::writeSensitivityReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {

    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    report->addColumn("TradeId", string());
    report->addColumn("Factor", string());
    report->addColumn("ShiftSize", double(), 6);
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("Delta", double(), 2);
    report->addColumn("Gamma", double(), 2);

    for (auto data : delta_) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string factor = data.first.second;
        Real shiftSize = factors_[factor];
        Real delta = data.second;
        Real gamma = gamma_[p];
        Real base = baseNPV_[id];
        if (fabs(delta) > outputThreshold || fabs(gamma) > outputThreshold) {
            report->next();
            report->add(id);
            report->add(factor);
            report->add(shiftSize);
            report->add(base);
            report->add(delta);
            report->add(gamma);
        }
    }
    report->end();
}

void SensitivityAnalysis::writeCrossGammaReport(const boost::shared_ptr<Report>& report, Real outputThreshold) {

    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    report->addColumn("TradeId", string());
    report->addColumn("Factor 1", string());
    report->addColumn("ShiftSize1", double(), 6);
    report->addColumn("Factor 2", string());
    report->addColumn("ShiftSize2", double(), 6);
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("CrossGamma", double(), 2);

    for (auto data : crossGamma_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        Real shiftSize1 = factors_[factor1];
        string factor2 = std::get<2>(data.first);
        Real shiftSize2 = factors_[factor2];
        Real crossGamma = data.second;
        Real base = baseNPV_[id];
        if (fabs(crossGamma) > outputThreshold) {
            report->next();
            report->add(id);
            report->add(factor1);
            report->add(shiftSize1);
            report->add(factor2);
            report->add(shiftSize2);
            report->add(base);
            report->add(crossGamma);
        }
    }
    report->end();
}

void SensitivityAnalysis::storeFactorShifts(const ShiftScenarioGenerator::ScenarioDescription& desc) {
    RiskFactorKey key1 = desc.key1();
    string factor1 = desc.factor1();
    if (factors_.find(factor1) == factors_.end()) {
        Real shift1Size = getShiftSize(key1);
        // cout << factor1 << "," << key1 << shift1Size << endl;
        factors_[factor1] = shift1Size;
    }
    if (desc.type() == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
        RiskFactorKey key2 = desc.key2();
        string factor2 = desc.factor2();
        if (factors_.find(factor2) == factors_.end()) {
            Real shift2Size = getShiftSize(key2);
            factors_[factor2] = shift2Size;
        }
    }
}

Real SensitivityAnalysis::getShiftSize(const RiskFactorKey& key) const {
    RiskFactorKey::KeyType keytype = key.keytype;
    string keylabel = key.name;
    Real shiftSize = 0.0;
    Real shiftMult = 1.0;
    if (keytype == RiskFactorKey::KeyType::FXSpot) {
        shiftSize = sensitivityData_->fxShiftData()[keylabel].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->fxShiftData()[keylabel].shiftType) == "RELATIVE") {
            shiftMult = simMarket_->fxSpot(keylabel, marketConfiguration_)->value();
        }
    } else if (keytype == RiskFactorKey::KeyType::EquitySpot) {
        shiftSize = sensitivityData_->equityShiftData()[keylabel].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->equityShiftData()[keylabel].shiftType) == "RELATIVE") {
            shiftMult = simMarket_->equitySpot(keylabel, marketConfiguration_)->value();
        }
    } else if (keytype == RiskFactorKey::KeyType::DiscountCurve) {
        string ccy = keylabel;
        shiftSize = sensitivityData_->discountCurveShiftData()[ccy].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->discountCurveShiftData()[ccy].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->discountCurveShiftData()[ccy].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::IndexCurve) {
        string idx = keylabel;
        shiftSize = sensitivityData_->indexCurveShiftData()[idx].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->indexCurveShiftData()[idx].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->indexCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts =
                simMarket_->iborIndex(idx, marketConfiguration_)->forwardingTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::YieldCurve) {
        string yc = keylabel;
        shiftSize = sensitivityData_->yieldCurveShiftData()[yc].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->yieldCurveShiftData()[yc].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->yieldCurveShiftData()[yc].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->yieldCurve(yc, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::EquityForecastCurve) {
        string ec = keylabel;
        shiftSize = sensitivityData_->equityForecastCurveShiftData()[ec].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->equityForecastCurveShiftData()[ec].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->equityForecastCurveShiftData()[ec].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->equityForecastCurve(ec, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::DividendYield) {
        string eq = keylabel;
        shiftSize = sensitivityData_->dividendYieldShiftData()[eq].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->dividendYieldShiftData()[eq].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->dividendYieldShiftData()[eq].shiftTenors[keyIdx];
            Handle<YieldTermStructure> ts = simMarket_->equityDividendCurve(eq, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = ts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::FXVolatility) {
        string pair = keylabel;
        shiftSize = sensitivityData_->fxVolShiftData()[pair].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->fxVolShiftData()[pair].shiftType) == "RELATIVE") {
            vector<Real> strikes = sensitivityData_->fxVolShiftData()[pair].shiftStrikes;
            QL_REQUIRE(strikes.size() == 0, "Only ATM FX vols supported");
            Real atmFwd = 0.0; // hardcoded, since only ATM supported
            Size keyIdx = key.index;
            Period p = sensitivityData_->fxVolShiftData()[pair].shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->fxVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, atmFwd);
            shiftMult = atmVol;
        }
    } else if (keytype == RiskFactorKey::KeyType::EquityVolatility) {
        string pair = keylabel;
        shiftSize = sensitivityData_->equityVolShiftData()[pair].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->equityVolShiftData()[pair].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->equityVolShiftData()[pair].shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->equityVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, Null<Real>());
            shiftMult = atmVol;
        }
    } else if (keytype == RiskFactorKey::KeyType::SwaptionVolatility) {
        string ccy = keylabel;
        shiftSize = sensitivityData_->swaptionVolShiftData()[ccy].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->swaptionVolShiftData()[ccy].shiftType) == "RELATIVE") {
            vector<Real> strikes = sensitivityData_->swaptionVolShiftData()[ccy].shiftStrikes;
            vector<Period> tenors = sensitivityData_->swaptionVolShiftData()[ccy].shiftTerms;
            vector<Period> expiries = sensitivityData_->swaptionVolShiftData()[ccy].shiftExpiries;
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
    } else if (keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        string ccy = keylabel;
        shiftSize = sensitivityData_->capFloorVolShiftData()[ccy].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->capFloorVolShiftData()[ccy].shiftType) == "RELATIVE") {
            vector<Real> strikes = sensitivityData_->capFloorVolShiftData()[ccy].shiftStrikes;
            vector<Period> expiries = sensitivityData_->capFloorVolShiftData()[ccy].shiftExpiries;
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
    } else if (keytype == RiskFactorKey::KeyType::CDSVolatility) {
        string name = keylabel;
        shiftSize = sensitivityData_->cdsVolShiftData()[name].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->cdsVolShiftData()[name].shiftType) == "RELATIVE") {
            vector<Period> expiries = sensitivityData_->cdsVolShiftData()[name].shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx;
            Period p_exp = expiries[expIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->cdsVol(name, marketConfiguration_);
            Time t_exp = vts->dayCounter().yearFraction(asof_, asof_ + p_exp);
            Real strike = 0.0; // FIXME
            Real atmVol = vts->blackVol(t_exp, strike);
            shiftMult = atmVol;
        }
    } else if (keytype == RiskFactorKey::KeyType::SurvivalProbability) {
        string name = keylabel;
        shiftSize = sensitivityData_->creditCurveShiftData()[name].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->creditCurveShiftData()[name].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->creditCurveShiftData()[name].shiftTenors[keyIdx];
            Handle<DefaultProbabilityTermStructure> ts = simMarket_->defaultCurve(name, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real prob = ts->survivalProbability(t);
            shiftMult = -std::log(prob) / t;
        }
    } else if (keytype == RiskFactorKey::KeyType::BaseCorrelation) {
        string name = keylabel;
        shiftSize = sensitivityData_->baseCorrelationShiftData()[name].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->baseCorrelationShiftData()[name].shiftType) == "RELATIVE") {
            vector<Real> lossLevels = sensitivityData_->baseCorrelationShiftData()[name].shiftLossLevels;
            vector<Period> terms = sensitivityData_->baseCorrelationShiftData()[name].shiftTerms;
            Size keyIdx = key.index;
            Size lossLevelIdx = keyIdx / terms.size();
            Real lossLevel = lossLevels[lossLevelIdx];
            Size termIdx = keyIdx % terms.size();
            Period term = terms[termIdx];
            Handle<BilinearBaseCorrelationTermStructure> ts = simMarket_->baseCorrelation(name, marketConfiguration_);
            Real bc = ts->correlation(asof_ + term, lossLevel, true); // extrapolate
            shiftMult = bc;
        }
    } else if (keytype == RiskFactorKey::KeyType::ZeroInflationCurve) {
        string idx = keylabel;
        shiftSize = sensitivityData_->zeroInflationCurveShiftData()[idx].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->zeroInflationCurveShiftData()[idx].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->zeroInflationCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<ZeroInflationTermStructure> yts =
                simMarket_->zeroInflationIndex(idx, marketConfiguration_)->zeroInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t);
            shiftMult = zeroRate;
        }
    } else if (keytype == RiskFactorKey::KeyType::YoYInflationCurve) {
        string idx = keylabel;
        shiftSize = sensitivityData_->yoyInflationCurveShiftData()[idx].shiftSize;
        if (boost::to_upper_copy(sensitivityData_->yoyInflationCurveShiftData()[idx].shiftType) == "RELATIVE") {
            Size keyIdx = key.index;
            Period p = sensitivityData_->yoyInflationCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<YoYInflationTermStructure> yts =
                simMarket_->yoyInflationIndex(idx, marketConfiguration_)->yoyInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real yoyRate = yts->yoyRate(t);
            shiftMult = yoyRate;
        }
    } else {
        // KeyType::CPIIndex does not get shifted
        QL_FAIL("KeyType not supported yet - " << keytype);
    }
    Real realShift = shiftSize * shiftMult;
    return realShift;
}

const std::set<std::string>& SensitivityAnalysis::trades() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return trades_;
}

const std::map<std::string, QuantLib::Real>& SensitivityAnalysis::factors() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return factors_;
}

const std::map<std::string, Real>& SensitivityAnalysis::baseNPV() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return baseNPV_;
}

const std::map<std::pair<std::string, std::string>, Real>& SensitivityAnalysis::delta() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return delta_;
}

const std::map<std::pair<std::string, std::string>, Real>& SensitivityAnalysis::gamma() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return gamma_;
}

const std::map<std::tuple<std::string, std::string, std::string>, Real>& SensitivityAnalysis::crossGamma() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return crossGamma_;
}
} // namespace analytics
} // namespace ore
