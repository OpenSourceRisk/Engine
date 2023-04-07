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

#include <orea/app/analytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/bufferedsensitivitystream.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/sensitivityanalysisplus.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/zerotoparcube.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>

#include <ored/marketdata/todaysmarket.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/builders/currencyswap.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/multilegoption.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

#include <boost/timer/timer.hpp>

#include <iostream>

using namespace ore::analytics;
using namespace ore::data;
using namespace boost::filesystem;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

Analytic::Analytic(std::unique_ptr<Impl> impl,
         const std::set<std::string>& analyticTypes,
         const boost::shared_ptr<InputParameters>& inputs,
         bool simulationConfig,
         bool sensitivityConfig,
         bool scenarioGeneratorConfig,
         bool crossAssetModelConfig)
    : impl_(std::move(impl)), types_(analyticTypes), inputs_(inputs) {
    
    // set these here, can be overwritten in setUpConfigurations
    if (inputs->curveConfigs().size() > 0)
        configurations().curveConfig = inputs->curveConfigs()[0];
    if (inputs->pricingEngine())
        configurations().engineData = inputs->pricingEngine();

    configurations().simulationConfigRequired = simulationConfig;
    configurations().sensitivityConfigRequired = sensitivityConfig;
    configurations().scenarioGeneratorConfigRequired = scenarioGeneratorConfig;
    configurations().crossAssetModelConfigRequired = crossAssetModelConfig;

    if (impl_) {
        impl_->setAnalytic(this);
        impl_->setGenerateAdditionalResults(inputs_->outputAdditionalResults());
    }

    setUpConfigurations();
}

void Analytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                         const std::set<std::string>& runTypes) {
    if (impl_)
        impl_->runAnalytic(loader, runTypes);
}

void Analytic::setUpConfigurations() {
    if (impl_)
        impl_->setUpConfigurations();
}

const std::string Analytic::label() const { 
    return impl_ ? impl_->label() : string(); 
}

// Analytic
bool Analytic::match(const std::set<std::string>& runTypes) {
    if (runTypes.size() == 0)
        return true;
    
    for (const auto& rt : runTypes) {
        if (types_.find(rt) != types_.end()) {
            LOG("Requested analytics " <<  to_string(runTypes) << " match analytic class " << label());
            return true;
        }
    }
    WLOG("None of the requested analytics " << to_string(runTypes) << " are covered by the analytic class " << label());
    return false;
}

std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> Analytic::todaysMarketParams() {
    buildConfigurations();
    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    if (configurations().todaysMarketParams)
        tmps.push_back(configurations().todaysMarketParams);

    for (const auto& a : dependentAnalytics_) {
        auto ctmps = a.second->todaysMarketParams();
        tmps.insert(end(tmps), begin(ctmps), end(ctmps));
    }

    return tmps;
}

boost::shared_ptr<EngineFactory> Analytic::Impl::engineFactory() {
    LOG("Analytic::engineFactory() called");
    // Note: Calling the constructor here with empty extry builders
    // Override this function in case you have got extra ones
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = to_string(generateAdditionalResults());
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    LOG("MarketContext::pricing = " << inputs_->marketConfig("pricing"));
    return boost::make_shared<EngineFactory>(edCopy, analytic()->market(), configurations,
                                             inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

void Analytic::buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const bool marketRequired) {
    LOG("Analytic::buildMarket called");    
    cpu_timer mtimer;

    QL_REQUIRE(loader, "market data loader not set");
    QL_REQUIRE(configurations().curveConfig, "curve configurations not set");
    
    // first build the market if we have a todaysMarketParams
    if (configurations().todaysMarketParams) {
        try {
            // Note: we usually update the loader with implied data, but we simply use the provided loader here
             loader_ = loader;
            // Check that the loader has quotes
            QL_REQUIRE( loader_->hasQuotes(inputs()->asof()),
                       "There are no quotes available for date " << inputs()->asof());
            // Build the market
            market_ = boost::make_shared<TodaysMarket>(inputs()->asof(), configurations().todaysMarketParams, loader_,
                                                       configurations().curveConfig, inputs()->continueOnError(),
                                                       true, inputs()->lazyMarketBuilding(), inputs()->refDataManager(),
                                                       false, *inputs()->iborFallbackConfig());
            // Note: we usually wrap the market into a PC market, but skip this step here
        } catch (const std::exception& e) {
            if (marketRequired)
                QL_FAIL("Failed to build market: " << e.what());
            else
                WLOG("Failed to build market: " << e.what());
        }
    } else {
        ALOG("Skip building the market due to missing today's market parameters in configurations"); 
    }
    mtimer.stop();
    LOG("Market Build time " << setprecision(2) << mtimer.format(default_places, "%w") << " sec");
}

void Analytic::marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr) {
    if (mcr)
        mcr->populateReport(market_, configurations().todaysMarketParams);
}

void Analytic::buildPortfolio() {
    // create a new empty portfolio
    portfolio_ = boost::make_shared<Portfolio>(inputs()->buildFailedTrades());

    inputs()->portfolio()->reset();
    // populate with trades
    for (const auto& [tradeId, trade] : inputs()->portfolio()->trades())
        portfolio()->add(trade);
    
    if (market_) {
        LOG("Build the portfolio");
        boost::shared_ptr<EngineFactory> factory = impl()->engineFactory();
        portfolio()->build(factory, "analytic/" + label());

        // remove dates that will have matured
        Date maturityDate = inputs()->asof();
        if (inputs()->portfolioFilterDate() != Null<Date>())
            maturityDate = inputs()->portfolioFilterDate();

        LOG("Filter trades that expire before " << maturityDate);
        portfolio()->removeMatured(maturityDate);
    } else {
        ALOG("Skip building the portfolio, because market not set");
    }
}

/*******************************************************************
 * MARKET Analytic
 *******************************************************************/

void MarketDataAnalyticImpl::setUpConfigurations() {    
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void MarketDataAnalyticImpl::runAnalytic( 
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
    const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    CONSOLEW("Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    /*
    if (inputs_->outputTodaysMarketCalibration()) {
        CONSOLEW("Market Calibration");
        LOG("Write todays market calibration report");
        auto t = boost::dynamic_pointer_cast<TodaysMarket>(analytic()->market());
        QL_REQUIRE(t != nullptr, "expected todays market instance");
        boost::shared_ptr<InMemoryReport> mktReport = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeTodaysMarketCalibrationReport(*mktReport, t->calibrationInfo());
        analytic()->reports()["MARKET"]["todaysmarketcalibration"] = mktReport;
        CONSOLE("OK");
    }

    if (inputs_->outputCurves()) {
        CONSOLEW("Curves Report");
        LOG("Write curves report");
        boost::shared_ptr<InMemoryReport> curvesReport = boost::make_shared<InMemoryReport>();
        DateGrid grid(inputs_->curvesGrid());
        std::string config = inputs_->curvesMarketConfig();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeCurves(*curvesReport, config, grid, *inputs_->todaysMarketParams(),
                         analytic()->market(), inputs_->continueOnError());
        analytic()->reports()["MARKET"]["curves"] = curvesReport;
        CONSOLE("OK");
    }
    */
}

/*******************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, STRESS
 *******************************************************************/

void PricingAnalyticImpl::setUpConfigurations() {    
    if (find(begin(analytic()->analyticTypes()), end(analytic()->analyticTypes()), "SENSITIVITY") !=
        end(analytic()->analyticTypes())) {
        analytic()->configurations().simulationConfigRequired = true;
        analytic()->configurations().sensitivityConfigRequired = true;
    }  

    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();

    setGenerateAdditionalResults(true);
}

void PricingAnalyticImpl::runAnalytic( 
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
    const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    QL_REQUIRE(inputs_->portfolio(), "PricingAnalytic::run: No portfolio loaded.");

    CONSOLEW("Pricing: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    // Check coverage
    for (const auto& rt : runTypes) {
        if (std::find(analytic()->analyticTypes().begin(), analytic()->analyticTypes().end(), rt) ==
            analytic()->analyticTypes().end()) {
            WLOG("requested analytic " << rt << " not covered by the PricingAnalytic");
        }
    }

    // This hook allows modifying the portfolio in derived classes before running the analytics below,
    // e.g. to apply SIMM exemptions.
    analytic()->modifyPortfolio();

    for (const auto& type : analytic()->analyticTypes()) {
        boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
        InMemoryReport tmpReport;
        // skip analytics not requested
        if (runTypes.find(type) == runTypes.end())
            continue;

        std::string effectiveResultCurrency =
            inputs_->resultCurrency().empty() ? inputs_->baseCurrency() : inputs_->resultCurrency();
        if (type == "NPV" || type == "NPV_LAGGED") {
            CONSOLEW("Pricing: NPV Report");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNpv(*report, effectiveResultCurrency, analytic()->market(), "",
                          analytic()->portfolio());
            analytic()->reports()[type]["npv"] = report;
            CONSOLE("OK");
            if (inputs_->outputAdditionalResults()) {
                CONSOLEW("Pricing: Additional Results");
                boost::shared_ptr<InMemoryReport> addReport = boost::make_shared<InMemoryReport>();;
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeAdditionalResultsReport(*addReport, analytic()->portfolio(), analytic()->market(),
                                                  effectiveResultCurrency);
                analytic()->reports()[type]["additional_results"] = addReport;
                CONSOLE("OK");
            }
            // FIXME: Leave this here as additional output within the NPV analytic, or store report as separate analytic?
            if (inputs_->outputTodaysMarketCalibration()) {
                CONSOLEW("Pricing: Market Calibration");
                LOG("Write todays market calibration report");
                auto t = boost::dynamic_pointer_cast<TodaysMarket>(analytic()->market());
                QL_REQUIRE(t != nullptr, "expected todays market instance");
                boost::shared_ptr<InMemoryReport> mktReport = boost::make_shared<InMemoryReport>();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeTodaysMarketCalibrationReport(*mktReport, t->calibrationInfo());
                analytic()->reports()[type]["todaysmarketcalibration"] = mktReport;
                CONSOLE("OK");
            }
            if (inputs_->outputCurves()) {
                CONSOLEW("Pricing: Curves Report");
                LOG("Write curves report");
                boost::shared_ptr<InMemoryReport> curvesReport = boost::make_shared<InMemoryReport>();
                DateGrid grid(inputs_->curvesGrid());
                std::string config = inputs_->curvesMarketConfig();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeCurves(*curvesReport, config, grid, *inputs_->todaysMarketParams(),
                                 analytic()->market(), inputs_->continueOnError());
                analytic()->reports()[type]["curves"] = curvesReport;
                CONSOLE("OK");
            }
        }
        else if (type == "CASHFLOW") {
            CONSOLEW("Pricing: Cashflow Report");
            string marketConfig = inputs_->marketConfig("pricing");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(*report, effectiveResultCurrency, analytic()->portfolio(),
                               analytic()->market(),
                               marketConfig, inputs_->includePastCashflows());
            analytic()->reports()[type]["cashflow"] = report;
            CONSOLE("OK");
        }
        else if (type == "CASHFLOWNPV") {
            CONSOLEW("Pricing: Cashflow NPV report");
            string marketConfig = inputs_->marketConfig("pricing");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(tmpReport, effectiveResultCurrency, analytic()->portfolio(),
                               analytic()->market(),
                               marketConfig, inputs_->includePastCashflows());
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflowNpv(*report, tmpReport, analytic()->market(), marketConfig,
                                  effectiveResultCurrency, inputs_->cashflowHorizon());
            analytic()->reports()[type]["cashflownpv"] = report;
            CONSOLE("OK");
        }
        else if (type == "STRESS") {
            CONSOLEW("Risk: Stress Test Report");
            LOG("Stress Test Analysis called");
            Settings::instance().evaluationDate() = inputs_->asof();
            std::string marketConfig = inputs_->marketConfig("pricing");
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
            boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
                analytic()->portfolio(), analytic()->market(), marketConfig, inputs_->pricingEngine(),
                inputs_->stressSimMarketParams(), inputs_->stressScenarioData(),
                *inputs_->curveConfigs().at(0), *analytic()->configurations().todaysMarketParams, nullptr,
                inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
                inputs_->continueOnError());
            stressTest->writeReport(report, inputs_->stressThreshold());
            analytic()->reports()[type]["stress"] = report;
            CONSOLE("OK");
        }
        else if (type == "SENSITIVITY") {
            CONSOLEW("Risk: Sensitivity Report");
            LOG("Sensi Analysis - Initialise");
            bool recalibrateModels = true;
            bool ccyConv = false;
            std::string configuration = inputs_->marketConfig("pricing");
	    boost::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
            if (inputs_->nThreads() == 1) {
                LOG("Single-threaded sensi analysis");
                std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
                std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    analytic()->portfolio(), analytic()->market(), configuration, inputs_->pricingEngine(),
                    analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                    recalibrateModels, inputs_->curveConfigs().at(0),
                    analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, false, inputs_->dryRun());
                LOG("Single-threaded sensi analysis created");
            }
            else {
                LOG("Multi-threaded sensi analysis");
                std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
                    const boost::shared_ptr<ReferenceDataManager>&, const boost::shared_ptr<TradeFactory>&)>
                    extraTradeBuilders = {};
                std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()> extraEngineBuilders = {};
                std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()> extraLegBuilders = {};
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    inputs_->nThreads(), inputs_->asof(), loader, analytic()->portfolio(),
                    Market::defaultConfiguration, inputs_->pricingEngine(),
                    analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                    recalibrateModels, inputs_->curveConfigs().at(0),
                    analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, false, inputs_->dryRun());
                LOG("Multi-threaded sensi analysis created");
            }
            // FIXME: Why are these disabled?
            set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
            boost::shared_ptr<ParSensitivityAnalysis> parAnalysis = nullptr;
            if (inputs_->parSensi() || inputs_->alignPillars()) {
                parAnalysis= boost::make_shared<ParSensitivityAnalysis>(
                    inputs_->asof(), analytic()->configurations().simMarketParams,
                    *analytic()->configurations().sensiScenarioData, "",
                    true, typesDisabled);
                if (inputs_->alignPillars()) {
                    LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
                    parAnalysis->alignPillars();
                    sensiAnalysis->overrideTenors(true);
                } else {
                    LOG("Sensi analysis - skip aligning pillars");
                }
            }

            LOG("Sensi analysis - generate");
            sensiAnalysis->registerProgressIndicator(boost::make_shared<ProgressLog>("sensitivities", 100, ORE_NOTICE));
            sensiAnalysis->generateSensitivities();

            LOG("Sensi analysis - write sensitivity report in memory");
            auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
            auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), baseCurrency);
            ReportWriter(inputs_->reportNaString())
                .writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity"] = report;

            LOG("Sensi analysis - write sensitivity scenario report in memory");
            boost::shared_ptr<InMemoryReport> scenarioReport = boost::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString())
                .writeScenarioReport(*scenarioReport, sensiAnalysis->sensiCube(),
                                     inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity_scenario"] = scenarioReport;

            if (inputs_->parSensi()) {
                LOG("Sensi analysis - par conversion");
                parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
                boost::shared_ptr<ParSensitivityConverter> parConverter =
                    boost::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
                auto parCube = boost::make_shared<ZeroToParCube>(sensiAnalysis->sensiCube(), parConverter, typesDisabled, true);
                LOG("Sensi analysis - write par sensitivity report in memory");
                boost::shared_ptr<ParSensitivityCubeStream> pss = boost::make_shared<ParSensitivityCubeStream>(parCube, baseCurrency);
                // If the stream is going to be reused - wrap it into a buffered stream to gain some
                // performance. The cost for this is the memory footpring of the buffer.
                // ss = boost::make_shared<ore::analytics::BufferedSensitivityStream>(ss);
                boost::shared_ptr<InMemoryReport> parSensiReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
                analytic()->reports()[type]["par_sensitivity"] = parSensiReport;

                if (inputs_->outputJacobi()) {
                    boost::shared_ptr<InMemoryReport> jacobiReport = boost::make_shared<InMemoryReport>();
                    writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
                    analytic()->reports()[type]["jacobi"] = jacobiReport;
                    
                    boost::shared_ptr<InMemoryReport> jacobiInverseReport = boost::make_shared<InMemoryReport>();
                    parConverter->writeConversionMatrix(*jacobiInverseReport);
                    analytic()->reports()[type]["jacobi_inverse"] = jacobiInverseReport;
                }
            }
            else {
                LOG("Sensi Analysis - skip par conversion");
            }
        
            LOG("Sensi Analysis - Completed");
            CONSOLE("OK");
        }
        else {
            QL_FAIL("PricingAnalytic type " << type << " invalid");
        }
    }
}

/***********************************************************************************
 * VAR Analytic: DELTA-VAR, DELTA-GAMMA-NORMAL-VAR, MONTE-CARLO-VAR
 ***********************************************************************************/

void VarAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void VarAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::set<std::string>& runTypes) {
    MEM_LOG;
    LOG("Running parametric VaR");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    LOG("VAR: Build Market");
    CONSOLEW("Risk: Build Market for VaR");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Risk: Build Portfolio for VaR");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    LOG("Build trade to portfolio id mapping");
    map<string, set<string>> tradePortfolio;
    for (auto const& [tradeId, trade] : analytic()->portfolio()->trades()) {
        tradePortfolio[tradeId].insert(trade->portfolioIds().begin(), trade->portfolioIds().end());
    }

    LOG("Build VaR calculator");
    auto calc = boost::make_shared<ParametricVarCalculator>(tradePortfolio, inputs_->portfolioFilter(), 
        inputs_->sensitivityStream(), inputs_->covarianceData(), inputs_->varQuantiles(), 
        inputs_->varMethod(), inputs_->mcVarSamples(), inputs_->mcVarSeed(), 
        inputs_->varBreakDown(), inputs_->salvageCovariance());

    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
    analytic()->reports()["VAR"]["var"] = report;

    LOG("Call VaR calculation");
    CONSOLEW("Risk: VaR Calculation");
    calc->calculate(*report);
    CONSOLE("OK");

    LOG("Parametric VaR completed");
    MEM_LOG;
}


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
                    WLOG(ore::analytics::StructuredAnalyticsWarningMessage(
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
    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
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
        analytic()->market(), analytic()->configurations().crossAssetModelData,
                                        inputs_->marketConfig("lgmcalibration"), inputs_->marketConfig("fxcalibration"),
                                        inputs_->marketConfig("eqcalibration"), inputs_->marketConfig("infcalibration"),
                                        inputs_->marketConfig("crcalibration"), inputs_->marketConfig("simulation"),
                                        false, continueOnCalibrationError);
    model_ = *modelBuilder.model();
}

void XvaAnalyticImpl::initCubeDepth() {

    if (cubeDepth_ == 0) {
        LOG("XVA: Set cube depth");
        // Determine the required cube depth:
        // - Without close-out grid and without storing flows we have a 3d cube (trades * dates * scenarios),
        //   otherwise we need a 4d "hypercube" with additonal dimension "depth"
        // - If we build an auxiliary close-out grid then we store default values at depth 0 and close-out at depth 1
        // - If we want to store cash flows that occur during the mpor, then we store them at depth 2
        cubeDepth_ = 1;
        if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag())
            cubeDepth_++;
        if (inputs_->storeFlows())
            cubeDepth_++;

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
    if (!scenarioData_) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
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
            calculators.push_back(boost::make_shared<MPORCalculator>(npvCalc, 0, 1));
        } else {
            calculators.push_back(boost::make_shared<NPVCalculator>(inputs_->exposureBaseCurrency()));
        }
        if (inputs_->storeFlows()) {
            // cash flow stored at index 1 (no close-out lag) or 2 (have close-out lag)
            calculators.push_back(boost::make_shared<CashflowCalculator>(inputs_->exposureBaseCurrency(),
                                                                         inputs_->asof(), grid_, cubeDepth_ - 1));
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

    // set cube interpretation depending on close-out lag

    if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag())
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(scenarioData_, grid_, inputs_->flipViewXVA());
    else
        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(scenarioData_, inputs_->flipViewXVA());

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

        // TODO we can skip the portfolio build and ssm build when using the mt engine?
        // TODO we assume no netting output cube is needed, this is only used by the sensitivity calculator in ore+

        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> boost::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return boost::make_shared<ore::analytics::SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return boost::make_shared<ore::analytics::SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples,
                                                                                        cubeDepth_, 0.0f);
        };

        std::function<boost::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                                 const std::vector<QuantLib::Date>&,
                                                                 const QuantLib::Size)>
            cptyCubeFactory;
        if (inputs_->storeSurvivalProbabilities()) {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> boost::shared_ptr<NPVCube> {
                return boost::make_shared<ore::analytics::SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
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
            boost::make_shared<ore::analytics::ScenarioFilter>(), inputs_->refDataManager(),
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
                // building the trades is only required for single-threaded runs
                if (inputs_->nThreads() == 1) {
                    trade->reset();
                    trade->build(factory);
                }
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
    
    if (!scenarioData_) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
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
            amcEngine.aggregationScenarioData() = scenarioData_;
        amcEngine.buildCube(amcPortfolio_, amcCube_);
    } else {
        auto cubeFactory = [this](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> boost::shared_ptr<NPVCube> {
            if (cubeDepth_ == 1)
                return boost::make_shared<ore::analytics::SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
            else
                return boost::make_shared<ore::analytics::SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples,
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
            amcEngine.aggregationScenarioData() = scenarioData_;
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

    if (!cubeInterpreter_) {
        // FIXME: Can we get the grid from the cube instead?
        QL_REQUIRE(analytic()->configurations().scenarioGeneratorData, "scenario generator data not set");
        boost::shared_ptr<ScenarioGeneratorData> sgd = analytic()->configurations().scenarioGeneratorData;
        LOG("withCloseOutLag=" << (sgd->withCloseOutLag() ? "Y" : "N"));
        if (sgd->withCloseOutLag())
            cubeInterpreter_ =
                boost::make_shared<MporGridCubeInterpretation>(scenarioData_, sgd->getGrid(), analytics["flipViewXVA"]);
        else
            cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(scenarioData_, analytics["flipViewXVA"]);
    }

    if (!dimCalculator_ && (analytics["mva"] || analytics["dim"])) {
        ALOG("dim calculator not set, create RegressionDynamicInitialMarginCalculator");
        dimCalculator_ = boost::make_shared<RegressionDynamicInitialMarginCalculator>(
            inputs_, analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder,
            dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth);
    }

    std::vector<Period> cvaSensiGrid = inputs_->cvaSensiGrid();
    Real cvaSensiShiftSize = inputs_->cvaSensiShiftSize();

    string flipViewBorrowingCurvePostfix = inputs_->flipViewBorrowingCurvePostfix();
    string flipViewLendingCurvePostfix = inputs_->flipViewLendingCurvePostfix();

    LOG("baseCurrency " << baseCurrency);

    postProcess_ = boost::make_shared<PostProcess>(
        analytic()->portfolio(), netting, analytic()->market(), marketConfiguration, cube_, scenarioData_, analytics,
        baseCurrency,
        allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix);
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
        scenarioData_ = inputs_->mktCube();
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
        analytic()->mktCubes()["XVA"]["scenariodata"] = scenarioData_;
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
        ore::analytics::ReportWriter(inputs_->reportNaString()).writeCube(*report, cube_, nettingSetMap);
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
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeTradeExposures(*report, postProcess_, tradeId);
                analytic()->reports()["XVA"]["exposure_trade_" + tradeId] = report;
            }
        }

        if (inputs_->exposureProfiles()) {
            for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
                auto exposureReport = boost::make_shared<InMemoryReport>();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["exposure_nettingset_" + nettingSet] = exposureReport;

                auto colvaReport = boost::make_shared<InMemoryReport>();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["colva_nettingset_" + nettingSet] = colvaReport;

                auto cvaSensiReport = boost::make_shared<InMemoryReport>();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
                analytic()->reports()["XVA"]["cva_sensitivity_nettingset_" + nettingSet] = cvaSensiReport;
            }
        }

        auto xvaReport = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), analytic()->portfolio(), postProcess_);
        analytic()->reports()["XVA"]["xva"] = xvaReport;

        if (inputs_->netCubeOutput()) {
            auto report = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString()).writeCube(*report, postProcess_->netCube());
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

        CONSOLE("OK");
    }

    // reset that mode
    ObservationMode::instance().setMode(inputs_->observationModel());
}

} // namespace analytics
} // namespace ore
