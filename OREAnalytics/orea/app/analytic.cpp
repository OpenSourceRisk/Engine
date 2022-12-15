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
#include <orea/engine/stresstest.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>

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
    //configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    return boost::make_shared<EngineFactory>(edCopy, market_, configurations, extraEngineBuilders, extraLegBuilders,
                                             inputs_->refDataManager(), *inputs_->iborFallbackConfig());
}

boost::shared_ptr<ore::data::EngineFactory> PricingAnalytic::engineFactory() {
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
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
    portfolio_ = boost::make_shared<Portfolio>();

    inputs_->portfolio()->reset();
    // populate with trades
    for (const auto& t : inputs_->portfolio()->trades())
        portfolio_->add(t);
    
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
    
    ObservationMode::instance().setMode(inputs_->observationModel());

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
                std::string config = inputs_->marketConfig("curves");
                ore::analytics::ReportWriter(inputs_->reportNaString())
                    .writeCurves(*curvesReport, config, grid, *inputs_->todaysMarketParams(),
                                 market_, inputs_->continueOnError());
                reports_[analytic]["curves"] = curvesReport;
                out_ << "OK" << endl;
            }
        }
        else if (analytic == "CASHFLOW") {
            out_ << setw(tab_) << left << "Pricing: Cashflow Report " << flush;
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(*report, portfolio_, market_, "");
            reports_[analytic]["cashflow"] = report;
            out_ << "OK" << endl;
        }
        else if (analytic == "CASHFLOWNPV") {
            out_ << setw(tab_) << left << "Pricing: Cashflow NPV report " << flush;
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(tmpReport, portfolio_, market_, "");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflowNpv(*report, tmpReport, market_, "", inputs_->baseCurrency(), inputs_->cashflowHorizon());
            reports_[analytic]["cashflownpv"] = report;
            out_ << "OK" << endl;
        }
        else if (analytic == "STRESS") {
            out_ << setw(tab_) << left << "Risk: Stress Test Report" << flush;
            LOG("Stress Test Analysis called");
            Settings::instance().evaluationDate() = inputs_->asof();
            std::string configuration = inputs_->marketConfig("pricing");
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;// = getExtraEngineBuilders();
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;// = getExtraLegBuilders();
	    boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
                 portfolio_, market_, configuration, inputs_->pricingEngine(), configurations_.simMarketParams,
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
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders;// = getExtraEngineBuilders();
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;// = getExtraLegBuilders();
	    boost::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
	    // FIXME: integrate the multi-threaded sensi analysis once released
            //if (inputs_->nThreads() == 1) {
            sensiAnalysis = boost::make_shared<SensitivityAnalysis>(
                    portfolio_, market_, configuration, inputs_->pricingEngine(),
                    configurations_.simMarketParams, configurations_.sensiScenarioData, recalibrateModels,
                    inputs_->curveConfigs().at(0), configurations_.todaysMarketParams, ccyConv, extraBuilders,
                    extraLegBuilders, inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, false,
                    inputs_->dryRun());
            // }
            // else {
            //     sensiAnalysis = boost::make_shared<oreplus::sensitivity::SensitivityAnalysis>(
            //         inputs_->nThreads(), inputs_->asof(), this->loader(), portfolio_, Market::defaultConfiguration,
            //         inputs_->pricingEngine(), configurations_.simMarketParams, configurations_.sensitivityScenarioData,
            //         recalibrateModels, inputs_->curveConfigs().at(0), configurations_.todaysMarketParams, ccyConv,
            //         getExtraTradeBuilders, getExtraEngineBuilders, getExtraLegBuilders, inputs_->refDataManager(),
            //         *inputs_->iborFallbackConfig(), true, false, true, inputs_->dryRun());
            // }
            

            // FIXME: integrate par sensitivity analysis here once released
            // try {
            //     LOG("Align pillars for the par sensitivity calculation");
            //     set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
            //     boost::shared_ptr<ParSensitivityAnalysis> parAnalysis = boost::make_shared<ParSensitivityAnalysis>(
            //         inputs_->asof(), configurations_.simMarketParams, *configurations_.sensitivityScenarioData, "",
            //         true, typesDisabled);
            //     parAnalysis->alignPillars();
            //     sensiAnalysis->overrideTenors(true);
            //     LOG("Pillars aligned");
            // } catch (...) {
            //     WLOG("Could not align pillars for sensitivity calculation, continuing without");
            // }
                        
            LOG("Sensi Analysis - Generate");
            boost::shared_ptr<NPVSensiCube> npvCube;
            sensiAnalysis->registerProgressIndicator(boost::make_shared<ProgressLog>("sensitivities"));
            sensiAnalysis->generateSensitivities(npvCube);

            LOG("Sensi Analysis - Write Report");
            auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), inputs_->baseCurrency());
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
            reports_[analytic]["sensitivity"] = report;

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
    for (auto const& t : portfolio_->trades()) {
        tradePortfolio[t->id()].insert(t->portfolioIds().begin(), t->portfolioIds().end());
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
        return boost::make_shared<EngineFactory>(edCopy, simMarket_, configurations,
                                                 extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                                 *inputs_->iborFallbackConfig());
    } else {
        // we just link to today's market if simulation is not required
        return boost::make_shared<EngineFactory>(edCopy, market_, configurations,
                                                 extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                                 *inputs_->iborFallbackConfig());        
    }
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
                                        inputs_->marketConfig("lgmCalibration"),
                                        inputs_->marketConfig("fxCalibration"),
                                        inputs_->marketConfig("eqCalibration"),
                                        inputs_->marketConfig("infCalibration"),
                                        inputs_->marketConfig("crCalibration"),
                                        inputs_->marketConfig("simulation"),
                                        dc, false, continueOnCalibrationError);
    model_ = *modelBuilder.model();
}

void XvaAnalytic::initCube(boost::shared_ptr<NPVCube>& cube, const std::vector<std::string>& ids, Size cubeDepth) {

    for (Size i = 0; i < grid_->valuationDates().size(); ++i)
        LOG("initCube: grid[" << i << "]=" << io::iso_date(grid_->valuationDates()[i]));
    
    if (cubeDepth == 1)
        cube = boost::make_shared<SinglePrecisionInMemoryCube>(inputs_->asof(),
            ids, grid_->valuationDates(), samples_, 0.0f);
    else
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(inputs_->asof(),
            ids, grid_->valuationDates(), samples_, cubeDepth, 0.0f);    
}

void XvaAnalytic::buildCube() {

    LOG("XVA::buildCube");

    // Valuation calculators
    string baseCurrency = inputs_->exposureBaseCurrency();
    vector<boost::shared_ptr<ValuationCalculator>> calculators;

    if (inputs_->scenarioGeneratorData()->withCloseOutLag()) {
        boost::shared_ptr<NPVCalculator> npvCalc = boost::make_shared<NPVCalculator>(baseCurrency);
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalc, 0, 1));
    } else {
        calculators.push_back(boost::make_shared<NPVCalculator>(baseCurrency));
    }

    if (inputs_->storeFlows()) {
        // cash flow stored at index 1 (no close-out lag) or 2 (have close-out lag)
        calculators.push_back(boost::make_shared<CashflowCalculator>(baseCurrency, inputs_->asof(), grid_, cubeDepth_ - 1));
    }

    if (inputs_->scenarioGeneratorData()->withCloseOutLag())
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(grid_, inputs_->flipViewXVA());
    else
        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(inputs_->flipViewXVA());

    vector<boost::shared_ptr<CounterpartyCalculator>> cptyCalculators;
    if (inputs_->storeSurvivalProbabilities()) {
        string configuration = inputs_->marketConfig("simulation");
        cptyCalculators.push_back(boost::make_shared<SurvivalProbabilityCalculator>(configuration));
    }

    // FIXME: integrate the multi-threaded engine here once released
    ValuationEngine engine(inputs_->asof(), grid_, simMarket_);

    ostringstream o;
    o << "XVA: Build Cube " << portfolio_->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
    out_ << setw(tab_) << o.str() << flush;
    LOG(o.str());

    auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), tab_, progressBarWidth_);
    auto progressLog = boost::make_shared<ProgressLog>("Building cube");
    engine.registerProgressIndicator(progressBar);
    engine.registerProgressIndicator(progressLog);

    bool mporStickyDate = inputs_->scenarioGeneratorData()->withMporStickyDate();
    engine.buildCube(portfolio_, cube_, calculators, mporStickyDate, nettingSetCube_, cptyCube_, cptyCalculators);
    
    out_ << "OK" << endl;

    LOG("XVA::buildCube done");

    LOG("Reset the global evaluation date");
    Settings::instance().evaluationDate() = inputs_->asof();
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
            cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(sgd->getGrid(), analytics["flipViewXVA"]);
        else
            cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(analytics["flipViewXVA"]);
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

    QL_REQUIRE(inputs_->portfolio(), "XVA: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    // today's market
    LOG("XVA: Build Market");
    out_ << setw(tab_) << left << "XVA: Build Market " << flush;
    buildMarket(loader, inputs_->curveConfigs()[0]);

    Size n = inputs_->portfolio()->size();

    if (inputs_->simulation()) {

        // Build the simulation market, build the portfolio and link it to this simulation market
    
        LOG("XVA: Build simulation market");
        buildScenarioSimMarket();

        LOG("XVA: Check global pricing engine parameters");
        auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
        auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
        bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);
        
        LOG("XVA: Build Scenario Generator");
        buildScenarioGenerator(continueOnErr);

        LOG("XVA: Attach Scenario Generator to ScenarioSimMarket");
        simMarket_->scenarioGenerator() = scenarioGenerator_;
        out_ << "OK" << std::endl;

        LOG("XVA: Build Portfolio of size " << n << " linked to the simulation market");
        out_ << setw(tab_) << left << "XVA: Build Portfolio " << flush;
        buildPortfolio();
        out_ << "OK" << std::endl;
    }
    else {
        out_ << "OK" << std::endl;

        // Build the portfolio and link it to today's market instead, as we do not need the MC machinery in this case
        // See engineFactory(), the factory uses sim market resp. today's market depending on inputs_->simulation()

        LOG("XVA: Build Portfolio of size " << n << " linked to today's market");
        out_ << setw(tab_) << left << "XVA: Build Portfolio " << flush;
        buildPortfolio();
        out_ << "OK" << std::endl;
    }
    
    if (portfolio_->size() < n) {
        ALOG("XVA: Built Portfolio of size " << portfolio_->size() << ", expected " << n);
    }

    if (inputs_->simulation()) {

        // Initialise and build cubes
        
        LOG("XVA: Init Cubes");
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
        initCube(cube_, portfolio_->ids(), cubeDepth_);
        // will be set/filled by the postprocessor
        nettingSetCube_ = nullptr; 
        // Init counterparty cube for the storage of survival probabilities
        if (inputs_->storeSurvivalProbabilities()) {
            auto counterparties = portfolio_->counterparties();
            counterparties.push_back(inputs_->dvaName());
            initCube(cptyCube_, counterparties, 1);
        } else {
            cptyCube_ = nullptr; 
        }
        
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;

        /******************************************
         * This is where the valuation work is done
         ******************************************/

        buildCube();

        LOG("NPV cube generation completed");

        // Write pricing stats report
        auto statsReport = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter().writePricingStats(*statsReport, portfolio_);
        reports_["xva"]["pricingstats_simulation"] = statsReport;
    }
    else {

        // Load a pre-built cube for post-processing

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

    /********************************************
     * This is where the aggregation work is done
     ********************************************/

    out_ << setw(tab_) << left << "XVA: Aggregation " << flush;
    runPostProcessor();
    out_ << "OK" << std::endl;

    /*************************************
     * Various (in-memory) reports/outputs
     *************************************/

    out_ << setw(tab_) << left << "XVA: Reports " << flush;
    LOG("Generating XVA reports and cube outputs");

    if (inputs_->exposureProfilesByTrade()) {
        for (auto t : postProcess_->tradeIds()) {
            auto report = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeTradeExposures(*report, postProcess_, t);
            reports_["xva"]["exposure_trade_" + t] = report;
        }
    }

    if (inputs_->exposureProfiles()) {
        for (auto n : postProcess_->nettingSetIds()) {
            auto exposureReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetExposures(*exposureReport, postProcess_, n);
            reports_["xva"]["exposure_nettingset_" + n] = exposureReport;

            auto colvaReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetColva(*colvaReport, postProcess_, n);
            reports_["xva"]["colva_nettingset_" + n] = colvaReport;

            auto cvaSensiReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, n);
            reports_["xva"]["cva_sensitivity_nettingset_" + n] = cvaSensiReport;
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
