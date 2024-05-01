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

#include <orea/cube/jointnpvsensicube.hpp>
#include <orea/cube/sensicube.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/deltascenariofactory.hpp>

#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

SensitivityAnalysis::SensitivityAnalysis(
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const QuantLib::ext::shared_ptr<ore::data::Market>& market,
    const string& marketConfiguration, const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensitivityData, const bool recalibrateModels,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
    const bool nonShiftedBaseCurrencyConversion, const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
    const IborFallbackConfig& iborFallbackConfig, const bool continueOnError, bool dryRun)
    : market_(market), marketConfiguration_(marketConfiguration), asof_(market ? market->asofDate() : Date()),
      simMarketData_(simMarketData), sensitivityData_(sensitivityData), recalibrateModels_(recalibrateModels),
      curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams), overrideTenors_(false),
      nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), continueOnError_(continueOnError), engineData_(engineData),
      portfolio_(portfolio), dryRun_(dryRun), useSingleThreadedEngine_(true) {}

SensitivityAnalysis::SensitivityAnalysis(
    const Size nThreads, const Date& asof, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const string& marketConfiguration,
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData, const bool recalibrateModels,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
    const bool nonShiftedBaseCurrencyConversion, const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
    const IborFallbackConfig& iborFallbackConfig, const bool continueOnError, bool dryRun, const std::string& context)
    : marketConfiguration_(marketConfiguration), asof_(asof), simMarketData_(simMarketData),
      sensitivityData_(sensitivityData), recalibrateModels_(recalibrateModels), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), overrideTenors_(false),
      nonShiftedBaseCurrencyConversion_(nonShiftedBaseCurrencyConversion), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), continueOnError_(continueOnError), engineData_(engineData),
      portfolio_(portfolio), dryRun_(dryRun), useSingleThreadedEngine_(false), nThreads_(nThreads), loader_(loader),
      context_(context) {}

namespace {
std::vector<std::pair<QuantLib::ext::shared_ptr<Portfolio>, QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>>>
splitPortfolioByScenarioGenerators(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, std::vector<std::string> ids,
    const std::vector<QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>>& scenarioGenerators) {
    DLOG("Splitting portfolio by scenario generators for sensitivity run(s)");
    std::vector<std::pair<QuantLib::ext::shared_ptr<Portfolio>, QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>>> result;
    for (auto const& gen : scenarioGenerators) {
        result.push_back(std::make_pair(QuantLib::ext::make_shared<Portfolio>(), gen));
    }
    for (auto const& [id, t] : portfolio->trades()) {
        if (auto f = std::find(ids.begin(), ids.end(), t->sensitivityTemplate()); f != ids.end()) {
            result[std::distance(ids.begin(), f)].first->add(t);
            TLOG("adding trade " << std::setw(35) << t->id() << " to sensi run for id " << std::setw(20)
                                 << t->sensitivityTemplate() << ", trade sensi-template: " << std::setw(20)
                                 << t->sensitivityTemplate());
        } else {
            result.front().first->add(t);
            TLOG("adding trade " << std::setw(35) << t->id() << " to sensi run for id " << std::setw(20) << "<default>"
                                 << ", trade sensi-template: " << std::setw(20) << t->sensitivityTemplate());
        }
    }
    return result;
}
} // namespace

void SensitivityAnalysis::generateSensitivities() {

    LOG("Sensitivity analysis started...");

    QL_REQUIRE(useSingleThreadedEngine_ || !nonShiftedBaseCurrencyConversion_,
               "SensitivityAnalysis::generateSensitivities(): multi-threaded engine does not support non-shifted base "
               "ccy conversion currently. This requires a small code extension. Contact Dev.");

    // collect the sensi template ids that are used in the sensitivity config

    std::set<std::string> sensiTemplateIdsFromSensiConfig = getShiftSpecKeys(*sensitivityData_);

    // collect the sensi template ids that are relevant for the portfolio

    std::set<std::string> sensiTemplateIdsFromPortfolio;
    for (auto const& [_, t] : portfolio_->trades())
        sensiTemplateIdsFromPortfolio.insert(t->sensitivityTemplate());

    // take the intersection of pricing engine ids and add en empty id for the default config as the first element

    std::vector<std::string> sensiTemplateIds{std::string()};
    std::set_intersection(sensiTemplateIdsFromSensiConfig.begin(), sensiTemplateIdsFromSensiConfig.end(),
                          sensiTemplateIdsFromPortfolio.begin(), sensiTemplateIdsFromPortfolio.end(),
                          std::back_inserter(sensiTemplateIds));

    LOG("Need to process "
        << sensiTemplateIds.size() << " sensi scenario sets resulting from " << sensiTemplateIdsFromSensiConfig.size()
        << " sensi templates in sensi config (different from default config) and "
        << sensiTemplateIdsFromPortfolio.size()
        << " sensi templates in portfolio (including default config, if configured in pe config for a trade)");

    if (useSingleThreadedEngine_) {

        // handle single threaded sensi analysis

        LOG("SensitivitiyAnalysis::generateSensitivities(): use single-threaded engine to generate sensi cube. Using "
            "configuration '"
            << marketConfiguration_ << "'");

        simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            market_, simMarketData_, marketConfiguration_,
            curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
            todaysMarketParams_ ? *todaysMarketParams_ : ore::data::TodaysMarketParameters(), continueOnError_,
            sensitivityData_->useSpreadedTermStructures(), continueOnError_, overrideTenors_, iborFallbackConfig_);

        std::vector<QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>> scenarioGenerators(sensiTemplateIds.size());
        for (Size i = 0; i < sensiTemplateIds.size(); ++i) {
            scenarioGenerators[i] = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
                sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_,
                QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario()), overrideTenors_,
                sensiTemplateIds[i], continueOnError_, simMarket_->baseScenarioAbsolute());
        }
        scenarioGenerator_ = scenarioGenerators.front();

        map<MarketContext, string> configurations;
        configurations[MarketContext::pricing] = marketConfiguration_;
        auto ed = QuantLib::ext::make_shared<EngineData>(*engineData_);
        ed->globalParameters()["RunType"] =
            std::string("Sensitivity") + (sensitivityData_->computeGamma() ? "DeltaGamma" : "Delta");

        QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>("1,0W", NullCalendar());
        vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
        if (nonShiftedBaseCurrencyConversion_)
            // use "original" FX rates to convert sensi to base currency
            calculators.push_back(QuantLib::ext::make_shared<NPVCalculatorFXT0>(simMarketData_->baseCcy(), market_));
        else
            // use the scenario FX rate when converting sensi to base currency
            calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData_->baseCcy()));

        sensiCubes_.clear();
        for (auto const& [pf, scenGen] :
             splitPortfolioByScenarioGenerators(portfolio_, sensiTemplateIds, scenarioGenerators)) {
            if (pf->trades().empty())
                continue;
            LOG("Run Sensitivity Scenarios for " << pf->size() << " out of " << portfolio_->size() << " trades.");
            QuantLib::ext::shared_ptr<NPVSensiCube> cube =
                QuantLib::ext::make_shared<DoublePrecisionSensiCube>(pf->ids(), asof_, scenGen->samples());
            simMarket_->scenarioGenerator() = scenGen;
            auto factory =
                QuantLib::ext::make_shared<EngineFactory>(ed, simMarket_, configurations, referenceData_, iborFallbackConfig_);
            pf->reset();
            pf->build(factory, "sensi analysis");
            if (recalibrateModels_)
                modelBuilders_ = factory->modelBuilders();
            else
                modelBuilders_.clear();
            ValuationEngine engine(asof_, dg, simMarket_, modelBuilders_);
            for (auto const& i : this->progressIndicators())
                engine.registerProgressIndicator(i);
            engine.buildCube(pf, cube, calculators, true, nullptr, nullptr, {}, dryRun_);

            sensiCubes_.push_back(QuantLib::ext::make_shared<SensitivityCube>(cube, scenGen->scenarioDescriptions(),
                                                                      scenarioGenerator_->shiftSizes(),
                                                                      scenGen->shiftSizes(), scenGen->shiftSchemes()));
        }
    } else {

        // handle request to use multi-threaded engine

        LOG("SensitivitiyAnalysis::generateSensitivities(): use multi-threaded engine to generate sensi cube. Using "
            "configuration '"
            << marketConfiguration_ << "'");

        market_ =
            QuantLib::ext::make_shared<ore::data::TodaysMarket>(asof_, todaysMarketParams_, loader_, curveConfigs_, true, true,
                                                        false, referenceData_, false, iborFallbackConfig_, false);

        simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            market_, simMarketData_, marketConfiguration_,
            curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
            todaysMarketParams_ ? *todaysMarketParams_ : ore::data::TodaysMarketParameters(), continueOnError_,
            sensitivityData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_);

        std::vector<QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>> scenarioGenerators(sensiTemplateIds.size());
        for (Size i = 0; i < sensiTemplateIds.size(); ++i) {
            scenarioGenerators[i] = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
                sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_,
                QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario()), overrideTenors_,
                sensiTemplateIds[i], continueOnError_, simMarket_->baseScenarioAbsolute());
        }
        scenarioGenerator_ = scenarioGenerators.front();

        auto ed = QuantLib::ext::make_shared<EngineData>(*engineData_);
        ed->globalParameters()["RunType"] =
            std::string("Sensitivity") + (sensitivityData_->computeGamma() ? "DeltaGamma" : "Delta");

        sensiCubes_.clear();
        for (auto const& [pf, scenGen] :
             splitPortfolioByScenarioGenerators(portfolio_, sensiTemplateIds, scenarioGenerators)) {
            if (pf->trades().empty())
                continue;

            MultiThreadedValuationEngine engine(
                nThreads_, asof_, QuantLib::ext::make_shared<ore::analytics::DateGrid>(), scenGen->numScenarios(), loader_,
                scenGen, ed, curveConfigs_, todaysMarketParams_, marketConfiguration_, simMarketData_,
                sensitivityData_->useSpreadedTermStructures(), false,
                QuantLib::ext::make_shared<ore::analytics::ScenarioFilter>(), referenceData_, iborFallbackConfig_, true, true,
                recalibrateModels_,
                [](const QuantLib::Date& asof, const std::set<std::string>& ids, const std::vector<QuantLib::Date>&,
                   const QuantLib::Size samples) {
                    return QuantLib::ext::make_shared<ore::analytics::DoublePrecisionSensiCube>(ids, asof, samples);
                },
                {}, {}, context_);
            for (auto const& i : this->progressIndicators())
                engine.registerProgressIndicator(i);

            auto baseCcy = simMarketData_->baseCcy();
            engine.buildCube(
                pf,
                [&baseCcy]() -> std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> {
                    return {QuantLib::ext::make_shared<NPVCalculator>(baseCcy)};
                },
                {}, true, dryRun_);
            std::vector<QuantLib::ext::shared_ptr<NPVSensiCube>> miniCubes;
            for (auto const& c : engine.outputCubes()) {
                miniCubes.push_back(QuantLib::ext::dynamic_pointer_cast<NPVSensiCube>(c));
                QL_REQUIRE(
                    miniCubes.back() != nullptr,
                    "SensitivityAnalysis::generateSensitivities(): internal error, could not cast to NPVSensiCube.");
            }
            auto cube = QuantLib::ext::make_shared<JointNPVSensiCube>(miniCubes, pf->ids());

            sensiCubes_.push_back(QuantLib::ext::make_shared<SensitivityCube>(cube, scenGen->scenarioDescriptions(),
                                                                      scenarioGenerator_->shiftSizes(),
                                                                      scenGen->shiftSizes(), scenGen->shiftSchemes()));
        }
    }

    simMarket_->scenarioGenerator() = scenarioGenerator_;

    LOG("Sensitivity analysis completed");
}

Real getShiftSize(const RiskFactorKey& key, const SensitivityScenarioData& sensiParams,
                  const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket, const string& marketConfiguration) {

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
        if (itr->second.shiftType == ShiftType::Relative) {
            shiftMult = simMarket->fxRate(keylabel, marketConfiguration)->value();
        }
    } break;
    case RiskFactorKey::KeyType::EquitySpot: {
        auto itr = sensiParams.equityShiftData().find(keylabel);
        QL_REQUIRE(itr != sensiParams.equityShiftData().end(), "shiftData not found for " << keylabel);
        shiftSize = itr->second.shiftSize;
        if (itr->second.shiftType == ShiftType::Relative) {
            shiftMult = simMarket->equitySpot(keylabel, marketConfiguration)->value();
        }
    } break;
    case RiskFactorKey::KeyType::DiscountCurve: {
        string ccy = keylabel;
        auto itr = sensiParams.discountCurveShiftData().find(ccy);
        QL_REQUIRE(itr != sensiParams.discountCurveShiftData().end(), "shiftData not found for " << ccy);
        shiftSize = itr->second->shiftSize;
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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

        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (itr->second->shiftType == ShiftType::Relative) {
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
        if (it->second->shiftType == ShiftType::Relative) {
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
        if (it->second.shiftType == ShiftType::Relative) {
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
        if (itr->second.shiftType == ShiftType::Relative) {
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
