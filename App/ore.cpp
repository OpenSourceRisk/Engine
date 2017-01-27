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
              boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<ore::data::Report> report);

void writeCashflow(boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<ore::data::Report> report);

void writeCurves(const Parameters& params, const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market, boost::shared_ptr<ore::data::Report> report);

void writeTradeExposures(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report,
                         const std::string& tradeId);

void writeNettingSetExposures(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report,
                              const std::string& nettingSetId);

void writeNettingSetColva(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report,
                          const std::string& nettingSetId);

void writeXVA(const Parameters& params, boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report);

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

        boost::filesystem::path p{outputPath};
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
            string curvesFile = outputPath + "/" + params.get("curves", "outputFileName");
            boost::shared_ptr<Report> curvesReport = boost::make_shared<CSVFileReport>(curvesFile);
            writeCurves(params, marketParameters, market, curvesReport);
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
            string npvFile = outputPath + "/" + params.get("npv", "outputFileName");
            boost::shared_ptr<Report> npvReport = boost::make_shared<CSVFileReport>(npvFile);
            writeNpv(params, market, params.get("markets", "pricing"), portfolio, npvReport);
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
            string cashflowFile = outputPath + "/" + params.get("cashflow", "outputFileName");
            boost::shared_ptr<Report> cashflowReport = boost::make_shared<CSVFileReport>(cashflowFile);
            writeCashflow(portfolio, cashflowReport);
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
            cout << setw(tab) << left << "Simulation... ";
            cout << "SKIP" << endl;
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

            for (auto t : postProcess->tradeIds()) {
                ostringstream o;
                o << outputPath << "/exposure_trade_" << t << ".csv";
                string tradeExposureFile = o.str();
                boost::shared_ptr<Report> tradeExposureReport = boost::make_shared<CSVFileReport>(tradeExposureFile);
                writeTradeExposures(postProcess, tradeExposureReport, t);
            }
            for (auto n : postProcess->nettingSetIds()) {
                ostringstream o1;
                o1 << outputPath << "/exposure_nettingset_" << n << ".csv";
                string nettingSetExposureFile = o1.str();
                boost::shared_ptr<Report> nettingSetExposureReport =
                    boost::make_shared<CSVFileReport>(nettingSetExposureFile);
                writeNettingSetExposures(postProcess, nettingSetExposureReport, n);

                ostringstream o2;
                o2 << outputPath << "/colva_nettingset_" << n << ".csv";
                string nettingSetColvaFile = o2.str();
                boost::shared_ptr<Report> nettingSetColvaReport =
                    boost::make_shared<CSVFileReport>(nettingSetColvaFile);
                writeNettingSetColva(postProcess, nettingSetColvaReport, n);
            }

            string XvaFile = outputPath + "/xva.csv";
            boost::shared_ptr<Report> xvaReport = boost::make_shared<CSVFileReport>(XvaFile);
            writeXVA(params, portfolio, postProcess, xvaReport);

            string rawCubeOutputFile = params.get("xva", "rawCubeOutputFile");
            CubeWriter cw1(outputPath + "/" + rawCubeOutputFile);
            map<string, string> nettingSetMap = portfolio->nettingSetMap();
            cw1.write(cube, nettingSetMap);

            string netCubeOutputFile = params.get("xva", "netCubeOutputFile");
            CubeWriter cw2(outputPath + "/" + netCubeOutputFile);
            cw2.write(postProcess->netCube(), nettingSetMap);

            if (analytics["dim"]) {
                string dimFile1 = outputPath + "/" + params.get("xva", "dimEvolutionFile");
                vector<string> dimFiles2;
                for (auto f : parseListOfValues(params.get("xva", "dimRegressionFiles")))
                    dimFiles2.push_back(outputPath + "/" + f);
                string nettingSet = params.get("xva", "dimOutputNettingSet");
                std::vector<Size> dimOutputGridPoints =
                    parseListOfValues<Size>(params.get("xva", "dimOutputGridPoints"), &parseInteger);
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

    return 0;
}

void writeNpv(const Parameters& params, boost::shared_ptr<Market> market, const std::string& configuration,
              boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<Report> report) {
    LOG("portfolio valuation");
    string baseCurrency = params.get("npv", "baseCurrency");
    DayCounter dc = ActualActual();
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(report, "error opening file ");
    report->addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("Maturity", Date())
        .addColumn("MaturityTime", double(), 6)
        .addColumn("NPV", double(), 6)
        .addColumn("NpvCurrency", string())
        .addColumn("NPV(Base)", double(), 6)
        .addColumn("BaseCurrency", string());
    for (auto trade : portfolio->trades()) {
        string npvCcy = trade->npvCurrency();
        Real fx = 1.0;
        if (npvCcy != baseCurrency)
            fx = market->fxSpot(npvCcy + baseCurrency, configuration)->value();
        try {
            Real npv = trade->instrument()->NPV();
            report->next()
                .add(trade->id())
                .add(trade->tradeType())
                .add(trade->maturity())
                .add(dc.yearFraction(today, trade->maturity()))
                .add(npv)
                .add(npvCcy)
                .add(npv * fx)
                .add(baseCurrency);
        } catch (std::exception& e) {
            ALOG("Exception during pricing trade " << trade->id() << ": " << e.what());
            report->next()
                .add(trade->id())
                .add(trade->tradeType())
                .add(trade->maturity())
                .add(dc.yearFraction(today, trade->maturity()))
                .add(Null<Real>())
                .add("#NA")
                .add(Null<Real>())
                .add("#NA");
        }
    }
    report->end();
    LOG("NPV file written");
}

void writeCashflow(boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<Report> report) {
    Date asof = Settings::instance().evaluationDate();
    QL_REQUIRE(report, "error opening report");
    LOG("Writing cashflow report for " << asof);
    report->addColumn("ID", string())
        .addColumn("Type", string())
        .addColumn("LegNo", Size())
        .addColumn("PayDate", Date())
        .addColumn("Amount", double(), 4)
        .addColumn("Currency", string())
        .addColumn("Coupon", double(), 10)
        .addColumn("Accrual", double(), 10)
        .addColumn("fixingDate", Date())
        .addColumn("fixingValue", double(), 10);

    const vector<boost::shared_ptr<Trade>>& trades = portfolio->trades();

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
                        Real amount = ptrFlow->amount();
                        if (payer)
                            amount *= -1.0;
                        std::string ccy = trades[k]->legCurrencies()[i];
                        boost::shared_ptr<QuantLib::Coupon> ptrCoupon =
                            boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                        Real coupon;
                        Real accrual;
                        if (ptrCoupon) {
                            coupon = ptrCoupon->rate();
                            accrual = ptrCoupon->accrualPeriod();
                        } else {
                            coupon = Null<Real>();
                            accrual = Null<Real>();
                        }
                        boost::shared_ptr<QuantLib::FloatingRateCoupon> ptrFloat =
                            boost::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                        Date fixingDate;
                        Real fixingValue;
                        if (ptrFloat) {
                            fixingDate = ptrFloat->fixingDate();
                            fixingValue = ptrFloat->index()->fixing(fixingDate);
                        } else {
                            fixingDate = Null<Date>();
                            fixingValue = Null<Real>();
                        }
                        report->next()
                            .add(trades[k]->id())
                            .add(trades[k]->tradeType())
                            .add(i)
                            .add(payDate)
                            .add(amount)
                            .add(ccy)
                            .add(coupon)
                            .add(accrual)
                            .add(fixingDate)
                            .add(fixingValue);
                    }
                }
            }
        } catch (std::exception& e) {
            LOG("Exception writing cashflow report : " << e.what());
        } catch (...) {
            LOG("Exception writing cashflow report : Unkown Exception");
        }
    }
    report->end();
    LOG("Cashflow report written");
}

void writeCurves(const Parameters& params, const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market, boost::shared_ptr<Report> report) {
    LOG("Write yield curve discount factors... ");

    string configID = params.get("curves", "configuration");
    QL_REQUIRE(marketConfig.hasConfiguration(configID), "curve configuration " << configID << " not found");

    map<string, string> discountCurves = marketConfig.discountingCurves(configID);
    map<string, string> YieldCurves = marketConfig.yieldCurves(configID);
    map<string, string> indexCurves = marketConfig.indexForwardingCurves(configID);
    string gridString = params.get("curves", "grid");
    DateGrid grid(gridString);

    vector<Handle<YieldTermStructure>> yieldCurves;

    report->addColumn("Tenor", Period()).addColumn("Date", Date());

    for (auto it : discountCurves) {
        report->addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->discountCurve(it.first));
    }
    for (auto it : YieldCurves) {
        report->addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->yieldCurve(it.first));
    }
    for (auto it : indexCurves) {
        report->addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->iborIndex(it.first)->forwardingTermStructure());
    }

    // Output the discount factors for each tenor in turn
    for (Size j = 0; j < grid.size(); ++j) {
        Date date = grid[j];
        report->next().add(grid.tenors()[j]).add(date);
        for (Size i = 0; i < yieldCurves.size(); ++i)
            report->add(yieldCurves[i]->discount(date));
    }
    report->end();
}

void writeTradeExposures(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<Report> report,
                         const string& tradeId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    QL_REQUIRE(report, "Error opening report");
    const vector<Real>& epe = postProcess->tradeEPE(tradeId);
    const vector<Real>& ene = postProcess->tradeENE(tradeId);
    const vector<Real>& ee_b = postProcess->tradeEE_B(tradeId);
    const vector<Real>& eee_b = postProcess->tradeEEE_B(tradeId);
    const vector<Real>& pfe = postProcess->tradePFE(tradeId);
    const vector<Real>& aepe = postProcess->allocatedTradeEPE(tradeId);
    const vector<Real>& aene = postProcess->allocatedTradeENE(tradeId);
    report->addColumn("TradeId", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double())
        .addColumn("ENE", double())
        .addColumn("AllocatedEPE", double())
        .addColumn("AllocatedENE", double())
        .addColumn("PFE", double())
        .addColumn("BaselEE", double())
        .addColumn("BaselEEE", double());

    report->next()
        .add(tradeId)
        .add(today)
        .add(0.0)
        .add(epe[0])
        .add(ene[0])
        .add(aepe[0])
        .add(aene[0])
        .add(pfe[0])
        .add(ee_b[0])
        .add(eee_b[0]);
    for (Size j = 0; j < dates.size(); ++j) {
        Time time = dc.yearFraction(today, dates[j]);
        report->next()
            .add(tradeId)
            .add(dates[j])
            .add(time)
            .add(epe[j + 1])
            .add(ene[j + 1])
            .add(aepe[j + 1])
            .add(aene[j + 1])
            .add(pfe[j + 1])
            .add(ee_b[j + 1])
            .add(eee_b[j + 1]);
    }
    report->end();
}

void writeNettingSetExposures(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<Report> report,
                              const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    QL_REQUIRE(report, "Error opening report");
    const vector<Real>& epe = postProcess->netEPE(nettingSetId);
    const vector<Real>& ene = postProcess->netENE(nettingSetId);
    const vector<Real>& ee_b = postProcess->netEE_B(nettingSetId);
    const vector<Real>& eee_b = postProcess->netEEE_B(nettingSetId);
    const vector<Real>& pfe = postProcess->netPFE(nettingSetId);
    const vector<Real>& ecb = postProcess->expectedCollateral(nettingSetId);
    report->addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2);

    report->next()
        .add(nettingSetId)
        .add(today)
        .add(0.0)
        .add(epe[0])
        .add(ene[0])
        .add(pfe[0])
        .add(ecb[0])
        .add(ee_b[0])
        .add(eee_b[0]);
    for (Size j = 0; j < dates.size(); ++j) {
        Real time = dc.yearFraction(today, dates[j]);
        report->next()
            .add(nettingSetId)
            .add(dates[j])
            .add(time)
            .add(epe[j + 1])
            .add(ene[j + 1])
            .add(pfe[j + 1])
            .add(ecb[j + 1])
            .add(ee_b[j + 1])
            .add(eee_b[j + 1]);
    }
    report->end();
}

void writeXVA(const Parameters& params, boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<Report> report) {
    string allocationMethod = params.get("xva", "allocationMethod");
    const vector<Date> dates = postProcess->cube()->dates();
    DayCounter dc = ActualActual();
    QL_REQUIRE(report, "Error opening report");
    report->addColumn("TradeId", string())
        .addColumn("NettingSetId", string())
        .addColumn("CVA", double(), 2)
        .addColumn("DVA", double(), 2)
        .addColumn("FBA", double(), 2)
        .addColumn("FCA", double(), 2)
        .addColumn("COLVA", double(), 2)
        .addColumn("MVA", double(), 2)
        .addColumn("CollateralFloor", double(), 2)
        .addColumn("AllocatedCVA", double(), 2)
        .addColumn("AllocatedDVA", double(), 2)
        .addColumn("AllocationMethod", string())
        .addColumn("BaselEPE", double(), 2)
        .addColumn("BaselEEPE", double(), 2);

    for (auto n : postProcess->nettingSetIds()) {
        report->next()
            .add("")
            .add(n)
            .add(postProcess->nettingSetCVA(n))
            .add(postProcess->nettingSetDVA(n))
            .add(postProcess->nettingSetFBA(n))
            .add(postProcess->nettingSetFCA(n))
            .add(postProcess->nettingSetCOLVA(n))
            .add(postProcess->nettingSetMVA(n))
            .add(postProcess->nettingSetCollateralFloor(n))
            .add(postProcess->nettingSetCVA(n))
            .add(postProcess->nettingSetDVA(n))
            .add(allocationMethod)
            .add(postProcess->netEPE_B(n))
            .add(postProcess->netEEPE_B(n));

        for (Size k = 0; k < portfolio->trades().size(); ++k) {
            string tid = portfolio->trades()[k]->id();
            string nid = portfolio->trades()[k]->envelope().nettingSetId();
            if (nid != n)
                continue;
            report->next()
                .add(tid)
                .add(nid)
                .add(postProcess->tradeCVA(tid))
                .add(postProcess->tradeDVA(tid))
                .add(postProcess->tradeFBA(tid))
                .add(postProcess->tradeFCA(tid))
                .add(Null<Real>())
                .add(Null<Real>())
                .add(Null<Real>())
                .add(postProcess->allocatedTradeCVA(tid))
                .add(postProcess->allocatedTradeDVA(tid))
                .add(allocationMethod)
                .add(postProcess->tradeEPE_B(tid))
                .add(postProcess->tradeEEPE_B(tid));
        }
    }
    report->end();
}

void writeNettingSetColva(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<Report> report,
                          const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    QL_REQUIRE(report, "Error opening report");
    const vector<Real>& collateral = postProcess->expectedCollateral(nettingSetId);
    const vector<Real>& colvaInc = postProcess->colvaIncrements(nettingSetId);
    const vector<Real>& floorInc = postProcess->collateralFloorIncrements(nettingSetId);
    Real colva = postProcess->nettingSetCOLVA(nettingSetId);
    Real floorValue = postProcess->nettingSetCollateralFloor(nettingSetId);

    report->addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 4)
        .addColumn("CollateralBalance", double(), 4)
        .addColumn("COLVA Increment", double(), 4)
        .addColumn("COLVA", double(), 4)
        .addColumn("CollateralFloor Increment", double(), 4)
        .addColumn("CollateralFloor", double(), 4);

    report->next()
        .add(nettingSetId)
        .add(Null<Date>())
        .add(Null<Real>())
        .add(Null<Real>())
        .add(Null<Real>())
        .add(colva)
        .add(Null<Real>())
        .add(floorValue);
    Real colvaSum = 0.0;
    Real floorSum = 0.0;
    for (Size j = 0; j < dates.size(); ++j) {
        Real time = dc.yearFraction(today, dates[j]);
        colvaSum += colvaInc[j + 1];
        floorSum += floorInc[j + 1];
        report->next()
            .add(nettingSetId)
            .add(dates[j])
            .add(time)
            .add(collateral[j + 1])
            .add(colvaInc[j + 1])
            .add(colvaSum)
            .add(floorInc[j + 1])
            .add(floorSum);
    }
    report->end();
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
