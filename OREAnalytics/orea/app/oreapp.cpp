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

#include <boost/algorithm/string.hpp>
#include <boost/timer.hpp>

#ifdef BOOST_MSVC
// disable warning C4503: '__LINE__Var': decorated name length exceeded, name was truncated
// This pragma statement needs to be at the top of the file - lower and it will not work:
// http://stackoverflow.com/questions/9673504/is-it-possible-to-disable-compiler-warning-c4503
// http://boost.2283326.n4.nabble.com/General-Warnings-and-pragmas-in-MSVC-td2587449.html
#pragma warning(disable : 4503)
#endif

#include <iostream>

#include <boost/filesystem.hpp>

#include <orea/orea.hpp>
#include <ored/ored.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>

#include <orea/app/oreapp.hpp>

#ifdef BOOST_MSVC
#include <orea/auto_link.hpp>
#include <ored/auto_link.hpp>
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
// Find the name of the correct boost library with which to link.
#define BOOST_LIB_NAME boost_regex
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_serialization
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_date_time
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_regex
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#endif

using namespace std;
using namespace ore::data;
using namespace ore::analytics;

namespace ore {
namespace analytics {

void OreApp::run() {

    boost::timer timer;

    try {
        std::cout << "ORE starting" << std::endl;
        setupLogFile();

        LOG("ORE starting");
        params_->log();

        if (params_->has("setup", "observationModel")) {
            string om = params_->get("setup", "observationModel");
            ObservationMode::instance().setMode(om);
            LOG("Observation Mode is " << om);
        }

        Date asof = asof_;
        string outputPath = params_->get("setup", "outputPath");
        string inputPath = params_->get("setup", "inputPath");

        /*************
         * Conventions
         */
        cout << setw(tab) << left << "Conventions... " << flush;
        getConventions();
        cout << "OK" << endl;

        /*********
         * Markets
         */
        cout << setw(tab) << left << "Market... " << flush;
        TodaysMarketParameters marketParameters = getMarketParameters();
        boost::shared_ptr<Market> market = buildMarket(marketParameters);

        cout << "OK" << endl;

        /************************
         * Pricing Engine Factory
         */
        cout << setw(tab) << left << "Engine factory... " << flush;
        boost::shared_ptr<EngineFactory> factory = buildFactory(market);
        cout << "OK" << endl;

        /******************************
         * Load and Build the Portfolio
         */
        cout << setw(tab) << left << "Portfolio... " << flush;
        boost::shared_ptr<Portfolio> portfolio = buildPortfolio(factory);
        cout << "OK" << endl;

        /************
         * Curve dump
         */
        cout << setw(tab) << left << "Curve Report... " << flush;
        if (params_->hasGroup("curves") && params_->get("curves", "active") == "Y") {
            string fileName = outputPath + "/" + params_->get("curves", "outputFileName");
            CSVFileReport curvesReport(fileName);
            DateGrid grid(params_->get("curves", "grid"));
            ReportWriter::writeCurves(curvesReport, params_->get("curves", "configuration"), grid, marketParameters,
                                      market);
            cout << "OK" << endl;
        } else {
            LOG("skip curve report");
            cout << "SKIP" << endl;
        }

        /*********************
         * Portfolio valuation
         */
        cout << setw(tab) << left << "NPV Report... " << flush;
        if (params_->hasGroup("npv") && params_->get("npv", "active") == "Y") {
            string fileName = outputPath + "/" + params_->get("npv", "outputFileName");
            CSVFileReport npvReport(fileName);
            ReportWriter::writeNpv(npvReport, params_->get("npv", "baseCurrency"), market,
                                   params_->get("markets", "pricing"), portfolio);
            cout << "OK" << endl;
        } else {
            LOG("skip portfolio valuation");
            cout << "SKIP" << endl;
        }

        /**********************
         * Cash flow generation
         */
        cout << setw(tab) << left << "Cashflow Report... " << flush;
        if (params_->hasGroup("cashflow") && params_->get("cashflow", "active") == "Y") {
            string fileName = outputPath + "/" + params_->get("cashflow", "outputFileName");
            CSVFileReport cashflowReport(fileName);
            ReportWriter::writeCashflow(cashflowReport, portfolio);
            cout << "OK" << endl;
        } else {
            LOG("skip cashflow generation");
            cout << "SKIP" << endl;
        }

        /******************************************
         * Simulation: Scenario and Cube Generation
         */

        boost::shared_ptr<AggregationScenarioData> inMemoryScenarioData;
        boost::shared_ptr<NPVCube> inMemoryCube;
        Size cubeDepth = 0;

        if (params_->hasGroup("simulation") && params_->get("simulation", "active") == "Y") {

            cout << setw(tab) << left << "Simulation Setup... ";
            fflush(stdout);
            LOG("Load Simulation Market Parameters");
            boost::shared_ptr<ScenarioSimMarketParameters> simMarketData = getSimMarketData();
            boost::shared_ptr<ScenarioGeneratorData> sgd = getScenarioGeneratorData();
            boost::shared_ptr<ScenarioGenerator> sg = buildScenarioGenerator(market, simMarketData, sgd);

            boost::shared_ptr<ore::analytics::DateGrid> grid = sgd->grid();

            LOG("Build Simulation Market");
            boost::shared_ptr<ScenarioSimMarket> simMarket = boost::make_shared<ScenarioSimMarket>(
                sg, market, simMarketData, *conventions_, params_->get("markets", "simulation"));

            boost::shared_ptr<EngineFactory> simFactory = buildSimFactory(simMarket);

            LOG("Build portfolio linked to sim market");
            boost::shared_ptr<Portfolio> simPortfolio = boost::make_shared<Portfolio>();
            string portfolioFile = inputPath + "/" + params_->get("setup", "portfolioFile");
            simPortfolio->load(portfolioFile);
            simPortfolio->build(simFactory);
            QL_REQUIRE(simPortfolio->size() == portfolio->size(),
                       "portfolio size mismatch, check simulation market setup");
            cout << "OK" << endl;

            LOG("Build valuation cube engine");
            Size samples = sgd->samples();
            string baseCurrency = params_->get("simulation", "baseCurrency");
            if (params_->has("simulation", "storeFlows") && params_->get("simulation", "storeFlows") == "Y")
                cubeDepth = 2; // NPV and FLOW
            else
                cubeDepth = 1; // NPV only

            // Valuation calculators
            vector<boost::shared_ptr<ValuationCalculator>> calculators;
            calculators.push_back(boost::make_shared<NPVCalculator>(baseCurrency));
            if (cubeDepth > 1)
                calculators.push_back(boost::make_shared<CashflowCalculator>(baseCurrency, asof, grid, 1));
            ValuationEngine engine(asof, grid, simMarket);

            ostringstream o;
            o << "Aggregation Scenario Data " << grid->size() << " x " << samples << "... ";
            cout << setw(tab) << o.str() << flush;
            inMemoryScenarioData = boost::make_shared<InMemoryAggregationScenarioData>(grid->size(), samples);
            // Set AggregationScenarioData
            simMarket->aggregationScenarioData() = inMemoryScenarioData;
            cout << "OK" << endl;

            o.str("");
            o << "Build Cube " << simPortfolio->size() << " x " << grid->size() << " x " << samples << "... ";
            LOG("Build cube");
            auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), tab);
            auto progressLog = boost::make_shared<ProgressLog>("Building cube...");
            engine.registerProgressIndicator(progressBar);
            engine.registerProgressIndicator(progressLog);
            if (cubeDepth == 1)
                inMemoryCube =
                    boost::make_shared<SinglePrecisionInMemoryCube>(asof, simPortfolio->ids(), grid->dates(), samples);
            else if (cubeDepth == 2)
                inMemoryCube = boost::make_shared<SinglePrecisionInMemoryCubeN>(asof, simPortfolio->ids(),
                                                                                grid->dates(), samples, cubeDepth);
            else {
                QL_FAIL("cube depth 1 or 2 expected");
            }

            engine.buildCube(simPortfolio, inMemoryCube, calculators);
            cout << "OK" << endl;

            cout << setw(tab) << left << "Write Cube... " << flush;
            LOG("Write cube");
            if (params_->has("simulation", "cubeFile")) {
                string cubeFileName = outputPath + "/" + params_->get("simulation", "cubeFile");
                inMemoryCube->save(cubeFileName);
                cout << "OK" << endl;
            } else
                cout << "SKIP" << endl;

            cout << setw(tab) << left << "Write Aggregation Scenario Data... " << flush;
            LOG("Write scenario data");
            if (params_->has("simulation", "additionalScenarioDataFileName")) {
                string outputFileNameAddScenData =
                    outputPath + "/" + params_->get("simulation", "additionalScenarioDataFileName");
                inMemoryScenarioData->save(outputFileNameAddScenData);
                cout << "OK" << endl;
            } else
                cout << "SKIP" << endl;
        } else {
            LOG("skip simulation");
            cout << setw(tab) << left << "Simulation... ";
            cout << "SKIP" << endl;
        }

        /*****************************
         * Aggregation and XVA Reports
         */
        cout << setw(tab) << left << "Aggregation and XVA Reports... " << flush;
        if (params_->hasGroup("xva") && params_->get("xva", "active") == "Y") {

            // We reset this here because the date grid building below depends on it.
            Settings::instance().evaluationDate() = asof;

            string csaFile = inputPath + "/" + params_->get("xva", "csaFile");
            boost::shared_ptr<NettingSetManager> netting = boost::make_shared<NettingSetManager>();
            netting->fromFile(csaFile);

            map<string, bool> analytics;
            analytics["exerciseNextBreak"] = parseBool(params_->get("xva", "exerciseNextBreak"));
            analytics["exposureProfiles"] = parseBool(params_->get("xva", "exposureProfiles"));
            analytics["cva"] = parseBool(params_->get("xva", "cva"));
            analytics["dva"] = parseBool(params_->get("xva", "dva"));
            analytics["fva"] = parseBool(params_->get("xva", "fva"));
            analytics["colva"] = parseBool(params_->get("xva", "colva"));
            analytics["collateralFloor"] = parseBool(params_->get("xva", "collateralFloor"));
            if (params_->has("xva", "mva"))
                analytics["mva"] = parseBool(params_->get("xva", "mva"));
            else
                analytics["mva"] = false;
            if (params_->has("xva", "dim"))
                analytics["dim"] = parseBool(params_->get("xva", "dim"));
            else
                analytics["dim"] = false;

            boost::shared_ptr<NPVCube> cube;
            if (inMemoryCube)
                cube = inMemoryCube;
            else {
                Size cubeDepth = 1;
                if (params_->has("xva", "hyperCube"))
                    cubeDepth = parseBool(params_->get("xva", "hyperCube")) ? 2 : 1;

                if (cubeDepth > 1)
                    cube = boost::make_shared<SinglePrecisionInMemoryCubeN>();
                else
                    cube = boost::make_shared<SinglePrecisionInMemoryCube>();
                string cubeFile = outputPath + "/" + params_->get("xva", "cubeFile");
                LOG("Load cube from file " << cubeFile);
                cube->load(cubeFile);
                LOG("Cube loading done");
            }

            QL_REQUIRE(cube->numIds() == portfolio->size(), "cube x dimension (" << cube->numIds()
                                                                                 << ") does not match portfolio size ("
                                                                                 << portfolio->size() << ")");

            boost::shared_ptr<AggregationScenarioData> scenarioData;
            if (inMemoryScenarioData)
                scenarioData = inMemoryScenarioData;
            else {
                scenarioData = boost::make_shared<InMemoryAggregationScenarioData>();
                string scenarioFile = outputPath + "/" + params_->get("xva", "scenarioFile");
                scenarioData->load(scenarioFile);
            }

            QL_REQUIRE(scenarioData->dimDates() == cube->dates().size(), "scenario dates do not match cube grid size");
            QL_REQUIRE(scenarioData->dimSamples() == cube->samples(),
                       "scenario sample size does not match cube sample size");

            string baseCurrency = params_->get("xva", "baseCurrency");
            string calculationType = params_->get("xva", "calculationType");
            string allocationMethod = params_->get("xva", "allocationMethod");
            Real marginalAllocationLimit = parseReal(params_->get("xva", "marginalAllocationLimit"));
            Real quantile = parseReal(params_->get("xva", "quantile"));
            string dvaName = params_->get("xva", "dvaName");
            string fvaLendingCurve = params_->get("xva", "fvaLendingCurve");
            string fvaBorrowingCurve = params_->get("xva", "fvaBorrowingCurve");
            Real collateralSpread = parseReal(params_->get("xva", "collateralSpread"));

            Real dimQuantile = 0.99;
            Size dimHorizonCalendarDays = 14;
            Size dimRegressionOrder = 0;
            vector<string> dimRegressors;
            Real dimScaling = 1.0;
            Size dimLocalRegressionEvaluations = 0;
            Real dimLocalRegressionBandwidth = 0.25;

            if (analytics["mva"] || analytics["dim"]) {
                dimQuantile = parseReal(params_->get("xva", "dimQuantile"));
                dimHorizonCalendarDays = parseInteger(params_->get("xva", "dimHorizonCalendarDays"));
                dimRegressionOrder = parseInteger(params_->get("xva", "dimRegressionOrder"));
                string dimRegressorsString = params_->get("xva", "dimRegressors");
                dimRegressors = parseListOfValues(dimRegressorsString);
                dimScaling = parseReal(params_->get("xva", "dimScaling"));
                dimLocalRegressionEvaluations = parseInteger(params_->get("xva", "dimLocalRegressionEvaluations"));
                dimLocalRegressionBandwidth = parseReal(params_->get("xva", "dimLocalRegressionBandwidth"));
            }

            string marketConfiguration = params_->get("markets", "simulation");

            boost::shared_ptr<PostProcess> postProcess = boost::make_shared<PostProcess>(
                portfolio, netting, market, marketConfiguration, cube, scenarioData, analytics, baseCurrency,
                allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
                fvaLendingCurve, collateralSpread, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder,
                dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth, dimScaling);

            for (auto t : postProcess->tradeIds()) {
                ostringstream o;
                o << outputPath << "/exposure_trade_" << t << ".csv";
                string tradeExposureFile = o.str();
                CSVFileReport tradeExposureReport(tradeExposureFile);
                ReportWriter::writeTradeExposures(tradeExposureReport, postProcess, t);
            }
            for (auto n : postProcess->nettingSetIds()) {
                ostringstream o1;
                o1 << outputPath << "/exposure_nettingset_" << n << ".csv";
                string nettingSetExposureFile = o1.str();
                CSVFileReport nettingSetExposureReport(nettingSetExposureFile);
                ReportWriter::writeNettingSetExposures(nettingSetExposureReport, postProcess, n);

                ostringstream o2;
                o2 << outputPath << "/colva_nettingset_" << n << ".csv";
                string nettingSetColvaFile = o2.str();
                CSVFileReport nettingSetColvaReport(nettingSetColvaFile);
                ReportWriter::writeNettingSetColva(nettingSetColvaReport, postProcess, n);
            }

            string XvaFile = outputPath + "/xva.csv";
            CSVFileReport xvaReport(XvaFile);
            ReportWriter::writeXVA(xvaReport, params_->get("xva", "allocationMethod"), portfolio, postProcess);

            string rawCubeOutputFile = params_->get("xva", "rawCubeOutputFile");
            CubeWriter cw1(outputPath + "/" + rawCubeOutputFile);
            map<string, string> nettingSetMap = portfolio->nettingSetMap();
            cw1.write(cube, nettingSetMap);

            string netCubeOutputFile = params_->get("xva", "netCubeOutputFile");
            CubeWriter cw2(outputPath + "/" + netCubeOutputFile);
            cw2.write(postProcess->netCube(), nettingSetMap);

            if (analytics["dim"]) {
                string dimFile1 = outputPath + "/" + params_->get("xva", "dimEvolutionFile");
                vector<string> dimFiles2;
                for (auto f : parseListOfValues(params_->get("xva", "dimRegressionFiles")))
                    dimFiles2.push_back(outputPath + "/" + f);
                string nettingSet = params_->get("xva", "dimOutputNettingSet");
                std::vector<Size> dimOutputGridPoints =
                    parseListOfValues<Size>(params_->get("xva", "dimOutputGridPoints"), &parseInteger);
                postProcess->exportDimEvolution(dimFile1, nettingSet);
                postProcess->exportDimRegression(dimFiles2, nettingSet, dimOutputGridPoints);
            }

            cout << "OK" << endl;
        } else {
            LOG("skip XVA reports");
            cout << "SKIP" << endl;
        }

    } catch (std::exception& e) {
        ALOG("Error: " << e.what());
        cout << "Error: " << e.what() << endl;
    }

    cout << "run time: " << setprecision(2) << timer.elapsed() << " sec" << endl;
    cout << "ORE done." << endl;

    LOG("ORE done.");
}
void OreApp::setupLogFile() {
    string outputPath = params_->get("setup", "outputPath");
    string logFile = outputPath + "/" + params_->get("setup", "logFile");
    Size logMask = 15; // Default level

    // Get log mask if available
    if (params_->has("setup", "logMask")) {
        logMask = static_cast<Size>(parseInteger(params_->get("setup", "logMask")));
    }

    boost::filesystem::path p{outputPath};
    if (!boost::filesystem::exists(p)) {
        boost::filesystem::create_directories(p);
    }
    QL_REQUIRE(boost::filesystem::is_directory(p), "output path '" << outputPath << "' is not a directory.");

    Log::instance().registerLogger(boost::make_shared<FileLogger>(logFile));
    Log::instance().setMask(logMask);
    Log::instance().switchOn();
}

void OreApp::getConventions() {
    Conventions conventions;
    string inputPath = params_->get("setup", "inputPath");
    string conventionsFile = inputPath + "/" + params_->get("setup", "conventionsFile");
    conventions.fromFile(conventionsFile);
    conventions_ = boost::make_shared<Conventions>(conventions);
}

boost::shared_ptr<Market> OreApp::buildMarket(TodaysMarketParameters& marketParams) {
    /*******************************
     * Market and fixing data loader
     */
    cout << setw(tab) << left << "Market data loader... " << flush;
    string inputPath = params_->get("setup", "inputPath");
    string marketFile = inputPath + "/" + params_->get("setup", "marketDataFile");
    string fixingFile = inputPath + "/" + params_->get("setup", "fixingDataFile");
    string implyTodaysFixingsString = params_->get("setup", "implyTodaysFixings");
    bool implyTodaysFixings = parseBool(implyTodaysFixingsString);
    CSVLoader loader(marketFile, fixingFile, implyTodaysFixings);
    cout << "OK" << endl;
    /**********************
     * Curve configurations
     */
    cout << setw(tab) << left << "Curve configuration... " << flush;
    CurveConfigurations curveConfigs;
    string curveConfigFile = inputPath + "/" + params_->get("setup", "curveConfigFile");
    curveConfigs.fromFile(curveConfigFile);
    cout << "OK" << endl;

    boost::shared_ptr<Market> market =
        boost::make_shared<TodaysMarket>(asof_, marketParams, loader, curveConfigs, *conventions_);

    return market;
}

TodaysMarketParameters OreApp::getMarketParameters() {
    TodaysMarketParameters marketParameters;
    string inputPath = params_->get("setup", "inputPath");
    string marketConfigFile = inputPath + "/" + params_->get("setup", "marketConfigFile");
    marketParameters.fromFile(marketConfigFile);

    return marketParameters;
}

boost::shared_ptr<EngineFactory> OreApp::buildFactory(boost::shared_ptr<Market> market) {
    string inputPath = params_->get("setup", "inputPath");

    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    string pricingEnginesFile = inputPath + "/" + params_->get("setup", "pricingEnginesFile");
    engineData->fromFile(pricingEnginesFile);

    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
    configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
    configurations[MarketContext::pricing] = params_->get("markets", "pricing");
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(engineData, market, configurations);
    return factory;
}

boost::shared_ptr<Portfolio> OreApp::buildPortfolio(boost::shared_ptr<EngineFactory> factory) {

    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    string inputPath = params_->get("setup", "inputPath");
    string portfolioFile = inputPath + "/" + params_->get("setup", "portfolioFile");
    portfolio->load(portfolioFile);
    portfolio->build(factory);

    return portfolio;
}

boost::shared_ptr<ScenarioSimMarketParameters> OreApp::getSimMarketData() {
    string inputPath = params_->get("setup", "inputPath");
    string simulationConfigFile = inputPath + "/" + params_->get("simulation", "simulationConfigFile");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    simMarketData->fromFile(simulationConfigFile);

    return simMarketData;
}
boost::shared_ptr<ScenarioGeneratorData> OreApp::getScenarioGeneratorData() {
    string inputPath = params_->get("setup", "inputPath");

    string simulationConfigFile = inputPath + "/" + params_->get("simulation", "simulationConfigFile");

    boost::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->fromFile(simulationConfigFile);

    return sgd;
}
boost::shared_ptr<ScenarioGenerator>
OreApp::buildScenarioGenerator(boost::shared_ptr<Market> market,
                               boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                               boost::shared_ptr<ScenarioGeneratorData> sgd) {
    string inputPath = params_->get("setup", "inputPath");

    LOG("Build Simulation Model");
    string simulationConfigFile = inputPath + "/" + params_->get("simulation", "simulationConfigFile");
    LOG("Load simulation model data from file: " << simulationConfigFile);
    boost::shared_ptr<CrossAssetModelData> modelData = boost::make_shared<CrossAssetModelData>();
    modelData->fromFile(simulationConfigFile);
    CrossAssetModelBuilder modelBuilder(market, params_->get("markets", "lgmcalibration"),
                                        params_->get("markets", "fxcalibration"),
                                        params_->get("markets", "simulation"));
    boost::shared_ptr<QuantExt::CrossAssetModel> model = modelBuilder.build(modelData);

    LOG("Load Simulation Parameters");
    ScenarioGeneratorBuilder sgb(sgd);
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    boost::shared_ptr<ScenarioGenerator> sg = sgb.build(
        model, sf, simMarketData, asof_, market, params_->get("markets", "simulation")); // pricing or simulation?
    // Optionally write out scenarios
    if (params_->has("simulation", "scenariodump")) {
        string outputPath = params_->get("setup", "outputPath");
        string filename = outputPath + "/" + params_->get("simulation", "scenariodump");
        sg = boost::make_shared<ScenarioWriter>(sg, filename);
    }
    return sg;
}

boost::shared_ptr<EngineFactory> OreApp::buildSimFactory(boost::shared_ptr<ScenarioSimMarket> simMarket) {

    LOG("Build engine factory for pricing under scenarios, linked to sim market");
    string inputPath = params_->get("setup", "inputPath");
    boost::shared_ptr<EngineData> simEngineData = boost::make_shared<EngineData>();
    string simPricingEnginesFile = inputPath + "/" + params_->get("simulation", "pricingEnginesFile");
    simEngineData->fromFile(simPricingEnginesFile);
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
    configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
    configurations[MarketContext::pricing] = params_->get("markets", "simulation");
    boost::shared_ptr<EngineFactory> simFactory =
        boost::make_shared<EngineFactory>(simEngineData, simMarket, configurations);

    return simFactory;
}
}
}
