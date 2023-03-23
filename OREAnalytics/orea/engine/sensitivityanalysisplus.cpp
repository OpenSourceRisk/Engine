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

#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenariosimmarketplus.hpp>
#include <orea/engine/sensitivityanalysisplus.hpp>

#include <orea/cube/jointnpvsensicube.hpp>
#include <orea/cube/sensicube.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

void SensitivityAnalysisPlus::initializeSimMarket(boost::shared_ptr<ore::analytics::ScenarioFactory> scenFact) {

    LOG("Initialise sim market for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                           << ")");
    simMarket_ = boost::make_shared<ScenarioSimMarketPlus>(
        market_, simMarketData_, marketConfiguration_,
        curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
        todaysMarketParams_ ? *todaysMarketParams_ : ore::data::TodaysMarketParameters(), continueOnError_,
        sensitivityData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_);
    LOG("Sim market initialised for sensitivity analysis");

    LOG("Create scenario factory for sensitivity analysis");
    boost::shared_ptr<ScenarioFactory> scenarioFactory;
    if (scenFact) {
        scenarioFactory = scenFact;
    } else {
        scenarioFactory = boost::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario());
        LOG("DeltaScenario factory created for sensitivity analysis");
    }

    LOG("Create scenario generator for sensitivity analysis (continueOnError=" << std::boolalpha << continueOnError_
                                                                               << ")");
    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(
        sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_, scenarioFactory, overrideTenors_,
        continueOnError_, simMarket_->baseScenarioAbsolute());
    LOG("Scenario generator created for sensitivity analysis");

    // Set simulation market's scenario generator
    simMarket_->scenarioGenerator() = scenarioGenerator_;
}

void SensitivityAnalysisPlus::initialize(boost::shared_ptr<ore::analytics::NPVSensiCube>& cube) {
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

void SensitivityAnalysisPlus::initializeCube(boost::shared_ptr<NPVSensiCube>& cube) const {
    cube = boost::make_shared<ore::analytics::DoublePrecisionSensiCube>(portfolio_->ids(), asof_,
                                                                        scenarioGenerator_->samples());
}

void SensitivityAnalysisPlus::resetPortfolio(const boost::shared_ptr<EngineFactory>& factory) {
    LOG("Resetting portfolio before running sensitivity analysis");
    portfolio_->reset();
    portfolio_->build(factory, "sensi analysis");
}

void SensitivityAnalysisPlus::addAnalyticFxSensitivities() {

    if (!analyticFxSensis_) {
        return;
    }

    ore::analytics::SensitivityAnalysis::addAnalyticFxSensitivities();

    // Logic for selected ORE+ FX trades would go here.
}

void SensitivityAnalysisPlus::generateSensitivities(boost::shared_ptr<NPVSensiCube> cube) {

    QL_REQUIRE(useSingleThreadedEngine_ || cube == nullptr,
               "SensitivityAnalysis::generateSensitivities(): when using multi-threaded engine no NPVSensiCube should "
               "be specified, it is built automatically");

    QL_REQUIRE(useSingleThreadedEngine_ || !nonShiftedBaseCurrencyConversion_,
               "SensitivityAnalysis::generateSensitivities(): multi-threaded engine does not support non-shifted base "
               "ccy conversion currently. This requires a a small code extension. Contact Dev.");

    QL_REQUIRE(useSingleThreadedEngine_ || recalibrateModels_,
               "SensitivityAnalysis::generateSensitivities(): multi-threaded engine does not support recalibrateModels "
               "= false. This requires a small code extension. Contact Dev.");

    if (useSingleThreadedEngine_) {
        ore::analytics::SensitivityAnalysis::generateSensitivities(cube);
        return;
    }

    // handle request to use multi-threaded engine

    LOG("SensitivitiyAnalysis::generateSensitivities(): use multi-threaded engine to generate sensi cube.");

    market_ =
        boost::make_shared<ore::data::TodaysMarket>(asof_, todaysMarketParams_, loader_, curveConfigs_, true, true,
                                                    false, referenceData_, false, iborFallbackConfig_, false);

    simMarket_ = boost::make_shared<ScenarioSimMarketPlus>(
        market_, simMarketData_, marketConfiguration_,
        curveConfigs_ ? *curveConfigs_ : ore::data::CurveConfigurations(),
        todaysMarketParams_ ? *todaysMarketParams_ : ore::data::TodaysMarketParameters(), continueOnError_,
        sensitivityData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_);

    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(
        sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_,
        boost::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario()), overrideTenors_, continueOnError_,
        simMarket_->baseScenarioAbsolute());

    simMarket_->scenarioGenerator() = scenarioGenerator_;

    auto ed = boost::make_shared<EngineData>(*engineData_);
    ed->globalParameters()["RunType"] =
        std::string("Sensitivity") + (sensitivityData_->computeGamma() ? "DeltaGamma" : "Delta");

    MultiThreadedValuationEngine engine(
        nThreads_, asof_, boost::make_shared<ore::analytics::DateGrid>(), scenarioGenerator_->numScenarios(), loader_,
        scenarioGenerator_, ed, curveConfigs_, todaysMarketParams_, marketConfiguration_, simMarketData_,
        sensitivityData_->useSpreadedTermStructures(), false, boost::make_shared<ore::analytics::ScenarioFilter>(),
        referenceData_, iborFallbackConfig_, true, true,
        [](const QuantLib::Date& asof, const std::set<std::string>& ids, const std::vector<QuantLib::Date>&,
           const QuantLib::Size samples) {
            return boost::make_shared<ore::analytics::DoublePrecisionSensiCube>(ids, asof, samples);
        },
        {}, {}, context_);
    for (auto const& i : this->progressIndicators())
        engine.registerProgressIndicator(i);

    auto baseCcy = simMarketData_->baseCcy();
    engine.buildCube(
        portfolio_,
        [&baseCcy]() -> std::vector<boost::shared_ptr<ValuationCalculator>> {
            return {boost::make_shared<NPVCalculator>(baseCcy)};
        },
        {}, true, dryRun_);
    std::vector<boost::shared_ptr<NPVSensiCube>> miniCubes;
    for (auto const& c : engine.outputCubes()) {
        miniCubes.push_back(boost::dynamic_pointer_cast<NPVSensiCube>(c));
        QL_REQUIRE(miniCubes.back() != nullptr,
                   "SensitivityAnalysis::generateSensitivities(): internal error, could not cast to NPVSensiCube.");
    }
    cube = boost::make_shared<JointNPVSensiCube>(miniCubes, portfolio_->ids());

    sensiCube_ =
        boost::make_shared<SensitivityCube>(cube, scenarioGenerator_->scenarioDescriptions(),
                                            scenarioGenerator_->shiftSizes(), sensitivityData_->twoSidedDeltas());

    addAnalyticFxSensitivities();

    initialized_ = true;
}

} // namespace analytics
} // namespace ore
