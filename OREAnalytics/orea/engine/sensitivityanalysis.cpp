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
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/currencies/metals.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/instruments/payment.hpp>
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
    const boost::shared_ptr<SensitivityScenarioData>& sensitivityData, const bool recalibrateModels,
    const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
    const bool nonShiftedBaseCurrencyConversion, const boost::shared_ptr<ReferenceDataManager>& referenceData,
    const IborFallbackConfig& iborFallbackConfig, const bool continueOnError, bool analyticFxSensis, bool dryRun)
    : market_(market), marketConfiguration_(marketConfiguration), asof_(market ? market->asofDate() : Date()),
      simMarketData_(simMarketData), sensitivityData_(sensitivityData), recalibrateModels_(recalibrateModels),
      curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams), overrideTenors_(false),
      nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), continueOnError_(continueOnError), engineData_(engineData),
      portfolio_(portfolio), analyticFxSensis_(analyticFxSensis), dryRun_(dryRun), initialized_(false),
      computed_(false) {}

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

    sensiCube_ =
        boost::make_shared<SensitivityCube>(cube, scenarioGenerator_->scenarioDescriptions(),
                                            scenarioGenerator_->shiftSizes(), sensitivityData_->twoSidedDeltas());

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
    engine.buildCube(portfolio_, cube, calculators, true, nullptr, nullptr, {}, dryRun_);

    addAnalyticFxSensitivities();

    computed_ = true;

    LOG("Sensitivity analysis completed");
}

void SensitivityAnalysis::initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact) {

    LOG("Initialise sim market for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                           << ")");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(
        market_, simMarketData_, marketConfiguration_,
        curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
        todaysMarketParams_ ? *todaysMarketParams_ : TodaysMarketParameters(), continueOnError_,
        sensitivityData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_);

    LOG("Sim market initialised for sensitivity analysis");

    LOG("Create scenario factory for sensitivity analysis");
    boost::shared_ptr<Scenario> baseScenario = simMarket_->baseScenario();
    boost::shared_ptr<ScenarioFactory> scenarioFactory =
        scenFact ? scenFact : boost::make_shared<CloneScenarioFactory>(baseScenario);
    LOG("Scenario factory created for sensitivity analysis");

    LOG("Create scenario generator for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                               << ")");
    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(
        sensitivityData_, baseScenario, simMarketData_, simMarket_, scenarioFactory, overrideTenors_, continueOnError_);
    LOG("Scenario generator created for sensitivity analysis");

    // Set simulation market's scenario generator
    simMarket_->scenarioGenerator() = scenarioGenerator_;
}

boost::shared_ptr<EngineFactory> SensitivityAnalysis::buildFactory() const {
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration_;
    auto ed = boost::make_shared<EngineData>(*engineData_);
    ed->globalParameters()["RunType"] =
        std::string("Sensitivity") + (sensitivityData_->computeGamma() ? "DeltaGamma" : "Delta");
    boost::shared_ptr<EngineFactory> factory =
        boost::make_shared<EngineFactory>(ed, simMarket_, configurations, referenceData_, iborFallbackConfig_);
    return factory;
}

void SensitivityAnalysis::resetPortfolio(const boost::shared_ptr<EngineFactory>& factory) {
    portfolio_->reset();
    portfolio_->build(factory, "sensi analysis");
}

void SensitivityAnalysis::initializeCube(boost::shared_ptr<NPVSensiCube>& cube) const {
    cube = boost::make_shared<DoublePrecisionSensiCube>(portfolio_->ids(), asof_, scenarioGenerator_->samples());
}

void SensitivityAnalysis::addAnalyticFxSensitivities() {

    if (!analyticFxSensis_) {
        return;
    }

    DLOG("addAnalyticFxSensitivities: start overwriting FX sensitivities with analytic values.");

    // We want simulation market in its initial state for calculating analytic values.
    simMarket_->reset();

    // The currency of the sensitivities.
    string baseCcy = simMarketData_->baseCcy();

    for (const auto& [tradeId, trade] : portfolio_->trades()) {

        if (auto fxo = boost::dynamic_pointer_cast<FxOption>(trade)) {

            DLOG("addAnalyticFxSensitivities: trade id " << tradeId << " is an FX option so include.");

            // Skip this option if it has matured.
            if (QuantLib::detail::simple_event(fxo->maturity()).hasOccurred()) {
                DLOG("addAnalyticFxSensitivities: skipping FX option with id "
                     << tradeId << " as it has matured. Maturity is " << io::iso_date(fxo->maturity()) << ".");
                continue;
            }

            Size tradeIdx = sensiCube_->tradeIdx().at(tradeId);

            const auto& domCcy = fxo->soldCurrency();
            if (isMetal(parseCurrency(domCcy))) {
                DLOG("addAnalyticFxSensitivities: FX option " << tradeId << " references the price of a metal, "
                                                              << domCcy << ", so skipping it.");
                continue;
            }
            string pairDomBase = domCcy + baseCcy;
            RiskFactorKey domCcyKey(RiskFactorKey::KeyType::FXSpot, pairDomBase);
            Real domCcySensi = Null<Real>();

            const auto& forCcy = fxo->boughtCurrency();
            if (isMetal(parseCurrency(forCcy))) {
                DLOG("addAnalyticFxSensitivities: FX option " << tradeId << " references the price of a metal, "
                                                              << forCcy << ", so skipping it.");
                continue;
            }
            string pairForBase = forCcy + baseCcy;
            RiskFactorKey forCcyKey(RiskFactorKey::KeyType::FXSpot, pairForBase);
            Real forCcySensi = Null<Real>();

            const auto& instWrapper = fxo->instrument();
            auto qlInst = instWrapper->qlInstrument();
            auto qlMult = instWrapper->multiplier();

            if (auto vo = boost::dynamic_pointer_cast<VanillaOption>(qlInst)) {

                // Some engines don't populate delta, e.g. AnalyticEuropeanEngineDeltaGamma, so need a try catch here.
                Real delta = Null<Real>();
                try {
                    delta = vo->delta();
                } catch (...) {
                    DLOG("addAnalyticFxSensitivities: underlying engine for trade id "
                         << tradeId << " does not provide delta so skipping this trade.");
                    continue;
                }

                // Delta may possibly still not populated at this point in some cases but unlikely.
                if (delta == Null<Real>()) {
                    DLOG("addAnalyticFxSensitivities: underlying engine for trade id "
                         << tradeId << " does not populate delta so skipping this trade.");
                    continue;
                }

                if (forCcy != baseCcy) {
                    Real shiftSize = getShiftSize(forCcyKey, *sensitivityData_, simMarket_);
                    forCcySensi = shiftSize * delta * qlMult;
                    TLOG("addAnalyticFxSensitivities: currency "
                         << forCcy << " sensitivity " << forCcySensi << ". (" << pairForBase
                         << " shift, delta, mult) = (" << std::fixed << std::setprecision(9) << shiftSize << ","
                         << delta << "," << qlMult << ").");
                }

                if (domCcy != baseCcy) {
                    string fxPair = forCcy + domCcy;
                    Real spot = simMarket_->fxRate(fxPair)->value();
                    Real altDelta = vo->NPV() - spot * delta;
                    Real shiftSize = getShiftSize(domCcyKey, *sensitivityData_, simMarket_);
                    domCcySensi = shiftSize * altDelta * qlMult;
                    TLOG("addAnalyticFxSensitivities: currency "
                         << domCcy << " sensitivity " << domCcySensi << ". (" << domCcy << " npv, " << fxPair << ", "
                         << pairDomBase << " shift, delta, mult) = (" << std::fixed << std::setprecision(9) << vo->NPV()
                         << "," << spot << "," << shiftSize << "," << altDelta << "," << qlMult << ").");
                }
            }

            // If we have nothing by now continue skip to next trade.
            if (domCcySensi == Null<Real>() && forCcySensi == Null<Real>()) {
                DLOG("addAnalyticFxSensitivities: could not populate an analytical FX delta for trade id "
                     << tradeId << " so skipping this trade.");
                continue;
            }

            // Need to include the premium also. Expect one additional "instrument" for this.
            // If greater than one, abandon attempt to add analytical FX sensitivities.
            // We can leave the original sensitivity if the premium currency is not equal to dom or for currency.
            const auto& addInsts = instWrapper->additionalInstruments();
            const auto& addMults = instWrapper->additionalMultipliers();
            if (addInsts.size() > 1) {
                DLOG("addAnalyticFxSensitivities: more than one additional instrument so skip.");
                continue;
            } else if (addInsts.size() == 1) {
                QL_REQUIRE(addMults.size() == 1, "addAnalyticFxSensitivities: number of additional multipliers "
                                                     << "should always equal the number of additional instruments.");
                if (auto fee = boost::dynamic_pointer_cast<QuantExt::Payment>(addInsts[0])) {
                    string feeCcy = fee->currency().code();
                    if (feeCcy != baseCcy && (feeCcy == domCcy || feeCcy == forCcy)) {

                        Real feeCcySensi;
                        if (domCcy == baseCcy) {
                            Real spot = simMarket_->fxRate(pairForBase)->value();
                            Real feeNpvForCcy = fee->NPV() / spot;
                            Real shiftSize = getShiftSize(forCcyKey, *sensitivityData_, simMarket_);
                            feeCcySensi = addMults[0] * shiftSize * feeNpvForCcy;
                            TLOG("addAnalyticFxSensitivities: fee sensitivity "
                                 << feeCcySensi << ". (feeCcy, baseCcy, feeNpvForCcy, " << pairForBase << ", "
                                 << pairForBase << " shift, mult) = (" << std::fixed << std::setprecision(9) << feeCcy
                                 << "," << baseCcy << "," << feeNpvForCcy << "," << spot << "," << shiftSize << ","
                                 << addMults[0] << ").");
                        } else {
                            Real shiftSize = getShiftSize(domCcyKey, *sensitivityData_, simMarket_);
                            feeCcySensi = addMults[0] * shiftSize * fee->NPV();
                            TLOG("addAnalyticFxSensitivities: fee sensitivity "
                                 << feeCcySensi << ". (feeCcy, baseCcy, feeNpvDomCcy, " << pairDomBase
                                 << " shift, mult) = (" << std::fixed << std::setprecision(9) << feeCcy << ","
                                 << baseCcy << "," << fee->NPV() << "," << shiftSize << "," << addMults[0] << ").");
                        }

                        if (feeCcy == domCcy) {
                            domCcySensi += feeCcySensi;
                        } else {
                            forCcySensi += feeCcySensi;
                        }
                    }
                }
            }

            // Overwrite the sensitivity cube entries.
            auto upFactors = sensiCube_->upFactors().left;
            auto downFactors = sensiCube_->downFactors();
            auto baseNpv = sensiCube_->npvCube()->getT0(tradeIdx);

            // Sensitivity to foreign currency.
            if (forCcySensi != Null<Real>()) {
                auto itu = upFactors.find(forCcyKey);
                if (itu != upFactors.end()) {
                    TLOG("Adding analytic " << forCcy << " up sensitivity to cube.");
                    sensiCube_->npvCube()->remove(tradeIdx, itu->second.index);
                    sensiCube_->npvCube()->set(baseNpv + forCcySensi, tradeIdx, itu->second.index);
                }
                auto itd = downFactors.find(forCcyKey);
                if (itd != downFactors.end()) {
                    TLOG("Adding analytic " << forCcy << " down sensitivity to cube.");
                    sensiCube_->npvCube()->remove(tradeIdx, itd->second.index);
                    sensiCube_->npvCube()->set(baseNpv - forCcySensi, tradeIdx, itd->second.index);
                }
            }

            // Sensitivity to domestic currency.
            if (domCcySensi != Null<Real>()) {
                auto itu = upFactors.find(domCcyKey);
                if (itu != upFactors.end()) {
                    TLOG("Adding analytic " << domCcy << " up sensitivity to cube.");
                    sensiCube_->npvCube()->remove(tradeIdx, itu->second.index);
                    sensiCube_->npvCube()->set(baseNpv + domCcySensi, tradeIdx, itu->second.index);
                }
                auto itd = downFactors.find(domCcyKey);
                if (itd != downFactors.end()) {
                    TLOG("Adding analytic " << domCcy << " down sensitivity to cube.");
                    sensiCube_->npvCube()->remove(tradeIdx, itd->second.index);
                    sensiCube_->npvCube()->set(baseNpv - domCcySensi, tradeIdx, itd->second.index);
                }
            }
        }
    }

    DLOG("addAnalyticFxSensitivities: finished overwriting FX sensitivities with analytic values.");
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
            shiftMult = simMarket->fxRate(keylabel, marketConfiguration)->value();
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
        auto itr = sensiParams.swaptionVolShiftData().find(keylabel);
        QL_REQUIRE(itr != sensiParams.swaptionVolShiftData().end(), "shiftData not found for " << keylabel);
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
            Handle<SwaptionVolatilityStructure> vts = simMarket->swaptionVol(keylabel, marketConfiguration);
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
            Handle<CreditVolCurve> vts = simMarket->cdsVol(name, marketConfiguration);
            // hardcoded 5y term
            Real atmVol = vts->volatility(asof + p_exp, 5.0, Null<Real>(), vts->type());
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
            Handle<DefaultProbabilityTermStructure> ts = simMarket->defaultCurve(name, marketConfiguration)->curve();
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
            Handle<QuantExt::BaseCorrelationTermStructure> ts = simMarket->baseCorrelation(name, marketConfiguration);
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
            shiftMult = 1.0;
            try {
                shiftMult = simMarket->securitySpread(keylabel, marketConfiguration)->value();
            } catch (...) {
                // if there is no spread given for a security, we return 1.0, this is not relevant anyway,
                // because there will be no scenario generated for this risk factor
            }
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
