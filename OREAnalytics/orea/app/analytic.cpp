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
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>

using namespace ore::analytics;
using namespace ore::data;
using namespace boost::filesystem;

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
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; // = getExtraEngineBuilders()
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders; // = getExtraLegBuilders()
    return boost::make_shared<EngineFactory>(inputs_->pricingEngine(), market_, map<MarketContext, string>(),
                                             extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

boost::shared_ptr<ore::data::EngineFactory> PricingAnalytic::engineFactory() {
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = "true";
    edCopy->globalParameters()["RunType"] = "NPV";
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders;// = getExtraEngineBuilders();
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;// = getExtraLegBuilders();
    return boost::make_shared<EngineFactory>(edCopy, market_, map<MarketContext, string>(), extraBuilders,
                                             extraLegBuilders, inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

void Analytic::buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, const boost::shared_ptr<CurveConfigurations>& curveConfig, 
                           const bool marketRequired) {
    LOG("Analytic::buildMarket called");
    
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
                                                       curveConfig, true, true, false, inputs_->refDataManager(), false,
                                                       *inputs_->iborFallbackConfig());

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

/*******************************************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, MARKETDATA, ADDITIONAL RESULTS
 *******************************************************************************************/

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

    // FIXME: Get this from inputs
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    // Check coverage
    for (const auto& rt : runTypes) {
        if (std::find(types_.begin(), types_.end(), rt) == types_.end()) {
            WLOG("requested analytic " << rt << " not covered by the PricingAnalytic");
        }
    }
    
    // Write reports
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
            out_ << "OK" << endl;
        }
        else if (analytic == "ADDITIONAL_RESULTS") {
            out_ << setw(tab_) << left << "Pricing: Additional Results " << flush;
            //path p = inputs_->resultsPath() / "additional_results.csv";
            // boost::shared_ptr<InMemoryReport> addResultsReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                // .writeAdditionalResultsReport(*addResultsReport, portfolio_, market_, inputs_->baseCurrency());
                .writeAdditionalResultsReport(*report, portfolio_, market_, inputs_->baseCurrency());
            //addResultsReport->toFile(p.string(), ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString(), true);    
            // reports_["ADDITIONAL_RESULTS"]["AdditionalResults"] = addResultsReport;
            out_ << "OK" << endl;
        }
        else if (analytic == "CASHFLOW") {
            out_ << setw(tab_) << left << "Pricing: Cashflow Report " << flush;
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(*report, portfolio_, market_, "");
            out_ << "OK" << endl;
        }
        else if (analytic == "CASHFLOWNPV") {
            out_ << setw(tab_) << left << "Pricing: Cashflow NPV report " << flush;
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflow(tmpReport, portfolio_, market_, "");
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeCashflowNpv(*report, tmpReport, market_, "", inputs_->baseCurrency(), inputs_->cashflowHorizon());
            out_ << "OK" << endl;
        }
        else if (analytic == "SENSITIVITY") {
            out_ << setw(tab_) << left << "Pricing: Sensitivity Report " << flush;
            LOG("Sensi Analysis - Initalise");
            bool recalibrateModels = true;
            bool ccyConv = false;
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders;// = getExtraEngineBuilders();
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;// = getExtraLegBuilders();
	    boost::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
	    // if (inputs_->nThreads() == 1) {
                sensiAnalysis = boost::make_shared<SensitivityAnalysis>(
                    portfolio_, market_, Market::defaultConfiguration, inputs_->pricingEngine(),
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
            LOG("Sensi Analysis - Completed");

            out_ << "OK" << endl;
        }
        else if (analytic == "MARKETDATA") {
            QL_FAIL("MARKETDATA not implemented yet");
        }
        else {
            QL_FAIL("PricingAnalytic::runAnalytic: Invalid PricingAnalytic type " << analytic);
        }
        reports_[analytic][""] = report;
    }

    if (inputs_->outputTodaysMarketCalibration()) {
        out_ << setw(tab_) << left << "TodaysMarket Calibration " << flush;
        LOG("Write todays market calibration report");
        path reportPath = inputs_->resultsPath() / "todaysmarketcalibration.csv";
        auto report = boost::make_shared<CSVFileReport>(reportPath.string(), ',', false, inputs_->csvQuoteChar(),
                                                        inputs_->reportNaString());
        QL_FAIL("TODO: outputTodaysMarketCalibration");
        // FIXME: use Analytic::marketCalibration instead of this ?
        // Static::todaysMarketCalibrationReport(market_, report);
        out_ << "OK" << endl;
    }
}

/***********************************************************************************
 * VAR Analytic: DELTA-VAR, DELTA-GAMMA-VAR, DELTA-GAMMA-NORMAL-VAR, MONTE-CARLO-VAR
 ***********************************************************************************/

void VarAnalytic::setUpConfigurations() { configurations_.todaysMarketParams = inputs_->todaysMarketParams(); }

void VarAnalytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::vector<std::string>& runTypes) {
    QL_FAIL("VarAnalytic::runAnalytic not implemented yet");
}

/********************************************************************
 * XVA Analytic: EXPOSURE, CVA, DVA, FVA, KVA, COLVA, COLLATERALFLOOR
 ********************************************************************/

void XvaAnalytic::setUpConfigurations() {
    configurations_.todaysMarketParams = inputs_->todaysMarketParams();
    configurations_.simMarketParams = inputs_->exposureSimMarketParams();
}

boost::shared_ptr<EngineFactory> XvaAnalytic::engineFactory() {
    std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders; 
    std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders;
    // Note: linked to the sim market here
    QL_REQUIRE(simMarket_, "Simulaton market not set");
    return boost::make_shared<EngineFactory>(inputs_->simulationPricingEngine(), simMarket_, map<MarketContext, string>(),
                                             extraEngineBuilders, extraLegBuilders, inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

void XvaAnalytic::buildScenarioSimMarket() {
    std::string configuration = "libor"; // FIXME
    bool useSpreadedTermStructures = false; // FIXME
    bool cacheSimData = true; // FIXME
    bool allowPartialScenarios = false; // FIXME
    simMarket_ = boost::make_shared<ScenarioSimMarket>(
            market_,
            inputs_->exposureSimMarketParams(),
            boost::make_shared<FixingManager>(inputs_->asof()),
            configuration,
            *inputs_->curveConfigs()[0],
            *inputs_->todaysMarketParams(),
            false, // do not continue on error
            useSpreadedTermStructures, cacheSimData, allowPartialScenarios,
            *inputs_->iborFallbackConfig());
}

void XvaAnalytic::buildScenarioGenerator(const bool continueOnCalibrationError) {
    if (!model_)
        buildCrossAssetModel(continueOnCalibrationError);
    ScenarioGeneratorBuilder sgb(inputs_->scenarioGeneratorData());
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    string config = Market::defaultConfiguration; // FIXME
    scenarioGenerator_ = sgb.build(model_, sf, inputs_->exposureSimMarketParams(), inputs_->asof(), market_, config); 
    grid_ = inputs_->scenarioGeneratorData()->getGrid();
    samples_ = inputs_->scenarioGeneratorData()->samples();

    // FIXME
    // Optionally write out scenarios
    // if (params_->has("simulation", "scenariodump")) {
    //     string filename = outputPath_ + "/" + params_->get("simulation", "scenariodump");
    //     sg = boost::make_shared<ScenarioWriter>(sg, filename);
    // }
}

void XvaAnalytic::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ")");

    // FIXME
    string lgmCalibMkt = Market::defaultConfiguration;
    string fxCalibMkt = Market::defaultConfiguration;
    string eqCalibMkt = Market::defaultConfiguration;
    string infCalibMkt = Market::defaultConfiguration;
    string crCalibMkt = Market::defaultConfiguration;
    string simulationMkt = Market::defaultConfiguration;
    DayCounter dc = ActualActual(ActualActual::ISDA);
    CrossAssetModelBuilder modelBuilder(market_, inputs_->crossAssetModelData(),
        lgmCalibMkt, fxCalibMkt, eqCalibMkt, infCalibMkt, crCalibMkt, simulationMkt,
        dc, false, continueOnCalibrationError);
    model_ = *modelBuilder.model();
}

void XvaAnalytic::initCube(boost::shared_ptr<NPVCube>& cube, const std::vector<std::string>& ids, Size cubeDepth) {
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
        // FIXME
        // const string configuration = params_->get("markets", "simulation");
        const string configuration = Market::defaultConfiguration;;
        cptyCalculators.push_back(boost::make_shared<SurvivalProbabilityCalculator>(configuration));
    }

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

    // FIXME
    // string marketConfiguration = params_->get("markets", "simulation");
    string marketConfiguration = "libor";//Market::defaultConfiguration;

    bool fullInitialCollateralisation = inputs_->fullInitialCollateralisation();

    if (!cubeInterpreter_) {
        boost::shared_ptr<ScenarioGeneratorData> sgd = inputs_->scenarioGeneratorData();
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
}

void XvaAnalytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::vector<std::string>& runTypes) {
    
    QL_REQUIRE(inputs_->portfolio(), "XVA: No portfolio loaded.");

    ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    // today's market
    LOG("XVA: Build Market");
    out_ << setw(tab_) << left << "XVA: Build Market " << flush;
    buildMarket(loader, inputs_->curveConfigs()[0]);

    LOG("XVA: Build simulation market");
    buildScenarioSimMarket();

    auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
    auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
    bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);

    LOG("XVA: Build Scenario Generator");
    buildScenarioGenerator(continueOnErr);

    LOG("XVA: Attach Scenario Generator to ScenarioSimMarket");
    simMarket_->scenarioGenerator() = scenarioGenerator_;
    out_ << "OK" << std::endl;

    out_ << setw(tab_) << left << "XVA: Build Portfolio " << flush;
    buildPortfolio();
    out_ << "OK" << std::endl;
    LOG("XVA: Built Portfolio of size " << portfolio_->size());

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
    nettingSetCube_ = nullptr;
    // Init counterparty cube for the storage of survival probabilities
    if (inputs_->storeSurvivalProbabilities()) {
        auto counterparties = portfolio_->counterparties();
        counterparties.push_back(inputs_->dvaName());
        initCube(cptyCube_, counterparties, 1);
    } else {
        cptyCube_ = nullptr;
    }

    LOG("XVA: Aggregation Scenario Data " << grid_->valuationDates().size() << " x " << samples_);
    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
    simMarket_->aggregationScenarioData() = scenarioData_;

    buildCube();

    // FIXME
    // writeCube(cube_, "cubeFile");
    // if (nettingSetCube_)
    //     writeCube(nettingSetCube_, "nettingSetCubeFile");
    // if (cptyCube_)
    //     writeCube(cptyCube_, "cptyCubeFile");
    // writeScenarioData();

    LOG("NPV cube generation completed");
    MEM_LOG;

    // FIXME
    // writePricingStats("pricingstats_xva.csv", portfolio);

    out_ << setw(tab_) << left << "XVA: Aggregation " << flush;
    runPostProcessor();
    out_ << "OK" << std::endl;

    out_ << setw(tab_) << left << "XVA: Reports " << flush;
    LOG("Writing XVA reports");

    if (inputs_->exposureProfilesByTrade()) {
        for (auto t : postProcess_->tradeIds()) {
            boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeTradeExposures(*report, postProcess_, t);
            reports_["exposure_trade_" + t][""] = report;
        }
    }
    if (inputs_->exposureProfiles()) {
        for (auto n : postProcess_->nettingSetIds()) {
            boost::shared_ptr<InMemoryReport> exposureReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetExposures(*exposureReport, postProcess_, n);
            reports_["exposure_nettingset_" + n][""] = exposureReport;

            boost::shared_ptr<InMemoryReport> colvaReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetColva(*colvaReport, postProcess_, n);
            reports_["colva_nettingset_" + n][""] = colvaReport;

            boost::shared_ptr<InMemoryReport> cvaSensiReport = boost::make_shared<InMemoryReport>();
            ore::analytics::ReportWriter(inputs_->reportNaString())
                .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, n);
            reports_["cva_sensitivity_nettingset_" + n][""] = cvaSensiReport;
        }
    }
    
    boost::shared_ptr<InMemoryReport> xvaReport = boost::make_shared<InMemoryReport>();
    ore::analytics::ReportWriter(inputs_->reportNaString())
        .writeXVA(*xvaReport, inputs_->exposureAllocationMethod(), portfolio_, postProcess_);
    reports_["xva"][""] = xvaReport;

    // FIXME
    // map<string, string> nettingSetMap = portfolio_->nettingSetMap();
    // string rawCubeOutputFile = params_->get("xva", "rawCubeOutputFile");
    // if (rawCubeOutputFile != "") {
    //     CubeWriter cw1(outputPath_ + "/" + rawCubeOutputFile);
    //     cw1.write(postProcess_->cube(), nettingSetMap);
    // }
    
    // string netCubeOutputFile = params_->get("xva", "netCubeOutputFile");
    // if (netCubeOutputFile != "") {
    //     CubeWriter cw2(outputPath_ + "/" + netCubeOutputFile);
    //     cw2.write(postProcess_->netCube(), nettingSetMap);
    // }

    out_ << "OK" << std::endl;
    
    // reset
    ObservationMode::instance().setMode(inputs_->observationModel());
}

} // namespace analytics
} // namespace ore
