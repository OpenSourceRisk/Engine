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

#include <boost/timer/timer.hpp>

#include <iostream>

using namespace ore::analytics;
using namespace ore::data;
using namespace boost::filesystem;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

// Analytic
std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> Analytic::todaysMarketParams() {
    buildConfigurations();
    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    if (configurations_.todaysMarketParams)
        tmps.push_back(configurations_.todaysMarketParams);
    return tmps;
}


boost::shared_ptr<EngineFactory> Analytic::engineFactory() {
    LOG("Analytic::engineFactory() called");
    // Note: Calling the constructor here with empty extry builders
    // Override this function in case you have got extra ones
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; 
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders; 
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    LOG("MarketContext::pricing = " << inputs_->marketConfig("pricing"));
    //configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    return boost::make_shared<EngineFactory>(edCopy, market_, configurations, extraEngineBuilders, extraLegBuilders,
                                             inputs_->refDataManager(), *inputs_->iborFallbackConfig());
}

boost::shared_ptr<ore::data::EngineFactory> PricingAnalytic::engineFactory() {
    LOG("PricingAnalytic::engineFactory() called");
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    LOG("MarketContext::pricing = " << inputs_->marketConfig("pricing"));
    //configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders;
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
    return boost::make_shared<EngineFactory>(edCopy, market_, configurations, extraBuilders, extraLegBuilders,
                                             inputs_->refDataManager(), *inputs_->iborFallbackConfig());
}

void Analytic::buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const boost::shared_ptr<CurveConfigurations>& curveConfig, 
                           const bool marketRequired) {
    LOG("Analytic::buildMarket called");    
    cpu_timer mtimer;
    
    // first build the market if we have a todaysMarketParams
    if (configurations_.todaysMarketParams) {
        try {
            // Note: we usually update the loader with implied data, but we simply use the provided loader here
            loader_ = loader;
            // Check that the loader has quotes
            QL_REQUIRE(loader_->hasQuotes(inputs_->asof()),
                       "There are no quotes available for date " << inputs_->asof());
            // Build the market
            market_ = boost::make_shared<TodaysMarket>(inputs_->asof(), configurations_.todaysMarketParams, loader_,
                                                       curveConfig, inputs_->continueOnError(),
                                                       true, inputs_->lazyMarketBuilding(), inputs_->refDataManager(),
                                                       false, *inputs_->iborFallbackConfig());
            // Note: we usually wrap the market into a PC market, but skip this step here
        } catch (const std::exception& e) {
            if (marketRequired)
                QL_FAIL("Failed to build market: " << e.what());
            else
                WLOG("Failed to build market: " << e.what());
        }
    } else {
        LOG("Skip building the market due to missing today's market parameters in configurations"); 
    }
    mtimer.stop();
    LOG("Market Build time " << setprecision(2) << mtimer.format(default_places, "%w") << " sec");
}

void Analytic::marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr) {
    if (mcr)
        mcr->populateReport(market_, configurations_.todaysMarketParams);
}

void Analytic::buildPortfolio() {
    // create a new empty portfolio
    portfolio_ = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());

    inputs_->portfolio()->reset();
    // populate with trades
    for (const auto& [tradeId, trade] : inputs_->portfolio()->trades())
        portfolio_->add(trade);
    
    if (market_) {
        LOG("Build the portfolio");
        boost::shared_ptr<EngineFactory> factory = engineFactory();
        portfolio_->build(factory, "analytic/" + label_);

        // remove dates that will have matured
        Date maturityDate = inputs_->asof();
        if (inputs_->portfolioFilterDate() != Null<Date>())
            maturityDate = inputs_->portfolioFilterDate();

        LOG("Filter trades that expire before " << maturityDate);
        portfolio_->removeMatured(maturityDate);
    } else {
        LOG("Skip building the portfolio");
    }
}

/*******************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, STRESS
 *******************************************************************/

void PricingAnalytic::setUpConfigurations() {
    configurations_.todaysMarketParams = inputs_->todaysMarketParams();
    configurations_.simMarketParams = inputs_->sensiSimMarketParams();
    configurations_.sensiScenarioData = inputs_->sensiScenarioData();
}

void PricingAnalytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::vector<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    QL_REQUIRE(inputs_->portfolio(), "PricingAnalytic::run: No portfolio loaded.");

    out_ << setw(tab_) << left << "Pricing: Build Market " << flush;
    buildMarket(loader, inputs_->curveConfigs()[0]);
    out_ << "OK" << std::endl;

    out_ << setw(tab_) << left << "Pricing: Build Portfolio " << flush;
    buildPortfolio();
    out_ << "OK" << std::endl;

    // Check coverage
    for (const auto& rt : runTypes) {
        if (std::find(types_.begin(), types_.end(), rt) == types_.end()) {
            WLOG("requested analytic " << rt << " not covered by the PricingAnalytic");
        }
    }
    
    for (const auto& analytic : types_) {
        boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
        InMemoryReport tmpReport;
        // skip analytics not requested
        if (runTypes.size() > 0 && std::find(runTypes.begin(), runTypes.end(), analytic) == runTypes.end())
            continue;

        if (analytic == "NPV") {
            out_ << setw(tab_) << left << "Pricing: NPV Report " << flush;
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNpv(*report, inputs_->baseCurrency(), market_, "", portfolio_);
            reports_[analytic]["npv"] = report;
            out_ << "OK" << endl;
            if (inputs_->outputAdditionalResults()) {
                out_ << setw(tab_) << left << "Pricing: Additional Results " << flush;
                boost::shared_ptr<InMemoryReport> addReport = boost::make_shared<InMemoryReport>();;
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeAdditionalResultsReport(*addReport, portfolio_, market_, inputs_->baseCurrency());
                reports_[analytic]["additional_results"] = addReport;
                out_ << "OK" << endl;
            }
            // FIXME: Leave this here as additional output within the NPV analytic?
            if (inputs_->outputTodaysMarketCalibration()) {
                out_ << setw(tab_) << left << "Pricing: Market Calibration " << flush;
                LOG("Write todays market calibration report");
                auto t = boost::dynamic_pointer_cast<TodaysMarket>(market_);
                QL_REQUIRE(t != nullptr, "expected todays market instance");
                boost::shared_ptr<InMemoryReport> mktReport = boost::make_shared<InMemoryReport>();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeTodaysMarketCalibrationReport(*mktReport, t->calibrationInfo());
                reports_[analytic]["marketcalibration"] = mktReport;
                out_ << "OK" << endl;
            }
            if (inputs_->outputCurves()) {
                out_ << setw(tab_) << left << "Pricing: Curves Report " << flush;
                LOG("Write curves report");
                boost::shared_ptr<InMemoryReport> curvesReport = boost::make_shared<InMemoryReport>();
                DateGrid grid(inputs_->curvesGrid());
                std::string config = inputs_->curvesMarketConfig();
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeCurves(*curvesReport, config, grid, *inputs_->todaysMarketParams(),
                                 market_, inputs_->continueOnError());
                reports_[analytic]["curves"] = curvesReport;
                out_ << "OK" << endl;
            }
        }
        else if (analytic == "CASHFLOW") {
            out_ << setw(tab_) << left << "Pricing: Cashflow Report " << flush;
            string marketConfig = inputs_->marketConfig("pricing");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(*report, portfolio_, market_, marketConfig, inputs_->includePastCashflows());
            reports_[analytic]["cashflow"] = report;
            out_ << "OK" << endl;
        }
        else if (analytic == "CASHFLOWNPV") {
            out_ << setw(tab_) << left << "Pricing: Cashflow NPV report " << flush;
            string marketConfig = inputs_->marketConfig("pricing");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(tmpReport, portfolio_, market_, marketConfig, inputs_->includePastCashflows());
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflowNpv(*report, tmpReport, market_, marketConfig, inputs_->baseCurrency(), inputs_->cashflowHorizon());
            reports_[analytic]["cashflownpv"] = report;
            out_ << "OK" << endl;
        }
        else if (analytic == "STRESS") {
            out_ << setw(tab_) << left << "Risk: Stress Test Report" << flush;
            LOG("Stress Test Analysis called");
            Settings::instance().evaluationDate() = inputs_->asof();
            std::string marketConfig = inputs_->marketConfig("pricing");
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
	    boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
                 portfolio_, market_, marketConfig, inputs_->pricingEngine(), inputs_->stressSimMarketParams(),
                 inputs_->stressScenarioData(), *inputs_->curveConfigs().at(0),
                 *configurations_.todaysMarketParams, nullptr, extraEngineBuilders, extraLegBuilders,
                 inputs_->refDataManager(), *inputs_->iborFallbackConfig(), inputs_->continueOnError());
            stressTest->writeReport(report, inputs_->stressThreshold());
            reports_[analytic]["stress"] = report;
            out_ << "OK" << endl;
        }
        else if (analytic == "SENSITIVITY") {
            out_ << setw(tab_) << left << "Risk: Sensitivity Report " << flush;
            LOG("Sensi Analysis - Initalise");
            bool recalibrateModels = true;
            bool ccyConv = false;
            std::string configuration = inputs_->marketConfig("pricing");
	    boost::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
	    // FIXME: Integrate with the multi-threaded sensi analysis 
            if (inputs_->nThreads() == 1) {
                std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
                std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    portfolio_, market_, configuration, inputs_->pricingEngine(),
                    configurations_.simMarketParams, configurations_.sensiScenarioData, recalibrateModels,
                    inputs_->curveConfigs().at(0), configurations_.todaysMarketParams, ccyConv, extraEngineBuilders,
                    extraLegBuilders, inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, false,
                    inputs_->dryRun());
            }
            else {
                std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
                    const boost::shared_ptr<ReferenceDataManager>&, const boost::shared_ptr<TradeFactory>&)>
                    extraTradeBuilders;
                std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()> extraEngineBuilders;
                std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()> extraLegBuilders;
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    inputs_->nThreads(), inputs_->asof(), this->loader(), portfolio_, Market::defaultConfiguration,
                    inputs_->pricingEngine(), configurations_.simMarketParams, configurations_.sensiScenarioData,
                    recalibrateModels, inputs_->curveConfigs().at(0), configurations_.todaysMarketParams, ccyConv,
                    extraTradeBuilders, extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, false, inputs_->dryRun());
            }

            // FIXME: Why are these disabled?
            set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
            boost::shared_ptr<ParSensitivityAnalysis> parAnalysis = nullptr;
            if (inputs_->parSensi()) {
                parAnalysis= boost::make_shared<ParSensitivityAnalysis>(
                    inputs_->asof(), configurations_.simMarketParams, *configurations_.sensiScenarioData, "",
                    true, typesDisabled);
                if (inputs_->alignPillars()) {
                    LOG("Sensi analysis - align pillars for the par conversion");
                    parAnalysis->alignPillars();
                    sensiAnalysis->overrideTenors(true);
                } else {
                    LOG("Sensi analysis - skip aligning pillars");
                }
            }
            
            LOG("Sensi analysis - generate");
            sensiAnalysis->registerProgressIndicator(boost::make_shared<ProgressLog>("sensitivities"));
            sensiAnalysis->generateSensitivities();

            LOG("Sensi analysis - write sensitivity report in memory");
            auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
            auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), baseCurrency);
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
            reports_[analytic]["sensitivity"] = report;

            LOG("Sensi analysis - write sensitivity scenario report in memory");
            boost::shared_ptr<InMemoryReport> scenarioReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeScenarioReport(*scenarioReport, sensiAnalysis->sensiCube(), inputs_->sensiThreshold());
            reports_[analytic]["sensitivity_scenario"] = scenarioReport;

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
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
                reports_[analytic]["par_sensitivity"] = parSensiReport;

                if (inputs_->outputJacobi()) {
                    boost::shared_ptr<InMemoryReport> jacobiReport = boost::make_shared<InMemoryReport>();
                    writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
                    reports_[analytic]["jacobi"] = jacobiReport;
                    
                    boost::shared_ptr<InMemoryReport> jacobiInverseReport = boost::make_shared<InMemoryReport>();
                    parConverter->writeConversionMatrix(*jacobiInverseReport);
                    reports_[analytic]["jacobi_inverse"] = jacobiInverseReport;
                }
            }
            else {
                LOG("Sensi Analysis - skip par conversion");
            }
        
            LOG("Sensi Analysis - Completed");
            out_ << "OK" << endl;
        }
        else {
            QL_FAIL("PricingAnalytic type " << analytic << " invalid");
        }
    }
}

/***********************************************************************************
 * VAR Analytic: DELTA-VAR, DELTA-GAMMA-NORMAL-VAR, MONTE-CARLO-VAR
 ***********************************************************************************/

void VarAnalytic::setUpConfigurations() { configurations_.todaysMarketParams = inputs_->todaysMarketParams(); }

void VarAnalytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::vector<std::string>& runTypes) {
    MEM_LOG;
    LOG("Running parametric VaR");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    LOG("VAR: Build Market");
    out_ << setw(tab_) << left << "Risk: Build Market for VaR " << flush;
    buildMarket(loader, inputs_->curveConfigs()[0]);
    out_ << "OK" << std::endl;

    out_ << setw(tab_) << left << "Risk: Build Portfolio for VaR" << flush;
    buildPortfolio();
    out_ << "OK" << std::endl;

    LOG("Build trade to portfolio id mapping");
    map<string, set<string>> tradePortfolio;
    for (auto const& [tradeId, trade] : portfolio_->trades()) {
        tradePortfolio[tradeId].insert(trade->portfolioIds().begin(), trade->portfolioIds().end());
    }

    // FIXME: Add Delta-Gamma (Saddlepoint) 
    LOG("Build VaR calculator");
    auto calc = boost::make_shared<ParametricVarCalculator>(tradePortfolio, inputs_->portfolioFilter(),
            inputs_->sensitivityStream(), inputs_->covarianceData(), inputs_->varQuantiles(), inputs_->varMethod(),
            inputs_->mcVarSamples(), inputs_->mcVarSeed(), inputs_->varBreakDown(), inputs_->salvageCovariance());

    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
    reports_["var"]["var"] = report;

    LOG("Call VaR calculation");
    out_ << setw(tab_) << left << "Risk: VaR Calculation " << flush;
    calc->calculate(*report);
    out_ << "OK" << endl;

    LOG("Parametric VaR completed");
    MEM_LOG;
}
    

/******************************************************************************
 * XVA Analytic: EXPOSURE, CVA, DVA, FVA, KVA, COLVA, COLLATERALFLOOR, DIM, MVA
 ******************************************************************************/

void XvaAnalytic::setUpConfigurations() {
    configurations_.todaysMarketParams = inputs_->todaysMarketParams();
    configurations_.simMarketParams = inputs_->exposureSimMarketParams();
}

boost::shared_ptr<EngineFactory> XvaAnalytic::engineFactory() {
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
    if (inputs_->simulation()) {
        // link to the sim market here
        QL_REQUIRE(simMarket_, "Simulaton market not set");
        engineFactory_ = boost::make_shared<EngineFactory>(edCopy, simMarket_, configurations,
                                                 extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                                 *inputs_->iborFallbackConfig());
    } else {
        // we just link to today's market if simulation is not required
        engineFactory_ = boost::make_shared<EngineFactory>(edCopy, market_, configurations,
                                                 extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                                 *inputs_->iborFallbackConfig());        
    }
    return engineFactory_;
}


void XvaAnalytic::buildScenarioSimMarket() {
    
    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(
            market_,
            inputs_->exposureSimMarketParams(),
            boost::make_shared<FixingManager>(inputs_->asof()),
            configuration,
            *inputs_->curveConfigs()[0],
            *inputs_->todaysMarketParams(),
            inputs_->continueOnError(), 
            false, true, false,
            *inputs_->iborFallbackConfig(),
            false);
}

void XvaAnalytic::buildScenarioGenerator(const bool continueOnCalibrationError) {
    if (!model_)
        buildCrossAssetModel(continueOnCalibrationError);
    ScenarioGeneratorBuilder sgb(inputs_->scenarioGeneratorData());
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    string config = inputs_->marketConfig("simulation");
    scenarioGenerator_ = sgb.build(model_, sf, inputs_->exposureSimMarketParams(), inputs_->asof(), market_, config); 
    QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator"); 
    grid_ = inputs_->scenarioGeneratorData()->getGrid();
    samples_ = inputs_->scenarioGeneratorData()->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));    
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));    

    if (inputs_->writeScenarios()) {
        auto report = boost::make_shared<InMemoryReport>();
        reports_["xva"]["scenario"] = report;
        scenarioGenerator_ = boost::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void XvaAnalytic::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ")");
    DayCounter dc = ActualActual(ActualActual::ISDA);
    CrossAssetModelBuilder modelBuilder(market_, inputs_->crossAssetModelData(),
                                        inputs_->marketConfig("lgmcalibration"),
                                        inputs_->marketConfig("fxcalibration"),
                                        inputs_->marketConfig("eqcalibration"),
                                        inputs_->marketConfig("infcalibration"),
                                        inputs_->marketConfig("crcalibration"),
                                        inputs_->marketConfig("simulation"),
                                        dc, false, continueOnCalibrationError);
    model_ = *modelBuilder.model();
}


void XvaAnalytic::initCubeDepth() {

    if (cubeDepth_ == 0) {
        LOG("XVA: Set cube depth");
        // Determine the required cube depth:
        // - Without close-out grid and without storing flows we have a 3d cube (trades * dates * scenarios),
        //   otherwise we need a 4d "hypercube" with additonal dimension "depth"
        // - If we build an auxiliary close-out grid then we store default values at depth 0 and close-out at depth 1
        // - If we want to store cash flows that occur during the mpor, then we store them at depth 2
        cubeDepth_ = 1;
        if (inputs_->scenarioGeneratorData()->withCloseOutLag())
            cubeDepth_++;
        if (inputs_->storeFlows())
            cubeDepth_++;

        LOG("XVA: Cube depth set to: " << cubeDepth_);
    }
}


void XvaAnalytic::initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth) {

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


void XvaAnalytic::initClassicRun(const boost::shared_ptr<Portfolio>& portfolio) {
        
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


boost::shared_ptr<Portfolio> XvaAnalytic::classicRun(const boost::shared_ptr<Portfolio>& portfolio) {
    LOG("XVA: classicRun");
   
    Size n = portfolio->size();

    // Create a new empty portfolio, fill it and link it to the simulation market
    // We don't use Analytic::buildPortfolio() here because we are possibly dealing with a sub-portfolio only.
    LOG("XVA: Build classic portfolio of size " << n << " linked to the simulation market");
    out_ << setw(tab_) << left << "XVA: Build Portfolio " << flush;
    classicPortfolio_ = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());
    portfolio->reset();
    for (const auto& [tradeId, trade] : portfolio->trades())
        classicPortfolio_->add(trade);    
    QL_REQUIRE(market_, "today's market not set");
    boost::shared_ptr<EngineFactory> factory = engineFactory();
    classicPortfolio_->build(factory, "analytic/" + label_);
    Date maturityDate = inputs_->asof();
    if (inputs_->portfolioFilterDate() != Null<Date>())
        maturityDate = inputs_->portfolioFilterDate();
    LOG("Filter trades that expire before " << maturityDate);
    classicPortfolio_->removeMatured(maturityDate);
    out_ << "OK" << std::endl;

    // Allocate cubes for the sub-portfolio we are processing here
    initClassicRun(classicPortfolio_);

    // This is where the valuation work is done
    buildClassicCube(classicPortfolio_);

    LOG("XVA: classicRun completed");

    return classicPortfolio_;
}


void XvaAnalytic::buildClassicCube(const boost::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA::buildCube");

    // set up valuation calculator factory

    auto calculators = [this]() {
        vector<boost::shared_ptr<ValuationCalculator>> calculators;
        if (inputs_->scenarioGeneratorData()->withCloseOutLag()) {
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

    if (inputs_->scenarioGeneratorData()->withCloseOutLag())
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(scenarioData_, grid_, inputs_->flipViewXVA());
    else
        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(scenarioData_, inputs_->flipViewXVA());

    // log message

    ostringstream o;
    o << "XVA: Build Cube " << portfolio->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
    out_ << setw(tab_) << o.str() << flush;
    LOG(o.str());

    // set up progress indicators

    auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), tab_, progressBarWidth_);
    auto progressLog = boost::make_shared<ProgressLog>("Building cube");

    if(inputs_->nThreads() == 1) {

        // single-threaded engine run

        ValuationEngine engine(inputs_->asof(), grid_, simMarket_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.buildCube(portfolio, cube_, calculators(), inputs_->scenarioGeneratorData()->withMporStickyDate(),
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
            inputs_->nThreads(), inputs_->asof(), grid_, samples_, loader_, scenarioGenerator_,
            inputs_->simulationPricingEngine(), inputs_->curveConfigs()[0], inputs_->todaysMarketParams(),
            inputs_->marketConfig("simulation"), inputs_->exposureSimMarketParams(), false, false,
            boost::make_shared<ore::analytics::ScenarioFilter>(), {}, {}, {}, inputs_->refDataManager(),
            *inputs_->iborFallbackConfig(), true, false, cubeFactory, {}, cptyCubeFactory, "xva-simulation");

        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, cptyCalculators,
                         inputs_->scenarioGeneratorData()->withMporStickyDate());

        cube_ = boost::make_shared<JointNPVCube>(engine.outputCubes(), portfolio->ids());

        if (inputs_->storeSurvivalProbabilities())
            cptyCube_ = boost::make_shared<JointNPVCube>(
                engine.outputCptyCubes(), portfolio->counterparties(), false,
                [](Real a, Real x) { return std::max(a, x); }, 0.0);
    }

    out_ << "OK" << endl;

    LOG("XVA::buildCube done");

    Settings::instance().evaluationDate() = inputs_->asof();
}


std::vector<boost::shared_ptr<EngineBuilder>>
getAmcEngineBuilders(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam, const std::vector<Date>& grid) {
    std::vector<boost::shared_ptr<EngineBuilder>> eb;
    eb.push_back(boost::make_shared<data::CamAmcCurrencySwapEngineBuilder>(cam, grid));
    eb.push_back(boost::make_shared<data::CamAmcFxOptionEngineBuilder>(cam, grid));
    eb.push_back(boost::make_shared<data::CamAmcMultiLegOptionEngineBuilder>(cam, grid));
    eb.push_back(boost::make_shared<data::CamAmcSwapEngineBuilder>(cam, grid));
    eb.push_back(boost::make_shared<data::LgmAmcBermudanSwaptionEngineBuilder>(cam, grid));
    // FIXME: Uncomment for release 10
    // eb.push_back(boost::make_shared<data::ScriptedTradeEngineBuilder>(ScriptLibraryStorage::scriptLibrary, cam, grid));
    return eb;
}


boost::shared_ptr<EngineFactory> XvaAnalytic::amcEngineFactory() {
    LOG("XvaAnalytic::engineFactory2() called");
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->amcPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; 
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders;
    auto factory = boost::make_shared<EngineFactory>(edCopy, market_, configurations,
                                                     extraEngineBuilders, extraLegBuilders,
                                                     inputs_->refDataManager(),
                                                     *inputs_->iborFallbackConfig());        
    return factory;
}


void XvaAnalytic::buildAmcPortfolio() {
    LOG("XVA: buildAmcPortfolio");
    std::string message = "XVA: Build AMC portfolio ";
    auto progressBar = boost::make_shared<SimpleProgressBar>(message, tab_, progressBarWidth_);
    auto progressLog = boost::make_shared<ProgressLog>(message);

    LOG("buildAmcPortfolio: Check sim dates");
    std::vector<Date> simDates =
        inputs_->scenarioGeneratorData()->withCloseOutLag() && !inputs_->scenarioGeneratorData()->withMporStickyDate() ?
        inputs_->scenarioGeneratorData()->getGrid()->dates() : inputs_->scenarioGeneratorData()->getGrid()->valuationDates();

    LOG("buildAmcPortfolio: Get engine builders");
    auto eb = getAmcEngineBuilders(model_, simDates);
    
    LOG("buildAmcPortfolio: Register additional engine builders");
    auto factory = amcEngineFactory();
    for(auto const& b : eb)
        factory->registerBuilder(b);

    LOG("buildAmcPortfolio: Load Portfolio");
    boost::shared_ptr<Portfolio> portfolio = inputs_->portfolio();
    std::vector<std::string> excludeTradeTypes = inputs_->amcExcludeTradeTypes();

    LOG("Build Portfolio with AMC Engine factory and select amc-enabled trades")
    amcPortfolio_ = boost::make_shared<Portfolio>();
    Size count = 0, amcCount = 0, total = portfolio->size();
    Real timingTraining = 0.0;
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        bool tradeBuilt = false;
        if (std::find(excludeTradeTypes.begin(), excludeTradeTypes.end(), trade->tradeType()) == excludeTradeTypes.end()) {
            try {
                trade->reset();
                trade->build(factory);
                tradeBuilt = true;
            } catch (const std::exception& e) {
                ALOG("trade " << tradeId << " could not be built with AMC factory: " << e.what());
            }
            if (tradeBuilt) {
                try {
                    boost::timer::cpu_timer timer;
                    // retrieving the amcCalculator triggers the training!
                    trade->instrument()->qlInstrument()->result<boost::shared_ptr<AmcCalculator>>("amcCalculator");
                    timer.stop();
                    Real tmp = timer.elapsed().wall * 1e-6;
                    amcPortfolio_->add(trade);
                    LOG("trade " << tradeId << " is added to amc portfolio (training took " << tmp << " ms), pv "
                        << trade->instrument()->NPV() << ", mat " << io::iso_date(trade->maturity()));
                    timingTraining += tmp;
                    ++amcCount;
                } catch (const std::exception& e) {
                    DLOG("trade " << tradeId << " can not processed by AMC valuation engine (" << e.what()
                                  << "), it will be simulated clasically");
                }
            }
        }
        ++count;
        progressBar->updateProgress(count, total);
        progressLog->updateProgress(count, total);
    }
    LOG("added " << amcCount << " of out " << count << " trades to amc simulation, training took " << timingTraining
                 << " ms, avg. " << timingTraining / static_cast<Real>(amcCount) << " ms/trade");

    out_ << "OK" << endl;

    LOG("XVA: buildAmcPortfolio completed");
}


void XvaAnalytic::amcRun(bool doClassicRun) {

    LOG("XVA: amcRun");

    initCubeDepth();
    
    initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);

    AMCValuationEngine amcEngine(model_, inputs_->scenarioGeneratorData(), market_,
                                 inputs_->exposureSimMarketParams()->additionalScenarioDataIndices(),
                                 inputs_->exposureSimMarketParams()->additionalScenarioDataCcys());
    std::string message = "XVA: Build AMC Cube " + std::to_string(amcPortfolio_->size()) + " x " +
        std::to_string(grid_->valuationDates().size()) + " x " + std::to_string(samples_) +
        "... ";
    auto progressBar = boost::make_shared<SimpleProgressBar>(message, tab_, progressBarWidth_);
    auto progressLog = boost::make_shared<ProgressLog>("Building AMC Cube...");
    amcEngine.registerProgressIndicator(progressBar);
    amcEngine.registerProgressIndicator(progressLog);

    if (!scenarioData_) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
    }

    // We only need to generate asd, if this does not happen in the classic run
    if (!doClassicRun)
        amcEngine.aggregationScenarioData() = scenarioData_;

    amcEngine.buildCube(amcPortfolio_, amcCube_);

    out_ << "OK" << endl;

    LOG("XVA: amcRun completed");
}
    

void XvaAnalytic::runPostProcessor() {
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

    if (!cubeInterpreter_) {
        // FIXME: Can we get the grid from the cube instead?
        QL_REQUIRE(inputs_->scenarioGeneratorData(), "scenario generator data not set");
        boost::shared_ptr<ScenarioGeneratorData> sgd = inputs_->scenarioGeneratorData();
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
            portfolio_, cube_, cubeInterpreter_, scenarioData_, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder,
            dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth);
    }

    std::vector<Period> cvaSensiGrid = inputs_->cvaSensiGrid();
    Real cvaSensiShiftSize = inputs_->cvaSensiShiftSize();

    string flipViewBorrowingCurvePostfix = inputs_->flipViewBorrowingCurvePostfix();
    string flipViewLendingCurvePostfix = inputs_->flipViewLendingCurvePostfix();

    LOG("baseCurrency " << baseCurrency);

    postProcess_ = boost::make_shared<PostProcess>(
        portfolio_, netting, market_, marketConfiguration, cube_, scenarioData_, analytics, baseCurrency,
        allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix);
    LOG("post done");
}

void XvaAnalytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::vector<std::string>& runTypes) {
    
    LOG("XVA analytic called with asof " << io::iso_date(inputs_->asof()));

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    LOG("XVA: Build Today's Market");
    out_ << setw(tab_) << left << "XVA: Build Market " << flush;
    buildMarket(loader, inputs_->curveConfigs()[0]);
    out_ << "OK" << endl;
    
    if (inputs_->simulation()) {
    
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
        inputs_->portfolio()->reset();
        auto residualPortfolio = boost::make_shared<Portfolio>(inputs_->buildFailedTrades());
        for (const auto& [tradeId, trade] : inputs_->portfolio()->trades()) 
            residualPortfolio->add(trade);

        if (inputs_->amc()) {
            // Build a separate sub-portfolio for the AMC cube generation and perform its training
            buildAmcPortfolio();

            // Build the residual portfolio for the classic cube generation, i.e. strip out the AMC part
            for (auto const& [tradeId, trade] : amcPortfolio_->trades())
                residualPortfolio->remove(tradeId);

            LOG("AMC portfolio size " << amcPortfolio_->size());
            LOG("Residual portfolio size " << residualPortfolio->size());

            doAmcRun = !amcPortfolio_->trades().empty();
            doClassicRun = !residualPortfolio->trades().empty();
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
            // Join the two cubes and set cube_ to the union; use the ordering of the
            // original portfolio for the trades in the cube
            cube_ = boost::make_shared<JointNPVCube>(cube_, amcCube_, inputs_->portfolio()->ids(), true);
            // If the cube has to be written, create a physical one from the wrapper
            if (inputs_->writeCube()) {
                boost::shared_ptr<NPVCube> tmpCube;
                initCube(tmpCube, cube_->ids(), cubeDepth_);
                for (Size i = 0; i < tmpCube->numIds(); ++i) 
                    for (Size d = 0; d < tmpCube->depth(); ++d)
                        tmpCube->setT0(cube_->getT0(i), i, d);
                for (Size i = 0; i < tmpCube->numIds(); ++i)
                    for (Size j = 0; j < tmpCube->numDates(); ++j)
                        for (Size k = 0; k < tmpCube->samples(); ++k)
                            for (Size d = 0; d < tmpCube->depth(); ++d)
                                tmpCube->set(cube_->get(i, j, k, d), i, j, k, d);
                cube_ = tmpCube;
            }
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
        portfolio_ = boost::make_shared<Portfolio>();
        for (const auto& [tradeId, trade] : classicPortfolio_->trades())
            portfolio_->add(trade);
        for (const auto& [tradeId, trade] : amcPortfolio_->trades())
            portfolio_->add(trade);
        LOG("Total portfolio size " << portfolio_->size());
        if (portfolio_->size() < inputs_->portfolio()->size()) {
            ALOG("input portfolio size is " << inputs_->portfolio()->size()
                 << ", but we have built only " << portfolio_->size() << " trades");
        }
    }
    else { // inputs_->simulation() == false

        // build the portfolio linked to today's market
        buildPortfolio();

        // ... and load a pre-built cube for post-processing

        LOG("Skip cube generation, load input cubes for XVA");
        out_ << setw(tab_) << left << "XVA: Load Cubes " << flush;
        QL_REQUIRE(inputs_->cube(), "input cube not povided");
        cube_= inputs_->cube();
        QL_REQUIRE(inputs_->mktCube(), "input market cube not provided"); 
        scenarioData_ = inputs_->mktCube();
        if (inputs_->nettingSetCube())
            nettingSetCube_= inputs_->nettingSetCube();
        if (inputs_->cptyCube())
            cptyCube_ = inputs_->cptyCube();
        out_ << "OK" << std::endl;
    }
    
    MEM_LOG;

    /*********************************************************************
     * This is where the aggregation work is done: call the post-processor
     *********************************************************************/

    out_ << setw(tab_) << left << "XVA: Aggregation " << flush;
    runPostProcessor();
    out_ << "OK" << std::endl;

    /******************************************************
     * Finally generate various (in-memory) reports/outputs
     ******************************************************/

    out_ << setw(tab_) << left << "XVA: Reports " << flush;
    LOG("Generating XVA reports and cube outputs");

    if (inputs_->exposureProfilesByTrade()) {
        for (const auto& [tradeId, tradeIdCubePos] : postProcess_->tradeIds()) {
            auto report = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString()).writeTradeExposures(*report, postProcess_, tradeId);
            reports_["xva"]["exposure_trade_" + tradeId] = report;
        }
    }

    if (inputs_->exposureProfiles()) {
        for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
            auto exposureReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
            reports_["xva"]["exposure_nettingset_" + nettingSet] = exposureReport;

            auto colvaReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
            reports_["xva"]["colva_nettingset_" + nettingSet] = colvaReport;

            auto cvaSensiReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
            reports_["xva"]["cva_sensitivity_nettingset_" + nettingSet] = cvaSensiReport;
        }
    }
    
    auto xvaReport = boost::make_shared<InMemoryReport>();
    ore::analytics::ReportWriter(inputs_->reportNaString())
        .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), portfolio_, postProcess_);
    reports_["xva"]["xva"] = xvaReport;

    // Return the cubes to serialalize
    if (inputs_->writeCube()) {
        npvCubes_["xva"]["cube"] = cube_;
        mktCubes_["xva"]["scenariodata"] = scenarioData_;
        if (nettingSetCube_) {
            npvCubes_["xva"]["nettingsetcube"] = nettingSetCube_;
        }
        if (cptyCube_) {
            npvCubes_["xva"]["cptycube"] = cptyCube_;
        }
    }

    // Generate cube reports to inspect
    if (inputs_->rawCubeOutput()) {
        map<string, string> nettingSetMap = portfolio_->nettingSetMap();
        auto report = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeCube(*report, postProcess_->cube(), nettingSetMap);
        reports_["xva"]["rawcube"] = report;
    }    
    if (inputs_->netCubeOutput()) {
        auto report = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeCube(*report, postProcess_->netCube());
        reports_["xva"]["netcube"] = report;
    }

    if (inputs_->dimAnalytic() || inputs_->mvaAnalytic()) {
        // Generate DIM evolution report
        auto dimEvolutionReport = boost::make_shared<InMemoryReport>();
        postProcess_->exportDimEvolution(*dimEvolutionReport);
        reports_["xva"]["dim_evolution"] = dimEvolutionReport;
    
        // Generate DIM regression reports
        vector<boost::shared_ptr<ore::data::Report>> dimRegReports;
        for (Size i = 0; i < inputs_->dimOutputGridPoints().size(); ++i) {
            auto rep = boost::make_shared<InMemoryReport>();
            dimRegReports.push_back(rep);
            reports_["xva"]["dim_regression_" + to_string(i)] = rep;
        }
        postProcess_->exportDimRegression(inputs_->dimOutputNettingSet(), inputs_->dimOutputGridPoints(), dimRegReports);
    }
    
    out_ << "OK" << std::endl;

    // reset that mode
    ObservationMode::instance().setMode(inputs_->observationModel());
}

} // namespace analytics
} // namespace ore
