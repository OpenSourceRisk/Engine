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

#include <ored/utilities/osutils.hpp>
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

    boost::shared_ptr<vector<ShiftScenarioGenerator::ScenarioDescription>> scenDesc = boost::make_shared<vector<ShiftScenarioGenerator::ScenarioDescription>>
        (scenarioGenerator_->scenarioDescriptions());
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
    

    collectResultsFromCube();
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

void SensitivityAnalysis::collectResultsFromCube() {

    /***********************************************
     * Collect results
     * - base NPVs,
     * - NPVs after single factor up shifts,
     * - NPVs after single factor down shifts
     * - deltas, gammas and cross gammas
     */
    Size ps = portfolio_->size();
    Size ss = scenarioGenerator_->samples();
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();
    for (Size i = 0; i < ps; ++i) {

        Real npv0 = sensiCube_->baseNPV(i);
        string id = portfolio_->trades()[i]->id();
        trades_.insert(id);

        for (Size j = 0; j < ss; ++j) {
            ShiftScenarioGenerator::ScenarioDescription desc = (*scenDesc)[j];
            ShiftScenarioGenerator::ScenarioDescription::Type type = desc.type();
            Real npv = sensiCube_->getNPV(i, j);
            if (type == ShiftScenarioGenerator::ScenarioDescription::Type::Up) {
                    // delta
                    string factor = desc.factor1();
                    pair<string, string> p(id, factor);
                    delta_[p] = npv - npv0;
                    storeFactorShifts(desc);
            } else if (type == ShiftScenarioGenerator::ScenarioDescription::Type::Down) {
                    // gamma
                    string factor = desc.factor1();
                    pair<string, string> p(id, factor);
                    Real u = sensiCube_->upNPV(i, factor);
                    Real d = sensiCube_->downNPV(i, factor);
                    gamma_[p] = u - 2.0 * npv0 + d; // = f_xx(x) * u^2
                    storeFactorShifts(desc);
            }
        }
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

    for (Size i = 0; i < portfolio_->size(); ++i) {
        string id = portfolio_->trades()[i]->id();
        Real base = sensiCube_->baseNPV(i);
        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {
            ShiftScenarioGenerator::ScenarioDescription desc = (*sensiCube_->scenDesc())[j];
            string type = desc.typeString();
            
            ostringstream o;
            o << desc.factor1();
            if (desc.factor2() != "")
                o << ":" << desc.factor2();
            string factor = o.str();

            Real npv = sensiCube_->getNPV(i, j);
            Real sensi = npv-base;
            
            if (fabs(sensi) > outputThreshold) {
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
        Real base = baseNPV(id);
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
    LOG("writing CrossGamma");
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");

    report->addColumn("TradeId", string());
    report->addColumn("Factor 1", string());
    report->addColumn("ShiftSize1", double(), 6);
    report->addColumn("Factor 2", string());
    report->addColumn("ShiftSize2", double(), 6);
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("CrossGamma", double(), 2);

    Size ps = portfolio_->size();
    Size ss = scenarioGenerator_->samples();
    boost::shared_ptr<vector<SensitivityScenarioGenerator::ScenarioDescription>> scenDesc = sensiCube_->scenDesc();
    for (Size i = 0; i < ps; ++i) {

        Real npv0 = sensiCube_->baseNPV(i);
        string id = portfolio_->trades()[i]->id();
        
        for (Size j = 0; j < ss; ++j) {
            ShiftScenarioGenerator::ScenarioDescription desc = (*scenDesc)[j];
            ShiftScenarioGenerator::ScenarioDescription::Type type = desc.type();
            Real npv = sensiCube_->getNPV(i, j);
            if (type == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
                // cross gamma
                string factor1 = desc.factor1();
                string factor2 = desc.factor2();
                // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
                Real up1 = sensiCube_->upNPV(i, factor1);
                Real up2 = sensiCube_->upNPV(i, factor2);

                Real cg = crossGamma( id, factor1, factor2); 
                
                Real shiftSize1 = factors_[factor1];
                Real shiftSize2 = factors_[factor2];
                if (fabs(cg) > outputThreshold) {
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
    }
    report->end();
    LOG("CrossGamma written");
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
    if(sensitivityData_->fxShiftData()[keylabel].shiftType == "RELATIVE") {
    	if (keytype == RiskFactorKey::KeyType::FXSpot) {
            shiftSize = sensitivityData_->fxShiftData()[keylabel].shiftSize;
            shiftMult = simMarket_->fxSpot(keylabel, marketConfiguration_)->value();
    	} else if (keytype == RiskFactorKey::KeyType::EquitySpot) {
            shiftSize = sensitivityData_->equityShiftData()[keylabel].shiftSize;
            shiftMult = simMarket_->equitySpot(keylabel, marketConfiguration_)->value();
    	} else if (keytype == RiskFactorKey::KeyType::DiscountCurve) {
            string ccy = keylabel;
            shiftSize = sensitivityData_->discountCurveShiftData()[ccy].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->discountCurveShiftData()[ccy].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->discountCurve(ccy, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::IndexCurve) {
            string idx = keylabel;
            shiftSize = sensitivityData_->indexCurveShiftData()[idx].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->indexCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts =
                simMarket_->iborIndex(idx, marketConfiguration_)->forwardingTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::YieldCurve) {
            string yc = keylabel;
            shiftSize = sensitivityData_->yieldCurveShiftData()[yc].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->yieldCurveShiftData()[yc].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->yieldCurve(yc, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::EquityForecastCurve) {
            string ec = keylabel;
            shiftSize = sensitivityData_->equityForecastCurveShiftData()[ec].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->equityForecastCurveShiftData()[ec].shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket_->equityForecastCurve(ec, marketConfiguration_);
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::DividendYield) {
            string eq = keylabel;
            shiftSize = sensitivityData_->dividendYieldShiftData()[eq].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->dividendYieldShiftData()[eq].shiftTenors[keyIdx];
            Handle<YieldTermStructure> ts = simMarket_->equityDividendCurve(eq, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = ts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::FXVolatility) {
            string pair = keylabel;
            shiftSize = sensitivityData_->fxVolShiftData()[pair].shiftSize;
            vector<Real> strikes = sensitivityData_->fxVolShiftData()[pair].shiftStrikes;
            QL_REQUIRE(strikes.size() == 0, "Only ATM FX vols supported");
            Real atmFwd = 0.0; // hardcoded, since only ATM supported
            Size keyIdx = key.index;
            Period p = sensitivityData_->fxVolShiftData()[pair].shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->fxVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, atmFwd);
            shiftMult = atmVol;
    	} else if (keytype == RiskFactorKey::KeyType::EquityVolatility) {
            string pair = keylabel;
            shiftSize = sensitivityData_->equityVolShiftData()[pair].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->equityVolShiftData()[pair].shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->equityVol(pair, marketConfiguration_);
            Time t = vts->dayCounter().yearFraction(asof_, asof_ + p);
            Real atmVol = vts->blackVol(t, Null<Real>());
            shiftMult = atmVol;
    	} else if (keytype == RiskFactorKey::KeyType::SwaptionVolatility) {
            string ccy = keylabel;
            shiftSize = sensitivityData_->swaptionVolShiftData()[ccy].shiftSize;
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
    	} else if (keytype == RiskFactorKey::KeyType::OptionletVolatility) {
            string ccy = keylabel;
            shiftSize = sensitivityData_->capFloorVolShiftData()[ccy].shiftSize;
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
    	} else if (keytype == RiskFactorKey::KeyType::CDSVolatility) {
            string name = keylabel;
            shiftSize = sensitivityData_->cdsVolShiftData()[name].shiftSize;
            vector<Period> expiries = sensitivityData_->cdsVolShiftData()[name].shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx;
            Period p_exp = expiries[expIdx];
            Handle<BlackVolTermStructure> vts = simMarket_->cdsVol(name, marketConfiguration_);
            Time t_exp = vts->dayCounter().yearFraction(asof_, asof_ + p_exp);
            Real strike = 0.0; // FIXME
            Real atmVol = vts->blackVol(t_exp, strike);
            shiftMult = atmVol;
    	} else if (keytype == RiskFactorKey::KeyType::SurvivalProbability) {
            string name = keylabel;
            shiftSize = sensitivityData_->creditCurveShiftData()[name].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->creditCurveShiftData()[name].shiftTenors[keyIdx];
            Handle<DefaultProbabilityTermStructure> ts = simMarket_->defaultCurve(name, marketConfiguration_);
            Time t = ts->dayCounter().yearFraction(asof_, asof_ + p);
            Real prob = ts->survivalProbability(t);
            shiftMult = -std::log(prob) / t;
    	} else if (keytype == RiskFactorKey::KeyType::BaseCorrelation) {
            string name = keylabel;
            shiftSize = sensitivityData_->baseCorrelationShiftData()[name].shiftSize;
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
    	} else if (keytype == RiskFactorKey::KeyType::ZeroInflationCurve) {
            string idx = keylabel;
            shiftSize = sensitivityData_->zeroInflationCurveShiftData()[idx].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->zeroInflationCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<ZeroInflationTermStructure> yts =
                simMarket_->zeroInflationIndex(idx, marketConfiguration_)->zeroInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real zeroRate = yts->zeroRate(t);
            shiftMult = zeroRate;
    	} else if (keytype == RiskFactorKey::KeyType::YoYInflationCurve) {
            string idx = keylabel;
            shiftSize = sensitivityData_->yoyInflationCurveShiftData()[idx].shiftSize;
            Size keyIdx = key.index;
            Period p = sensitivityData_->yoyInflationCurveShiftData()[idx].shiftTenors[keyIdx];
            Handle<YoYInflationTermStructure> yts =
                simMarket_->yoyInflationIndex(idx, marketConfiguration_)->yoyInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof_, asof_ + p);
            Real yoyRate = yts->yoyRate(t);
            shiftMult = yoyRate;
    	} else {
            // KeyType::CPIIndex does not get shifted
            QL_FAIL("KeyType not supported yet - " << keytype);
    	}
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

Real SensitivityAnalysis::baseNPV(std::string& id) const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return sensiCube_->baseNPV(id);
}

const std::map<std::pair<std::string, std::string>, Real>& SensitivityAnalysis::delta() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return delta_;
}

const std::map<std::pair<std::string, std::string>, Real>& SensitivityAnalysis::gamma() const {
    QL_REQUIRE(computed_, "Sensitivities have not been successfully computed");
    return gamma_;
}

Real SensitivityAnalysis::crossGamma(const std::string& trade, const std::string& factor1, const std::string& factor2) const {
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


