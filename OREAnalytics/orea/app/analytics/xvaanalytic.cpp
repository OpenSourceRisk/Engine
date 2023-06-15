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
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/multistatenpvcalculator.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/observationmode.hpp>
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

void  XvaAnalyticImpl::checkConfigurations(const boost::shared_ptr<Portfolio>& portfolio) {
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
                        "XvaAnalytic", "Inconsistent MPoR period",
                        "For netting set " + key +", close-out lag is not consistent with the netting-set's mpor "));
            }
        }
    }
}

boost::shared_ptr<EngineFactory> XvaAnalyticImpl::engineFactory() {
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


void XvaAnalyticImpl::buildScenarioSimMarket() {
    
    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(
            analytic()->market(),
            analytic()->configurations().simMarketParams,
            boost::make_shared<FixingManager>(inputs_->asof()),
            configuration,
            *inputs_->curveConfigs()[0],
            *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), 
            false, true, false,
            *inputs_->iborFallbackConfig(),
            false);
}

void XvaAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError) {
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
        analytic()->reports()["XVA"]["scenario"] = report;
        scenarioGenerator_ = boost::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void XvaAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ")");
    CrossAssetModelBuilder modelBuilder(
        analytic()->market(), analytic()->configurations().crossAssetModelData, inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "",
        inputs_->salvageCorrelationMatrix() ? SalvagingAlgorithm::Spectral : SalvagingAlgorithm::None);
    model_ = *modelBuilder.model();
}

void XvaAnalyticImpl::initCubeDepth() {
    if (cubeDepth_ == 0) {
        LOG("XVA: Set cube depth");
        cubeDepth_ = cubeInterpreter_->requiredNpvCubeDepth();
        LOG("XVA: Cube depth set to: " << cubeDepth_);
    }
}

void XvaAnalyticImpl::initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth) {

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


void XvaAnalyticImpl::initClassicRun(const boost::shared_ptr<Portfolio>& portfolio) {
        
    LOG("XVA: initClassicRun");

    initCubeDepth();

    // May have been set already
    if (scenarioData_.empty()) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
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


boost::shared_ptr<Portfolio> XvaAnalyticImpl::classicRun(const boost::shared_ptr<Portfolio>& portfolio) {
    LOG("XVA: classicRun");


    Size n = portfolio->size();
    // Create a new empty portfolio, fill it and link it to the simulation market
    // We don't use Analytic::buildPortfolio() here because we are possibly dealing with a sub-portfolio only.
    LOG("XVA: Build classic portfolio of size " << n << " linked to the simulation market");
    CONSOLEW("XVA: Build Portfolio");
    classicPortfolio_ = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());
    portfolio->reset();
    for (const auto& [tradeId, trade] : portfolio->trades())
        classicPortfolio_->add(trade);    
    QL_REQUIRE(analytic()->market(), "today's market not set");
    boost::shared_ptr<EngineFactory> factory = engineFactory();
    classicPortfolio_->build(factory, "analytic/" + label());
    Date maturityDate = inputs_->asof();
    if (inputs_->portfolioFilterDate() != Null<Date>())
        maturityDate = inputs_->portfolioFilterDate();
    LOG("Filter trades that expire before " << maturityDate);
    classicPortfolio_->removeMatured(maturityDate);
    CONSOLE("OK");

    // Allocate cubes for the sub-portfolio we are processing here
    initClassicRun(classicPortfolio_);

    // This is where the valuation work is done
    buildClassicCube(classicPortfolio_);

    LOG("XVA: classicRun completed");

    return classicPortfolio_;
}


void XvaAnalyticImpl::buildClassicCube(const boost::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA::buildCube"); 
    
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
    o << "XVA: Build Cube " << portfolio->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
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
            cptyCubeFactory;
        if (inputs_->storeSurvivalProbabilities()) {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> boost::shared_ptr<NPVCube> {
                return boost::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            };
        } else {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> boost::shared_ptr<NPVCube> { return nullptr; };
        }

        MultiThreadedValuationEngine engine(
            inputs_->nThreads(), inputs_->asof(), grid_, samples_,  analytic()->loader(), scenarioGenerator_,
            inputs_->simulationPricingEngine(), inputs_->curveConfigs()[0], analytic()->configurations().todaysMarketParams,
            inputs_->marketConfig("simulation"), analytic()->configurations().simMarketParams, false, false,
            boost::make_shared<ScenarioFilter>(), inputs_->refDataManager(),
            *inputs_->iborFallbackConfig(), true, false, cubeFactory, {}, cptyCubeFactory, "xva-simulation");

        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, cptyCalculators,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate());

        cube_ = boost::make_shared<JointNPVCube>(engine.outputCubes(), portfolio->ids());

        if (inputs_->storeSurvivalProbabilities())
            cptyCube_ = boost::make_shared<JointNPVCube>(
                engine.outputCptyCubes(), portfolio->counterparties(), false,
                [](Real a, Real x) { return std::max(a, x); }, 0.0);
    }

    CONSOLE("OK");

    LOG("XVA::buildCube done");

    Settings::instance().evaluationDate() = inputs_->asof();
}

boost::shared_ptr<EngineFactory>
XvaAnalyticImpl::amcEngineFactory(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                               const std::vector<Date>& grid) {
    LOG("XvaAnalytic::engineFactory() called");
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->amcPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; 
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders;
    auto factory = boost::make_shared<EngineFactory>(
        edCopy, analytic()->market(), configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
        EngineBuilderFactory::instance().generateAmcEngineBuilders(cam, grid), true);
    return factory;
}

void XvaAnalyticImpl::buildAmcPortfolio() {
    LOG("XVA: buildAmcPortfolio");
    CONSOLEW("XVA: Build AMC portfolio");

    LOG("buildAmcPortfolio: Check sim dates");
    std::vector<Date> simDates =
        analytic()->configurations().scenarioGeneratorData->withCloseOutLag() && !analytic()->configurations().scenarioGeneratorData->withMporStickyDate() ?
        analytic()->configurations().scenarioGeneratorData->getGrid()->dates() : analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();
    
    LOG("buildAmcPortfolio: Register additional engine builders");
    auto factory = amcEngineFactory(model_, simDates);

    LOG("buildAmcPortfolio: Load Portfolio");
    boost::shared_ptr<Portfolio> portfolio = inputs_->portfolio();

    LOG("Build Portfolio with AMC Engine factory and select amc-enabled trades")
    amcPortfolio_ = boost::make_shared<Portfolio>();
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        if (inputs_->amcTradeTypes().find(trade->tradeType()) != inputs_->amcTradeTypes().end()) {
            try {
                trade->reset();
                trade->build(factory);
                amcPortfolio_->add(trade);
                DLOG("trade " << tradeId << " is added to amc portfolio");
            } catch (const std::exception& e) {
                ALOG(StructuredTradeErrorMessage(trade, "Error building trade for AMC simulation", e.what()));
            }
        }
    }
    LOG("AMC portfolio built, size is " << amcPortfolio_->size());

    CONSOLE("OK");

    LOG("XVA: buildAmcPortfolio completed");
}

void XvaAnalyticImpl::amcRun(bool doClassicRun) {

    LOG("XVA: amcRun");

    if (scenarioData_.empty()) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_.linkTo(
            boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_));
        simMarket_->aggregationScenarioData() = *scenarioData_;
    }

    initCubeDepth();

    std::string message = "XVA: Build AMC Cube " + std::to_string(amcPortfolio_->size()) + " x " +
                          std::to_string(grid_->valuationDates().size()) + " x " + std::to_string(samples_) + "... ";
    auto progressBar = boost::make_shared<SimpleProgressBar>(message, ConsoleLog::instance().width(), ConsoleLog::instance().progressBarWidth());
    auto progressLog = boost::make_shared<ProgressLog>("Building AMC Cube...", 100, ORE_NOTICE);

    if (inputs_->nThreads() == 1) {
        initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);
        AMCValuationEngine amcEngine(model_, inputs_->scenarioGeneratorData(), analytic()->market(),
                                     inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
                                     inputs_->exposureSimMarketParams()->additionalScenarioDataCcys());
        amcEngine.registerProgressIndicator(progressBar);
        amcEngine.registerProgressIndicator(progressLog);
        // We only need to generate asd, if this does not happen in the classic run
        if (!doClassicRun)
            amcEngine.aggregationScenarioData() = *scenarioData_;
        amcEngine.buildCube(amcPortfolio_, amcCube_);
    } else {
        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> boost::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return boost::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return boost::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples,
                                                                                        cubeDepth_, 0.0f);
        };
        AMCValuationEngine amcEngine(
            inputs_->nThreads(), inputs_->asof(), samples_,  analytic()->loader(), inputs_->scenarioGeneratorData(),
            inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
            inputs_->exposureSimMarketParams()->additionalScenarioDataCcys(), inputs_->crossAssetModelData(),
            inputs_->amcPricingEngine(), inputs_->curveConfigs()[0], analytic()->configurations().todaysMarketParams,
            inputs_->marketConfig("lgmcalibration"), inputs_->marketConfig("fxcalibration"),
            inputs_->marketConfig("eqcalibration"), inputs_->marketConfig("infcalibration"),
            inputs_->marketConfig("crcalibration"), inputs_->marketConfig("simulation"), inputs_->refDataManager(),
            *inputs_->iborFallbackConfig(), true, cubeFactory);

        amcEngine.registerProgressIndicator(progressBar);
        amcEngine.registerProgressIndicator(progressLog);
        // as for the single-threaded case, we only need to generate asd, if this does not happen in the classic run
        if (!doClassicRun)
            amcEngine.aggregationScenarioData() = *scenarioData_;
        amcEngine.buildCube(amcPortfolio_);
        amcCube_ = boost::make_shared<JointNPVCube>(amcEngine.outputCubes());
    }

    CONSOLE("OK");

    LOG("XVA: amcRun completed");
}

void XvaAnalyticImpl::runPostProcessor() {
    boost::shared_ptr<NettingSetManager> netting = inputs_->nettingSetManager();
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
        ALOG("dim calculator not set, create RegressionDynamicInitialMarginCalculator");
        dimCalculator_ = boost::make_shared<RegressionDynamicInitialMarginCalculator>(
            inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, *scenarioData_, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder,
            dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth);
    }

    std::vector<Period> cvaSensiGrid = inputs_->cvaSensiGrid();
    Real cvaSensiShiftSize = inputs_->cvaSensiShiftSize();

    string flipViewBorrowingCurvePostfix = inputs_->flipViewBorrowingCurvePostfix();
    string flipViewLendingCurvePostfix = inputs_->flipViewLendingCurvePostfix();

    LOG("baseCurrency " << baseCurrency);
    
    bool withMporStickyDate = analytic()->configurations().scenarioGeneratorData->withMporStickyDate();
    ScenarioGeneratorData::MporCashFlowMode mporCashFlowMode = analytic()->configurations().scenarioGeneratorData->mporCashFlowMode();
    if (withMporStickyDate)
        QL_REQUIRE(mporCashFlowMode == ScenarioGeneratorData::MporCashFlowMode::NonePay, "MporMode StickyDate supports only MporCashFlowMode NonePay");
    if (!inputs_->storeFlows() && !withMporStickyDate)
        QL_REQUIRE(mporCashFlowMode == ScenarioGeneratorData::MporCashFlowMode::BothPay, "If cube does not hold any mpor flows and MporMode is set to ActualDate, then MporCashFlowMode must be set to BothPay");
    
    postProcess_ = boost::make_shared<PostProcess>(
        analytic()->portfolio(), netting, analytic()->market(), marketConfiguration, cube_, *scenarioData_, analytics,
        baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix, inputs_->creditSimulationParameters(), inputs_->creditMigrationDistributionGrid(),
        inputs_->creditMigrationTimeSteps(), creditStateCorrelationMatrix(), withMporStickyDate, mporCashFlowMode);
    LOG("post done");
}

void XvaAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::set<std::string>& runTypes) {
    
    LOG("XVA analytic called with asof " << io::iso_date(inputs_->asof()));

    if (runTypes.find("EXPOSURE") != runTypes.end() || runTypes.empty())
        runSimulation_ = true;

    if (runTypes.find("XVA") != runTypes.end() || runTypes.empty())
        runXva_ = true;

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    LOG("XVA: Build Today's Market");
    CONSOLEW("XVA: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    
    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
    cubeInterpreter_ = boost::make_shared<CubeInterpretation>(
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
        auto residualPortfolio = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());

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
            amcPortfolio_ = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());
        
        if (doClassicRun)
            classicPortfolio_ = classicRun(residualPortfolio);
        else
            classicPortfolio_ = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());

        /***************************************************
         * We may have two cubes now that need to be merged
         ***************************************************/
        
        if (doClassicRun && doAmcRun) {
            LOG("Joining classical and AMC cube");
            cube_ = boost::make_shared<JointNPVCube>(cube_, amcCube_);
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
        auto newPortfolio = boost::make_shared<Portfolio>();
        for (const auto& [tradeId, trade] : classicPortfolio_->trades())
            newPortfolio->add(trade);
        for (const auto& [tradeId, trade] : amcPortfolio_->trades())
            newPortfolio->add(trade);
        LOG("Total portfolio size " << newPortfolio->size());
        if (newPortfolio->size() < inputs_->portfolio()->size()) {
            ALOG("input portfolio size is " << inputs_->portfolio()->size() << 
                ", but we have built only " << newPortfolio->size() << " trades");
        }
        analytic()->setPortfolio(newPortfolio);
    } else { // runSimulation_

        // build the portfolio linked to today's market
        analytic()->buildPortfolio();

        // ... and load a pre-built cube for post-processing

        LOG("Skip cube generation, load input cubes for XVA");
        CONSOLEW("XVA: Load Cubes");
        QL_REQUIRE(inputs_->cube(), "XVA without EXPOSURE requires an NPV cube as input");
        cube_= inputs_->cube();
        QL_REQUIRE(inputs_->mktCube(), "XVA without EXPOSURE requires a market cube as input");
        scenarioData_.linkTo(inputs_->mktCube());
        if (inputs_->nettingSetCube())
            nettingSetCube_= inputs_->nettingSetCube();
        if (inputs_->cptyCube())
            cptyCube_ = inputs_->cptyCube();
        CONSOLE("OK");
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
        auto report = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString()).writeCube(*report, cube_, nettingSetMap);
        analytic()->reports()["XVA"]["rawcube"] = report;
    }

    if (runXva_) {

        /*********************************************************************
         * This is where the aggregation work is done: call the post-processor
         *********************************************************************/

        CONSOLEW("XVA: Aggregation");
        runPostProcessor();
        CONSOLE("OK");

        /******************************************************
         * Finally generate various (in-memory) reports/outputs
         ******************************************************/

        CONSOLEW("XVA: Reports");
        LOG("Generating XVA reports and cube outputs");

        if (inputs_->exposureProfilesByTrade()) {
            for (const auto& [tradeId, tradeIdCubePos] : postProcess_->tradeIds()) {
                auto report = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeTradeExposures(*report, postProcess_, tradeId);
                analytic()->reports()["XVA"]["exposure_trade_" + tradeId] = report;
            }
        }

        if (inputs_->exposureProfiles()) {
            for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
                auto exposureReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["exposure_nettingset_" + nettingSet] = exposureReport;

                auto colvaReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["colva_nettingset_" + nettingSet] = colvaReport;

                auto cvaSensiReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["cva_sensitivity_nettingset_" + nettingSet] = cvaSensiReport;
            }
        }

        auto xvaReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), analytic()->portfolio(), postProcess_);
        analytic()->reports()["XVA"]["xva"] = xvaReport;

        if (inputs_->netCubeOutput()) {
            auto report = boost::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString()).writeCube(*report, postProcess_->netCube());
            analytic()->reports()["XVA"]["netcube"] = report;
        }

        if (inputs_->dimAnalytic() || inputs_->mvaAnalytic()) {
            // Generate DIM evolution report
            auto dimEvolutionReport = boost::make_shared<InMemoryReport>();
            postProcess_->exportDimEvolution(*dimEvolutionReport);
            analytic()->reports()["XVA"]["dim_evolution"] = dimEvolutionReport;

            // Generate DIM regression reports
            vector<boost::shared_ptr<ore::data::Report>> dimRegReports;
            for (Size i = 0; i < inputs_->dimOutputGridPoints().size(); ++i) {
                auto rep = boost::make_shared<InMemoryReport>();
                dimRegReports.push_back(rep);
                analytic()->reports()["XVA"]["dim_regression_" + to_string(i)] = rep;
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
                auto rep = boost::make_shared<InMemoryReport>();
                analytic()->reports()["XVA"]["credit_migration_" + to_string(inputs_->creditMigrationTimeSteps()[i])] =
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
    }

    // reset that mode
    ObservationMode::instance().setMode(inputs_->observationModel());
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
