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

#include <orea/cube/cubewriter.hpp>
#include <orea/cube/sensicube.hpp>
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
#include <ql/math/comparison.hpp>
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

SensitivityAnalysis::SensitivityAnalysis(
    const boost::shared_ptr<ore::data::Portfolio>& portfolio, const boost::shared_ptr<ore::data::Market>& market,
    const string& marketConfiguration, const boost::shared_ptr<ore::data::EngineData>& engineData,
    const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
    const boost::shared_ptr<SensitivityScenarioData>& sensitivityData, const Conventions& conventions,
    const bool recalibrateModels, const CurveConfigurations& curveConfigs,
    const TodaysMarketParameters& todaysMarketParams, const bool nonShiftedBaseCurrencyConversion,
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders,
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders,
    const boost::shared_ptr<ReferenceDataManager>& referenceData, const bool continueOnError, bool xccyDiscounting)
    : market_(market), marketConfiguration_(marketConfiguration), asof_(market->asofDate()),
      simMarketData_(simMarketData), sensitivityData_(sensitivityData), conventions_(conventions),
      recalibrateModels_(recalibrateModels), curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams),
      overrideTenors_(false), nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion),
      extraEngineBuilders_(extraEngineBuilders), extraLegBuilders_(extraLegBuilders), referenceData_(referenceData),
      continueOnError_(continueOnError), engineData_(engineData), portfolio_(portfolio),
      xccyDiscounting_(xccyDiscounting), initialized_(false), computed_(false) {}

std::vector<boost::shared_ptr<ValuationCalculator>> SensitivityAnalysis::buildValuationCalculators() const {
    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    if (nonShiftedBaseCurrencyConversion_) // use "original" FX rates to convert sensi to base currency
        calculators.push_back(boost::make_shared<NPVCalculatorFXT0>(simMarketData_->baseCcy(), market_));
    else // use the scenario FX rate when converting sensi to base currency
        calculators.push_back(boost::make_shared<NPVCalculator>(simMarketData_->baseCcy()));
    return calculators;
}

void SensitivityAnalysis::initialize(boost::shared_ptr<NPVSensiCube>& cube) {
    LOG("Build Sensitivity Scenario Generator and Simulation Market");
    initializeSimMarket();

    LOG("Build Engine Factory and rebuild portfolio");
    boost::shared_ptr<EngineFactory> factory = buildFactory(extraEngineBuilders_, extraLegBuilders_);
    resetPortfolio(factory);
    if (recalibrateModels_)
        modelBuilders_ = factory->modelBuilders();
    else
        modelBuilders_.clear();

    if (!cube) {
        LOG("Build the cube object to store sensitivities");
        initializeCube(cube);
    }

    sensiCube_ = boost::make_shared<SensitivityCube>(cube, scenarioGenerator_->scenarioDescriptions(),
                                                     scenarioGenerator_->shiftSizes());
    initialized_ = true;
}

void SensitivityAnalysis::generateSensitivities(boost::shared_ptr<NPVSensiCube> cube) {

    QL_REQUIRE(!initialized_, "unexpected state of SensitivitiesAnalysis object");

    // initialize the helper member objects
    initialize(cube);
    QL_REQUIRE(initialized_, "SensitivitiesAnalysis member objects not correctly initialized");
    boost::shared_ptr<DateGrid> dg = boost::make_shared<DateGrid>("1,0W", NullCalendar());
    vector<boost::shared_ptr<ValuationCalculator>> calculators = buildValuationCalculators();
    ValuationEngine engine(asof_, dg, simMarket_, modelBuilders_);
    for (auto const& i : this->progressIndicators())
        engine.registerProgressIndicator(i);
    LOG("Run Sensitivity Scenarios");
    engine.buildCube(portfolio_, cube, calculators);

    computed_ = true;
    LOG("Sensitivity analysis completed");
}

void SensitivityAnalysis::initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact) {

    LOG("Initialise sim market for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                           << ")");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(market_, simMarketData_, conventions_, marketConfiguration_,
                                                       curveConfigs_, todaysMarketParams_, continueOnError_);

    LOG("Sim market initialised for sensitivity analysis");

    LOG("Create scenario factory for sensitivity analysis");
    boost::shared_ptr<Scenario> baseScenario = simMarket_->baseScenario();
    boost::shared_ptr<ScenarioFactory> scenarioFactory =
        scenFact ? scenFact : boost::make_shared<CloneScenarioFactory>(baseScenario);
    LOG("Scenario factory created for sensitivity analysis");

    LOG("Create scenario generator for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                               << ")");
    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(
        sensitivityData_, baseScenario, simMarketData_, scenarioFactory, overrideTenors_, continueOnError_);
    LOG("Scenario generator created for sensitivity analysis");

    // Set simulation market's scenario generator
    simMarket_->scenarioGenerator() = scenarioGenerator_;
}

boost::shared_ptr<EngineFactory>
SensitivityAnalysis::buildFactory(const std::vector<boost::shared_ptr<EngineBuilder>> extraBuilders,
                                  const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders) const {
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration_;
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(
        engineData_, simMarket_, configurations, extraBuilders, extraLegBuilders, referenceData_);
    return factory;
}

void SensitivityAnalysis::resetPortfolio(const boost::shared_ptr<EngineFactory>& factory) {
    portfolio_->reset();
    portfolio_->build(factory);
}

void SensitivityAnalysis::initializeCube(boost::shared_ptr<NPVSensiCube>& cube) const {
    cube = boost::make_shared<DoublePrecisionSensiCube>(portfolio_->ids(), asof_, scenarioGenerator_->samples());
}

Real getShiftSize(const RiskFactorKey& key, const SensitivityScenarioData& sensiParams,
                  const boost::shared_ptr<ScenarioSimMarket>& simMarket, const string& marketConfiguration) {

    Date asof = simMarket->asofDate();
    RiskFactorKey::KeyType keytype = key.keytype;
    string keylabel = key.name;
    Real shiftSize = 0.0;
    Real shiftMult = 1.0;
    switch (keytype) {
    case RiskFactorKey::KeyType::FXSpot: {
        auto itr = sensiParams.fxShiftData().find(keylabel);
        QL_REQUIRE(itr != sensiParams.fxShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            shiftMult = simMarket->fxSpot(keylabel, marketConfiguration)->value();
        }
    } break;
    case RiskFactorKey::KeyType::EquitySpot: {
        auto itr = sensiParams.equityShiftData().find(keylabel);
        QL_REQUIRE(itr != sensiParams.equityShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            shiftMult = simMarket->equitySpot(keylabel, marketConfiguration)->value();
        }
    } break;
    case RiskFactorKey::KeyType::DiscountCurve: {
        string ccy = keylabel;
        auto itr = sensiParams.discountCurveShiftData().find(ccy);
        QL_REQUIRE(itr != sensiParams.discountCurveShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket->discountCurve(ccy, marketConfiguration);
            Time t = yts->dayCounter().yearFraction(asof, asof + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::IndexCurve: {
        string idx = keylabel;
        auto itr = sensiParams.indexCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensiParams.indexCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket->iborIndex(idx, marketConfiguration)->forwardingTermStructure();
            Time t = yts->dayCounter().yearFraction(asof, asof + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::YieldCurve: {
        string yc = keylabel;
        auto itr = sensiParams.yieldCurveShiftData().find(yc);
        QL_REQUIRE(itr != sensiParams.yieldCurveShiftData().end(), "shiftData not found for " << yc);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> yts = simMarket->yieldCurve(yc, marketConfiguration);
            Time t = yts->dayCounter().yearFraction(asof, asof + p);
            Real zeroRate = yts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::DividendYield: {
        string eq = keylabel;
        auto itr = sensiParams.dividendYieldShiftData().find(eq);
        QL_REQUIRE(itr != sensiParams.dividendYieldShiftData().end(), "shiftData not found for " << eq);
        shiftSize = itr->second->shiftSize;

        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<YieldTermStructure> ts = simMarket->equityDividendCurve(eq, marketConfiguration);
            Time t = ts->dayCounter().yearFraction(asof, asof + p);
            Real zeroRate = ts->zeroRate(t, Continuous);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::FXVolatility: {
        string pair = keylabel;
        auto itr = sensiParams.fxVolShiftData().find(pair);
        QL_REQUIRE(itr != sensiParams.fxVolShiftData().end(), "shiftData not found for " << pair);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = sensiParams.fxVolShiftData().at(pair).shiftStrikes;
            QL_REQUIRE(strikes.size() == 1 && close_enough(strikes[0], 0), "Only ATM FX vols supported");
            Real atmFwd = 0.0; // hardcoded, since only ATM supported
            Size keyIdx = key.index;
            Period p = itr->second.shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket->fxVol(pair, marketConfiguration);
            Time t = vts->dayCounter().yearFraction(asof, asof + p);
            Real atmVol = vts->blackVol(t, atmFwd);
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::EquityVolatility: {
        string pair = keylabel;
        auto itr = sensiParams.equityVolShiftData().find(pair);
        QL_REQUIRE(itr != sensiParams.equityVolShiftData().end(), "shiftData not found for " << pair);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second.shiftExpiries[keyIdx];
            Handle<BlackVolTermStructure> vts = simMarket->equityVol(pair, marketConfiguration);
            Time t = vts->dayCounter().yearFraction(asof, asof + p);
            Real atmVol = vts->blackVol(t, Null<Real>());
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::SwaptionVolatility: {
        string ccy = keylabel;
        auto itr = sensiParams.swaptionVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensiParams.swaptionVolShiftData().end(), "shiftData not found for " << ccy);
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
            Handle<SwaptionVolatilityStructure> vts = simMarket->swaptionVol(ccy, marketConfiguration);
            // Time t_exp = vts->dayCounter().yearFraction(asof, asof + p_exp);
            // Time t_ten = vts->dayCounter().yearFraction(asof, asof + p_ten);
            // Real atmVol = vts->volatility(t_exp, t_ten, Null<Real>());
            Real vol = vts->volatility(p_exp, p_ten, strike);
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::YieldVolatility: {
        string securityId = keylabel;
        auto itr = sensiParams.yieldVolShiftData().find(securityId);
        QL_REQUIRE(itr != sensiParams.yieldVolShiftData().end(), "shiftData not found for " << securityId);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second.shiftStrikes;
            QL_REQUIRE(strikes.size() == 1 && close_enough(strikes[0], 0.0),
                       "shift strikes should be {0.0} for yield volatilities");
            vector<Period> tenors = itr->second.shiftTerms;
            vector<Period> expiries = itr->second.shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx / (tenors.size() * strikes.size());
            Period p_exp = expiries[expIdx];
            Size tenIdx = keyIdx % tenors.size();
            Period p_ten = tenors[tenIdx];
            Real strike = Null<Real>(); // for cubes this will be ATM
            Handle<SwaptionVolatilityStructure> vts = simMarket->yieldVol(securityId, marketConfiguration);
            Real vol = vts->volatility(p_exp, p_ten, strike);
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::OptionletVolatility: {
        string ccy = keylabel;
        auto itr = sensiParams.capFloorVolShiftData().find(ccy);
        QL_REQUIRE(itr != sensiParams.capFloorVolShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second->shiftStrikes;
            vector<Period> expiries = itr->second->shiftExpiries;
            QL_REQUIRE(strikes.size() > 0, "Only strike capfloor vols supported");
            shiftMult = simMarket->baseScenario()->get(
                RiskFactorKey(RiskFactorKey::KeyType::OptionletVolatility, ccy, key.index));
        }
    } break;
    case RiskFactorKey::KeyType::CDSVolatility: {
        string name = keylabel;
        auto itr = sensiParams.cdsVolShiftData().find(name);
        QL_REQUIRE(itr != sensiParams.cdsVolShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Period> expiries = itr->second.shiftExpiries;
            Size keyIdx = key.index;
            Size expIdx = keyIdx;
            Period p_exp = expiries[expIdx];
            Handle<BlackVolTermStructure> vts = simMarket->cdsVol(name, marketConfiguration);
            Time t_exp = vts->dayCounter().yearFraction(asof, asof + p_exp);
            Real strike = 0.0; // FIXME
            Real atmVol = vts->blackVol(t_exp, strike);
            shiftMult = atmVol;
        }
    } break;
    case RiskFactorKey::KeyType::SurvivalProbability: {
        string name = keylabel;
        auto itr = sensiParams.creditCurveShiftData().find(name);
        QL_REQUIRE(itr != sensiParams.creditCurveShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<DefaultProbabilityTermStructure> ts = simMarket->defaultCurve(name, marketConfiguration);
            Time t = ts->dayCounter().yearFraction(asof, asof + p);
            Real prob = ts->survivalProbability(t);
            shiftMult = -std::log(prob) / t;
        }
    } break;
    case RiskFactorKey::KeyType::BaseCorrelation: {
        string name = keylabel;
        auto itr = sensiParams.baseCorrelationShiftData().find(name);
        QL_REQUIRE(itr != sensiParams.baseCorrelationShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> lossLevels = itr->second.shiftLossLevels;
            vector<Period> terms = itr->second.shiftTerms;
            Size keyIdx = key.index;
            Size lossLevelIdx = keyIdx / terms.size();
            Real lossLevel = lossLevels[lossLevelIdx];
            Size termIdx = keyIdx % terms.size();
            Period term = terms[termIdx];
            Handle<BilinearBaseCorrelationTermStructure> ts = simMarket->baseCorrelation(name, marketConfiguration);
            Real bc = ts->correlation(asof + term, lossLevel, true); // extrapolate
            shiftMult = bc;
        }
    } break;
    case RiskFactorKey::KeyType::ZeroInflationCurve: {
        string idx = keylabel;
        auto itr = sensiParams.zeroInflationCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensiParams.zeroInflationCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = itr->second->shiftTenors[keyIdx];
            Handle<ZeroInflationTermStructure> yts =
                simMarket->zeroInflationIndex(idx, marketConfiguration)->zeroInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof, asof + p);
            Real zeroRate = yts->zeroRate(t);
            shiftMult = zeroRate;
        }
    } break;
    case RiskFactorKey::KeyType::YoYInflationCurve: {
        string idx = keylabel;
        auto itr = sensiParams.yoyInflationCurveShiftData().find(idx);
        QL_REQUIRE(itr != sensiParams.yoyInflationCurveShiftData().end(), "shiftData not found for " << idx);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size keyIdx = key.index;
            Period p = sensiParams.yoyInflationCurveShiftData().at(idx)->shiftTenors[keyIdx];
            Handle<YoYInflationTermStructure> yts =
                simMarket->yoyInflationIndex(idx, marketConfiguration)->yoyInflationTermStructure();
            Time t = yts->dayCounter().yearFraction(asof, asof + p);
            Real yoyRate = yts->yoyRate(t);
            shiftMult = yoyRate;
        }
    } break;
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility: {
        string name = keylabel;
        auto itr = sensiParams.yoyInflationCapFloorVolShiftData().find(name);
        QL_REQUIRE(itr != sensiParams.yoyInflationCapFloorVolShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second->shiftStrikes;
            vector<Period> expiries = itr->second->shiftExpiries;
            QL_REQUIRE(strikes.size() > 0, "Only strike yoy inflation capfloor vols supported");
            Size keyIdx = key.index;
            Size expIdx = keyIdx / strikes.size();
            Period p_exp = expiries[expIdx];
            Size strIdx = keyIdx % strikes.size();
            Real strike = strikes[strIdx];
            Handle<QuantExt::YoYOptionletVolatilitySurface> vts = simMarket->yoyCapFloorVol(name, marketConfiguration);
            Real vol = vts->volatility(p_exp, strike, vts->observationLag());
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility: {
        string name = keylabel;
        auto itr = sensiParams.zeroInflationCapFloorVolShiftData().find(name);
        QL_REQUIRE(itr != sensiParams.zeroInflationCapFloorVolShiftData().end(), "shiftData not found for " << name);
        shiftSize = itr->second->shiftSize;
        if (parseShiftType(itr->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            vector<Real> strikes = itr->second->shiftStrikes;
            vector<Period> expiries = itr->second->shiftExpiries;
            QL_REQUIRE(strikes.size() > 0, "Only strike zc inflation capfloor vols supported");
            Size keyIdx = key.index;
            Size expIdx = keyIdx / strikes.size();
            Period p_exp = expiries[expIdx];
            Size strIdx = keyIdx % strikes.size();
            Real strike = strikes[strIdx];
            Handle<CPIVolatilitySurface> vts =
                simMarket->cpiInflationCapFloorVolatilitySurface(name, marketConfiguration);
            Real vol = vts->volatility(p_exp, strike, vts->observationLag());
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::CommodityCurve: {
        auto it = sensiParams.commodityCurveShiftData().find(keylabel);
        QL_REQUIRE(it != sensiParams.commodityCurveShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = it->second->shiftSize;
        if (parseShiftType(it->second->shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Period p = it->second->shiftTenors[key.index];
            Handle<PriceTermStructure> priceCurve = simMarket->commodityPriceCurve(keylabel, marketConfiguration);
            Time t = priceCurve->dayCounter().yearFraction(asof, asof + p);
            shiftMult = priceCurve->price(t);
        }
    } break;
    case RiskFactorKey::KeyType::CommodityVolatility: {
        auto it = sensiParams.commodityVolShiftData().find(keylabel);
        QL_REQUIRE(it != sensiParams.commodityVolShiftData().end(), "shiftData not found for " << keylabel);

        shiftSize = it->second.shiftSize;
        if (parseShiftType(it->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            Size moneynessIndex = key.index / it->second.shiftExpiries.size();
            Size expiryIndex = key.index % it->second.shiftExpiries.size();
            Real moneyness = it->second.shiftStrikes[moneynessIndex];
            Period expiry = it->second.shiftExpiries[expiryIndex];
            Real spotValue = simMarket->commodityPriceCurve(keylabel, marketConfiguration)->price(0);
            Handle<BlackVolTermStructure> vts = simMarket->commodityVolatility(keylabel, marketConfiguration);
            Time t = vts->dayCounter().yearFraction(asof, asof + expiry);
            Real vol = vts->blackVol(t, moneyness * spotValue);
            shiftMult = vol;
        }
    } break;
    case RiskFactorKey::KeyType::SecuritySpread: {
        auto itr = sensiParams.securityShiftData().find(keylabel);
        QL_REQUIRE(itr != sensiParams.securityShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (parseShiftType(itr->second.shiftType) == SensitivityScenarioGenerator::ShiftType::Relative) {
            shiftMult = simMarket->securitySpread(keylabel, marketConfiguration)->value();
        }
    } break;
    default:
        // KeyType::CPIIndex does not get shifted
        QL_FAIL("KeyType not supported yet - " << keytype);
    }
    Real realShift = shiftSize * shiftMult;
    return realShift;
}

} // namespace analytics
} // namespace ore
