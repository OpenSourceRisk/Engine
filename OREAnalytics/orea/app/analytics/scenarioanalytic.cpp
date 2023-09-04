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

#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/multistatenpvcalculator.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariogeneratortransform.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/******************************************************************************
 * XVA Analytic: EXPOSURE, CVA, DVA, FVA, KVA, COLVA, COLLATERALFLOOR, DIM, MVA
 ******************************************************************************/

void ScenarioAnalyticImpl::setUpConfigurations() {
    LOG("ScenarioAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->exposureSimMarketParams();
    analytic()->configurations().scenarioGeneratorData = inputs_->scenarioGeneratorData();
    analytic()->configurations().crossAssetModelData = inputs_->crossAssetModelData();
}

void  ScenarioAnalyticImpl::checkConfigurations(const boost::shared_ptr<Portfolio>& portfolio) {
    //find the unique nettingset keys in portfolio
    std::map<std::string, std::string> nettingSetMap =  portfolio->nettingSetMap();
    std::vector<std::string> nettingSetKeys;
    for(std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it)
        nettingSetKeys.push_back(it->second);
    //unique nettingset keys
    sort(nettingSetKeys.begin(), nettingSetKeys.end());
    nettingSetKeys.erase(unique( nettingSetKeys.begin(), nettingSetKeys.end() ), nettingSetKeys.end());
    //controls on calcType and grid type, if netting-set has an active CSA in place
    for(auto const& key : nettingSetKeys){
        LOG("For netting-set "<<key<<"CSA flag is "<<inputs_->nettingSetManager()->get(key)->activeCsaFlag());
        if (inputs_->nettingSetManager()->get(key)->activeCsaFlag()){
            string calculationType = inputs_->collateralCalculationType();
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()){
                QL_REQUIRE(calculationType == "NoLag", "For nettingSetID "<<key<< ", CSA is active and a close-out grid is configured in the simulation.xml. Therefore, calculation type "<<calculationType<<" is not admissable. It must be set to NoLag!");
                LOG("For netting-set "<<key<<", calculation type is "<<calculationType);
            }
            else{
                QL_REQUIRE(calculationType != "NoLag", "For nettingSetID "<<key<< ", CSA is active and a close-out grid is not configured in the simulation.xml. Therefore, calculation type " <<calculationType<<" is not admissable. It must be set to either Symmetric or AsymmerticCVA or AsymmetricDVA!" );
                LOG("For netting-set "<<key<<", calculation type is "<<calculationType);
            }
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag() && analytic()->configurations().scenarioGeneratorData->closeOutLag() != 0*Days){
                Period mpor_simulation = analytic()->configurations().scenarioGeneratorData->closeOutLag();                   
                Period mpor_netting = inputs_->nettingSetManager()->get(key)->csaDetails()->marginPeriodOfRisk();
                if (mpor_simulation != mpor_netting)
                    WLOG(StructuredAnalyticsWarningMessage(
                        "ScenarioAnalytic", "Inconsistent MPoR period",
                        "For netting set " + key +", close-out lag is not consistent with the netting-set's mpor "));
            }
        }
    }
}

boost::shared_ptr<EngineFactory> ScenarioAnalyticImpl::engineFactory() {
    LOG("XvaAnalytic::engineFactory() called");
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->simulationPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    //configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; 
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders;

    if (runSimulation_) {
        // link to the sim market here
        QL_REQUIRE(simMarket_, "Simulaton market not set");
        engineFactory_ = boost::make_shared<EngineFactory>(edCopy, simMarket_, configurations,
                                                           inputs_->refDataManager(), *inputs_->iborFallbackConfig());
    } else {
        // we just link to today's market if simulation is not required
        engineFactory_ = boost::make_shared<EngineFactory>(edCopy, analytic()->market(), configurations, inputs_->refDataManager(),
                                                           *inputs_->iborFallbackConfig());
    }
    return engineFactory_;
}


void ScenarioAnalyticImpl::buildScenarioSimMarket() {
    
    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(
            analytic()->market(),
            analytic()->configurations().simMarketParams,
            boost::make_shared<FixingManager>(inputs_->asof()),
            configuration,
            *inputs_->curveConfigs().get(),
            *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), 
            false, true, false,
            *inputs_->iborFallbackConfig(),
            false);
}

void ScenarioAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError) {
    if (!model_)
        buildCrossAssetModel(continueOnCalibrationError);
    ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    string config = inputs_->marketConfig("simulation");
    scenarioGenerator_ = sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), analytic()->market(), config); 
    QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator"); 
    samples_ = analytic()->configurations().scenarioGeneratorData->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));    
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));    

    if (inputs_->writeScenarios()) {
        auto report = boost::make_shared<InMemoryReport>();
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario"] = report;
        scenarioGenerator_ = boost::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void ScenarioAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("SCENARIO_STATISTICS: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ")");
    CrossAssetModelBuilder modelBuilder(
        analytic()->market(), analytic()->configurations().crossAssetModelData, inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "",
        inputs_->salvageCorrelationMatrix() ? SalvagingAlgorithm::Spectral : SalvagingAlgorithm::None,
        "xva cam building");
    model_ = *modelBuilder.model();
}

void ScenarioAnalyticImpl::initCubeDepth() {
    if (cubeDepth_ == 0) {
        LOG("SCENARIO_STATISTICS: Set cube depth");
        cubeDepth_ = cubeInterpreter_->requiredNpvCubeDepth();
        LOG("SCENARIO_STATISTICS: Cube depth set to: " << cubeDepth_);
    }
}

void ScenarioAnalyticImpl::initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth) {

    LOG("Init cube with depth " << cubeDepth);

    for (Size i = 0; i < grid_->valuationDates().size(); ++i)
        DLOG("initCube: grid[" << i << "]=" << io::iso_date(grid_->valuationDates()[i]));
    
    if (cubeDepth == 1)
        cube = boost::make_shared<SinglePrecisionInMemoryCube>(inputs_->asof(),
            ids, grid_->valuationDates(), samples_, 0.0f);
    else
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(inputs_->asof(),
            ids, grid_->valuationDates(), samples_, cubeDepth, 0.0f);    
}

void ScenarioAnalyticImpl::initRun(const boost::shared_ptr<Portfolio>& portfolio) {
        
    LOG("SCENARIOSTATISTICS: initRun");

    initCubeDepth();

    // May have been set already
    if (scenarioData_.empty()) {
        LOG("SCENARIO_STATISTICS: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_.linkTo(
            boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_));
        simMarket_->aggregationScenarioData() = *scenarioData_;

    }

    // We can skip the cube initialization if the mt val engine is used, since it builds its own cubes
    if (inputs_->nThreads() == 1) {
        if (portfolio->size() > 0)
            initCube(cube_, portfolio->ids(), cubeDepth_);
        // not required by any calculators in ore at the moment
        nettingSetCube_ = nullptr;
        cptyCube_ = nullptr;
    }

    LOG("SCENARIO_STATISTICS: initClassicRun completed");
}

void ScenarioAnalyticImpl::buildCube(const boost::shared_ptr<Portfolio>& portfolio) {

    LOG("SCENARIO_STATISTICS::buildCube"); 
    
    // set up valuation calculator factory
    auto calculators = [this]() {
        vector<boost::shared_ptr<ValuationCalculator>> calculators;
        if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()) {
            boost::shared_ptr<NPVCalculator> npvCalc =
                boost::make_shared<NPVCalculator>(inputs_->exposureBaseCurrency());
            calculators.push_back(boost::make_shared<MPORCalculator>(npvCalc, cubeInterpreter_->defaultDateNpvIndex(),
                                                                     cubeInterpreter_->closeOutDateNpvIndex()));
        } else
            calculators.push_back(boost::make_shared<NPVCalculator>(inputs_->exposureBaseCurrency()));
        if (inputs_->storeFlows())
            calculators.push_back(boost::make_shared<CashflowCalculator>(
                inputs_->exposureBaseCurrency(), inputs_->asof(), grid_, cubeInterpreter_->mporFlowsIndex()));
        if(inputs_->storeCreditStateNPVs() > 0) {
            calculators.push_back(boost::make_shared<MultiStateNPVCalculator>(inputs_->exposureBaseCurrency(),
                                                                              cubeInterpreter_->creditStateNPVsIndex(),
                                                                              inputs_->storeCreditStateNPVs()));
        }
        return calculators;
    };

    // set up cpty calculator factory

    auto cptyCalculators = [this]() {
        vector<boost::shared_ptr<CounterpartyCalculator>> cptyCalculators;
        if (inputs_->storeSurvivalProbabilities()) {
            string configuration = inputs_->marketConfig("simulation");
            cptyCalculators.push_back(boost::make_shared<SurvivalProbabilityCalculator>(configuration));
        }
        return cptyCalculators;
    };

    // log message

    ostringstream o;
    o << "SCENARIO_STATISTICS: Build Cube " << portfolio->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
    CONSOLEW(o.str());
    LOG(o.str());

    // set up progress indicators

    auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), ConsoleLog::instance().width(), ConsoleLog::instance().progressBarWidth());
    auto progressLog = boost::make_shared<ProgressLog>("Building cube", 100, ORE_NOTICE);

    if(inputs_->nThreads() == 1) {

        // single-threaded engine run

        ValuationEngine engine(inputs_->asof(), grid_, simMarket_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.buildCube(portfolio, cube_, calculators(), analytic()->configurations().scenarioGeneratorData->withMporStickyDate(),
                         nettingSetCube_, cptyCube_, cptyCalculators());
    } else {

        // multi-threaded engine run

        /* TODO we assume no netting output cube is needed. Currently there are no valuation calculators in ore that require this cube. */

        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> boost::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return boost::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return boost::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples,
                                                                                        cubeDepth_, 0.0f);
        };

        std::function<boost::shared_ptr<NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                                 const std::vector<QuantLib::Date>&,
                                                                 const QuantLib::Size)>
        cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                const std::vector<QuantLib::Date>& dates,
                                const Size samples) -> boost::shared_ptr<NPVCube> { return nullptr; };

        MultiThreadedValuationEngine engine(
            inputs_->nThreads(), inputs_->asof(), grid_, samples_,  analytic()->loader(), scenarioGenerator_,
            inputs_->simulationPricingEngine(), inputs_->curveConfigs().get(), analytic()->configurations().todaysMarketParams,
            inputs_->marketConfig("simulation"), analytic()->configurations().simMarketParams, false, false,
            boost::make_shared<ScenarioFilter>(), inputs_->refDataManager(),
            *inputs_->iborFallbackConfig(), true, false, cubeFactory, {}, cptyCubeFactory, "simulation");

        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, cptyCalculators,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate());

        cube_ = boost::make_shared<JointNPVCube>(engine.outputCubes(), portfolio->ids());
    }

    CONSOLE("OK");

    LOG("SCENARIO_STATISTICS::buildCube done");

    Settings::instance().evaluationDate() = inputs_->asof();
}

void ScenarioAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::set<std::string>& runTypes) {

    LOG("Scenario analytic called with asof " << io::iso_date(inputs_->asof()));

    Settings::instance().evaluationDate() = inputs_->asof();
    //ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    LOG("SCENARIO_STATISTICS: Build Today's Market");
    CONSOLEW("SCENARIO_STATISTICS: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
    cubeInterpreter_ = boost::make_shared<CubeInterpretation>(
        inputs_->storeFlows(), analytic()->configurations().scenarioGeneratorData->withCloseOutLag(), scenarioData_,
        grid_);

    LOG("SCENARIO_STATISTICS: Build simulation market");
    buildScenarioSimMarket();

    LOG("SCENARIO_STATISTICS: Build Scenario Generator");
    auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
    auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
    bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);
    buildScenarioGenerator(continueOnErr);

    LOG("SCENARIO_STATISTICS: Attach Scenario Generator to ScenarioSimMarket");
    simMarket_->scenarioGenerator() = scenarioGenerator_;

    analytic()->buildPortfolio();

    // Allocate cubes for the sub-portfolio we are processing here
    initRun(analytic()->portfolio());

    // This is where the valuation work is done
    buildCube(analytic()->portfolio());

    LOG("NPV cube generation completed");

    MEM_LOG;

    // Output scenario statistics and distribution reports
    const vector<RiskFactorKey>& keys = simMarket_->baseScenario()->keys();
    if (inputs_->scenrioOutputZeroRate()) {
        // Transform discount factors into zero rates
        auto sgTransformed = boost::make_shared<ScenarioGeneratorTransform>(
            scenarioGenerator_, simMarket_, analytic()->configurations().simMarketParams);

            auto statsReport = boost::make_shared<InMemoryReport>();
        sgTransformed->reset();
            ReportWriter().writeScenarioStatistics(sgTransformed, keys,
                                               analytic()->configurations().scenarioGeneratorData->samples(),
                                               grid_->valuationDates(), *statsReport);
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario_statistics"] = statsReport;

        auto distributionReport = boost::make_shared<InMemoryReport>();
        sgTransformed->reset();
        ReportWriter().writeScenarioDistributions(
            sgTransformed, keys, analytic()->configurations().scenarioGeneratorData->samples(),
            grid_->valuationDates(), inputs_->scenarioDistributionSteps(), *distributionReport);
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario_distribution"] = distributionReport;
    } else {
        auto statsReport = boost::make_shared<InMemoryReport>();
        scenarioGenerator_->reset();
        ReportWriter().writeScenarioStatistics(scenarioGenerator_, keys,
                                               analytic()->configurations().scenarioGeneratorData->samples(),
                                               grid_->valuationDates(), *statsReport);
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario_statistics"] = statsReport;

        auto distributionReport = boost::make_shared<InMemoryReport>();
        scenarioGenerator_->reset();
        ReportWriter().writeScenarioDistributions(
            scenarioGenerator_, keys, analytic()->configurations().scenarioGeneratorData->samples(),
            grid_->valuationDates(), inputs_->scenarioDistributionSteps(), *distributionReport);
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario_distribution"] = distributionReport;
    }


    // reset that mode
    //ObservationMode::instance().setMode(inputs_->observationModel());
}

} // namespace analytics
} // namespace ore
