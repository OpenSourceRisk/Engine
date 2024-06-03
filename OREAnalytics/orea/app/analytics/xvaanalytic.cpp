/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/aggregation/dimflatcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/cptycalculator.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/multistatenpvcalculator.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/******************************************************************************
 * XVA Analytic: EXPOSURE, CVA, DVA, FVA, KVA, COLVA, COLLATERALFLOOR, DIM, MVA
 ******************************************************************************/

void XvaAnalyticImpl::setUpConfigurations() {
    LOG("XvaAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->exposureSimMarketParams();
    analytic()->configurations().scenarioGeneratorData = inputs_->scenarioGeneratorData();
    analytic()->configurations().crossAssetModelData = inputs_->crossAssetModelData();
}

void XvaAnalyticImpl::checkConfigurations(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
    // find the unique nettingset keys in portfolio
    std::map<std::string, std::string> nettingSetMap = portfolio->nettingSetMap();
    std::vector<std::string> nettingSetKeys;
    for (std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it)
        nettingSetKeys.push_back(it->second);
    // unique nettingset keys
    sort(nettingSetKeys.begin(), nettingSetKeys.end());
    nettingSetKeys.erase(unique(nettingSetKeys.begin(), nettingSetKeys.end()), nettingSetKeys.end());
    // controls on calcType and grid type, if netting-set has an active CSA in place
    for (auto const& key : nettingSetKeys) {
        LOG("For netting-set " << key << "CSA flag is " << inputs_->nettingSetManager()->get(key)->activeCsaFlag());
        if (inputs_->nettingSetManager()->get(key)->activeCsaFlag()) {
            string calculationType = inputs_->collateralCalculationType();
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()) {
                QL_REQUIRE(calculationType == "NoLag",
                           "For nettingSetID " << key
                                               << ", CSA is active and a close-out grid is configured in the "
                                                  "simulation.xml. Therefore, calculation type "
                                               << calculationType << " is not admissable. It must be set to NoLag!");
                LOG("For netting-set " << key << ", calculation type is " << calculationType);
            } else {
                QL_REQUIRE(
                    calculationType != "NoLag",
                    "For nettingSetID "
                        << key
                        << ", CSA is active and a close-out grid is not configured in the simulation.xml. Therefore, "
                           "calculation type "
                        << calculationType
                        << " is not admissable. It must be set to either Symmetric or AsymmerticCVA or AsymmetricDVA!");
                LOG("For netting-set " << key << ", calculation type is " << calculationType);
            }
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag() &&
                analytic()->configurations().scenarioGeneratorData->closeOutLag() != 0 * Days) {
                Period mpor_simulation = analytic()->configurations().scenarioGeneratorData->closeOutLag();
                Period mpor_netting = inputs_->nettingSetManager()->get(key)->csaDetails()->marginPeriodOfRisk();
                if (mpor_simulation != mpor_netting)
                    StructuredAnalyticsWarningMessage(
                        "XvaAnalytic", "Inconsistent MPoR period",
                        "For netting set " + key + ", close-out lag is not consistent with the netting-set's mpor ")
                        .log();
            }
        }
    }
}

QuantLib::ext::shared_ptr<EngineFactory> XvaAnalyticImpl::engineFactory() {
    LOG("XvaAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy =
        QuantLib::ext::make_shared<EngineData>(*inputs_->simulationPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    // configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders;
    std::vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders;

    if (runSimulation_) {
        // link to the sim market here
        QL_REQUIRE(simMarket_, "Simulaton market not set");
        engineFactory_ = QuantLib::ext::make_shared<EngineFactory>(
            edCopy, simMarket_, configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig());
    } else {
        // we just link to today's market if simulation is not required
        engineFactory_ = QuantLib::ext::make_shared<EngineFactory>(
            edCopy, analytic()->market(), configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig());
    }
    return engineFactory_;
}

void XvaAnalyticImpl::buildScenarioSimMarket() {

    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams,
        QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
        *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), false, true, false,
        *inputs_->iborFallbackConfig(), false, offsetScenario_);

    if (offsetScenario_ == nullptr) {
        simMarketCalibration_ = simMarket_;
        offsetSimMarket_ = simMarket_;
    } else {
        // set useSpreadedTermstructure to true, yield better results in calibration of the CAM
        simMarketCalibration_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), analytic()->configurations().simMarketParams,
            QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
            *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), true, true, false,
            *inputs_->iborFallbackConfig(), false, offsetScenario_);

        // Create a third market used for AMC and Postprocessor, holds a larger simmarket, e.g. default curves
        offsetSimMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), offsetSimMarketParams_, QuantLib::ext::make_shared<FixingManager>(inputs_->asof()),
            configuration, *inputs_->curveConfigs().get(), *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), true, true, false, *inputs_->iborFallbackConfig(), false, offsetScenario_);

        TLOG("XvaAnalytic: Offset Scenario used in building SimMarket");
        TLOG("XvaAnalytic: Offset scenario is absolute = " << offsetScenario_->isAbsolute());
        TLOG("RfKey,OffsetScenarioValue");
        for (const auto& key : offsetScenario_->keys()) {
            TLOG(key << " : " << offsetScenario_->get(key));
        }
    }

    TLOG("XvaAnalytic:Finished building Scenario SimMarket");
    TLOG("RfKey,BaseScenarioValue,BaseScenarioAbsValue");
    for (const auto& key : simMarket_->baseScenario()->keys()) {
        TLOG(key << "," << simMarket_->baseScenario()->get(key) << "," << simMarket_->baseScenarioAbsolute()->get(key));
    }
    TLOG("XvaAnalytic: Finished building Scenario SimMarket for model calibration (useSpreadedTermStructure)");
    TLOG("RfKey,BaseScenarioValue,BaseScenarioAbsValue");
    for (const auto& key : simMarketCalibration_->baseScenario()->keys()) {
        TLOG(key << "," << simMarketCalibration_->baseScenario()->get(key) << ","
                 << simMarketCalibration_->baseScenarioAbsolute()->get(key));
    }
}

void XvaAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError) {
    if (!model_)
        buildCrossAssetModel(continueOnCalibrationError);
    ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    string config = inputs_->marketConfig("simulation");
    auto market = offsetScenario_ == nullptr ? analytic()->market() : simMarketCalibration_;
    scenarioGenerator_ =
        sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), market, config);
    QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator");
    samples_ = analytic()->configurations().scenarioGeneratorData->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));

    if (inputs_->writeScenarios()) {
        auto report = QuantLib::ext::make_shared<InMemoryReport>();
        analytic()->reports()["XVA"]["scenario"] = report;
        scenarioGenerator_ = QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void XvaAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = " << std::boolalpha << continueOnCalibrationError
                                                                     << ")");
    ext::shared_ptr<Market> market = offsetScenario_ != nullptr ? simMarketCalibration_ : analytic()->market();
    QL_REQUIRE(market != nullptr, "Internal error, buildCrossAssetModel needs to be called after the market is built.");
    CrossAssetModelBuilder modelBuilder(
        market, analytic()->configurations().crossAssetModelData, inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "",
        inputs_->salvageCorrelationMatrix() ? SalvagingAlgorithm::Spectral : SalvagingAlgorithm::None,
        "xva cam building");
    model_ = *modelBuilder.model();
}

void XvaAnalyticImpl::initCubeDepth() {
    if (cubeDepth_ == 0) {
        LOG("XVA: Set cube depth");
        cubeDepth_ = cubeInterpreter_->requiredNpvCubeDepth();
        LOG("XVA: Cube depth set to: " << cubeDepth_);
    }
}

void XvaAnalyticImpl::initCube(QuantLib::ext::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids,
                               Size cubeDepth) {

    LOG("Init cube with depth " << cubeDepth);

    for (Size i = 0; i < grid_->valuationDates().size(); ++i)
        DLOG("initCube: grid[" << i << "]=" << io::iso_date(grid_->valuationDates()[i]));

    if (cubeDepth == 1)
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                       samples_, 0.0f);
    else
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                        samples_, cubeDepth, 0.0f);
}

void XvaAnalyticImpl::initClassicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA: initClassicRun");

    initCubeDepth();

    // May have been set already
    if (scenarioData_.empty()) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_.linkTo(
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_));
        simMarket_->aggregationScenarioData() = *scenarioData_;
    }

    // We can skip the cube initialization if the mt val engine is used, since it builds its own cubes
    if (inputs_->nThreads() == 1) {
        if (portfolio->size() > 0)
            initCube(cube_, portfolio->ids(), cubeDepth_);
        // not required by any calculators in ore at the moment
        nettingSetCube_ = nullptr;
        // Init counterparty cube for the storage of survival probabilities
        if (inputs_->storeSurvivalProbabilities()) {
            // Use full list of counterparties, not just those in the sub-portflio
            auto counterparties = inputs_->portfolio()->counterparties();
            counterparties.insert(inputs_->dvaName());
            initCube(cptyCube_, counterparties, 1);
        } else {
            cptyCube_ = nullptr;
        }
    }

    LOG("XVA: initClassicRun completed");
}

QuantLib::ext::shared_ptr<Portfolio>
XvaAnalyticImpl::classicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
    LOG("XVA: classicRun");

    Size n = portfolio->size();
    // Create a new empty portfolio, fill it and link it to the simulation market
    // We don't use Analytic::buildPortfolio() here because we are possibly dealing with a sub-portfolio only.
    LOG("XVA: Build classic portfolio of size " << n << " linked to the simulation market");
    const string msg = "XVA: Build Portfolio";
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    classicPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());
    portfolio->reset();
    for (const auto& [tradeId, trade] : portfolio->trades())
        classicPortfolio_->add(trade);
    QL_REQUIRE(analytic()->market(), "today's market not set");
    QuantLib::ext::shared_ptr<EngineFactory> factory = engineFactory();
    classicPortfolio_->build(factory, "analytic/" + label());
    Date maturityDate = inputs_->asof();
    if (inputs_->portfolioFilterDate() != Null<Date>())
        maturityDate = inputs_->portfolioFilterDate();
    LOG("Filter trades that expire before " << maturityDate);
    classicPortfolio_->removeMatured(maturityDate);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    // Allocate cubes for the sub-portfolio we are processing here
    initClassicRun(classicPortfolio_);

    // This is where the valuation work is done
    buildClassicCube(classicPortfolio_);

    LOG("XVA: classicRun completed");

    return classicPortfolio_;
}

void XvaAnalyticImpl::buildClassicCube(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA::buildCube");

    // set up valuation calculator factory
    auto calculators = [this]() {
        vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
        if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()) {
            QuantLib::ext::shared_ptr<NPVCalculator> npvCalc =
                QuantLib::ext::make_shared<NPVCalculator>(inputs_->exposureBaseCurrency());
            calculators.push_back(QuantLib::ext::make_shared<MPORCalculator>(
                npvCalc, cubeInterpreter_->defaultDateNpvIndex(), cubeInterpreter_->closeOutDateNpvIndex()));
        } else
            calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(inputs_->exposureBaseCurrency()));
        if (inputs_->storeFlows())
            calculators.push_back(QuantLib::ext::make_shared<CashflowCalculator>(
                inputs_->exposureBaseCurrency(), inputs_->asof(), grid_, cubeInterpreter_->mporFlowsIndex()));
        if (inputs_->storeCreditStateNPVs() > 0) {
            calculators.push_back(QuantLib::ext::make_shared<MultiStateNPVCalculator>(
                inputs_->exposureBaseCurrency(), cubeInterpreter_->creditStateNPVsIndex(),
                inputs_->storeCreditStateNPVs()));
        }
        return calculators;
    };

    // set up cpty calculator factory

    auto cptyCalculators = [this]() {
        vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>> cptyCalculators;
        if (inputs_->storeSurvivalProbabilities()) {
            string configuration = inputs_->marketConfig("simulation");
            cptyCalculators.push_back(QuantLib::ext::make_shared<SurvivalProbabilityCalculator>(configuration));
        }
        return cptyCalculators;
    };

    // log message

    ostringstream o;
    o << "XVA: Build Cube " << portfolio->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
    CONSOLEW(o.str());
    LOG(o.str());

    // set up progress indicators

    auto progressBar = QuantLib::ext::make_shared<SimpleProgressBar>(o.str(), ConsoleLog::instance().width(),
                                                                     ConsoleLog::instance().progressBarWidth());
    auto progressLog = QuantLib::ext::make_shared<ProgressLog>("XVA: Building cube", 100, oreSeverity::notice);

    if (inputs_->nThreads() == 1) {

        // single-threaded engine run

        ValuationEngine engine(inputs_->asof(), grid_, simMarket_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.buildCube(portfolio, cube_, calculators(),
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), nettingSetCube_,
                         cptyCube_, cptyCalculators());
    } else {

        // multi-threaded engine run

        /* TODO we assume no netting output cube is needed. Currently there are no valuation calculators in ore that
         * require this cube. */

        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples, cubeDepth_,
                                                                                0.0f);
        };

        std::function<QuantLib::ext::shared_ptr<NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                         const std::vector<QuantLib::Date>&, const QuantLib::Size)>
            cptyCubeFactory;
        if (inputs_->storeSurvivalProbabilities()) {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
                return QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            };
        } else {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> { return nullptr; };
        }

        MultiThreadedValuationEngine engine(
            inputs_->nThreads(), inputs_->asof(), grid_, samples_, analytic()->loader(), scenarioGenerator_,
            inputs_->simulationPricingEngine(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, inputs_->marketConfig("simulation"),
            analytic()->configurations().simMarketParams, false, false, QuantLib::ext::make_shared<ScenarioFilter>(),
            inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, false, false, cubeFactory, {},
            cptyCubeFactory, "xva-simulation", offsetScenario_);

        engine.setAggregationScenarioData(*scenarioData_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, cptyCalculators,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate());

        cube_ = QuantLib::ext::make_shared<JointNPVCube>(engine.outputCubes(), portfolio->ids());

        if (inputs_->storeSurvivalProbabilities())
            cptyCube_ = QuantLib::ext::make_shared<JointNPVCube>(
                engine.outputCptyCubes(), portfolio->counterparties(), false,
                [](Real a, Real x) { return std::max(a, x); }, 0.0);
    }

    CONSOLE("OK");

    LOG("XVA::buildCube done");

    Settings::instance().evaluationDate() = inputs_->asof();
}

QuantLib::ext::shared_ptr<EngineFactory>
XvaAnalyticImpl::amcEngineFactory(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                  const std::vector<Date>& grid) {
    LOG("XvaAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*inputs_->amcPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    ext::shared_ptr<ore::data::Market> market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;
    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, market, configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
        EngineBuilderFactory::instance().generateAmcEngineBuilders(cam, grid), true);
    return factory;
}

void XvaAnalyticImpl::buildAmcPortfolio() {
    LOG("XVA: buildAmcPortfolio");
    const string msg = "XVA: Build AMC portfolio";
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();

    LOG("buildAmcPortfolio: Check sim dates");
    std::vector<Date> simDates = analytic()->configurations().scenarioGeneratorData->withCloseOutLag() &&
                                         !analytic()->configurations().scenarioGeneratorData->withMporStickyDate()
                                     ? analytic()->configurations().scenarioGeneratorData->getGrid()->dates()
                                     : analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();

    LOG("buildAmcPortfolio: Register additional engine builders");
    auto factory = amcEngineFactory(model_, simDates);

    LOG("buildAmcPortfolio: Load Portfolio");
    QuantLib::ext::shared_ptr<Portfolio> portfolio = inputs_->portfolio();

    LOG("Build Portfolio with AMC Engine factory and select amc-enabled trades")
    amcPortfolio_ = QuantLib::ext::make_shared<Portfolio>();
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        if (inputs_->amcTradeTypes().find(trade->tradeType()) != inputs_->amcTradeTypes().end()) {
            try {
                trade->reset();
                trade->build(factory);
                amcPortfolio_->add(trade);
                DLOG("trade " << tradeId << " is added to amc portfolio");
            } catch (const std::exception& e) {
                StructuredTradeErrorMessage(trade, "Error building trade for AMC simulation", e.what()).log();
            }
        }
    }
    LOG("AMC portfolio built, size is " << amcPortfolio_->size());

    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    LOG("XVA: buildAmcPortfolio completed");
}

void XvaAnalyticImpl::amcRun(bool doClassicRun) {

    LOG("XVA: amcRun");

    if (scenarioData_.empty()) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_.linkTo(
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_));
        simMarket_->aggregationScenarioData() = *scenarioData_;
    }

    initCubeDepth();

    std::string message = "XVA: Build AMC Cube " + std::to_string(amcPortfolio_->size()) + " x " +
                          std::to_string(grid_->valuationDates().size()) + " x " + std::to_string(samples_) + "... ";
    auto progressBar = QuantLib::ext::make_shared<SimpleProgressBar>(message, ConsoleLog::instance().width(),
                                                                     ConsoleLog::instance().progressBarWidth());
    auto progressLog = QuantLib::ext::make_shared<ProgressLog>("XVA: Building AMC Cube...", 100, oreSeverity::notice);

    if (inputs_->nThreads() == 1) {
        initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);
        ext::shared_ptr<ore::data::Market> market =
            offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

        AMCValuationEngine amcEngine(model_, inputs_->scenarioGeneratorData(), market,
                                     inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
                                     inputs_->exposureSimMarketParams()->additionalScenarioDataCcys(),
                                     inputs_->exposureSimMarketParams()->additionalScenarioDataNumberOfCreditStates());
        amcEngine.registerProgressIndicator(progressBar);
        amcEngine.registerProgressIndicator(progressLog);
        if (!scenarioData_.empty())
            amcEngine.aggregationScenarioData() = *scenarioData_;
        amcEngine.buildCube(amcPortfolio_, amcCube_);
    } else {
        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples, cubeDepth_,
                                                                                0.0f);
        };

        auto simMarketParams =
            offsetScenario_ == nullptr ? analytic()->configurations().simMarketParams : offsetSimMarketParams_;

        AMCValuationEngine amcEngine(
            inputs_->nThreads(), inputs_->asof(), samples_, analytic()->loader(), inputs_->scenarioGeneratorData(),
            inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
            inputs_->exposureSimMarketParams()->additionalScenarioDataCcys(),
            inputs_->exposureSimMarketParams()->additionalScenarioDataNumberOfCreditStates(),
            inputs_->crossAssetModelData(), inputs_->amcPricingEngine(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, inputs_->marketConfig("lgmcalibration"),
            inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
            inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
            inputs_->marketConfig("simulation"), inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true,
            cubeFactory, offsetScenario_, simMarketParams);

        amcEngine.registerProgressIndicator(progressBar);
        amcEngine.registerProgressIndicator(progressLog);
        if (!scenarioData_.empty())
            amcEngine.aggregationScenarioData() = *scenarioData_;
        amcEngine.buildCube(amcPortfolio_);
        amcCube_ = QuantLib::ext::make_shared<JointNPVCube>(amcEngine.outputCubes());
    }

    CONSOLE("OK");

    LOG("XVA: amcRun completed");
}

void XvaAnalyticImpl::runPostProcessor() {
    QuantLib::ext::shared_ptr<NettingSetManager> netting = inputs_->nettingSetManager();
    QuantLib::ext::shared_ptr<CollateralBalances> balances = inputs_->collateralBalances();
    map<string, bool> analytics;
    analytics["exerciseNextBreak"] = inputs_->exerciseNextBreak();
    analytics["cva"] = inputs_->cvaAnalytic();
    analytics["dva"] = inputs_->dvaAnalytic();
    analytics["fva"] = inputs_->fvaAnalytic();
    analytics["colva"] = inputs_->colvaAnalytic();
    analytics["collateralFloor"] = inputs_->collateralFloorAnalytic();
    analytics["mva"] = inputs_->mvaAnalytic();
    analytics["kva"] = inputs_->kvaAnalytic();
    analytics["dim"] = inputs_->dimAnalytic();
    analytics["dynamicCredit"] = inputs_->dynamicCredit();
    analytics["cvaSensi"] = inputs_->cvaSensi();
    analytics["flipViewXVA"] = inputs_->flipViewXVA();
    analytics["creditMigration"] = inputs_->creditMigrationAnalytic();

    string baseCurrency = inputs_->xvaBaseCurrency();
    string calculationType = inputs_->collateralCalculationType();
    string allocationMethod = inputs_->exposureAllocationMethod();
    Real marginalAllocationLimit = inputs_->marginalAllocationLimit();
    Real quantile = inputs_->pfeQuantile();
    string dvaName = inputs_->dvaName();
    string fvaLendingCurve = inputs_->fvaLendingCurve();
    string fvaBorrowingCurve = inputs_->fvaBorrowingCurve();

    Real dimQuantile = inputs_->dimQuantile();
    Size dimHorizonCalendarDays = inputs_->dimHorizonCalendarDays();
    Size dimRegressionOrder = inputs_->dimRegressionOrder();
    vector<string> dimRegressors = inputs_->dimRegressors();
    Size dimLocalRegressionEvaluations = inputs_->dimLocalRegressionEvaluations();
    Real dimLocalRegressionBandwidth = inputs_->dimLocalRegressionBandwidth();

    Real kvaCapitalDiscountRate = inputs_->kvaCapitalDiscountRate();
    Real kvaAlpha = inputs_->kvaAlpha();
    Real kvaRegAdjustment = inputs_->kvaRegAdjustment();
    Real kvaCapitalHurdle = inputs_->kvaCapitalHurdle();
    Real kvaOurPdFloor = inputs_->kvaOurPdFloor();
    Real kvaTheirPdFloor = inputs_->kvaTheirPdFloor();
    Real kvaOurCvaRiskWeight = inputs_->kvaOurCvaRiskWeight();
    Real kvaTheirCvaRiskWeight = inputs_->kvaTheirCvaRiskWeight();

    string marketConfiguration = inputs_->marketConfig("simulation");

    bool fullInitialCollateralisation = inputs_->fullInitialCollateralisation();

    checkConfigurations(analytic()->portfolio());

    if (!dimCalculator_ && (analytics["mva"] || analytics["dim"])) {
        if (inputs_->dimModel() == "Regression") {
            LOG("dim calculator not set, create RegressionDynamicInitialMarginCalculator");
            std::map<std::string, Real> currentIM;
            if (inputs_->collateralBalances()) {
                for (auto const& [n, b] : inputs_->collateralBalances()->collateralBalances()) {
                    currentIM[n.nettingSetId()] =
                        b->initialMargin() * (b->currency() == baseCurrency
                                                  ? 1.0
                                                  : analytic()
                                                        ->market()
                                                        ->fxRate(b->currency() + baseCurrency, marketConfiguration)
                                                        ->value());
                }
            }
            dimCalculator_ = QuantLib::ext::make_shared<RegressionDynamicInitialMarginCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, *scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimRegressionOrder, dimRegressors, dimLocalRegressionEvaluations,
                dimLocalRegressionBandwidth, currentIM);
        } else {
            LOG("dim calculator not set, create FlatDynamicInitialMarginCalculator");
            dimCalculator_ = QuantLib::ext::make_shared<FlatDynamicInitialMarginCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, *scenarioData_);
        }
    }

    std::vector<Period> cvaSensiGrid = inputs_->cvaSensiGrid();
    Real cvaSensiShiftSize = inputs_->cvaSensiShiftSize();

    string flipViewBorrowingCurvePostfix = inputs_->flipViewBorrowingCurvePostfix();
    string flipViewLendingCurvePostfix = inputs_->flipViewLendingCurvePostfix();

    LOG("baseCurrency " << baseCurrency);

    auto market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

    postProcess_ = QuantLib::ext::make_shared<PostProcess>(
        analytic()->portfolio(), netting, balances, market, marketConfiguration, cube_, *scenarioData_, analytics,
        baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix, inputs_->creditSimulationParameters(), inputs_->creditMigrationDistributionGrid(),
        inputs_->creditMigrationTimeSteps(), creditStateCorrelationMatrix(),
        analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), inputs_->mporCashFlowMode());
    LOG("post done");
}

void XvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::set<std::string>& runTypes) {

    if (inputs_->amcCg()) {
        LOG("XVA analytic is running with amc cg engine (experimental).");
        // note: market configs both set to simulation, see note in xvaenginecg, we'd need inccy config in sim market
        // there...
        XvaEngineCG engine(
            inputs_->nThreads(), inputs_->asof(), loader, inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
            inputs_->amcPricingEngine(), inputs_->crossAssetModelData(), inputs_->scenarioGeneratorData(),
            inputs_->portfolio(), inputs_->marketConfig("simulation"), inputs_->marketConfig("simulation"),
            inputs_->xvaCgSensiScenarioData(), inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
            inputs_->xvaCgBumpSensis(), inputs_->xvaCgUseExternalComputeDevice(),
            inputs_->xvaCgExternalDeviceCompatibilityMode(), inputs_->xvaCgUseDoublePrecisionForExternalCalculation(),
            inputs_->xvaCgExternalComputeDevice(), true, true);

        analytic()->reports()["XVA"]["xvacg-exposure"] = engine.exposureReport();
        if (inputs_->xvaCgSensiScenarioData())
            analytic()->reports()["XVA"]["xvacg-cva-sensi-scenario"] = engine.sensiReport();
        return;
    }

    LOG("XVA analytic called with asof " << io::iso_date(inputs_->asof()));
    ProgressMessage("Running XVA Analytic", 0, 1).log();

    if (runTypes.find("EXPOSURE") != runTypes.end() || runTypes.empty())
        runSimulation_ = true;

    if (runTypes.find("XVA") != runTypes.end() || runTypes.empty())
        runXva_ = true;

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    const string msg = "XVA: Build Today's Market";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
    cubeInterpreter_ = QuantLib::ext::make_shared<CubeInterpretation>(
        inputs_->storeFlows(), analytic()->configurations().scenarioGeneratorData->withCloseOutLag(), scenarioData_,
        grid_, inputs_->storeCreditStateNPVs(), inputs_->flipViewXVA());

    if (runSimulation_) {
        LOG("XVA: Build simulation market");
        buildScenarioSimMarket();

        LOG("XVA: Build Scenario Generator");
        auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
        auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
        bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);
        buildScenarioGenerator(continueOnErr);

        LOG("XVA: Attach Scenario Generator to ScenarioSimMarket");
        simMarket_->scenarioGenerator() = scenarioGenerator_;

        // We may have to build two cubes below for complementary sub-portfolios, a classical cube and an AMC cube
        bool doClassicRun = true;
        bool doAmcRun = false;

        // Initialize the residual "classical" portfolio that we do not process using AMC
        auto residualPortfolio = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        if (inputs_->amc()) {
            // Build a separate sub-portfolio for the AMC cube generation and perform its training
            buildAmcPortfolio();

            // Build the residual portfolio for the classic cube generation, i.e. strip out the AMC part
            for (auto const& [tradeId, trade] : inputs_->portfolio()->trades()) {
                if (inputs_->amcTradeTypes().find(trade->tradeType()) == inputs_->amcTradeTypes().end())
                    residualPortfolio->add(trade);
            }

            LOG("AMC portfolio size " << amcPortfolio_->size());
            LOG("Residual portfolio size " << residualPortfolio->size());

            doAmcRun = !amcPortfolio_->trades().empty();
            doClassicRun = !residualPortfolio->trades().empty();
        } else {
            for (const auto& [tradeId, trade] : inputs_->portfolio()->trades())
                residualPortfolio->add(trade);
        }

        /********************************************************************************
         * This is where we build cubes and the "classic" valuation work is done
         * The bulk of the AMC work is done before in the AMC portfolio building/training
         ********************************************************************************/

        if (doAmcRun)
            amcRun(doClassicRun);
        else
            amcPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        if (doClassicRun)
            classicPortfolio_ = classicRun(residualPortfolio);
        else
            classicPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        /***************************************************
         * We may have two cubes now that need to be merged
         ***************************************************/

        if (doClassicRun && doAmcRun) {
            LOG("Joining classical and AMC cube");
            cube_ = QuantLib::ext::make_shared<JointNPVCube>(cube_, amcCube_);
        } else if (!doClassicRun && doAmcRun) {
            LOG("We have generated an AMC cube only");
            cube_ = amcCube_;
        } else {
            WLOG("We have generated a classic cube only");
        }

        LOG("NPV cube generation completed");

        /***********************************************************************
         * We may have two non-empty portfolios to be merged for post processing
         ***********************************************************************/

        LOG("Classic portfolio size " << classicPortfolio_->size());
        LOG("AMC portfolio size " << amcPortfolio_->size());
        auto newPortfolio = QuantLib::ext::make_shared<Portfolio>();
        for (const auto& [tradeId, trade] : classicPortfolio_->trades())
            newPortfolio->add(trade);
        for (const auto& [tradeId, trade] : amcPortfolio_->trades())
            newPortfolio->add(trade);
        LOG("Total portfolio size " << newPortfolio->size());
        if (newPortfolio->size() < inputs_->portfolio()->size()) {
            ALOG("input portfolio size is " << inputs_->portfolio()->size() << ", but we have built only "
                                            << newPortfolio->size() << " trades");
        }
        analytic()->setPortfolio(newPortfolio);
    } else { // runSimulation_

        // build the portfolio linked to today's market
        analytic()->buildPortfolio();

        // ... and load a pre-built cube for post-processing

        LOG("Skip cube generation, load input cubes for XVA");
        const string msg = "XVA: Load Cubes";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        QL_REQUIRE(inputs_->cube(), "XVA without EXPOSURE requires an NPV cube as input");
        cube_ = inputs_->cube();
        QL_REQUIRE(inputs_->mktCube(), "XVA without EXPOSURE requires a market cube as input");
        scenarioData_.linkTo(inputs_->mktCube());
        if (inputs_->nettingSetCube())
            nettingSetCube_ = inputs_->nettingSetCube();
        if (inputs_->cptyCube())
            cptyCube_ = inputs_->cptyCube();
        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();
    }

    MEM_LOG;

    // Return the cubes to serialalize
    if (inputs_->writeCube()) {
        analytic()->npvCubes()["XVA"]["cube"] = cube_;
        analytic()->mktCubes()["XVA"]["scenariodata"] = *scenarioData_;
        if (nettingSetCube_) {
            analytic()->npvCubes()["XVA"]["nettingsetcube"] = nettingSetCube_;
        }
        if (cptyCube_) {
            analytic()->npvCubes()["XVA"]["cptycube"] = cptyCube_;
        }
    }

    // Generate cube reports to inspect
    if (inputs_->rawCubeOutput()) {
        map<string, string> nettingSetMap = analytic()->portfolio()->nettingSetMap();
        auto report = QuantLib::ext::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString()).writeCube(*report, cube_, nettingSetMap);
        analytic()->reports()["XVA"]["rawcube"] = report;
    }

    if (runXva_) {

        /*********************************************************************
         * This is where the aggregation work is done: call the post-processor
         *********************************************************************/

        string msg = "XVA: Aggregation";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        runPostProcessor();
        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();

        /******************************************************
         * Finally generate various (in-memory) reports/outputs
         ******************************************************/

        msg = "XVA: Reports";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        LOG("Generating XVA reports and cube outputs");

        if (inputs_->exposureProfilesByTrade()) {
            for (const auto& [tradeId, tradeIdCubePos] : postProcess_->tradeIds()) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>();
                try {
                    ReportWriter(inputs_->reportNaString()).writeTradeExposures(*report, postProcess_, tradeId);
                    analytic()->reports()["XVA"]["exposure_trade_" + tradeId] = report;
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Trade Exposure Report", "Error processing trade.", e.what(),
                                                    {{"tradeId", tradeId}})
                        .log();
                }
            }
        }

        if (inputs_->exposureProfiles()) {
            for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
                auto exposureReport = QuantLib::ext::make_shared<InMemoryReport>();
                try {
                    ReportWriter(inputs_->reportNaString())
                        .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
                    analytic()->reports()["XVA"]["exposure_nettingset_" + nettingSet] = exposureReport;
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Netting Set Exposure Report", "Error processing netting set.",
                                                    e.what(), {{"nettingSetId", nettingSet}})
                        .log();
                }

                auto colvaReport = QuantLib::ext::make_shared<InMemoryReport>();
                try {
                    ReportWriter(inputs_->reportNaString())
                        .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
                    analytic()->reports()["XVA"]["colva_nettingset_" + nettingSet] = colvaReport;
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Netting Set Colva Report", "Error processing netting set.",
                                                    e.what(), {{"nettingSetId", nettingSet}})
                        .log();
                }

                auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>();
                try {
                    ReportWriter(inputs_->reportNaString())
                        .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
                    analytic()->reports()["XVA"]["cva_sensitivity_nettingset_" + nettingSet] = cvaSensiReport;
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Cva Sensi Report", "Error processing netting set.", e.what(),
                                                    {{"nettingSetId", nettingSet}})
                        .log();
                }
            }
        }

        auto xvaReport = QuantLib::ext::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), analytic()->portfolio(), postProcess_);
        analytic()->reports()["XVA"]["xva"] = xvaReport;

        if (inputs_->netCubeOutput()) {
            auto report = QuantLib::ext::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString()).writeCube(*report, postProcess_->netCube());
            analytic()->reports()["XVA"]["netcube"] = report;
        }

        if (inputs_->dimAnalytic() || inputs_->mvaAnalytic()) {
            // Generate DIM evolution report
            auto dimEvolutionReport = QuantLib::ext::make_shared<InMemoryReport>();
            postProcess_->exportDimEvolution(*dimEvolutionReport);
            analytic()->reports()["XVA"]["dim_evolution"] = dimEvolutionReport;

            // Generate DIM regression reports
            vector<QuantLib::ext::shared_ptr<ore::data::Report>> dimRegReports;
            for (Size i = 0; i < inputs_->dimOutputGridPoints().size(); ++i) {
                auto rep = QuantLib::ext::make_shared<InMemoryReport>();
                dimRegReports.push_back(rep);
                analytic()->reports()["XVA"]["dim_regression_" + std::to_string(i)] = rep;
            }
            postProcess_->exportDimRegression(inputs_->dimOutputNettingSet(), inputs_->dimOutputGridPoints(),
                                              dimRegReports);
        }

        if (inputs_->creditMigrationAnalytic()) {
            QL_REQUIRE(
                postProcess_->creditMigrationPdf().size() == inputs_->creditMigrationTimeSteps().size(),
                "XvaAnalyticImpl::runAnalytic(): inconsistent post process results for credit migration pdf / cdf ("
                    << postProcess_->creditMigrationPdf().size() << ") and input credit migration time steps ("
                    << inputs_->creditMigrationTimeSteps().size() << ")");
            for (Size i = 0; i < postProcess_->creditMigrationPdf().size(); ++i) {
                auto rep = QuantLib::ext::make_shared<InMemoryReport>();
                analytic()
                    ->reports()["XVA"]["credit_migration_" + std::to_string(inputs_->creditMigrationTimeSteps()[i])] =
                    rep;
                (*rep)
                    .addColumn("upperBucketBound", double(), 6)
                    .addColumn("pdf", double(), 8)
                    .addColumn("cdf", double(), 8);
                for (Size j = 0; j < postProcess_->creditMigrationPdf()[i].size(); ++j) {
                    (*rep)
                        .next()
                        .add(postProcess_->creditMigrationUpperBucketBounds()[j])
                        .add(postProcess_->creditMigrationPdf()[i][j])
                        .add(postProcess_->creditMigrationCdf()[i][j]);
                }
                rep->end();
            }
        }

        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();
    }

    // reset that mode
    ObservationMode::instance().setMode(inputs_->observationModel());
    ProgressMessage("Running XVA Analytic", 1, 1).log();
}

Matrix XvaAnalyticImpl::creditStateCorrelationMatrix() const {

    CorrelationMatrixBuilder cmb;
    for (auto const& [pair, value] : analytic()->configurations().crossAssetModelData->correlations()) {
        cmb.addCorrelation(pair.first, pair.second, value);
    }

    CorrelationMatrixBuilder::ProcessInfo processInfo;
    processInfo[CrossAssetModel::AssetType::CrState] = {
        {"CrState", analytic()->configurations().simMarketParams->numberOfCreditStates()}};

    return cmb.correlationMatrix(processInfo);
}

} // namespace analytics
} // namespace ore
