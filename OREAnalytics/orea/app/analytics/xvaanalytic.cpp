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
#include <orea/aggregation/dimdirectcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/aggregation/dimhelper.hpp>
#include <orea/aggregation/dynamicdeltavarcalculator.hpp>
#include <orea/aggregation/dynamicsimmcalculator.hpp>
#include <orea/aggregation/simmhelper.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/sparsenpvcube.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/cptycalculator.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/sensitivitycalculator.hpp>
#include <orea/engine/simmsensitivitystoragemanager.hpp>
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
    std::set<std::string> nettingSetKeys;
    for (std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it)
        nettingSetKeys.insert(it->second);
    // controls on calcType and grid type, if netting-set has an active CSA in place
    for (auto const& key : nettingSetKeys) {
        if (!inputs_->nettingSetManager()->has(key)) {
            StructuredAnalyticsWarningMessage(
                "XvaAnalytic", "Netting set definition not found",
                "Definition for netting set " + key + " is not found. "
                "Configuration check is not performed on this netting set.")
                .log();
            continue;
        }
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

void XvaAnalyticImpl::applyConfigurationFallback(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
    // find the unique undefined nettingset keys in portfolio
    std::map<std::string, std::string> nettingSetMap = portfolio->nettingSetMap();
    std::set<std::string> nettingSetKeys;
    for (std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it) {
        if (!inputs_->nettingSetManager()->has(it->second)) {
            StructuredTradeErrorMessage(
                it->first, portfolio->get(it->first)->tradeType(),
                "Netting set definition is not found.",
                "Definition for netting set " + it->second + " is not found. "
                "A fallback of 'uncollateralised' netting set will be used, results for this netting set may be invalid.")
                .log();
                nettingSetKeys.insert(it->second);
        }
    }
    // add fallback netting set definitions
    for (auto const& key : nettingSetKeys) {
        StructuredAnalyticsErrorMessage(
            "XvaAnalytic", "Netting set definition not found",
            "Definition for netting set " + key + " is not found. "
            "A fallback of 'uncollateralised' netting set will be used, results for this netting set may be invalid.")
            .log();
        inputs_->nettingSetManager()->add(QuantLib::ext::make_shared<NettingSetDefinition>(key));
    }
}

QuantLib::ext::shared_ptr<EngineFactory> XvaAnalyticImpl::engineFactory() {
    LOG("XvaAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy =
        QuantLib::ext::make_shared<EngineData>(*inputs_->simulationPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    edCopy->globalParameters()["McType"] = "Classic";
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
        *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), false, true,
        inputs_->allowPartialScenarios(), *inputs_->iborFallbackConfig(), false, offsetScenario_);

    if (offsetScenario_ == nullptr) {
        simMarketCalibration_ = simMarket_;
        offsetSimMarket_ = simMarket_;
    } else {
        // set useSpreadedTermstructure to true, yield better results in calibration of the CAM
        simMarketCalibration_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), offsetSimMarketParams_,
            QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
            *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), true, true,
            inputs_->allowPartialScenarios(), *inputs_->iborFallbackConfig(), false, offsetScenario_);

        // Create a third market used for AMC and Postprocessor, holds a larger simmarket, e.g. default curves
        offsetSimMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), offsetSimMarketParams_, QuantLib::ext::make_shared<FixingManager>(inputs_->asof()),
            configuration, *inputs_->curveConfigs().get(), *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), true, true, inputs_->allowPartialScenarios(), *inputs_->iborFallbackConfig(),
            false, offsetScenario_);

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

void XvaAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError, const bool allowModelFallbacks) {
    if (inputs_->scenarioReader()) {
        auto loader = QuantLib::ext::make_shared<SimpleScenarioLoader>(inputs_->scenarioReader());
        auto slg = QuantLib::ext::make_shared<ScenarioLoaderPathGenerator>(loader, inputs_->asof(), grid_->dates(),
                                                                       grid_->timeGrid());
        scenarioGenerator_ = slg;
    } else {
        if (!model_)
            buildCrossAssetModel(continueOnCalibrationError, allowModelFallbacks);
        ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
        QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
        string config = inputs_->marketConfig("simulation");
        auto market = offsetScenario_ == nullptr ? analytic()->market() : simMarketCalibration_;
        scenarioGenerator_ =
            sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), market, config,
                      QuantLib::ext::make_shared<MultiPathGeneratorFactory>(), inputs_->amcPathDataOutput());
        QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator");
    }
    samples_ = analytic()->configurations().scenarioGeneratorData->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));
    if (inputs_->writeScenarios()) {
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        analytic()->addReport(LABEL, "scenario", report);
        scenarioGenerator_ =
            QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report, std::vector<RiskFactorKey>{}, false);
    }
}

void XvaAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError, const bool allowModelFallbacks) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ", allowModelFallbacks = " << allowModelFallbacks << ")");
    ext::shared_ptr<Market> market = offsetScenario_ != nullptr ? simMarketCalibration_ : analytic()->market();
    QL_REQUIRE(market != nullptr, "Internal error, buildCrossAssetModel needs to be called after the market is built.");

    CrossAssetModelBuilder modelBuilder(market, analytic()->configurations().crossAssetModelData,
                                        inputs_->marketConfig("lgmcalibration"), inputs_->marketConfig("fxcalibration"),
                                        inputs_->marketConfig("eqcalibration"), inputs_->marketConfig("infcalibration"),
                                        inputs_->marketConfig("crcalibration"), inputs_->marketConfig("simulation"),
                                        false, continueOnCalibrationError, "", "xva cam building", false,
                                        allowModelFallbacks);

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

    if (inputs_->xvaUseDoublePrecisionCubes())
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                   samples_, cubeDepth, 0.0f);
    else
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                  samples_, cubeDepth, 0.0f);
}

std::set<std::string> XvaAnalyticImpl::getNettingSetIds(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) const {
    // collect netting set ids from portfolio
    std::set<std::string> nettingSetIds;
    for (auto const& [tradeId,trade] : portfolio->trades())
        nettingSetIds.insert(trade->envelope().nettingSetId());
    return nettingSetIds;
}

void XvaAnalyticImpl::initClassicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA: initClassicRun");

    initCubeDepth();

    // May have been set already
    if (scenarioData_ == nullptr) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ =
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
    }

    // We can skip the cube initialization if the mt val engine is used, since it builds its own cubes
    if (inputs_->nThreads() == 1) {
        if (portfolio->size() > 0)
            initCube(cube_, portfolio->ids(), cubeDepth_);
	
	// not required by any calculators in ore at the moment
        nettingSetCube_ = nullptr;
	// except in this case with a classic single-threaded run, left here for validation purposes:
        if (inputs_->storeSensis()) {
            // Create the sensitivity storage manager
            // FIXME: Does the storage manager check consistency with the sensis provided by the delta/gamma engines?
            bool sensitivities2ndOrder = false;
            vector<Real> curveSensitivityGrid = inputs_->curveSensiGrid();
            vector<Real> vegaOptSensitivityGrid = inputs_->vegaSensiGrid();
            vector<Real> vegaUndSensitivityGrid = inputs_->vegaSensiGrid();
            vector<Real> fxVegaSensitivityGrid = inputs_->vegaSensiGrid();
            Size n = curveSensitivityGrid.size();
            Size u = vegaOptSensitivityGrid.size();
            Size v = vegaUndSensitivityGrid.size();
            Size w = fxVegaSensitivityGrid.size();
	    QL_REQUIRE(n + u + v + w > 0, "store sensis chosen, but sensitivity grids not set"); 
            // first cube index can be set to 0, since at the moment we only use the netting-set cube for sensi storage
            if (inputs_->dimModel() == "SimmAnalytic")
                sensitivityStorageManager_ = QuantLib::ext::make_shared<ore::analytics::SimmSensitivityStorageManager>(
                    analytic()->configurations().crossAssetModelData->currencies(), 0);
            else
                sensitivityStorageManager_ = QuantLib::ext::make_shared<ore::analytics::CamSensitivityStorageManager>(
                    analytic()->configurations().crossAssetModelData->currencies(), n, u, v, w, 0,
                    sensitivities2ndOrder);
            QL_REQUIRE(sensitivityStorageManager_, "creating sensitivity storage manager failed");

            // Create the netting set cube 
	    Size samples = analytic()->configurations().scenarioGeneratorData->samples();
            vector<Date> dates = analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();
            std::set<std::string> nettingSets = getNettingSetIds(portfolio);
	    LOG("Initialise netting set cube for "
		<< nettingSets.size() << " netting sets, " << dates.size() << " valuation dates, " << samples
		<< " samples, " << sensitivityStorageManager_->getRequiredSize() << " sensitivities to store");
            nettingSetCube_ = QuantLib::ext::make_shared<SinglePrecisionSparseNpvCube>(
                inputs_->asof(), nettingSets, dates, samples, sensitivityStorageManager_->getRequiredSize(), 0.0f);
        }

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
	// Ensure that the NPV calculator is executed before the exercise calculator
	if (inputs_->storeExerciseValues())
            calculators.push_back(QuantLib::ext::make_shared<ExerciseCalculator>(
                inputs_->exposureBaseCurrency(), cubeInterpreter_->exerciseValueIndex()));
        if (inputs_->storeCreditStateNPVs() > 0) {
            calculators.push_back(QuantLib::ext::make_shared<MultiStateNPVCalculator>(
                inputs_->exposureBaseCurrency(), cubeInterpreter_->creditStateNPVsIndex(),
                inputs_->storeCreditStateNPVs()));
        }
        if (inputs_->storeSensis()) {
	    LOG("CamSensitivityStorageManager: store sensis true");
	    QL_REQUIRE(sensitivityStorageManager_, "sensitivity storage manager not set");
            calculators.push_back(QuantLib::ext::make_shared<SensitivityCalculator>(sensitivityStorageManager_));
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
        engine.buildCube(portfolio, cube_, calculators(), ValuationEngine::ErrorPolicy::RemoveAll,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), nettingSetCube_,
                         cptyCube_, cptyCalculators());
    } else {

        // multi-threaded engine run

        /* TODO we assume no netting output cube is needed. Currently there are no valuation calculators in ore that
         * require this cube. */

        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
            if (inputs_->xvaUseDoublePrecisionCubes())
                return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, cubeDepth_, 0.0);
            else
                return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, cubeDepth_, 0.0);
        };

        std::function<QuantLib::ext::shared_ptr<NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                         const std::vector<QuantLib::Date>&, const QuantLib::Size)>
            cptyCubeFactory;
        if (inputs_->storeSurvivalProbabilities()) {
            cptyCubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                     const std::vector<QuantLib::Date>& dates,
                                     const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
                if (inputs_->xvaUseDoublePrecisionCubes())
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, 0.0f);
                else
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, 0.0f);
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

        engine.setAggregationScenarioData(scenarioData_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, ValuationEngine::ErrorPolicy::RemoveAll, cptyCalculators,
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
                                  const std::vector<Date>& simDates, const std::vector<Date>& stickyCloseOutDates) {
    LOG("XvaAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*inputs_->amcPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    edCopy->globalParameters()["McType"] = "American";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    ext::shared_ptr<ore::data::Market> market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;
    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, market, configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
        EngineBuilderFactory::instance().generateAmcEngineBuilders(cam, simDates, stickyCloseOutDates), true);
    return factory;
}

void XvaAnalyticImpl::buildAmcPortfolio() {

    LOG("XVA: buildAmcPortfolio");
    const string msg = "XVA: Build AMC portfolio";
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();

    std::vector<Date> simDates, stickyCloseOutDates;
    if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag() &&
        analytic()->configurations().scenarioGeneratorData->withMporStickyDate()) {
        simDates = analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();
        stickyCloseOutDates = analytic()->configurations().scenarioGeneratorData->getGrid()->closeOutDates();
    } else {
        simDates = analytic()->configurations().scenarioGeneratorData->getGrid()->dates();
    }

    LOG("buildAmcPortfolio: Register additional engine builders");
    auto factory = amcEngineFactory(model_, simDates, stickyCloseOutDates);

    LOG("buildAmcPortfolio: Load Portfolio");
    QuantLib::ext::shared_ptr<Portfolio> portfolio = inputs_->portfolio();

    LOG("Build Portfolio with AMC Engine factory and select amc-enabled trades")
    amcPortfolio_ = QuantLib::ext::make_shared<Portfolio>();
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        if (inputs_->amcTradeTypes().find(trade->tradeType()) != inputs_->amcTradeTypes().end()) {
            if (inputs_->amcCg() != XvaEngineCG::Mode::CubeGeneration) {
                auto t = trade;
                auto [ft, success] = buildTrade(t, factory, "analytic/" + label(), false, true, true);
                if (success)
                    amcPortfolio_->add(trade);
                else
                    amcPortfolio_->add(ft);
            } else {
                // the amc-cg engine will build the trades itself
                trade->reset();
                amcPortfolio_->add(trade);
            }
            DLOG("trade " << tradeId << " is added to amc portfolio");
        }
    }

    LOG("AMC portfolio built, size is " << amcPortfolio_->size());

    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    LOG("XVA: buildAmcPortfolio completed");
}

void XvaAnalyticImpl::amcRun(bool doClassicRun, bool continueOnCalibrationError, bool allowModelFallbacks) {

    LOG("XVA: amcRun");

    if (scenarioData_ == nullptr) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ =
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
    }

    initCubeDepth();

    std::string message = "XVA: Build AMC Cube " + std::to_string(amcPortfolio_->size()) + " x " +
                          std::to_string(grid_->valuationDates().size()) + " x " + std::to_string(samples_) + "... ";
    CONSOLEW(message);
    auto progressBar = QuantLib::ext::make_shared<SimpleProgressBar>(message, ConsoleLog::instance().width(),
                                                                     ConsoleLog::instance().progressBarWidth());
    auto progressLog = QuantLib::ext::make_shared<ProgressLog>("XVA: Build AMC Cube...", 100, oreSeverity::notice);

    if (inputs_->amcCg() == XvaEngineCG::Mode::CubeGeneration) {

        // cube generation with amc-cg engine

        initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);

        if (inputs_->xvaCgDynamicIM()) {
            // cube storing dynamic IM per netting set (total margin, delta, vega, curvature), i.e. depth 4
            Size imCubeDepth = 4;
            nettingSetCube_ = QuantLib::ext::make_shared<SinglePrecisionSparseNpvCube>(
                inputs_->asof(), getNettingSetIds(amcPortfolio_), grid_->valuationDates(), samples_, imCubeDepth, 0.0f);
        }

        XvaEngineCG engine(
            inputs_->amcCg(), inputs_->nThreads(), inputs_->asof(), analytic()->loader(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
            inputs_->amcCgPricingEngine(), inputs_->crossAssetModelData(), inputs_->scenarioGeneratorData(),
            amcPortfolio_, inputs_->marketConfig("simulation"), inputs_->marketConfig("lgmcalibration"),
            inputs_->xvaCgSensiScenarioData(), inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
            inputs_->xvaCgBumpSensis(), inputs_->xvaCgDynamicIM(), inputs_->xvaCgDynamicIMStepSize(),
            inputs_->xvaCgRegressionOrder(), inputs_->xvaCgRegressionVarianceCutoff(),
            inputs_->xvaCgTradeLevelBreakdown(), inputs_->xvaCgUseRedBlocks(), inputs_->xvaCgUseExternalComputeDevice(),
            inputs_->xvaCgExternalDeviceCompatibilityMode(), inputs_->xvaCgUseDoublePrecisionForExternalCalculation(),
            inputs_->xvaCgExternalComputeDevice(), inputs_->xvaCgUsePythonIntegration(), true, true, true,
            "xva analytic");

        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.setAggregationScenarioData(scenarioData_);
        engine.setOffsetScenario(offsetScenario_);
        engine.setNpvOutputCube(amcCube_);
        if(inputs_->xvaCgDynamicIM()) {
            engine.setDynamicIMOutputCube(nettingSetCube_);
        }
        engine.run();

    } else {

        // cube generation with amc engine

        if (inputs_->nThreads() == 1) {
            initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);
            ext::shared_ptr<ore::data::Market> market =
                offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

            AMCValuationEngine amcEngine(
                model_, inputs_->scenarioGeneratorData(), market,
                inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
                inputs_->exposureSimMarketParams()->additionalScenarioDataCcys(),
                inputs_->exposureSimMarketParams()->additionalScenarioDataNumberOfCreditStates(),
                inputs_->amcPathDataInput(), inputs_->amcPathDataOutput(), inputs_->amcIndividualTrainingInput(),
                inputs_->amcIndividualTrainingOutput());
            amcEngine.registerProgressIndicator(progressBar);
            amcEngine.registerProgressIndicator(progressLog);
            amcEngine.aggregationScenarioData() = scenarioData_;
            amcEngine.buildCube(amcPortfolio_, amcCube_);
        } else {
            auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                      const std::vector<QuantLib::Date>& dates,
                                      const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
                if (inputs_->xvaUseDoublePrecisionCubes())
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, cubeDepth_,
                                                                               0.0);
                else
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, cubeDepth_,
                                                                              0.0);
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
                inputs_->marketConfig("simulation"), inputs_->amcPathDataInput(), inputs_->amcPathDataOutput(),
                inputs_->amcIndividualTrainingInput(), inputs_->amcIndividualTrainingOutput(),
                inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, cubeFactory, offsetScenario_,
                simMarketParams, continueOnCalibrationError, allowModelFallbacks);

            amcEngine.registerProgressIndicator(progressBar);
            amcEngine.registerProgressIndicator(progressLog);
            amcEngine.aggregationScenarioData() = scenarioData_;
            amcEngine.buildCube(amcPortfolio_);
            amcCube_ = QuantLib::ext::make_shared<JointNPVCube>(amcEngine.outputCubes());
        }
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
    analytics["exposureProfilesUseCloseOutValues"] = inputs_->exposureProfilesUseCloseOutValues();

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
    bool firstMporCollateralAdjustment = inputs_->firstMporCollateralAdjustment();
    checkConfigurations(analytic()->portfolio());
    applyConfigurationFallback(analytic()->portfolio());

    if (!dimCalculator_ && (analytics["mva"] || analytics["dim"])) {
        LOG("dim calculator not set, create one");
	    std::map<std::string, Real> currentIM;
        if (inputs_->collateralBalances()) {
            for (auto const& [n, b] : inputs_->collateralBalances()->collateralBalances()) {
                currentIM[n.nettingSetId()] =
                    b->initialMargin() *
                    (b->currency() == baseCurrency
                         ? 1.0
                         : analytic()->market()->fxRate(b->currency() + baseCurrency, marketConfiguration)->value());
            }
        }

        DLOG("Create a '" << inputs_->dimModel() << "' Dynamic Initial Margin Calculator");

        if (inputs_->dimModel() == "Regression") {
            dimCalculator_ = QuantLib::ext::make_shared<RegressionDynamicInitialMarginCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimRegressionOrder, dimRegressors, dimLocalRegressionEvaluations,
                dimLocalRegressionBandwidth, currentIM);
        } else if (inputs_->dimModel() == "DeltaVaR" ||
		   inputs_->dimModel() == "DeltaGammaNormalVaR" ||
		   inputs_->dimModel() == "DeltaGammaVaR") {
            QL_REQUIRE(nettingSetCube_ && sensitivityStorageManager_,
                       "netting set cube or sensitivity storage manager not set - "
                           << "is this a single-threaded classic run storing sensis?");
            // delta 1, delta-gamma-normal 2, delta-gamma 3
            Size ddvOrder;
            if (inputs_->dimModel() == "DeltaVaR")
                ddvOrder = 1;
            else if (inputs_->dimModel() == "DeltaGammaNormalVaR")
                ddvOrder = 2;
            else
                ddvOrder = 3;
            QuantLib::ext::shared_ptr<DimHelper> dimHelper = QuantLib::ext::make_shared<DimHelper>(
                model_, nettingSetCube_, sensitivityStorageManager_, inputs_->curveSensiGrid(), dimHorizonCalendarDays);
            dimCalculator_ = QuantLib::ext::make_shared<DynamicDeltaVaRCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimHelper, ddvOrder, currentIM);
	} else if (inputs_->dimModel() == "SimmAnalytic") {
            QL_REQUIRE(nettingSetCube_ && sensitivityStorageManager_,
                       "netting set cube or sensitivity storage manager not set - "
                           << "is this a single-threaded classic run storing sensis?");
            QuantLib::ext::shared_ptr<SimmHelper> simmHelper = QuantLib::ext::make_shared<SimmHelper>(
                analytic()->configurations().crossAssetModelData->currencies(),
		nettingSetCube_, scenarioData_, sensitivityStorageManager_, analytic()->market());
            Size imCubeDepth = 6; // allow for total, delta, vega and curvature margin at depths 0-3, fx delta and ir delta at depths 4-5
            dimCalculator_ = QuantLib::ext::make_shared<DynamicSimmCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, simmHelper, dimQuantile,
                dimHorizonCalendarDays, currentIM, imCubeDepth);
        } else if (inputs_->dimModel() == "DynamicIM") {
            QL_REQUIRE(nettingSetCube_ && inputs_->xvaCgDynamicIM() &&
                           inputs_->amcCg() == XvaEngineCG::Mode::CubeGeneration,
                       "dim model is set to DynamicIM, this requires amcCg=CubeGeneration, xvaCgDynamicIM=true");
            dimCalculator_ = QuantLib::ext::make_shared<DirectDynamicInitialMarginCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, nettingSetCube_, currentIM);
        } else {
            WLOG("dim model not specified, create FlatDynamicInitialMarginCalculator");
            dimCalculator_ = QuantLib::ext::make_shared<FlatDynamicInitialMarginCalculator>(
                inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_);
        }
    }

    std::vector<Period> cvaSensiGrid = inputs_->cvaSensiGrid();
    Real cvaSensiShiftSize = inputs_->cvaSensiShiftSize();

    string flipViewBorrowingCurvePostfix = inputs_->flipViewBorrowingCurvePostfix();
    string flipViewLendingCurvePostfix = inputs_->flipViewLendingCurvePostfix();

    LOG("baseCurrency " << baseCurrency);

    auto market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

    postProcess_ = QuantLib::ext::make_shared<PostProcess>(
        analytic()->portfolio(), netting, balances, market, marketConfiguration, cube_, scenarioData_, analytics,
        baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix, inputs_->creditSimulationParameters(), inputs_->creditMigrationDistributionGrid(),
        inputs_->creditMigrationTimeSteps(), creditStateCorrelationMatrix(),
        analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), inputs_->mporCashFlowMode(),
        firstMporCollateralAdjustment, inputs_->continueOnError(), inputs_->xvaUseDoublePrecisionCubes());
    LOG("post done");
}

void XvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::set<std::string>& runTypes) {

    LOG("XVA analytic is running with amc cg mode '" << inputs_->amcCg() << "'.");

    QL_REQUIRE(!((offsetScenario_ == nullptr) ^ (offsetSimMarketParams_ == nullptr)),
               "Need offsetScenario and corresponding simMarketParameter");

    SavedSettings settings;

    optional<bool> localIncTodaysCashFlows = inputs_->exposureIncludeTodaysCashFlows();
    Settings::instance().includeTodaysCashFlows() = localIncTodaysCashFlows;
    LOG("Simulation IncludeTodaysCashFlows is defined: " << (localIncTodaysCashFlows ? "true" : "false"));
    if (localIncTodaysCashFlows) {
        LOG("Exposure IncludeTodaysCashFlows is set to " << (*localIncTodaysCashFlows ? "true" : "false"));
    }

    bool localIncRefDateEvents = inputs_->exposureIncludeReferenceDateEvents();
    Settings::instance().includeReferenceDateEvents() = localIncRefDateEvents;
    LOG("Simulation IncludeReferenceDateEvents is set to " << (localIncRefDateEvents ? "true" : "false"));

    LOG("XVA analytic called with asof " << io::iso_date(inputs_->asof()));
    ProgressMessage("Running XVA Analytic", 0, 1).log();

    if (runTypes.find("EXPOSURE") != runTypes.end() || runTypes.empty())
        runSimulation_ = true;

    if (runTypes.find("XVA") != runTypes.end() || runTypes.empty())
        runXva_ = true;

    if (runTypes.find("PFE") != runTypes.end() || runTypes.empty())
        runPFE_ = true;

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    const string msg = "XVA: Build Today's Market";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    if (inputs_->amcCg() == XvaEngineCG::Mode::Full) {
        // note: market configs both set to simulation, see note in xvaenginecg, we'd need inccy config
        // in sim market there...
        // TODO expose dynamic delta var flag to config (hardcoded to true at the moment)
        XvaEngineCG engine(
            inputs_->amcCg(), inputs_->nThreads(), inputs_->asof(), analytic()->loader(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
            inputs_->amcCgPricingEngine(), inputs_->crossAssetModelData(), inputs_->scenarioGeneratorData(),
            inputs_->portfolio(), inputs_->marketConfig("simulation"), inputs_->marketConfig("simulation"),
            inputs_->xvaCgSensiScenarioData(), inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
            inputs_->xvaCgBumpSensis(), inputs_->xvaCgDynamicIM(), inputs_->xvaCgDynamicIMStepSize(),
            inputs_->xvaCgRegressionOrder(), inputs_->xvaCgRegressionVarianceCutoff(),
            inputs_->xvaCgTradeLevelBreakdown(), inputs_->xvaCgUseRedBlocks(), inputs_->xvaCgUseExternalComputeDevice(),
            inputs_->xvaCgExternalDeviceCompatibilityMode(), inputs_->xvaCgUseDoublePrecisionForExternalCalculation(),
            inputs_->xvaCgExternalComputeDevice(), inputs_->xvaCgUsePythonIntegration(), true, true, true,
            "xva analytic");

        engine.run();

        analytic()->addReport(LABEL, "xvacg-exposure", engine.exposureReport());
        if (inputs_->xvaCgSensiScenarioData())
            analytic()->addReport(LABEL, "xvacg-cva-sensi-scenario", engine.sensiReport());
        return;
    }

    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
    cubeInterpreter_ = QuantLib::ext::make_shared<CubeInterpretation>(
        inputs_->storeFlows(), analytic()->configurations().scenarioGeneratorData->withCloseOutLag(),
        inputs_->storeExerciseValues(), grid_, inputs_->storeCreditStateNPVs(), inputs_->flipViewXVA());

    if (runSimulation_) {
        LOG("XVA: Build simulation market");
        buildScenarioSimMarket();

        LOG("XVA: Build Scenario Generator");
        auto continueOnErr = false;
        auto allowModelFallbacks = false;
        auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
        if (auto c = globalParams.find("ContinueOnCalibrationError"); c != globalParams.end())
            continueOnErr = parseBool(c->second);
        if (auto c = globalParams.find("AllowModelFallbacks"); c != globalParams.end())
            allowModelFallbacks = parseBool(c->second);
        buildScenarioGenerator(continueOnErr, allowModelFallbacks);

        LOG("XVA: Attach Scenario Generator to ScenarioSimMarket");
        simMarket_->scenarioGenerator() = scenarioGenerator_;

        // We may have to build two cubes below for complementary sub-portfolios, a classical cube and an AMC cube
        bool doClassicRun = true;
        bool doAmcRun = false;

        // Initialize the residual "classical" portfolio that we do not process using AMC
        auto residualPortfolio = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        if (inputs_->amc() || inputs_->amcCg() == XvaEngineCG::Mode::CubeGeneration) {
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

            analytic()->enrichIndexFixings(amcPortfolio_);
        } else {
            for (const auto& [tradeId, trade] : inputs_->portfolio()->trades())
                residualPortfolio->add(trade);
        }

        analytic()->enrichIndexFixings(residualPortfolio);

        /********************************************************************************
         * This is where we build cubes and the "classic" valuation work is done
         * The bulk of the AMC work is done before in the AMC portfolio building/training
         ********************************************************************************/

        if (doAmcRun)
            amcRun(doClassicRun, continueOnErr, allowModelFallbacks);
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
        //
        // during simulation stage, trades may be built using amc engine factory
        // instead of classic engine factory, resulting in trade errors from the following buildPortfolio()
        //
        // when buildFailedTrades is set to False, trade errors are emitted in structured log, because
        // the trades will be removed from the portfolio and do NOT participate in the post-processing.
        // we have a genuine interest in such errors
        //
        // when buildFailedTrades is set to True, trade errors are NOT emitted in structured log, because
        // the trades will NOT be removed from the portfolio and DO participate in the post-processing.
        // any genuine error should have been reported during simulation stage
        analytic()->buildPortfolio(!inputs_->buildFailedTrades());

        analytic()->enrichIndexFixings(analytic()->portfolio());

        // ... and load a pre-built cube for post-processing

        LOG("Skip cube generation, load input cubes for XVA");
        const string msg = "XVA: Load Cubes";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        QL_REQUIRE(inputs_->cube(), "XVA without EXPOSURE requires an NPV cube as input");
        cube_ = inputs_->cube();
        QL_REQUIRE(inputs_->mktCube(), "XVA without EXPOSURE requires a market cube as input");
        scenarioData_ = inputs_->mktCube();
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
        analytic()->npvCubes()[LABEL]["cube"] = cube_;
        analytic()->mktCubes()[LABEL]["scenariodata"] = scenarioData_;
        if (nettingSetCube_) {
            analytic()->npvCubes()[LABEL]["nettingsetcube"] = nettingSetCube_;
        }
        if (cptyCube_) {
            analytic()->npvCubes()[LABEL]["cptycube"] = cptyCube_;
        }
    }

    // Generate cube reports to inspect
    if (inputs_->rawCubeOutput()) {
        map<string, string> nettingSetMap = analytic()->portfolio()->nettingSetMap();
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeCube(*report, cube_, nettingSetMap);
        analytic()->addReport(LABEL, "rawcube", report);
    }

    if (runXva_ || runPFE_) {

        /*********************************************************************
         * This is where the aggregation work is done: call the post-processor
         *********************************************************************/

        string runStr = "";
        if (runXva_ && runPFE_) {
            runStr = "XVA and PFE";
        } else if (!runXva_ && runPFE_) {
            runStr = "PFE";
        } else if (runXva_ && !runPFE_) {
            runStr = "XVA";
        }

        string msg = runStr + ": Aggregation";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        runPostProcessor();
        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();

        /******************************************************
         * Finally generate various (in-memory) reports/outputs
         ******************************************************/

        msg = runStr + ": Reports";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        LOG("Generating " + runStr + " reports and cube outputs");

        if (inputs_->exposureProfilesByTrade()) {
            for (const auto& [tradeId, tradeIdCubePos] : postProcess_->tradeIds()) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                try {
                    ReportWriter(inputs_->reportNaString()).writeTradeExposures(*report, postProcess_, tradeId);
                    analytic()->addReport(LABEL, "exposure_trade_" + tradeId, report);
                } catch (const std::exception& e) {
                    QuantLib::ext::shared_ptr<Trade> failedTrade = postProcess_->portfolio()->trades().find(tradeId)->second;
                    map<string, string> subfields;
                    subfields.insert({"tradeId", tradeId});
                    subfields.insert({"tradeType", failedTrade->tradeType()});
                    StructuredAnalyticsErrorMessage("Trade Exposure Report", "Error processing trade.", e.what(),
                                                    subfields)
                        .log();
                }
            }
        }

        if (inputs_->exposureProfiles() || runPFE_) {
            for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
                auto exposureReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                try {
                    ReportWriter(inputs_->reportNaString())
                        .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
                    analytic()->addReport(LABEL, "exposure_nettingset_" + nettingSet, exposureReport);
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Netting Set Exposure Report", "Error processing netting set.",
                                                    e.what(), {{"nettingSetId", nettingSet}})
                        .log();
                }
                if (runXva_) {
                    auto colvaReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    try {
                        ReportWriter(inputs_->reportNaString())
                            .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
                        analytic()->addReport(LABEL, "colva_nettingset_" + nettingSet, colvaReport);
                    } catch (const std::exception& e) {
                        StructuredAnalyticsErrorMessage("Netting Set Colva Report", "Error processing netting set.",
                                                        e.what(), {{"nettingSetId", nettingSet}})
                            .log();
                    }

                    auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    try {
                        ReportWriter(inputs_->reportNaString())
                            .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
                        analytic()->addReport(LABEL, "cva_sensitivity_nettingset_" + nettingSet, cvaSensiReport);
                    } catch (const std::exception& e) {
                        StructuredAnalyticsErrorMessage("Cva Sensi Report", "Error processing netting set.", e.what(),
                                                        {{"nettingSetId", nettingSet}})
                            .log();
                    }
                }
            }
        }

        if (runXva_) {
            auto xvaReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
            ReportWriter(inputs_->reportNaString())
                .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), analytic()->portfolio(), postProcess_);
            analytic()->addReport(LABEL, "xva", xvaReport);

            if (inputs_->netCubeOutput()) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                ReportWriter(inputs_->reportNaString()).writeCube(*report, postProcess_->netCube());
                analytic()->addReport(LABEL, "netcube", report);
            }

            if (inputs_->timeAveragedNettedExposureOutput()) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                ReportWriter(inputs_->reportNaString())
                    .writeTimeAveragedNettedExposure(*report, postProcess_->timeAveragedNettedExposure());
                analytic()->addReport(LABEL, "timeAveragedNettedExposure", report);
            }

            if (inputs_->dimAnalytic() || inputs_->mvaAnalytic()) {
                // Generate DIM evolution report
                auto dimEvolutionReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimEvolution(*dimEvolutionReport);
                analytic()->addReport(LABEL, "dim_evolution", dimEvolutionReport);

                // Generate DIM distribution report
                auto dimDistributionReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimDistribution(*dimDistributionReport, inputs_->dimDistributionGridSize(),
                                                    inputs_->dimDistributionCoveredStdDevs());
                analytic()->addReport(LABEL, "dim_distribution", dimDistributionReport);

		auto dimCubeReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimCube(*dimCubeReport);
                analytic()->addReport(LABEL, "dim_cube", dimCubeReport);

	// Generate DIM regression reports
                vector<QuantLib::ext::shared_ptr<ore::data::Report>> dimRegReports;
                for (Size i = 0; i < inputs_->dimOutputGridPoints().size(); ++i) {
                    auto rep = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    dimRegReports.push_back(rep);
                    analytic()->addReport(LABEL, "dim_regression_" + std::to_string(i), rep);
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
                    auto rep = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    analytic()
                        ->addReport("XVA", "credit_migration_" + std::to_string(inputs_->creditMigrationTimeSteps()[i]), rep);
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
