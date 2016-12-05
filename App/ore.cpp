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

#include "ore.hpp"

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

void writeNpv(const Parameters& params, boost::shared_ptr<Market> market, const std::string& configuration,
              boost::shared_ptr<Portfolio> portfolio);

void writeCashflow(const Parameters& params, boost::shared_ptr<Portfolio> portfolio);

void writeCurves(const Parameters& params, const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market);

void writeTradeExposures(const Parameters& params, boost::shared_ptr<PostProcess> postProcess);

void writeNettingSetExposures(const Parameters& params, boost::shared_ptr<PostProcess> postProcess);

void writeNettingSetColva(const Parameters& params, boost::shared_ptr<PostProcess> postProcess);

void writeXVA(const Parameters& params, boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess);

int main(int argc, char** argv) {

    if (argc == 2 && (string(argv[1]) == "-v" || string(argv[1]) == "--version")) {
        cout << "ORE version " << OPEN_SOURCE_RISK_VERSION << endl;
        exit(0);
    }

    boost::timer timer;

    try {
        std::cout << "ORE starting" << std::endl;

        Size tab = 40;

        if (argc != 2) {
            std::cout << endl << "usage: ORE path/to/ore.xml" << endl << endl;
            return -1;
        }

        string inputFile(argv[1]);
        Parameters params;
        params.fromFile(inputFile);

        string outputPath = params.get("setup", "outputPath");
        string logFile = outputPath + "/" + params.get("setup", "logFile");
        Size logMask = 15; // Default level

        // Get log mask if available
        if (params.has("setup", "logMask")) {
            logMask = static_cast<Size>(parseInteger(params.get("setup", "logMask")));
        }

        boost::filesystem::path p{ outputPath };
        if (!boost::filesystem::exists(p)) {
            boost::filesystem::create_directories(p);
        }
        QL_REQUIRE(boost::filesystem::is_directory(p), "output path '" << outputPath << "' is not a directory.");

        Log::instance().registerLogger(boost::make_shared<FileLogger>(logFile));
        Log::instance().setMask(logMask);
        Log::instance().switchOn();

        LOG("ORE starting");
        params.log();

        if (params.has("setup", "observationModel")) {
            string om = params.get("setup", "observationModel");
            ObservationMode::instance().setMode(om);
            LOG("Observation Mode is " << om);
        }

        string asofString = params.get("setup", "asofDate");
        Date asof = parseDate(asofString);
        Settings::instance().evaluationDate() = asof;

        /*******************************
         * Market and fixing data loader
         */
        cout << setw(tab) << left << "Market data loader... " << flush;
        string inputPath = params.get("setup", "inputPath");
        string marketFile = inputPath + "/" + params.get("setup", "marketDataFile");
        string fixingFile = inputPath + "/" + params.get("setup", "fixingDataFile");
        string implyTodaysFixingsString = params.get("setup", "implyTodaysFixings");
        bool implyTodaysFixings = parseBool(implyTodaysFixingsString);
        CSVLoader loader(marketFile, fixingFile, implyTodaysFixings);
        cout << "OK" << endl;

        /*************
         * Conventions
         */
        cout << setw(tab) << left << "Conventions... " << flush;
        Conventions conventions;
        string conventionsFile = inputPath + "/" + params.get("setup", "conventionsFile");
        conventions.fromFile(conventionsFile);
        cout << "OK" << endl;

        /**********************
         * Curve configurations
         */
        cout << setw(tab) << left << "Curve configuration... " << flush;
        CurveConfigurations curveConfigs;
        string curveConfigFile = inputPath + "/" + params.get("setup", "curveConfigFile");
        curveConfigs.fromFile(curveConfigFile);
        cout << "OK" << endl;

        /*********
         * Markets
         */
        cout << setw(tab) << left << "Market... " << flush;
        TodaysMarketParameters marketParameters;
        string marketConfigFile = inputPath + "/" + params.get("setup", "marketConfigFile");
        marketParameters.fromFile(marketConfigFile);

        boost::shared_ptr<Market> market =
            boost::make_shared<TodaysMarket>(asof, marketParameters, loader, curveConfigs, conventions);
        cout << "OK" << endl;

        /************************
         * Pricing Engine Factory
         */
        cout << setw(tab) << left << "Engine factory... " << flush;
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        string pricingEnginesFile = inputPath + "/" + params.get("setup", "pricingEnginesFile");
        engineData->fromFile(pricingEnginesFile);

        map<MarketContext, string> configurations;
        configurations[MarketContext::irCalibration] = params.get("markets", "lgmcalibration");
        configurations[MarketContext::fxCalibration] = params.get("markets", "fxcalibration");
        configurations[MarketContext::pricing] = params.get("markets", "pricing");
        boost::shared_ptr<EngineFactory> factory =
            boost::make_shared<EngineFactory>(engineData, market, configurations);
        cout << "OK" << endl;

        /******************************
         * Load and Build the Portfolio
         */
        cout << setw(tab) << left << "Portfolio... " << flush;
        boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
        string portfolioFile = inputPath + "/" + params.get("setup", "portfolioFile");
        portfolio->load(portfolioFile);
        portfolio->build(factory);
        cout << "OK" << endl;

        /************
         * Curve dump
         */
        cout << setw(tab) << left << "Curve Report... " << flush;
        if (params.hasGroup("curves") && params.get("curves", "active") == "Y") {
            writeCurves(params, marketParameters, market);
            cout << "OK" << endl;
        } else {
            LOG("skip curve report");
            cout << "SKIP" << endl;
        }

        /*********************
         * Portfolio valuation
         */
        cout << setw(tab) << left << "NPV Report... " << flush;
        if (params.hasGroup("npv") && params.get("npv", "active") == "Y") {
            writeNpv(params, market, params.get("markets", "pricing"), portfolio);
            cout << "OK" << endl;
        } else {
            LOG("skip portfolio valuation");
            cout << "SKIP" << endl;
        }

        /**********************
         * Cash flow generation
         */
        cout << setw(tab) << left << "Cashflow Report... " << flush;
        if (params.hasGroup("cashflow") && params.get("cashflow", "active") == "Y") {
            writeCashflow(params, portfolio);
            cout << "OK" << endl;
        } else {
            LOG("skip cashflow generation");
            cout << "SKIP" << endl;
        }

        /**********************
         * Sensitivity analysis
         */
        if (params.hasGroup("sensitivity") && params.get("sensitivity", "active") == "Y") {

            cout << setw(tab) << left << "Sensitivity Report... " << flush;
            // We reset this here because the date grid building below depends on it.
            Settings::instance().evaluationDate() = asof;

            LOG("Get Simulation Market Parameters");
            string marketConfigFile = inputPath + "/" + params.get("sensitivity", "marketConfigFile");
            boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
            simMarketData->fromFile(marketConfigFile);

            LOG("Get Sensitivity Parameters");
            string sensitivityConfigFile = inputPath + "/" + params.get("sensitivity", "sensitivityConfigFile");
            boost::shared_ptr<SensitivityScenarioData> sensiData(new SensitivityScenarioData);
            sensiData->fromFile(sensitivityConfigFile);

            LOG("Get Engine Data");
            string sensiPricingEnginesFile = inputPath + "/" + params.get("sensitivity", "pricingEnginesFile");
            boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
            engineData->fromFile(sensiPricingEnginesFile);

            LOG("Get Portfolio");
            boost::shared_ptr<Portfolio> sensiPortfolio = boost::make_shared<Portfolio>();
	    // Just load here. We build the portfolio in SensitivityAnalysis, after building SimMarket.
	    sensiPortfolio->load(portfolioFile);
	      
            LOG("Build Sensitivity Analysis");
	    string marketConfiguration =  params.get("markets", "pricing");
            boost::shared_ptr<SensitivityAnalysis> sensiAnalysis =
                boost::make_shared<SensitivityAnalysis>(sensiPortfolio, market, marketConfiguration,
                                                        engineData, simMarketData, sensiData, conventions);
	    
            string outputFile1 = outputPath + "/" + params.get("sensitivity", "scenarioOutputFile");
	    Real sensiThreshold = parseReal(params.get("sensitivity", "outputSensitivityThreshold"));
	    sensiAnalysis->writeScenarioReport(outputFile1, sensiThreshold);

	    string outputFile2 = outputPath + "/" + params.get("sensitivity", "sensitivityOutputFile");
	    sensiAnalysis->writeSensitivityReport(outputFile2, sensiThreshold);

	    string outputFile3 = outputPath + "/" + params.get("sensitivity", "crossGammaOutputFile");
	    sensiAnalysis->writeCrossGammaReport(outputFile3, sensiThreshold);

            cout << "OK" << endl;

        } else {
            LOG("skip sensitivity report");
	}

        /******************************************
         * Simulation: Scenario and Cube Generation
         */

        boost::shared_ptr<AggregationScenarioData> inMemoryScenarioData;
        boost::shared_ptr<NPVCube> inMemoryCube;
        Size cubeDepth = 0;

        if (params.hasGroup("simulation") && params.get("simulation", "active") == "Y") {

            cout << setw(tab) << left << "Simulation Setup... ";
            fflush(stdout);
            LOG("Build Simulation Model");
            string simulationConfigFile = inputPath + "/" + params.get("simulation", "simulationConfigFile");
            LOG("Load simulation model data from file: " << simulationConfigFile);
            boost::shared_ptr<CrossAssetModelData> modelData = boost::make_shared<CrossAssetModelData>();
            modelData->fromFile(simulationConfigFile);
            CrossAssetModelBuilder modelBuilder(market, params.get("markets", "lgmcalibration"),
                                                params.get("markets", "fxcalibration"),
                                                params.get("markets", "simulation"));
            boost::shared_ptr<QuantExt::CrossAssetModel> model = modelBuilder.build(modelData);

            LOG("Load Simulation Market Parameters");
            boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
            simMarketData->fromFile(simulationConfigFile);

            LOG("Load Simulation Parameters");
            boost::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
            sgd->fromFile(simulationConfigFile);
            ScenarioGeneratorBuilder sgb(sgd);
            boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
            boost::shared_ptr<ScenarioGenerator> sg = sgb.build(
                model, sf, simMarketData, asof, market, params.get("markets", "simulation")); // pricing or simulation?

            // Optionally write out scenarios
            if (params.has("simulation", "scenariodump")) {
                string filename = outputPath + "/" + params.get("simulation", "scenariodump");
                sg = boost::make_shared<ScenarioWriter>(sg, filename);
            }

            boost::shared_ptr<ore::analytics::DateGrid> grid = sgd->grid();

            LOG("Build Simulation Market");
            boost::shared_ptr<ScenarioSimMarket> simMarket = boost::make_shared<ScenarioSimMarket>(
                sg, market, simMarketData, conventions, params.get("markets", "simulation"));

            LOG("Build engine factory for pricing under scenarios, linked to sim market");
            boost::shared_ptr<EngineData> simEngineData = boost::make_shared<EngineData>();
            string simPricingEnginesFile = inputPath + "/" + params.get("simulation", "pricingEnginesFile");
            simEngineData->fromFile(simPricingEnginesFile);
            map<MarketContext, string> configurations;
            configurations[MarketContext::irCalibration] = params.get("markets", "lgmcalibration");
            configurations[MarketContext::fxCalibration] = params.get("markets", "fxcalibration");
            configurations[MarketContext::pricing] = params.get("markets", "simulation");
            boost::shared_ptr<EngineFactory> simFactory =
                boost::make_shared<EngineFactory>(simEngineData, simMarket, configurations);

            LOG("Build portfolio linked to sim market");
            boost::shared_ptr<Portfolio> simPortfolio = boost::make_shared<Portfolio>();
            simPortfolio->load(portfolioFile);
            simPortfolio->build(simFactory);
            QL_REQUIRE(simPortfolio->size() == portfolio->size(),
                       "portfolio size mismatch, check simulation market setup");
            cout << "OK" << endl;

            LOG("Build valuation cube engine");
            Size samples = sgd->samples();
            string baseCurrency = params.get("simulation", "baseCurrency");
            if (params.has("simulation", "storeFlows") && params.get("simulation", "storeFlows") == "Y")
                cubeDepth = 2; // NPV and FLOW
            else
                cubeDepth = 1; // NPV only

            // Valuation calculators
            vector<boost::shared_ptr<ValuationCalculator> > calculators;
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
            if (params.has("simulation", "cubeFile")) {
                string cubeFileName = outputPath + "/" + params.get("simulation", "cubeFile");
                inMemoryCube->save(cubeFileName);
                cout << "OK" << endl;
            } else
                cout << "SKIP" << endl;

            cout << setw(tab) << left << "Write Aggregation Scenario Data... " << flush;
            LOG("Write scenario data");
            if (params.has("simulation", "additionalScenarioDataFileName")) {
                string outputFileNameAddScenData =
                    outputPath + "/" + params.get("simulation", "additionalScenarioDataFileName");
                inMemoryScenarioData->save(outputFileNameAddScenData);
                cout << "OK" << endl;
            } else
                cout << "SKIP" << endl;
        } else {
            LOG("skip simulation");
        }

        /*****************************
         * Aggregation and XVA Reports
         */
        cout << setw(tab) << left << "Aggregation and XVA Reports... " << flush;
        if (params.hasGroup("xva") && params.get("xva", "active") == "Y") {

            // We reset this here because the date grid building below depends on it.
            Settings::instance().evaluationDate() = asof;

            string csaFile = inputPath + "/" + params.get("xva", "csaFile");
            boost::shared_ptr<NettingSetManager> netting = boost::make_shared<NettingSetManager>();
            netting->fromFile(csaFile);

            map<string, bool> analytics;
            analytics["exerciseNextBreak"] = parseBool(params.get("xva", "exerciseNextBreak"));
            analytics["exposureProfiles"] = parseBool(params.get("xva", "exposureProfiles"));
            analytics["cva"] = parseBool(params.get("xva", "cva"));
            analytics["dva"] = parseBool(params.get("xva", "dva"));
            analytics["fva"] = parseBool(params.get("xva", "fva"));
            analytics["colva"] = parseBool(params.get("xva", "colva"));
            analytics["collateralFloor"] = parseBool(params.get("xva", "collateralFloor"));
            if (params.has("xva", "mva"))
                analytics["mva"] = parseBool(params.get("xva", "mva"));
            else
                analytics["mva"] = false;
            if (params.has("xva", "dim"))
                analytics["dim"] = parseBool(params.get("xva", "dim"));
            else
                analytics["dim"] = false;

            boost::shared_ptr<NPVCube> cube;
            if (inMemoryCube)
                cube = inMemoryCube;
            else {
                Size cubeDepth = 1;
                if (params.has("xva", "hyperCube"))
                    cubeDepth = parseBool(params.get("xva", "hyperCube")) ? 2 : 1;

                if (cubeDepth > 1)
                    cube = boost::make_shared<SinglePrecisionInMemoryCubeN>();
                else
                    cube = boost::make_shared<SinglePrecisionInMemoryCube>();
                string cubeFile = outputPath + "/" + params.get("xva", "cubeFile");
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
                string scenarioFile = outputPath + "/" + params.get("xva", "scenarioFile");
                scenarioData->load(scenarioFile);
            }

            QL_REQUIRE(scenarioData->dimDates() == cube->dates().size(), "scenario dates do not match cube grid size");
            QL_REQUIRE(scenarioData->dimSamples() == cube->samples(),
                       "scenario sample size does not match cube sample size");

            string baseCurrency = params.get("xva", "baseCurrency");
            string calculationType = params.get("xva", "calculationType");
            string allocationMethod = params.get("xva", "allocationMethod");
            Real marginalAllocationLimit = parseReal(params.get("xva", "marginalAllocationLimit"));
            Real quantile = parseReal(params.get("xva", "quantile"));
            string dvaName = params.get("xva", "dvaName");
            string fvaLendingCurve = params.get("xva", "fvaLendingCurve");
            string fvaBorrowingCurve = params.get("xva", "fvaBorrowingCurve");
            Real collateralSpread = parseReal(params.get("xva", "collateralSpread"));

            Real dimQuantile = 0.99;
            Size dimHorizonCalendarDays = 14;
            Size dimRegressionOrder = 0;
            vector<string> dimRegressors;
            Real dimScaling = 1.0;
            Size dimLocalRegressionEvaluations = 0;
            Real dimLocalRegressionBandwidth = 0.25;

            if (analytics["mva"] || analytics["dim"]) {
                dimQuantile = parseReal(params.get("xva", "dimQuantile"));
                dimHorizonCalendarDays = parseInteger(params.get("xva", "dimHorizonCalendarDays"));
                dimRegressionOrder = parseInteger(params.get("xva", "dimRegressionOrder"));
                string dimRegressorsString = params.get("xva", "dimRegressors");
                dimRegressors = parseListOfValues(dimRegressorsString);
                dimScaling = parseReal(params.get("xva", "dimScaling"));
                dimLocalRegressionEvaluations = parseInteger(params.get("xva", "dimLocalRegressionEvaluations"));
                dimLocalRegressionBandwidth = parseReal(params.get("xva", "dimLocalRegressionBandwidth"));
            }

            string marketConfiguration = params.get("markets", "simulation");

            boost::shared_ptr<PostProcess> postProcess = boost::make_shared<PostProcess>(
                portfolio, netting, market, marketConfiguration, cube, scenarioData, analytics, baseCurrency,
                allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
                fvaLendingCurve, collateralSpread, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder,
                dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth, dimScaling);

            writeTradeExposures(params, postProcess);
            writeNettingSetExposures(params, postProcess);
            writeXVA(params, portfolio, postProcess);
            writeNettingSetColva(params, postProcess);

            string rawCubeOutputFile = params.get("xva", "rawCubeOutputFile");
            CubeWriter cw1(outputPath + "/" + rawCubeOutputFile);
            map<string, string> nettingSetMap = portfolio->nettingSetMap();
            cw1.write(cube, nettingSetMap);

            string netCubeOutputFile = params.get("xva", "netCubeOutputFile");
            CubeWriter cw2(outputPath + "/" + netCubeOutputFile);
            cw2.write(postProcess->netCube(), nettingSetMap);

            if (analytics["dim"]) {
                string dimFile1 = outputPath + "/" + params.get("xva", "dimEvolutionFile");
                string dimFile2 = outputPath + "/" + params.get("xva", "dimRegressionFile");
                string nettingSet = params.get("xva", "dimOutputNettingSet");
                int dimOutputGridPoint = parseInteger(params.get("xva", "dimOutputGridPoint"));
                postProcess->exportDimEvolution(dimFile1, nettingSet);
                postProcess->exportDimRegression(dimFile2, nettingSet, dimOutputGridPoint);
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

    return 0;
}

void writeNpv(const Parameters& params, boost::shared_ptr<Market> market, const std::string& configuration,
              boost::shared_ptr<Portfolio> portfolio) {
    LOG("portfolio valuation");
    // Date asof = Settings::instance().evaluationDate();
    string outputPath = params.get("setup", "outputPath");
    string npvFile = outputPath + "/" + params.get("npv", "outputFileName");
    string baseCurrency = params.get("npv", "baseCurrency");
    ofstream file;
    file.open(npvFile.c_str());
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    DayCounter dc = ActualActual();
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(file.is_open(), "error opening file " << npvFile);
    file << "#TradeId,TradeType,Maturity,MaturityTime,NPV,NpvCurrency,NPV(Base),BaseCurrency" << endl;
    for (auto trade : portfolio->trades()) {
        string npvCcy = trade->npvCurrency();
        Real fx = 1.0;
        if (npvCcy != baseCurrency)
            fx = market->fxSpot(npvCcy + baseCurrency, configuration)->value();
        file << trade->id() << sep << trade->tradeType() << sep << io::iso_date(trade->maturity()) << sep
             << dc.yearFraction(today, trade->maturity()) << sep;
        try {
            Real npv = trade->instrument()->NPV();
            file << npv << sep << npvCcy << sep << npv * fx << sep << baseCurrency << endl;
        } catch (std::exception& e) {
            ALOG("Exception during pricing trade " << trade->id() << ": " << e.what());
            file << "#NA" << sep << "#NA" << sep << "#NA" << sep << "#NA" << endl;
        }
    }
    file.close();
    LOG("NPV file written to " << npvFile);
}

void writeCashflow(const Parameters& params, boost::shared_ptr<Portfolio> portfolio) {
    Date asof = Settings::instance().evaluationDate();
    string outputPath = params.get("setup", "outputPath");
    string fileName = outputPath + "/" + params.get("cashflow", "outputFileName");
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';

    LOG("Writing cashflow report to " << fileName << " for " << asof);

    file << "#ID" << sep << "Type" << sep << "LegNo" << sep << "PayDate" << sep << "Amount" << sep << "Currency" << sep
         << "Coupon" << sep << "Accrual" << sep << "fixingDate" << sep << "fixingValue" << sep << endl;

    const vector<boost::shared_ptr<Trade> >& trades = portfolio->trades();

    for (Size k = 0; k < trades.size(); k++) {
        if (trades[k]->tradeType() == "Swaption" || trades[k]->tradeType() == "CapFloor") {
            WLOG("cashflow for " << trades[k]->tradeType() << " " << trades[k]->id() << " skipped");
            continue;
        }
        try {
            const vector<Leg>& legs = trades[k]->legs();
            for (size_t i = 0; i < legs.size(); i++) {
                const QuantLib::Leg& leg = legs[i];
                bool payer = trades[k]->legPayers()[i];
                string ccy = trades[k]->legCurrencies()[i];
                for (size_t j = 0; j < leg.size(); j++) {
                    boost::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
                    Date payDate = ptrFlow->date();
                    if (payDate >= asof) {
                        file << setprecision(0) << trades[k]->id() << sep << trades[k]->tradeType() << sep << i << sep
                             << QuantLib::io::iso_date(payDate) << sep;
                        Real amount = ptrFlow->amount();
                        if (payer)
                            amount *= -1.0;
                        file << setprecision(4) << amount << sep << ccy << sep;

                        std::string ccy = trades[k]->legCurrencies()[i];

                        boost::shared_ptr<QuantLib::Coupon> ptrCoupon =
                            boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                        if (ptrCoupon) {
                            Real coupon = ptrCoupon->rate();
                            Real accrual = ptrCoupon->accrualPeriod();
                            file << setprecision(10) << coupon << sep << setprecision(10) << accrual << sep;
                        } else
                            file << sep << sep;

                        boost::shared_ptr<QuantLib::FloatingRateCoupon> ptrFloat =
                            boost::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                        if (ptrFloat) {
                            Date fixingDate = ptrFloat->fixingDate();
                            Real fixingValue = ptrFloat->index()->fixing(fixingDate);
                            file << QuantLib::io::iso_date(fixingDate) << sep << fixingValue << endl;
                        } else
                            file << sep << sep << endl;
                    }
                }
            }
        } catch (std::exception& e) {
            LOG("Exception writing to " << fileName << " : " << e.what());
        } catch (...) {
            LOG("Exception writing to " << fileName << " : Unkown Exception");
        }
    }
    file.close();
    LOG("Cashflow report written to " << fileName);
}

void writeCurves(const Parameters& params, const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market) {
    LOG("Write yield curve discount factors... ");

    string outputPath = params.get("setup", "outputPath");
    string fileName = outputPath + "/" + params.get("curves", "outputFileName");
    ofstream file(fileName.c_str());
    QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
    file.precision(15);
    char sep = ',';

    string configID = params.get("curves", "configuration");
    QL_REQUIRE(marketConfig.hasConfiguration(configID), "curve configuration " << configID << " not found");

    map<string, string> discountCurves = marketConfig.discountingCurves(configID);
    map<string, string> YieldCurves = marketConfig.yieldCurves(configID);
    map<string, string> indexCurves = marketConfig.indexForwardingCurves(configID);
    string gridString = params.get("curves", "grid");
    DateGrid grid(gridString);

    vector<Handle<YieldTermStructure> > yieldCurves;

    file << "Tenor" << sep << "Date";
    for (auto it : discountCurves) {
        file << sep << it.first;
        yieldCurves.push_back(market->discountCurve(it.first, configID));
    }
    for (auto it : YieldCurves) {
        file << sep << it.first;
        yieldCurves.push_back(market->yieldCurve(it.first, configID));
    }
    for (auto it : indexCurves) {
        file << sep << it.first;
        yieldCurves.push_back(market->iborIndex(it.first, configID)->forwardingTermStructure());
    }
    file << endl;

    // Output the discount factors for each tenor in turn
    for (Size j = 0; j < grid.size(); ++j) {
        Date date = grid[j];
        file << grid.tenors()[j] << sep << QuantLib::io::iso_date(date);
        for (Size i = 0; i < yieldCurves.size(); ++i)
            file << sep << yieldCurves[i]->discount(date);
        file << endl;
    }

    file.close();
}

void writeTradeExposures(const Parameters& params, boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    for (Size i = 0; i < postProcess->tradeIds().size(); ++i) {
        string tradeId = postProcess->tradeIds()[i];
        ostringstream o;
        o << outputPath << "/exposure_trade_" << tradeId << ".csv";
        string fileName = o.str();
        ofstream file(fileName.c_str());
        QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
        const vector<Real>& epe = postProcess->tradeEPE(tradeId);
        const vector<Real>& ene = postProcess->tradeENE(tradeId);
        const vector<Real>& ee_b = postProcess->tradeEE_B(tradeId);
        const vector<Real>& eee_b = postProcess->tradeEEE_B(tradeId);
        const vector<Real>& pfe = postProcess->tradePFE(tradeId);
        const vector<Real>& aepe = postProcess->allocatedTradeEPE(tradeId);
        const vector<Real>& aene = postProcess->allocatedTradeENE(tradeId);
        file << "#TradeId,Date,Time,EPE,ENE,AllocatedEPE,AllocatedENE,PFE,BaselEE,BaselEEE" << endl;
        file << tradeId << "," << QuantLib::io::iso_date(today) << "," << 0.0 << "," << epe[0] << "," << ene[0] << ","
             << aepe[0] << "," << aene[0] << "," << pfe[0] << "," << ee_b[0] << "," << eee_b[0] << endl;
        for (Size j = 0; j < dates.size(); ++j) {
            Time time = dc.yearFraction(today, dates[j]);
            file << tradeId << "," << QuantLib::io::iso_date(dates[j]) << "," << time << "," << epe[j + 1] << ","
                 << ene[j + 1] << "," << aepe[j + 1] << "," << aene[j + 1] << "," << pfe[j + 1] << "," << ee_b[j + 1]
                 << "," << eee_b[j + 1] << endl;
        }
        file.close();
    }
}

void writeNettingSetExposures(const Parameters& params, boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    for (auto n : postProcess->nettingSetIds()) {
        ostringstream o;
        o << outputPath << "/exposure_nettingset_" << n << ".csv";
        string fileName = o.str();
        ofstream file(fileName.c_str());
        QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
        const vector<Real>& epe = postProcess->netEPE(n);
        const vector<Real>& ene = postProcess->netENE(n);
        const vector<Real>& ee_b = postProcess->netEE_B(n);
        const vector<Real>& eee_b = postProcess->netEEE_B(n);
        const vector<Real>& pfe = postProcess->netPFE(n);
        const vector<Real>& ecb = postProcess->expectedCollateral(n);
        file << "#NettingSet,Date,Time,EPE,ENE,PFE,ExpectedCollateral,BaselEE,BaselEEE" << endl;
        file << n << "," << QuantLib::io::iso_date(today) << "," << 0.0 << "," << epe[0] << "," << ene[0] << ","
             << pfe[0] << "," << ecb[0] << "," << ee_b[0] << "," << eee_b[0] << endl;
        for (Size j = 0; j < dates.size(); ++j) {
            Real time = dc.yearFraction(today, dates[j]);
            file << n << "," << QuantLib::io::iso_date(dates[j]) << "," << time << "," << epe[j + 1] << ","
                 << ene[j + 1] << "," << pfe[j + 1] << "," << ecb[j + 1] << "," << ee_b[j + 1] << "," << eee_b[j + 1]
                 << endl;
        }
        file.close();
    }
}

void writeXVA(const Parameters& params, boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    string allocationMethod = params.get("xva", "allocationMethod");
    const vector<Date> dates = postProcess->cube()->dates();
    DayCounter dc = ActualActual();
    string fileName = outputPath + "/xva.csv";
    ofstream file(fileName.c_str());
    QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
    file
        << "#TradeId,NettingSetId,CVA,DVA,FBA,FCA,COLVA,MVA,CollateralFloor,AllocatedCVA,AllocatedDVA,AllocationMethod,"
           "BaselEPE,BaselEEPE"
        << endl;
    for (auto n : postProcess->nettingSetIds()) {
        file << "," << n << "," << postProcess->nettingSetCVA(n) << "," << postProcess->nettingSetDVA(n) << ","
             << postProcess->nettingSetFBA(n) << "," << postProcess->nettingSetFCA(n) << ","
             << postProcess->nettingSetCOLVA(n) << "," << postProcess->nettingSetMVA(n) << ","
             << postProcess->nettingSetCollateralFloor(n) << "," << postProcess->nettingSetCVA(n) << ","
             << postProcess->nettingSetDVA(n) << "," << allocationMethod << "," << postProcess->netEPE_B(n) << ","
             << postProcess->netEEPE_B(n) << endl;
        for (Size k = 0; k < portfolio->trades().size(); ++k) {
            string tid = portfolio->trades()[k]->id();
            string nid = portfolio->trades()[k]->envelope().nettingSetId();
            if (nid != n)
                continue;
            file << tid << "," << nid << "," << postProcess->tradeCVA(tid) << "," << postProcess->tradeDVA(tid) << ","
                 << postProcess->tradeFBA(tid) << "," << postProcess->tradeFCA(tid) << ","
                 << "n/a," // no trade COLVA
                 << "n/a," // no trade MVA
                 << "n/a," // no trade collateral floor
                 << postProcess->allocatedTradeCVA(tid) << "," << postProcess->allocatedTradeDVA(tid) << ","
                 << allocationMethod << "," << postProcess->tradeEPE_B(tid) << "," << postProcess->tradeEEPE_B(tid)
                 << endl;
        }
    }
    file.close();
}

void writeNettingSetColva(const Parameters& params, boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    for (auto n : postProcess->nettingSetIds()) {
        ostringstream o;
        o << outputPath << "/colva_nettingset_" << n << ".csv";
        string fileName = o.str();
        ofstream file(fileName.c_str());
        QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
        const vector<Real>& collateral = postProcess->expectedCollateral(n);
        const vector<Real>& colvaInc = postProcess->colvaIncrements(n);
        const vector<Real>& floorInc = postProcess->collateralFloorIncrements(n);
        Real colva = postProcess->nettingSetCOLVA(n);
        Real floorValue = postProcess->nettingSetCollateralFloor(n);
        file
            << "#NettingSet,Date,Time,CollateralBalance,COLVA Increment,COLVA,CollateralFloor Increment,CollateralFloor"
            << endl;
        file << n << ",,,," << colva << "," << floorValue << endl;
        Real colvaSum = 0.0;
        Real floorSum = 0.0;
        for (Size j = 0; j < dates.size(); ++j) {
            Real time = dc.yearFraction(today, dates[j]);
            colvaSum += colvaInc[j + 1];
            floorSum += floorInc[j + 1];
            file << n << "," << QuantLib::io::iso_date(dates[j]) << "," << time << "," << collateral[j + 1] << ","
                 << colvaInc[j + 1] << "," << colvaSum << "," << floorInc[j + 1] << "," << floorSum << endl;
        }
        file.close();
    }
}

bool Parameters::hasGroup(const string& groupName) const { return (data_.find(groupName) != data_.end()); }

bool Parameters::has(const string& groupName, const string& paramName) const {
    QL_REQUIRE(hasGroup(groupName), "param group '" << groupName << "' not found");
    auto it = data_.find(groupName);
    return (it->second.find(paramName) != it->second.end());
}

string Parameters::get(const string& groupName, const string& paramName) const {
    QL_REQUIRE(has(groupName, paramName), "parameter " << paramName << " not found in param group " << groupName);
    auto it = data_.find(groupName);
    return it->second.find(paramName)->second;
}

void Parameters::fromFile(const string& fileName) {
    LOG("load ORE configuration from " << fileName);
    clear();
    XMLDocument doc(fileName);
    fromXML(doc.getFirstNode("ORE"));
    LOG("load ORE configuration from " << fileName << " done.");
}

void Parameters::clear() { data_.clear(); }

void Parameters::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ORE");

    XMLNode* setupNode = XMLUtils::getChildNode(node, "Setup");
    QL_REQUIRE(setupNode, "node Setup not found in parameter file");
    map<string, string> setupMap;
    for (XMLNode* child = XMLUtils::getChildNode(setupNode); child; child = XMLUtils::getNextSibling(child)) {
        string key = XMLUtils::getAttribute(child, "name");
        string value = XMLUtils::getNodeValue(child);
        setupMap[key] = value;
    }
    data_["setup"] = setupMap;

    XMLNode* marketsNode = XMLUtils::getChildNode(node, "Markets");
    if (marketsNode) {
        map<string, string> marketsMap;
        for (XMLNode* child = XMLUtils::getChildNode(marketsNode); child; child = XMLUtils::getNextSibling(child)) {
            string key = XMLUtils::getAttribute(child, "name");
            string value = XMLUtils::getNodeValue(child);
            marketsMap[key] = value;
        }
        data_["markets"] = marketsMap;
    }

    XMLNode* analyticsNode = XMLUtils::getChildNode(node, "Analytics");
    if (analyticsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(analyticsNode); child; child = XMLUtils::getNextSibling(child)) {
            string groupName = XMLUtils::getAttribute(child, "type");
            map<string, string> analyticsMap;
            for (XMLNode* paramNode = XMLUtils::getChildNode(child); paramNode;
                 paramNode = XMLUtils::getNextSibling(paramNode)) {
                string key = XMLUtils::getAttribute(paramNode, "name");
                string value = XMLUtils::getNodeValue(paramNode);
                analyticsMap[key] = value;
            }
            data_[groupName] = analyticsMap;
        }
    }
}

XMLNode* Parameters::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ORE");
    QL_FAIL("Parameters::toXML not implemented yet");
    return node;
}

void Parameters::log() {
    LOG("Parameters:");
    for (auto p : data_)
        for (auto pp : p.second)
            LOG("group = " << p.first << " : " << pp.first << " = " << pp.second);
}
