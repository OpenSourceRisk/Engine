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


#ifdef BOOST_MSVC
// disable warning C4503: '__LINE__Var': decorated name length exceeded, name was truncated
// This pragma statement needs to be at the top of the file - lower and it will not work:
// http://stackoverflow.com/questions/9673504/is-it-possible-to-disable-compiler-warning-c4503
// http://boost.2283326.n4.nabble.com/General-Warnings-and-pragmas-in-MSVC-td2587449.html
#pragma warning(disable : 4503)
#endif

#include <orea/app/marketdatainmemoryloader.hpp>
#include <orea/app/oreapp.hpp>
#include <orea/orea.hpp>
#include <ored/ored.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/utilities/currencyconfig.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/timer/timer.hpp>

#include <iostream>

using namespace std;
using namespace ore::data;
using namespace ore::analytics;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

std::set<std::string> OREApp::getAnalyticTypes() {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    return analyticsManager_->requestedAnalytics();
}

std::set<std::string> OREApp::getSupportedAnalyticTypes() {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    return analyticsManager_->validAnalytics();
}

const boost::shared_ptr<Analytic>& OREApp::getAnalytic(std::string type) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    return analyticsManager_->getAnalytic(type);
}

std::set<std::string> OREApp::getReportNames() {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    std::set<std::string> names;
    for (const auto& rep : analyticsManager_->reports()) {
        for (auto b : rep.second) {
            string reportName = b.first;
            if (names.find(reportName) == names.end())
                names.insert(reportName);
            else {
                ALOG("report name " << reportName
                     << " occurs more than once, will retrieve the first report with that only");
            }
        }
    }
    return names;
}

boost::shared_ptr<PlainInMemoryReport> OREApp::getReport(std::string reportName) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    for (const auto& rep : analyticsManager_->reports()) {
        for (auto b : rep.second) {
            if (reportName == b.first)
                return boost::make_shared<PlainInMemoryReport>(b.second);
        }
    }
    QL_FAIL("report " << reportName << " not found in results");
}

std::set<std::string> OREApp::getCubeNames() {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    std::set<std::string> names;
    for (const auto& c : analyticsManager_->npvCubes()) {
        for (auto b : c.second) {
            string cubeName = b.first;
            if (names.find(cubeName) == names.end())
                names.insert(cubeName);
            else {
                ALOG("cube name " << cubeName
                     << " occurs more than once, will retrieve the first cube with that name only");
            }
        }
    }
    return names;
}
    
boost::shared_ptr<NPVCube> OREApp::getCube(std::string cubeName) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    for (const auto& c : analyticsManager_->npvCubes()) {
        for (auto b : c.second) {
            if (cubeName == b.first)
                return b.second;
        }
    }
    QL_FAIL("report " << cubeName << " not found in results");
}

std::vector<std::string> OREApp::getErrors() {
    std::vector<std::string> errors;
    while (fbLogger_ && fbLogger_->logger->hasNext())
        errors.push_back(fbLogger_->logger->next());
    return errors;
}
    
void OREApp::run(const std::vector<std::string>& marketData,
                 const std::vector<std::string>& fixingData) {
    try {
        LOG("ORE analytics starting");

        QL_REQUIRE(inputs_, "ORE input parameters not set");
        
        // Set global evaluation date, though already set in the OREAppInputParameters c'tor
        Settings::instance().evaluationDate() = inputs_->asof();

        // FIXME
        QL_REQUIRE(inputs_->pricingEngine(), "pricingEngine not set");
        GlobalPseudoCurrencyMarketParameters::instance().set(inputs_->pricingEngine()->globalParameters());

        // Initialize the global conventions 
        QL_REQUIRE(inputs_->conventions(), "conventions not set");
        InstrumentConventions::instance().setConventions(inputs_->conventions());

        // Create a market data loader that takes input from the provided vectors
        auto loader = boost::make_shared<MarketDataInMemoryLoader>(inputs_, marketData, fixingData);

        // Create the analytics manager
        analyticsManager_ = boost::make_shared<AnalyticsManager>(inputs_, loader);
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics:");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        // Run the requested analytics
        analyticsManager_->runAnalytics(inputs_->analytics());

        // Leave any report writing to the calling aplication
    }
    catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        ALOG(oss.str());
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
        return;
    }
    
    LOG("ORE analytics done");
}

vector<string> OREApp::getFileNames(const string& fileString, const string& path) {
    vector<string> fileNames;
    boost::split(fileNames, fileString, boost::is_any_of(",;"), boost::token_compress_on);
    for (auto it = fileNames.begin(); it < fileNames.end(); it++) {
        boost::trim(*it);
        *it = path + "/" + *it;
    }
    return fileNames;
}

boost::shared_ptr<CSVLoader> OREApp::buildCsvLoader(const boost::shared_ptr<Parameters>& params) {
    bool implyTodaysFixings = false;
    vector<string> marketFiles = {};
    vector<string> fixingFiles = {};
    vector<string> dividendFiles = {};

    std::string inputPath = params_->get("setup", "inputPath");

    std::string tmp = params_->get("setup", "implyTodaysFixings", false);
    if (tmp != "")
        implyTodaysFixings = ore::data::parseBool(tmp);

    tmp = params->get("setup", "marketDataFile", false);
    if (tmp != "")
        marketFiles = getFileNames(tmp, inputPath);
    else {
        ALOG("market data file not found");
    }

    tmp = params->get("setup", "fixingDataFile", false);
    if (tmp != "")
        fixingFiles = getFileNames(tmp, inputPath);
    else {
        ALOG("fixing data file not found");
    }
    
    tmp = params->get("setup", "dividendDataFile", false);
    if (tmp != "")
        dividendFiles = getFileNames(tmp, inputPath);
    else {
        WLOG("dividend data file not found");
    }

    auto loader = boost::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings);

    return loader;
}

void OREApp::analytics() {

    try {
        LOG("ORE analytics starting");

        QL_REQUIRE(params_, "ORE input parameters not set");
                
        Settings::instance().evaluationDate() = inputs_->asof();

        GlobalPseudoCurrencyMarketParameters::instance().set(inputs_->pricingEngine()->globalParameters());

        // Initialize the global conventions 
        InstrumentConventions::instance().setConventions(inputs_->conventions());

        // Create a market data loader that reads market data, fixings, dividends from csv files
        auto csvLoader = buildCsvLoader(params_);
        auto loader = boost::make_shared<MarketDataCsvLoader>(inputs_, csvLoader);

        // Create the analytics manager
        analyticsManager_ = boost::make_shared<AnalyticsManager>(inputs_, loader);
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        // Run the requested analytics
        analyticsManager_->runAnalytics(inputs_->analytics());

        // Write reports to files in the results path
        Analytic::analytic_reports reports = analyticsManager_->reports();
        analyticsManager_->toFile(reports,
                                  inputs_->resultsPath().string(), outputs_->fileNameMap(),
                                  inputs_->csvSeparator(), inputs_->csvCommentCharacter(),
                                  inputs_->csvQuoteChar(), inputs_->reportNaString());

        // Write npv cube(s)
        for (auto a : analyticsManager_->npvCubes()) {
            for (auto b : a.second) {
                LOG("write npv cube " << b.first);
                string reportName = b.first;
                std::string fileName = inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "dat");
                LOG("write npv cube " << reportName << " to file " << fileName);
                saveCube(fileName, *b.second);
            }
        }
        
        // Write market cube(s)
        for (auto a : analyticsManager_->mktCubes()) {
            for (auto b : a.second) {
                string reportName = b.first;
                std::string fileName = inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "dat");
                LOG("write market cube " << reportName << " to file " << fileName);
                saveAggregationScenarioData(fileName, *b.second);
            }
        }        
    }
    catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        ALOG(oss.str());
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
    }

    LOG("ORE analytics done");
}

OREApp::OREApp(boost::shared_ptr<Parameters> params, bool console)
    : params_(params), inputs_(nullptr), cubeDepth_(0) {

    // Read all inputs from params and files referenced in params
    CONSOLEW("Loading inputs");
    inputs_ = boost::make_shared<InputParameters>();
    buildInputParameters(inputs_, params_);
    outputs_ = boost::make_shared<OutputParameters>(params_);
    CONSOLE("OK");
    
    // Set global evaluation date
    asof_ = inputs_->asof();
    Settings::instance().evaluationDate() = asof_;

    // initialise some pointers
    conventions_ = boost::make_shared<Conventions>();
    InstrumentConventions::instance().setConventions(conventions_);
    if (console) {
        ConsoleLog::instance().switchOn();
    }
    
    marketParameters_ = boost::make_shared<TodaysMarketParameters>();
    curveConfigs_ = boost::make_shared<CurveConfigurations>();

    // Set up logging
    setupLog();

    // Read setup
    readSetup();
}

OREApp::OREApp(const boost::shared_ptr<InputParameters>& inputs, const std::string& logFile, Size logLevel,
               bool console)
    : params_(nullptr), inputs_(inputs), asof_(inputs->asof()), cubeDepth_(0) {

    // Initialise Singletons
    Settings::instance().evaluationDate() = asof_;
    InstrumentConventions::instance().setConventions(inputs_->conventions());
    if (console) {
        ConsoleLog::instance().switchOn();
    }

    // Close any open loggers
    closeLog();

    // Initialise file logger and buffered logger
    string logFilePath = (inputs_->resultsPath() / logFile).string();
    boost::filesystem::path p{inputs_->resultsPath()};
    if (!boost::filesystem::exists(p))
        boost::filesystem::create_directories(p);
    QL_REQUIRE(boost::filesystem::is_directory(p),
               "output path '" << inputs_->resultsPath() << "' is not a directory.");

    // Report StructuredErrorMessages with level WARNING, ERROR, CRITICAL, ALERT
    fbLogger_ = boost::make_shared<FilteredBufferedLoggerGuard>();
    Log::instance().registerLogger(boost::make_shared<FileLogger>(logFilePath));
    Log::instance().setMask(logLevel);
    Log::instance().switchOn();
}

OREApp::~OREApp() {
    // Close logs
    closeLog();
}

void OREApp::run(bool useAnalytics) {

    cpu_timer timer;
    
    try {

        if (useAnalytics) {
            analytics();
            return;
        }

        CONSOLE("=====================================");
        CONSOLE("=== DEPRECATED useAnalytics=false ===");
        CONSOLE("=====================================");
        CONSOLE("\nORE starting");

        LOG("ORE starting: DEPRECATED ORE App");
        // readSetup();

        /*********
         * Load Reference Data
         */
        CONSOLEW("Reference... ");
        getReferenceData();
        CONSOLE("OK");

        /*********
         * Build Markets
         */
        cpu_timer mtimer;
        buildMarket();
        mtimer.stop();
        LOG("Market Build Time " << setprecision(2) << mtimer.format(default_places, "%w") << " sec");

        /************************
         * Build Pricing Engine Factory
         */
        CONSOLEW("Engine factory... ");
        engineFactory_ = buildEngineFactory(market_, "setup", true);
        CONSOLE("OK");

        /************************
         * Set global pseudo currency market parameters
         * TODO cleaner integration with analytics
         */
        GlobalPseudoCurrencyMarketParameters::instance().set(getEngineData("setup")->globalParameters());

        /******************************
         * Load and Build the Portfolio
         */
        CONSOLEW("Portfolio... ");
        portfolio_ = buildPortfolio(engineFactory_, buildFailedTrades_);
        CONSOLE("OK");

        /******************************
         * Write initial reports
         */
        writeInitialReports();

        /**********************
         * Output pricing stats
         */

        writePricingStats("pricingstats_npv.csv", portfolio_);

        /**************************
         * Write base scenario file
         */
        CONSOLEW("Write Base Scenario...");
        if (writeBaseScenario_) {
            writeBaseScenario();
            CONSOLE("OK");
        } else {
            LOG("skip base scenario");
            CONSOLE("SKIP");
        }

        /**********************
         * Sensitivity analysis
         */
        CONSOLEW("Sensitivity Report...");
        if (sensitivity_) {
            // We reset this here because the date grid building in sensitivity analysis depends on it.
            Settings::instance().evaluationDate() = asof_;
            sensitivityRunner_ = getSensitivityRunner();
            sensitivityRunner_->runSensitivityAnalysis(market_, curveConfigs_, marketParameters_);
            CONSOLE("OK");
        } else {
            CONSOLE("SKIP");
        }

        /****************
         * Stress testing
         */
        if (stress_) {
            runStressTest();
        } else {
            LOG("skip stress test");
            CONSOLEW("Stress testing...");
            CONSOLE("SKIP");
        }

        /****************
         * Parametric VaR
         */
        if (parametricVar_) {
            runParametricVar();
        } else {
            LOG("skip parametric var");
            CONSOLEW("Parametric VaR...");
            CONSOLE("SKIP");
        }

        /***************************************************
         * Use XVA runner if we want both simulation and XVA
         */
        bool useXvaRunner = false;
        if (params_->hasGroup("xva") && params_->has("xva", "useXvaRunner"))
            useXvaRunner = parseBool(params_->get("xva", "useXvaRunner"));

        if (simulate_ && xva_ && useXvaRunner) {

	    LOG("Use XvaRunner");

	    // if (cptyCube_) {
	    //     LOG("with cptyCube");
	    // 	QL_REQUIRE(cptyCube_->numIds() == portfolio_->counterparties().size() + 1,
            //               "cptyCube x dimension (" << cptyCube_->numIds() << ") does not match portfolio size ("
            //                                        << portfolio_->counterparties().size() << " minus 1)");
            // }
	    // else {
	    //    LOG("without cptyCube");
	    // }

            // // Use pre-generated scenarios
            // if (!scenarioData_)
            //     loadScenarioData();

            // QL_REQUIRE(scenarioData_->dimDates() == cube_->dates().size(),
            //            "scenario dates do not match cube grid size");
            // QL_REQUIRE(scenarioData_->dimSamples() == cube_->samples(),
            //            "scenario sample size does not match cube sample size");

	    CONSOLEW("XVA simulation...");
	    boost::shared_ptr<XvaRunner> xva = getXvaRunner();
            xva->runXva(market_, true);
            postProcess_ = xva->postProcess();
            CONSOLE("OK");

            CONSOLEW("Write XVA Reports...");
            writeXVAReports();
            if (writeDIMReport_)
                writeDIMReport();
            CONSOLE("OK");

        } else {

            /******************************************
             * Simulation: Scenario and Cube Generation
             */
            if (simulate_) {
                generateNPVCube();
            } else {
                LOG("skip simulation");
                CONSOLEW("Simulation...");
                CONSOLE("SKIP");
            }

            /*****************************
             * Aggregation and XVA Reports
             */
            CONSOLEW("Aggregation and XVA Reports...");
            if (xva_) {
                // We reset this here because the date grid building below depends on it.
                Settings::instance().evaluationDate() = asof_;

                // Use pre-generated cube
                if (!cube_)
                    loadCube();

                QL_REQUIRE(cube_->numIds() == portfolio_->size(),
                           "cube x dimension (" << cube_->numIds() << ") does not match portfolio size ("
                                                << portfolio_->size() << ")");

                // Use pre-generated scenarios
                if (!scenarioData_)
                    loadScenarioData();

                QL_REQUIRE(scenarioData_->dimDates() == cube_->dates().size(),
                           "scenario dates do not match cube grid size");
                QL_REQUIRE(scenarioData_->dimSamples() == cube_->samples(),
                           "scenario sample size does not match cube sample size");

                runPostProcessor();
                CONSOLE("OK");
                CONSOLEW("Write Reports...");
                writeXVAReports();
                if (writeDIMReport_)
                    writeDIMReport();
                CONSOLE("OK");
            } else {
                LOG("skip XVA reports");
                CONSOLE("SKIP");
            }
        }

    } catch (std::exception& e) {
        ALOG("Error: " << e.what());
        CONSOLE("Error: " << e.what());
        return;
    }

    timer.stop();
    CONSOLE("run time: " << setprecision(2) << timer.format(default_places, "%w") << " sec");
    CONSOLE("ORE done.");

    LOG("ORE done.");
}

void OREApp::buildInputParameters(boost::shared_ptr<InputParameters> inputs,
                                  const boost::shared_ptr<Parameters>& params) {
    QL_REQUIRE(inputs, "InputParameters not created yet");

    LOG("buildInputParameters starting");

    // switch default for backward compatibility
    inputs->setEntireMarket(true); 
    inputs->setAllFixings(true); 
    inputs->setEomInflationFixings(false);
    inputs->setUseMarketDataFixings(false);
    inputs->setBuildFailedTrades(false);

    QL_REQUIRE(params_->hasGroup("setup"), "parameter group 'setup' missing");

    std::string inputPath = params_->get("setup", "inputPath");
    std::string outputPath = params_->get("setup", "outputPath");

    // Load calendar adjustments
    std::string tmp =  params_->get("setup", "calendarAdjustment", false);
    if (tmp != "") {
        CalendarAdjustmentConfig calendarAdjustments;
        string calendarAdjustmentFile = inputPath + "/" + tmp;
        LOG("Loading calendar adjustments from file: " << calendarAdjustmentFile);
        calendarAdjustments.fromFile(calendarAdjustmentFile);
    } else {
        WLOG("Calendar adjustments not found, using defaults");
    }

    // Load currency configs
    tmp = params_->get("setup", "currencyConfiguration", false);
    if (tmp != "") {
        CurrencyConfig currencyConfig;
        string currencyConfigFile = inputPath + "/" + tmp;
        LOG("Loading currency configurations from file: " << currencyConfigFile);
        currencyConfig.fromFile(currencyConfigFile);
    } else {
        WLOG("Currency configurations not found, using defaults");
    }

    inputs->setAsOfDate(params_->get("setup", "asofDate"));
    
    // Set it immediately, otherwise the scenario generator grid below will be based on today's date
    Settings::instance().evaluationDate() = inputs->asof();
    
    inputs->setResultsPath(outputPath);

    inputs->setBaseCurrency(params_->get("npv", "baseCurrency"));

    tmp = params_->get("setup", "useMarketDataFixings", false);
    if (tmp != "")
        inputs->setUseMarketDataFixings(parseBool(tmp));

    tmp = params_->get("setup", "dryRun", false);
    if (tmp != "")
        inputs->setDryRun(parseBool(tmp));

    tmp = params_->get("setup", "reportNaString", false);
    if (tmp != "")
        inputs->setReportNaString(tmp);

    tmp = params_->get("setup", "eomInflationFixings", false);
    if (tmp != "")
        inputs->setEomInflationFixings(parseBool(tmp));

    tmp = params_->get("setup", "nThreads", false);
    if (tmp != "")
        inputs->setThreads(parseInteger(tmp));

    tmp = params_->get("setup", "entireMarket", false);
    if (tmp != "")
        inputs->setEntireMarket(parseBool(tmp));

    tmp = params_->get("setup", "iborFallbackOverride", false);
    if (tmp != "")
        inputs->setIborFallbackOverride(parseBool(tmp));

    tmp = params_->get("setup", "continueOnError", false);
    if (tmp != "")
        inputs->setContinueOnError(parseBool(tmp));

    tmp = params_->get("setup", "lazyMarketBuilding", false);
    if (tmp != "")
        inputs->setLazyMarketBuilding(parseBool(tmp));

    tmp = params_->get("setup", "buildFailedTrades", false);
    if (tmp != "")
        inputs->setBuildFailedTrades(parseBool(tmp));

    tmp = params_->get("setup", "observationModel", false);
    if (tmp != "") {
        inputs->setObservationModel(tmp);
        ObservationMode::instance().setMode(inputs->observationModel());
        LOG("Observation Mode is " << inputs->observationModel());
    }

    tmp = params_->get("setup", "implyTodaysFixings", false);
    if (tmp != "")
        inputs->setImplyTodaysFixings(ore::data::parseBool(tmp));

    tmp = params_->get("setup", "referenceDataFile", false);
    if (tmp != "") {
        string refDataFile = inputPath + "/" + tmp;
        LOG("Loading reference data from file: " << refDataFile);
        inputs->setRefDataManagerFromFile(refDataFile);
    } else {
        WLOG("Reference data not found");
    }

    if (params_->has("setup", "conventionsFile") && params_->get("setup", "conventionsFile") != "") {
        string conventionsFile = inputPath + "/" + params_->get("setup", "conventionsFile");
        LOG("Loading conventions from file: " << conventionsFile);
        inputs->setConventionsFromFile(conventionsFile);
    } else {
        ALOG("Conventions not found");
    }

    if (params_->has("setup", "iborFallbackConfig") && params_->get("setup", "iborFallbackConfig") != "") {
        std::string tmp = inputPath + "/" + params_->get("setup", "iborFallbackConfig");
        LOG("Loading Ibor fallback config from file: " << tmp);
        inputs->setIborFallbackConfigFromFile(tmp);
    } else {
        WLOG("Using default Ibor fallback config");
    }

    if (params_->has("setup", "curveConfigFile") && params_->get("setup", "curveConfigFile") != "") {
        string curveConfigFile = inputPath + "/" + params_->get("setup", "curveConfigFile");
        LOG("Load curve configurations from file: ");
        inputs->setCurveConfigsFromFile(curveConfigFile);
    } else {
        ALOG("no curve configs loaded");
    }
    
    tmp = params_->get("setup", "pricingEnginesFile", false);
    if (tmp != "") {
        string pricingEnginesFile = inputPath + "/" + tmp;
        LOG("Load pricing engine data from file: " << pricingEnginesFile);
        inputs->setPricingEngineFromFile(pricingEnginesFile);
    } else {
        ALOG("Pricing engine data not found");
    }
    
    tmp = params_->get("setup", "marketConfigFile", false);
    if (tmp != "") {
        string marketConfigFile = inputPath + "/" + tmp;
        LOG("Loading today's market parameters from file" << marketConfigFile);
        inputs->setTodaysMarketParamsFromFile(marketConfigFile);
    } else {
        ALOG("Today's market parameters not found");
    }

    if (params_->has("setup", "buildFailedTrades"))
        inputs->setBuildFailedTrades(parseBool(params_->get("setup", "buildFailedTrades")));

    tmp = params_->get("setup", "portfolioFile");
    if (tmp != "") {
        inputs->setPortfolioFromFile(tmp, inputPath);
    } else {
        ALOG("Portfolio data not found");
    }

    if (params_->hasGroup("markets")) {
        inputs->setMarketConfigs(params_->markets());
        for (auto m: inputs->marketConfigs())
            LOG("MarketContext::" << m.first << " = " << m.second);
    }

    /*************
     * NPV
     *************/

    tmp = params_->get("npv", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        inputs->insertAnalytic("NPV");

    tmp = params_->get("npv", "additionalResults", false);
    if (tmp != "")
        inputs->setOutputAdditionalResults(parseBool(tmp));

    /*************
     * CASHFLOW
     *************/

    tmp = params_->get("cashflow", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        inputs->insertAnalytic("CASHFLOW");

    tmp = params_->get("cashflow", "includePastCashflows", false);
    if (tmp != "")
        inputs->setIncludePastCashflows(parseBool(tmp));
    
    /*************
     * Curves
     *************/

    tmp = params_->get("curves", "active", false);
    if (tmp != "") 
        inputs->setOutputCurves(parseBool(tmp));
    
    tmp = params_->get("curves", "grid", false);
    if (tmp != "") 
        inputs->setCurvesGrid(tmp);

    tmp = params_->get("curves", "configuration", false);
    if (tmp != "") 
        inputs->setCurvesMarketConfig(tmp);

    tmp = params_->get("curves", "outputTodaysMarketCalibration", false);
    if (tmp != "")
        inputs->setOutputTodaysMarketCalibration(parseBool(tmp));

    /*************
     * SENSITIVITY
     *************/

    // FIXME: The following are not loaded from params so far, relying on defaults
    // xbsParConversion_ = false;
    // analyticFxSensis_ = true;
    // useSensiSpreadedTermStructures_ = true;

    tmp = params_->get("sensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        inputs->insertAnalytic("SENSITIVITY");

        tmp = params_->get("sensitivity", "parSensitivity", false);
        if (tmp != "")
            inputs->setParSensi(parseBool(tmp));

        tmp = params_->get("sensitivity", "outputJacobi", false);
        if (tmp != "")
            inputs->setOutputJacobi(parseBool(tmp));

        tmp = params_->get("sensitivity", "alignPillars", false);
        if (tmp != "")
            inputs->setAlignPillars(parseBool(tmp));

        tmp = params_->get("sensitivity", "marketConfigFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Loading sensitivity scenario sim market parameters from file" << file);
            inputs->setSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }

        tmp = params_->get("sensitivity", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Load sensitivity scenario data from file" << file);
            inputs->setSensiScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }

        tmp = params_->get("sensitivity", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Load pricing engine data from file: " << file);
            inputs->setSensiPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for sensitivity analysis, using global");
            // FIXME
            inputs->setSensiPricingEngine(inputs->pricingEngine());
        }

        tmp = params_->get("sensitivity", "outputSensitivityThreshold", false);
        if (tmp != "")
            inputs->setSensiThreshold(parseReal(tmp));
    }

    /****************
     * STRESS
     ****************/

    tmp = params_->get("stress", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        inputs->insertAnalytic("STRESS");
        inputs->setStressPricingEngine(inputs->pricingEngine());
        tmp = params_->get("stress", "marketConfigFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Loading stress test scenario sim market parameters from file" << file);
            inputs->setStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for stress testing not loaded");
        }
        
        tmp = params_->get("stress", "stressConfigFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Load stress test scenario data from file" << file);
            inputs->setStressScenarioDataFromFile(file);
        } else {
            WLOG("Stress scenario data not loaded");
        }
        
        tmp = params_->get("stress", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = inputPath + "/" + tmp;
            LOG("Load pricing engine data from file: " << file);
            inputs->setStressPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }
        
        tmp = params_->get("stress", "outputThreshold", false); 
        if (tmp != "")
            inputs->setStressThreshold(parseReal(tmp));
    }

    /****************
     * VaR
     ****************/

    tmp = params_->get("parametricVar", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        inputs->insertAnalytic("VAR");
        
        tmp = params_->get("parametricVar", "salvageCovarianceMatrix", false);
        if (tmp != "")
            inputs->setSalvageCovariance(parseBool(tmp));
        
        tmp = params_->get("parametricVar", "quantiles", false);
        if (tmp != "")
            inputs->setVarQuantiles(tmp);
        
        tmp = params_->get("parametricVar", "breakdown", false);
        if (tmp != "")
            inputs->setVarBreakDown(parseBool(tmp));
        
        tmp = params_->get("parametricVar", "portfolioFilter", false);
        if (tmp != "")
            inputs->setPortfolioFilter(tmp);
        
        tmp = params_->get("parametricVar", "method", false);
        if (tmp != "")
            inputs->setVarMethod(tmp);
        
        tmp = params_->get("parametricVar", "mcSamples", false);
        if (tmp != "")
            inputs->setMcVarSamples(parseInteger(tmp));
        
        tmp = params_->get("parametricVar", "mcSeed", false);
        if (tmp != "")
            inputs->setMcVarSeed(parseInteger(tmp));
        
        tmp = params_->get("parametricVar", "covarianceInputFile", false);
        QL_REQUIRE(tmp != "", "covarianceInputFile not provided");
        std::string covFile = inputPath + "/" + tmp;
        LOG("Load Covariance Data from file " << covFile);
        inputs->setCovarianceDataFromFile(covFile);
        
        tmp = params_->get("parametricVar", "sensitivityInputFile", false);
        QL_REQUIRE(tmp != "", "sensitivityInputFile not provided");
        std::string sensiFile = inputPath + "/" + tmp;
        LOG("Get sensitivity data from file " << sensiFile);
        inputs->setSensitivityStreamFromFile(sensiFile);
    }
    
    /************
     * Simulation
     ************/

    tmp = params_->get("simulation", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        inputs->insertAnalytic("EXPOSURE");
    }

    // check this here because we need to know 10 lines below
    tmp = params_->get("xva", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        inputs->insertAnalytic("XVA");
    
    tmp = params_->get("simulation", "amc", false);
    if (tmp != "")
        inputs->setAmc(parseBool(tmp));

    tmp = params_->get("simulation", "amcTradeTypes", false);
    if (tmp != "")
        inputs->setAmcTradeTypes(tmp);

    inputs->setSimulationPricingEngine(inputs->pricingEngine());
    inputs->setExposureObservationModel(inputs->observationModel());
    inputs->setExposureBaseCurrency(inputs->baseCurrency());

    if (inputs->analytics().find("EXPOSURE") != inputs->analytics().end() ||
        inputs->analytics().find("XVA") != inputs->analytics().end()) {
        tmp = params_->get("simulation", "simulationConfigFile", false) ;
        if (tmp != "") {
            string simulationConfigFile = inputPath + "/" + tmp;
            LOG("Loading simulation config from file" << simulationConfigFile);
            inputs->setExposureSimMarketParamsFromFile(simulationConfigFile);
            inputs->setCrossAssetModelDataFromFile(simulationConfigFile);
            inputs->setScenarioGeneratorDataFromFile(simulationConfigFile);
            auto grid = inputs->scenarioGeneratorData()->getGrid();
            DLOG("grid size=" << grid->size() << ", dates=" << grid->dates().size()
                << ", valuationDates=" << grid->valuationDates().size()
                << ", closeOutDates=" << grid->closeOutDates().size());
        } else {
            ALOG("Simulation market, model and scenario generator data not loaded");
        }
        
        tmp = params_->get("simulation", "pricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = inputPath + "/" + tmp;
            LOG("Load simulation pricing engine data from file: " << pricingEnginesFile);
            inputs->setSimulationPricingEngineFromFile(pricingEnginesFile);
        } else {
            WLOG("Simulation pricing engine data not found, using standard pricing engines");
        }

        tmp = params_->get("simulation", "amcPricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = inputPath + "/" + tmp;
            LOG("Load amc pricing engine data from file: " << pricingEnginesFile);
            inputs->setAmcPricingEngineFromFile(pricingEnginesFile);
        } else {
            WLOG("AMC pricing engine data not found, using standard pricing engines");
            inputs->setAmcPricingEngine(inputs->pricingEngine());
        }

        inputs->setExposureBaseCurrency(inputs->baseCurrency());
        tmp = params_->get("simulation", "baseCurrency", false);
        if (tmp != "")
            inputs->setExposureBaseCurrency(tmp);
        
        tmp = params_->get("simulation", "observationModel", false);
        if (tmp != "")
            inputs->setExposureObservationModel(tmp);
        else
            inputs->setExposureObservationModel(inputs->observationModel());
        
        tmp = params_->get("simulation", "storeFlows", false);
        if (tmp == "Y")
            inputs->setStoreFlows(true);
        
        tmp = params_->get("simulation", "storeSurvivalProbabilities", false);
        if (tmp == "Y")
            inputs->setStoreSurvivalProbabilities(true);
        
        tmp = params_->get("simulation", "nettingSetId", false);
        if (tmp != "")
            inputs->setNettingSetId(tmp);
        
        tmp = params_->get("simulation", "cubeFile", false);
        if (tmp != "")
            inputs->setWriteCube(true);
        
        tmp = params_->get("simulation", "scenariodump", false);
        if (tmp != "")
            inputs->setWriteScenarios(true);
    }

    /**********************
     * XVA specifically
     **********************/

    tmp = params_->get("xva", "baseCurrency", false);
    if (tmp != "")
        inputs->setXvaBaseCurrency(tmp);
    else
        inputs->setXvaBaseCurrency(inputs->exposureBaseCurrency());

    if (inputs->analytics().find("XVA") != inputs->analytics().end() &&
        inputs->analytics().find("EXPOSURE") == inputs->analytics().end()) {
        inputs->setLoadCube(true);
        tmp = params_->get("xva", "cubeFile", false);
        if (tmp != "") {
            string cubeFile = inputs->resultsPath().string() + "/" + tmp;
            LOG("Load cube from file " << cubeFile);
            inputs->setCubeFromFile(cubeFile);
            LOG("Cube loading done: ids=" << inputs->cube()->numIds()
                << " dates=" << inputs->cube()->numDates()
                << " samples=" << inputs->cube()->samples()
                << " depth=" << inputs->cube()->depth());
        } else {
            ALOG("cube file name not provided");
        }
    }

    if (inputs->analytics().find("XVA") != inputs->analytics().end() ||
        inputs->analytics().find("EXPOSURE") != inputs->analytics().end()) {
        tmp = params_->get("xva", "csaFile", false);
        QL_REQUIRE(tmp != "", "Netting set manager is required for XVA");
        string csaFile = inputPath + "/" + tmp;
        LOG("Loading netting and csa data from file" << csaFile);
        inputs->setNettingSetManagerFromFile(csaFile);
    }
    
    tmp = params_->get("xva", "nettingSetCubeFile", false);
    if (inputs->loadCube() && tmp != "") {
        string cubeFile = inputs->resultsPath().string() + "/" + tmp;
        LOG("Load nettingset cube from file " << cubeFile);
        inputs->setNettingSetCubeFromFile(cubeFile);
        DLOG("NettingSetCube loading done: ids=" << nettingSetCube_->numIds() << " dates=" << nettingSetCube_->numDates()
            << " samples=" << nettingSetCube_->samples() << " depth=" << nettingSetCube_->depth());
    }

    tmp = params_->get("xva", "cptyCubeFile", false);
    if (inputs->loadCube() && tmp != "") {
        string cubeFile = inputs->resultsPath().string() + "/" + tmp;
        LOG("Load cpty cube from file " << cubeFile);
        inputs->setCptyCubeFromFile(cubeFile);
        DLOG("CptyCube loading done: ids=" << cptyCube_->numIds() << " dates=" << cptyCube_->numDates()
            << " samples=" << cptyCube_->samples() << " depth=" << cptyCube_->depth());
    }

    tmp = params_->get("xva", "scenarioFile", false);
    if (inputs->loadCube() && tmp != "") {
        string cubeFile = inputs->resultsPath().string() + "/" + tmp;
        LOG("Load agg scen data from file " << cubeFile);
        inputs->setMarketCubeFromFile(cubeFile);
        LOG("MktCube loading done");
    }
    
    tmp = params_->get("xva", "flipViewXVA", false);
    if (tmp != "")
        inputs->setFlipViewXVA(parseBool(tmp));

    tmp = params_->get("xva", "fullInitialCollateralisation", false);
    if (tmp != "")
        inputs->setFullInitialCollateralisation(parseBool(tmp));

    tmp = params_->get("xva", "exposureProfilesByTrade", false);
    if (tmp != "")
        inputs->setExposureProfilesByTrade(parseBool(tmp));

    tmp = params_->get("xva", "exposureProfiles", false);
    if (tmp != "")
        inputs->setExposureProfiles(parseBool(tmp));

    tmp = params_->get("xva", "quantile", false);
    if (tmp != "")
        inputs->setPfeQuantile(parseReal(tmp));

    tmp = params_->get("xva", "calculationType", false);
    if (tmp != "")
        inputs->setCollateralCalculationType(tmp);

    tmp = params_->get("xva", "allocationMethod", false);
    if (tmp != "")
        inputs->setExposureAllocationMethod(tmp);

    tmp = params_->get("xva", "marginalAllocationLimit", false);
    if (tmp != "")
        inputs->setMarginalAllocationLimit(parseReal(tmp));

    tmp = params_->get("xva", "exerciseNextBreak", false);
    if (tmp != "")
        inputs->setExerciseNextBreak(parseBool(tmp));

    tmp = params_->get("xva", "cva", false);
    if (tmp != "")
        inputs->setCvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dva", false);
    if (tmp != "")
        inputs->setDvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "fva", false);
    if (tmp != "")
        inputs->setFvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "colva", false);
    if (tmp != "")
        inputs->setColvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "collateralFloor", false);
    if (tmp != "")
        inputs->setCollateralFloorAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dim", false);
    if (tmp != "")
        inputs->setDimAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "mva", false);
    if (tmp != "")
        inputs->setMvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "kva", false);
    if (tmp != "")
        inputs->setKvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dynamicCredit", false);
    if (tmp != "")
        inputs->setDynamicCredit(parseBool(tmp));

    tmp = params_->get("xva", "cvaSensi", false);
    if (tmp != "")
        inputs->setCvaSensi(parseBool(tmp));

    tmp = params_->get("xva", "cvaSensiGrid", false);
    if (tmp != "")
        inputs->setCvaSensiGrid(tmp);

    tmp = params_->get("xva", "cvaSensiShiftSize", false);
    if (tmp != "")
        inputs->setCvaSensiShiftSize(parseReal(tmp));

    tmp = params_->get("xva", "dvaName", false);
    if (tmp != "")
        inputs->setDvaName(tmp);

    tmp = params_->get("xva", "rawCubeOutputFile", false);
    if (tmp != "") {
        inputs->setRawCubeOutputFile(tmp);
        inputs->setRawCubeOutput(true);
    }

    tmp = params_->get("xva", "netCubeOutputFile", false);
    if (tmp != "") {
        inputs->setNetCubeOutputFile(tmp);
        inputs->setNetCubeOutput(true);
    }

    // FVA

    tmp = params_->get("xva", "fvaBorrowingCurve", false);
    if (tmp != "")
        inputs->setFvaBorrowingCurve(tmp);

    tmp = params_->get("xva", "fvaLendingCurve", false);
    if (tmp != "")
        inputs->setFvaLendingCurve(tmp);

    tmp = params_->get("xva", "flipViewBorrowingCurvePostfix", false);
    if (tmp != "")
        inputs->setFlipViewBorrowingCurvePostfix(tmp);

    tmp = params_->get("xva", "flipViewLendingCurvePostfix", false);
    if (tmp != "")
        inputs->setFlipViewLendingCurvePostfix(tmp);

    // DIM

    tmp = params_->get("xva", "dimQuantile", false);
    if (tmp != "")
        inputs->setDimQuantile(parseReal(tmp));

    tmp = params_->get("xva", "dimHorizonCalendarDays", false);
    if (tmp != "")
        inputs->setDimHorizonCalendarDays(parseInteger(tmp));

    tmp = params_->get("xva", "dimRegressionOrder", false);
    if (tmp != "")
        inputs->setDimRegressionOrder(parseInteger(tmp));

    tmp = params_->get("xva", "dimRegressors", false);
    if (tmp != "")
        inputs->setDimRegressors(tmp);

    tmp = params_->get("xva", "dimOutputGridPoints", false);
    if (tmp != "")
        inputs->setDimOutputGridPoints(tmp);

    tmp = params_->get("xva", "dimOutputNettingSet", false);
    if (tmp != "")
        inputs->setDimOutputNettingSet(tmp);
    
    tmp = params_->get("xva", "dimLocalRegressionEvaluations", false);
    if (tmp != "")
        inputs->setDimLocalRegressionEvaluations(parseInteger(tmp));

    tmp = params_->get("xva", "dimLocalRegressionBandwidth", false);
    if (tmp != "")
        inputs->setDimLocalRegressionBandwidth(parseReal(tmp));

    // KVA

    tmp = params_->get("xva", "kvaCapitalDiscountRate", false);
    if (tmp != "")
        inputs->setKvaCapitalDiscountRate(parseReal(tmp));

    tmp = params_->get("xva", "kvaAlpha", false);
    if (tmp != "")
        inputs->setKvaAlpha(parseReal(tmp));

    tmp = params_->get("xva", "kvaRegAdjustment", false);
    if (tmp != "")
        inputs->setKvaRegAdjustment(parseReal(tmp));

    tmp = params_->get("xva", "kvaCapitalHurdle", false);
    if (tmp != "")
        inputs->setKvaCapitalHurdle(parseReal(tmp));

    tmp = params_->get("xva", "kvaOurPdFloor", false);
    if (tmp != "")
        inputs->setKvaOurPdFloor(parseReal(tmp));

    tmp = params_->get("xva", "kvaTheirPdFloor", false);
    if (tmp != "")
        inputs->setKvaTheirPdFloor(parseReal(tmp));

    tmp = params_->get("xva", "kvaOurCvaRiskWeight", false);
    if (tmp != "")
        inputs->setKvaOurCvaRiskWeight(parseReal(tmp));

    tmp = params_->get("xva", "kvaTheirCvaRiskWeight", false);
    if (tmp != "")
        inputs->setKvaTheirCvaRiskWeight(parseReal(tmp));

    // cashflow npv and dynamic backtesting

    tmp = params_->get("cashflow", "cashFlowHorizon", false);
    if (tmp != "")
        inputs->setCashflowHorizon(tmp);

    tmp = params_->get("cashflow", "portfolioFilterDate", false);
    if (tmp != "")
        inputs->setPortfolioFilterDate(tmp);

}
    
void OREApp::writePricingStats(const std::string& filename, const boost::shared_ptr<Portfolio>& portfolio) {
    LOG("write pricing stats report");
    CSVFileReport pricingStatsReport(outputPath_ + "/" + filename);
    ore::analytics::ReportWriter().writePricingStats(pricingStatsReport, portfolio);
    LOG("pricing stats report written");
}

boost::shared_ptr<XvaRunner> OREApp::getXvaRunner() {
    LOG(" OREApp::getXvaRunner() called");

    string baseCcy = params_->get("simulation", "baseCurrency");
    boost::shared_ptr<EngineData> engineData = getEngineData("simulation");
    boost::shared_ptr<NettingSetManager> nettingSetManager = initNettingSetManager();
    boost::shared_ptr<TodaysMarketParameters> marketParameters = getMarketParameters();
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketParameters = getSimMarketData();
    boost::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData = getScenarioGeneratorData();
    boost::shared_ptr<CrossAssetModelData> modelData = getCrossAssetModelData();

    map<string, bool> analytics;
    analytics["exerciseNextBreak"] = parseBool(params_->get("xva", "exerciseNextBreak"));
    analytics["cva"] = parseBool(params_->get("xva", "cva"));
    analytics["dva"] = parseBool(params_->get("xva", "dva"));
    analytics["fva"] = parseBool(params_->get("xva", "fva"));
    analytics["colva"] = parseBool(params_->get("xva", "colva"));
    analytics["collateralFloor"] = parseBool(params_->get("xva", "collateralFloor"));
    if (params_->has("xva", "kva"))
        analytics["kva"] = parseBool(params_->get("xva", "kva"));
    else
        analytics["kva"] = false;
    if (params_->has("xva", "mva"))
        analytics["mva"] = parseBool(params_->get("xva", "mva"));
    else
        analytics["mva"] = false;
    if (params_->has("xva", "dim"))
        analytics["dim"] = parseBool(params_->get("xva", "dim"));
    else
        analytics["dim"] = false;
    if (params_->has("xva", "cvaSensi"))
        analytics["cvaSensi"] = parseBool(params_->get("xva", "cvaSensi"));
    else
        analytics["cvaSensi"] = false;

    const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr;
    QuantLib::Real dimQuantile = 0.99;
    QuantLib::Size dimHorizonCalendarDays = 14;
    string dvaName = params_->get("xva", "dvaName");
    string fvaLendingCurve = params_->get("xva", "fvaLendingCurve");
    string fvaBorrowingCurve = params_->get("xva", "fvaBorrowingCurve");
    string calculationType = params_->get("xva", "calculationType");
    bool fullInitialCollateralisation = false;
    if (params_->has("xva", "fullInitialCollateralisation")) {
        fullInitialCollateralisation = parseBool(params_->get("xva", "fullInitialCollateralisation"));
    }

    bool storeFlows = params_->has("simulation", "storeFlows") && parseBool(params_->get("simulation", "storeFlows"));

    analytics["flipViewXVA"] = false;
    if (params_->has("xva", "flipViewXVA")) {
        analytics["flipViewXVA"] = parseBool(params_->get("xva", "flipViewXVA"));
    }
    string flipViewBorrowingCurvePostfix = "_BORROW";
    string flipViewLendingCurvePostfix = "_LEND";
    if (analytics["flipViewXVA"]) {
        flipViewBorrowingCurvePostfix = params_->get("xva", "flipViewBorrowingCurvePostfix");
        flipViewLendingCurvePostfix = params_->get("xva", "flipViewLendingCurvePostfix");
    }

    boost::shared_ptr<XvaRunner> xva = boost::make_shared<XvaRunner>(
        asof_, baseCcy, portfolio_, nettingSetManager, engineData, curveConfigs_, marketParameters, simMarketParameters,
        scenarioGeneratorData, modelData, referenceData, iborFallbackConfig_, dimQuantile, dimHorizonCalendarDays,
        analytics, calculationType, dvaName, fvaBorrowingCurve, fvaLendingCurve, fullInitialCollateralisation,
        storeFlows);

    return xva;
}

void OREApp::readSetup() {

    params_->log();

    inputPath_ = params_->get("setup", "inputPath");
    outputPath_ = params_->get("setup", "outputPath");

    if (params_->has("setup", "observationModel")) {
        string om = params_->get("setup", "observationModel");
        ObservationMode::instance().setMode(om);
        LOG("Observation Mode is " << om);
    }
    if (params_->has("setup", "calendarAdjustment") && params_->get("setup", "calendarAdjustment") != "") {
        CalendarAdjustmentConfig calendarAdjustments;
        string calendarAdjustmentFile = inputPath_ + "/" + params_->get("setup", "calendarAdjustment");
        LOG("Load calendarAdjustment from file" << calendarAdjustmentFile);
        calendarAdjustments.fromFile(calendarAdjustmentFile);
    }
    if (params_->has("setup", "currencyConfiguration") && params_->get("setup", "currencyConfiguration") != "") {
        CurrencyConfig currencyConfig;
        string currencyConfigFile = inputPath_ + "/" + params_->get("setup", "currencyConfiguration");
        LOG("Load currency configurations from file " << currencyConfigFile);
        currencyConfig.fromFile(currencyConfigFile);
    }

    iborFallbackConfig_ = IborFallbackConfig::defaultConfig();
    if (params_->has("setup", "iborFallbackConfig") && params_->get("setup", "iborFallbackConfig") != "") {
        std::string tmp = inputPath_ + "/" + params_->get("setup", "iborFallbackConfig");
        LOG("Load iborFallbackConfig from file" << tmp);
        iborFallbackConfig_.fromFile(tmp);
    } else {
        LOG("Using default iborFallbackConfig");
    }

    writeInitialReports_ = true;
    simulate_ = (params_->hasGroup("simulation") && params_->get("simulation", "active") == "Y") ? true : false;
    buildSimMarket_ = true;
    xva_ = (params_->hasGroup("xva") && params_->get("xva", "active") == "Y") ? true : false;
    writeDIMReport_ = (params_->hasGroup("xva") && params_->has("xva", "dim") && parseBool(params_->get("xva", "dim")))
                          ? true
                          : false;
    sensitivity_ = (params_->hasGroup("sensitivity") && params_->get("sensitivity", "active") == "Y") ? true : false;
    stress_ = (params_->hasGroup("stress") && params_->get("stress", "active") == "Y") ? true : false;
    parametricVar_ =
        (params_->hasGroup("parametricVar") && params_->get("parametricVar", "active") == "Y") ? true : false;
    writeBaseScenario_ =
        (params_->hasGroup("baseScenario") && params_->get("baseScenario", "active") == "Y") ? true : false;

    continueOnError_ = false;
    if (params_->has("setup", "continueOnError"))
        continueOnError_ = parseBool(params_->get("setup", "continueOnError"));

    lazyMarketBuilding_ = true;
    if (params_->has("setup", "lazyMarketBuilding"))
        lazyMarketBuilding_ = parseBool(params_->get("setup", "lazyMarketBuilding"));

    buildFailedTrades_ = false;
    if (params_->has("setup", "buildFailedTrades"))
        buildFailedTrades_ = parseBool(params_->get("setup", "buildFailedTrades"));

}

void OREApp::setupLog() {
    closeLog();

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

void OREApp::closeLog() { Log::instance().removeAllLoggers(); }

void OREApp::getReferenceData() {
    if (params_->has("setup", "referenceDataFile") && params_->get("setup", "referenceDataFile") != "") {
        string referenceDataFile = inputPath_ + "/" + params_->get("setup", "referenceDataFile");
        referenceData_ = boost::make_shared<BasicReferenceDataManager>(referenceDataFile);
    } else {
        LOG("No referenceDataFile file loaded");
    }
}

void OREApp::getConventions() {
    if (params_->has("setup", "conventionsFile") && params_->get("setup", "conventionsFile") != "") {
        string conventionsFile = inputPath_ + "/" + params_->get("setup", "conventionsFile");
        conventions_->fromFile(conventionsFile);
    } else {
        WLOG("No conventions file loaded");
    }
}

boost::shared_ptr<TodaysMarketParameters> OREApp::getMarketParameters() {
    if (params_->has("setup", "marketConfigFile") && params_->get("setup", "marketConfigFile") != "") {
        string marketConfigFile = inputPath_ + "/" + params_->get("setup", "marketConfigFile");
        marketParameters_->fromFile(marketConfigFile);
    } else {
        WLOG("No market parameters loaded");
    }
    return marketParameters_;
}

boost::shared_ptr<EngineData> OREApp::getEngineData(string groupName) {
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    string pricingEnginesFile = inputPath_ + "/" + params_->get(groupName, "pricingEnginesFile");
    engineData->fromFile(pricingEnginesFile);
    return engineData;
}

boost::shared_ptr<CrossAssetModelData> OREApp::getCrossAssetModelData() {
    string simulationConfigFile = inputPath_ + "/" + params_->get("simulation", "simulationConfigFile");
    boost::shared_ptr<CrossAssetModelData> modelData = boost::make_shared<CrossAssetModelData>();
    modelData->fromFile(simulationConfigFile);
    return modelData;
}

boost::shared_ptr<EngineFactory> OREApp::buildEngineFactory(const boost::shared_ptr<Market>& market,
                                                            const string& groupName,
                                                            const bool generateAdditionalResults) const {
    MEM_LOG;
    LOG("Building an engine factory")

    map<MarketContext, string> configurations;
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    string pricingEnginesFile = inputPath_ + "/" + params_->get(groupName, "pricingEnginesFile");
    if (params_->get(groupName, "pricingEnginesFile") != "")
        engineData->fromFile(pricingEnginesFile);
    engineData->globalParameters()["GenerateAdditionalResults"] = generateAdditionalResults ? "true" : "false";
    engineData->globalParameters()["RunType"] = groupName == "simulation" ? "Exposure" : "NPV";
    configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
    configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
    configurations[MarketContext::pricing] = params_->get("markets", "pricing");
    LOG("MarketContext::pricing = " << params_->get("markets", "pricing"));
    boost::shared_ptr<EngineFactory> factory =
        boost::make_shared<EngineFactory>(engineData, market, configurations, referenceData_, iborFallbackConfig_);

    LOG("Engine factory built");
    MEM_LOG;

    return factory;
}

boost::shared_ptr<Portfolio> OREApp::buildPortfolio(const boost::shared_ptr<EngineFactory>& factory, bool buildFailedTrades) {
    MEM_LOG;
    LOG("Building portfolio");
    boost::shared_ptr<Portfolio> portfolio = loadPortfolio(buildFailedTrades);
    portfolio->build(factory, "oreapp");
    LOG("Portfolio built");
    MEM_LOG;
    return portfolio;
}

boost::shared_ptr<Portfolio> OREApp::loadPortfolio(bool buildFailedTrades) {
    string portfoliosString = params_->get("setup", "portfolioFile");
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>(buildFailedTrades);
    if (params_->get("setup", "portfolioFile") == "")
        return portfolio;
    vector<string> portfolioFiles = getFileNames(portfoliosString, inputPath_);
    for (auto portfolioFile : portfolioFiles) {
        portfolio->fromFile(portfolioFile);
    }
    return portfolio;
}

boost::shared_ptr<ScenarioSimMarketParameters> OREApp::getSimMarketData() {
    string simulationConfigFile = inputPath_ + "/" + params_->get("simulation", "simulationConfigFile");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    simMarketData->fromFile(simulationConfigFile);
    return simMarketData;
}

boost::shared_ptr<ScenarioGeneratorData> OREApp::getScenarioGeneratorData() {
    string simulationConfigFile = inputPath_ + "/" + params_->get("simulation", "simulationConfigFile");
    boost::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->fromFile(simulationConfigFile);
    auto grid = sgd->getGrid();
    DLOG("grid size=" << grid->size() << ", dates=" << grid->dates().size()
                      << ", valuationDates=" << grid->valuationDates().size()
                      << ", closeOutDates=" << grid->closeOutDates().size());
    useCloseOutLag_ = sgd->withCloseOutLag();
    useMporStickyDate_ = sgd->withMporStickyDate();
    return sgd;
}

boost::shared_ptr<QuantExt::CrossAssetModel> OREApp::buildCam(boost::shared_ptr<Market> market,
                                                              const bool continueOnCalibrationError) {
    LOG("Build Simulation Model (continueOnCalibrationError = " << std::boolalpha << continueOnCalibrationError << ")");
    string simulationConfigFile = inputPath_ + "/" + params_->get("simulation", "simulationConfigFile");
    LOG("Load simulation model data from file: " << simulationConfigFile);
    boost::shared_ptr<CrossAssetModelData> modelData = boost::make_shared<CrossAssetModelData>();
    modelData->fromFile(simulationConfigFile);
    string lgmCalibrationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "lgmcalibration"))
        lgmCalibrationMarketStr = params_->get("markets", "lgmcalibration");
    string fxCalibrationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "fxcalibration"))
        fxCalibrationMarketStr = params_->get("markets", "fxcalibration");
    string eqCalibrationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "eqcalibration"))
        eqCalibrationMarketStr = params_->get("markets", "eqcalibration");
    string infCalibrationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "infcalibration"))
        infCalibrationMarketStr = params_->get("markets", "infcalibration");
    string crCalibrationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "crcalibration"))
        crCalibrationMarketStr = params_->get("markets", "crcalibration");
    string simulationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "simulation"))
        simulationMarketStr = params_->get("markets", "simulation");

    CrossAssetModelBuilder modelBuilder(market, modelData, lgmCalibrationMarketStr, fxCalibrationMarketStr,
                                        eqCalibrationMarketStr, infCalibrationMarketStr, crCalibrationMarketStr,
                                        simulationMarketStr, false, continueOnCalibrationError);
    boost::shared_ptr<QuantExt::CrossAssetModel> model = *modelBuilder.model();
    return model;
}

boost::shared_ptr<ScenarioGenerator>
OREApp::buildScenarioGenerator(boost::shared_ptr<Market> market,
                               boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                               boost::shared_ptr<ScenarioGeneratorData> sgd, const bool continueOnCalibrationError) {
    boost::shared_ptr<QuantExt::CrossAssetModel> model = buildCam(market, continueOnCalibrationError);
    LOG("Load Simulation Parameters");
    ScenarioGeneratorBuilder sgb(sgd);
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    boost::shared_ptr<ScenarioGenerator> sg = sgb.build(
        model, sf, simMarketData, asof_, market, params_->get("markets", "simulation")); // pricing or simulation?
    // Optionally write out scenarios
    if (params_->has("simulation", "scenariodump")) {
        string filename = outputPath_ + "/" + params_->get("simulation", "scenariodump");
        sg = boost::make_shared<ScenarioWriter>(sg, filename);
    }
    return sg;
}

void OREApp::writeInitialReports() {

    MEM_LOG;
    LOG("Writing initial reports");

    /************
     * Curve dump
     */
    CONSOLEW("Curve Report...");
    if (params_->hasGroup("curves") && params_->get("curves", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("curves", "outputFileName");
        CSVFileReport curvesReport(fileName);
        DateGrid grid(params_->get("curves", "grid"));
        getReportWriter()->writeCurves(curvesReport, params_->get("curves", "configuration"), grid, *marketParameters_,
                                       market_, continueOnError_);
        CONSOLE("OK");
    } else {
        LOG("skip curve report");
        CONSOLE("SKIP");
    }

    /*********************
     * Portfolio valuation
     */
    CONSOLEW("NPV Report...");
    if (params_->hasGroup("npv") && params_->get("npv", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("npv", "outputFileName");
        CSVFileReport npvReport(fileName);
        getReportWriter()->writeNpv(npvReport, params_->get("npv", "baseCurrency"), market_,
                                    params_->get("markets", "pricing"), portfolio_);
        CONSOLE("OK");
    } else {
        LOG("skip portfolio valuation");
        CONSOLE("SKIP");
    }

    /*********************
     * Additional Results
     */
    CONSOLEW("Additional Results...");
    if (params_->hasGroup("additionalResults") && params_->get("additionalResults", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("additionalResults", "outputFileName");
        CSVFileReport addResultReport(fileName);
        getReportWriter()->writeAdditionalResultsReport(addResultReport, portfolio_, market_, params_->get("npv", "baseCurrency"));
        CONSOLE("OK");
    } else {
        LOG("skip additional results");
        CONSOLE("SKIP");
    }

    /*********************
     * TodaysMarket calibration
     */
    CONSOLEW("TodaysMarket Calibration...");
    if (params_->hasGroup("todaysMarketCalibration") && params_->get("todaysMarketCalibration", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("todaysMarketCalibration", "outputFileName");
        CSVFileReport todaysMarketCalibrationReport(fileName);
        auto todaysMarket = boost::dynamic_pointer_cast<TodaysMarket>(market_);
        if(todaysMarket) {
            getReportWriter()->writeTodaysMarketCalibrationReport(todaysMarketCalibrationReport,
                                                                  todaysMarket->calibrationInfo());
        }
        CONSOLE("OK");
    } else {
        LOG("skip additional results");
        CONSOLE("SKIP");
    }

    /**********************
     * Cash flow generation
     */
    CONSOLEW("Cashflow Report...");
    if (params_->hasGroup("cashflow") && params_->get("cashflow", "active") == "Y") {
        bool includePastCashflows = params_->has("cashflow", "includePastCashflows") &&
                                    parseBool(params_->get("cashflow", "includePastCashflows"));
        string fileName = outputPath_ + "/" + params_->get("cashflow", "outputFileName");
        CSVFileReport cashflowReport(fileName);
        getReportWriter()->writeCashflow(cashflowReport, params_->get("npv", "baseCurrency"), portfolio_, market_,
                                         params_->get("markets", "pricing"), includePastCashflows);
        CONSOLE("OK");
    } else {
        LOG("skip cashflow generation");
        CONSOLE("SKIP");
    }

    /**********************
     * Cash flow NPV Report
     * NPV of future cash flows with payment dates from tomorrow up to today+horizonCalendarDays
     */
    CONSOLEW("Cashflow NPV report...");
    if (params_->hasGroup("cashflowNpv") && params_->get("cashflowNpv", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("cashflowNpv", "outputFileName");
        Date horizon = Date::maxDate();
        if (params_->has("cashflowNpv", "horizon"))
            horizon = parseDate(params_->get("cashflowNpv", "horizon"));
        string baseCcy = params_->get("npv", "baseCurrency");

        InMemoryReport cashflowReport;
        bool includePastCashflows = false;
        string config = params_->get("markets", "pricing");
        getReportWriter()->writeCashflow(cashflowReport, params_->get("npv", "baseCurrency"), portfolio_, market_,
                                         config, includePastCashflows);

        CSVFileReport cfNpvReport(fileName);
        getReportWriter()->writeCashflowNpv(cfNpvReport, cashflowReport, market_, config, baseCcy, horizon);
        CONSOLE("OK");
    } else {
        LOG("skip cashflow npv report");
        CONSOLE("SKIP");
    }

    LOG("Initial reports written");
    MEM_LOG;
}

boost::shared_ptr<ReportWriter> OREApp::getReportWriter() const {
    return boost::shared_ptr<ReportWriter>(getReportWriterImpl());
}

boost::shared_ptr<SensitivityRunner> OREApp::getSensitivityRunner() {
    return boost::make_shared<SensitivityRunner>(params_, referenceData_, iborFallbackConfig_, continueOnError_);
}

void OREApp::runStressTest() {

    MEM_LOG;
    LOG("Running stress test");

    CONSOLEW("Stress Test Report...");
    // We reset this here because the date grid building below depends on it.
    Settings::instance().evaluationDate() = asof_;

    LOG("Get Simulation Market Parameters");
    string marketConfigFile = inputPath_ + "/" + params_->get("stress", "marketConfigFile");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    simMarketData->fromFile(marketConfigFile);

    LOG("Get Stress Test Parameters");
    string stressConfigFile = inputPath_ + "/" + params_->get("stress", "stressConfigFile");
    boost::shared_ptr<StressTestScenarioData> stressData(new StressTestScenarioData);
    stressData->fromFile(stressConfigFile);

    LOG("Get Engine Data");
    string pricingEnginesFile = inputPath_ + "/" + params_->get("stress", "pricingEnginesFile");
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->fromFile(pricingEnginesFile);

    LOG("Get Portfolio");
    boost::shared_ptr<Portfolio> portfolio = loadPortfolio();

    LOG("Build Stress Test");
    string marketConfiguration = params_->get("markets", "pricing");
    boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
        portfolio, market_, marketConfiguration, engineData, simMarketData, stressData, *curveConfigs_,
        *marketParameters_, nullptr, referenceData_, iborFallbackConfig_, continueOnError_);

    string outputFile = outputPath_ + "/" + params_->get("stress", "scenarioOutputFile");
    Real threshold = parseReal(params_->get("stress", "outputThreshold"));
    boost::shared_ptr<Report> stressReport = boost::make_shared<CSVFileReport>(outputFile);
    stressTest->writeReport(stressReport, threshold);

    CONSOLE("OK");

    LOG("Stress test completed");
    MEM_LOG;

    writePricingStats("pricingstats_stress.csv", portfolio);
}

void OREApp::runParametricVar() {

    MEM_LOG;
    LOG("Running parametric VaR");

    CONSOLEW("Parametric VaR Report...");

    LOG("Get sensitivity data");
    string sensiFile = inputPath_ + "/" + params_->get("parametricVar", "sensitivityInputFile");
    auto ss = boost::make_shared<SensitivityFileStream>(sensiFile);

    LOG("Build trade to portfolio id mapping");
    map<string, set<string>> tradePortfolio;
    for (auto const& [tradeId, trade] : portfolio_->trades()) {
        tradePortfolio[tradeId].insert(trade->portfolioIds().begin(), trade->portfolioIds().end());
    }

    LOG("Load covariance matrix data");
    map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarData;
    loadCovarianceDataFromCsv(covarData, inputPath_ + "/" + params_->get("parametricVar", "covarianceInputFile"));

    Size mcSamples = Null<Size>(), mcSeed = Null<Size>();
    string method = params_->get("parametricVar", "method");
    if (method == "MonteCarlo") {
        mcSamples = parseInteger(params_->get("parametricVar", "mcSamples"));
        mcSeed = parseInteger(params_->get("parametricVar", "mcSeed"));
    }

    string portfolioFilter =
        params_->has("parametricVar", "portfolioFilter") ? params_->get("parametricVar", "portfolioFilter") : "";

    LOG("Build parametric var report");
    auto calc =
        buildParametricVarCalculator(tradePortfolio, portfolioFilter, ss, covarData,
                                     parseListOfValues<Real>(params_->get("parametricVar", "quantiles"), &parseReal),
                                     method, mcSamples, mcSeed, parseBool(params_->get("parametricVar", "breakdown")),
                                     parseBool(params_->get("parametricVar", "salvageCovarianceMatrix")));

    CSVFileReport report(outputPath_ + "/" + params_->get("parametricVar", "outputFile"));
    calc->calculate(report);
    CONSOLE("OK");

    LOG("Parametric VaR completed");
    MEM_LOG;
}

boost::shared_ptr<ParametricVarCalculator>
OREApp::buildParametricVarCalculator(const std::map<std::string, std::set<std::string>>& tradePortfolio,
                                     const std::string& portfolioFilter,
                                     const boost::shared_ptr<SensitivityStream>& sensitivities,
                                     const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                                     const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                                     const Size mcSeed, const bool breakdown, const bool salvageCovarianceMatrix) {
    return boost::make_shared<ParametricVarCalculator>(tradePortfolio, portfolioFilter, sensitivities, covariance, p,
                                                       method, mcSamples, mcSeed, breakdown, salvageCovarianceMatrix);
}

void OREApp::writeBaseScenario() {

    MEM_LOG;
    LOG("Writing base scenario");

    Date today = Settings::instance().evaluationDate();

    LOG("Get Market Configuration");
    string marketConfiguration = params_->get("baseScenario", "marketConfiguration");

    LOG("Get Simulation Market Parameters");
    string marketConfigFile = inputPath_ + "/" + params_->get("baseScenario", "marketConfigFile");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    simMarketData->fromFile(marketConfigFile);

    auto simMarket = boost::make_shared<ScenarioSimMarket>(market_, simMarketData, marketConfiguration, *curveConfigs_,
                                                           *marketParameters_, continueOnError_, false, false, false,
                                                           iborFallbackConfig_, false);
    boost::shared_ptr<Scenario> scenario = simMarket->baseScenario();
    QL_REQUIRE(scenario->asof() == today, "dates do not match");

    string outputFile = outputPath_ + "/" + params_->get("baseScenario", "outputFileName");

    string separator = params_->get("baseScenario", "separator");
    QL_REQUIRE(separator.length() == 1, "separator needs length 1: " << separator);
    const char sep = separator.c_str()[0];

    bool append = parseBool(params_->get("baseScenario", "append"));
    bool writeHeader = parseBool(params_->get("baseScenario", "header"));
    string mode = append ? "a+" : "w+";

    ScenarioWriter sw(outputFile, sep, mode);
    sw.writeScenario(scenario, writeHeader);

    DLOG("Base scenario written to file " << outputFile);

    LOG("Base scenario written");
    MEM_LOG;
}

void OREApp::initAggregationScenarioData() {
    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
}

void OREApp::initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, const Size cubeDepth) {
    QL_REQUIRE(cubeDepth > 0, "zero cube depth given");
    if (cubeDepth == 1)
        cube = boost::make_shared<SinglePrecisionInMemoryCube>(asof_, ids, grid_->valuationDates(), samples_, 0.0f);
    else
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(asof_, ids, grid_->valuationDates(), samples_,
                                                                cubeDepth, 0.0f);

    LOG("init NPV cube with depth: " << cubeDepth);
}

void OREApp::buildNPVCube() {
    LOG("Build valuation cube engine");
    // Valuation calculators
    string baseCurrency = params_->get("simulation", "baseCurrency");
    vector<boost::shared_ptr<ValuationCalculator>> calculators;

    if (useCloseOutLag_) {
        boost::shared_ptr<NPVCalculator> npvCalc = boost::make_shared<NPVCalculator>(baseCurrency);
        // calculators.push_back(boost::make_shared<NPVCalculator>(baseCurrency));
        // default date value stored at index 0, close-out value at index 1
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalc, 0, 1));
    } else {
        calculators.push_back(boost::make_shared<NPVCalculator>(baseCurrency));
    }

    if (storeFlows_) {
        // cash flow stored at index 1 (no close-out lag) or 2 (have close-out lag)
        calculators.push_back(boost::make_shared<CashflowCalculator>(baseCurrency, asof_, grid_, cubeDepth_ - 1));
    }

    bool flipViewXVA = false;
    if (params_->has("xva", "flipViewXVA")) {
        flipViewXVA = parseBool(params_->get("xva", "flipViewXVA"));
    }

    if (useCloseOutLag_)
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(scenarioData_, grid_, flipViewXVA);
    else
        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>(scenarioData_, flipViewXVA);

    vector<boost::shared_ptr<CounterpartyCalculator>> cptyCalculators;

    if (storeSp_) {
        const string configuration = params_->get("markets", "simulation");
        cptyCalculators.push_back(boost::make_shared<SurvivalProbabilityCalculator>(configuration));
    }

    LOG("Build cube");
    ValuationEngine engine(asof_, grid_, simMarket_);
    ostringstream o;
    o.str("");
    o << "Build Cube " << simPortfolio_->size() << " x " << grid_->valuationDates().size() << " x " << samples_
      << "... ";

    auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), ConsoleLog::instance().width(),
                                                             ConsoleLog::instance().progressBarWidth());
    auto progressLog = boost::make_shared<ProgressLog>("Building cube...");
    if (ConsoleLog::instance().enabled()) 
        engine.registerProgressIndicator(progressBar);
    engine.registerProgressIndicator(progressLog);

    engine.buildCube(simPortfolio_, cube_, calculators, useMporStickyDate_, nettingSetCube_, cptyCube_, cptyCalculators);

    CONSOLE("OK");
}

void OREApp::setCubeDepth(const boost::shared_ptr<ScenarioGeneratorData>& sgd) {
    cubeDepth_ = 1;
    if (sgd->withCloseOutLag())
        cubeDepth_++;
    if (params_->has("simulation", "storeFlows") && parseBool(params_->get("simulation", "storeFlows"))) {
        cubeDepth_++;
    }
}

void OREApp::initialiseNPVCubeGeneration(boost::shared_ptr<Portfolio> portfolio) {
    CONSOLEW("Simulation Setup...");
    LOG("Load Simulation Market Parameters");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData = getSimMarketData();
    boost::shared_ptr<ScenarioGeneratorData> sgd = getScenarioGeneratorData();
    grid_ = sgd->getGrid();
    samples_ = sgd->samples();

    if (buildSimMarket_) {
        LOG("Build Simulation Market");

        simMarket_ = boost::make_shared<ScenarioSimMarket>(
            market_, simMarketData, boost::make_shared<FixingManager>(asof_), params_->get("markets", "simulation"),
            *curveConfigs_, *marketParameters_, continueOnError_, false, true, false, iborFallbackConfig_, false);
        string groupName = "simulation";
        boost::shared_ptr<EngineFactory> simFactory = buildEngineFactory(simMarket_, groupName);

        auto continueOnCalErr = simFactory->engineData()->globalParameters().find("ContinueOnCalibrationError");
        boost::shared_ptr<ScenarioGenerator> sg =
            buildScenarioGenerator(market_, simMarketData, sgd,
                                   continueOnCalErr != simFactory->engineData()->globalParameters().end() &&
                                       parseBool(continueOnCalErr->second));
        simMarket_->scenarioGenerator() = sg;

        LOG("Build portfolio linked to sim market");
        Size n = portfolio->size();
        portfolio->build(simFactory, "oreapp/sim");
        simPortfolio_ = portfolio;
        if (simPortfolio_->size() != n) {
            ALOG("There were errors during the sim portfolio building - check the sim market setup? Could build "
                 << simPortfolio_->size() << " trades out of " << n);
        }
        CONSOLE("OK");
    }

    setCubeDepth(sgd);

    storeFlows_ = params_->has("simulation", "storeFlows") && parseBool(params_->get("simulation", "storeFlows"));

    nettingSetCube_ = nullptr;

    initCube(cube_, simPortfolio_->ids(), cubeDepth_);
    
    // initialise counterparty cube for the storage of survival probabilities
    storeSp_ = false;
    if (params_->has("simulation", "storeSurvivalProbabilities") &&
        params_->get("simulation", "storeSurvivalProbabilities") == "Y") {
        storeSp_ = true;
        std::set<std::string> counterparties = portfolio->counterparties();
        counterparties.insert(params_->get("xva", "dvaName"));
        initCube(cptyCube_, counterparties, 1);
    } else {
        cptyCube_ = nullptr;
    }

    CONSOLEW("Aggregation Scenario Data " << grid_->valuationDates().size() << " x " << samples_ << "... ");

    initAggregationScenarioData();
    // Set AggregationScenarioData
    simMarket_->aggregationScenarioData() = scenarioData_;
    CONSOLE("OK");
}

void OREApp::generateNPVCube() {
    MEM_LOG;
    LOG("Running NPV cube generation");

    boost::shared_ptr<Portfolio> portfolio = loadPortfolio();
    initialiseNPVCubeGeneration(portfolio);
    buildNPVCube();
    writeCube(cube_, "cubeFile");
    if (nettingSetCube_)
        writeCube(nettingSetCube_, "nettingSetCubeFile");
    if (cptyCube_)
        writeCube(cptyCube_, "cptyCubeFile");
    writeScenarioData();

    LOG("NPV cube generation completed");
    MEM_LOG;
    writePricingStats("pricingstats_xva.csv", portfolio);
}

void OREApp::writeCube(boost::shared_ptr<NPVCube> cube, const std::string& cubeFileParam) {
    CONSOLEW("Write Cube...");
    if (params_->has("simulation", cubeFileParam)) {
        string cubeFileName = outputPath_ + "/" + params_->get("simulation", cubeFileParam);
        saveCube(cubeFileName, *cube);
        LOG("Write cube '" << cubeFileName << "'");
        CONSOLE("OK");
    } else {
        LOG("Did not write cube, since parameter simulation/" << cubeFileParam << " not specified.");
        CONSOLE("SKIP");
    }
}

void OREApp::writeScenarioData() {
    CONSOLEW("Write Aggregation Scenario Data...");
    LOG("Write scenario data");
    bool skipped = true;
    if (params_->has("simulation", "aggregationScenarioDataFileName")) {
        // binary output
        string outputFileNameAddScenData =
            outputPath_ + "/" + params_->get("simulation", "aggregationScenarioDataFileName");
        saveAggregationScenarioData(outputFileNameAddScenData, *scenarioData_);
        CONSOLE("OK");
        skipped = false;
    }
    if (params_->has("simulation", "aggregationScenarioDataDump")) {
        // csv output
        string outputFileNameAddScenData =
            outputPath_ + "/" + params_->get("simulation", "aggregationScenarioDataDump");
        CSVFileReport report(outputFileNameAddScenData);
        getReportWriter()->writeAggregationScenarioData(report, *scenarioData_);
        skipped = false;
    }
    if (skipped) {
        CONSOLE("SKIP");
    }
}

void OREApp::loadScenarioData() {
    string scenarioFile = outputPath_ + "/" + params_->get("xva", "scenarioFile");
    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>();
    scenarioData_ = loadAggregationScenarioData(scenarioFile);
}

void OREApp::loadCube() {

    // load usual NPV cube on trade level

    string cubeFile = outputPath_ + "/" + params_->get("xva", "cubeFile");
    bool hyperCube = false;
    if (params_->has("xva", "hyperCube"))
        hyperCube = parseBool(params_->get("xva", "hyperCube"));

    if (hyperCube)
        cube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
    else
        cube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
    LOG("Load cube from file " << cubeFile);
    cube_ = ore::analytics::loadCube(cubeFile);
    cubeDepth_ = cube_->depth();
    LOG("Cube loading done: ids=" << cube_->numIds() << " dates=" << cube_->numDates()
                                  << " samples=" << cube_->samples() << " depth=" << cube_->depth());

    // load additional cube on netting set level (if specified)

    if (params_->has("xva", "nettingSetCubeFile") && params_->get("xva", "nettingSetCubeFile") != "") {
        string cubeFile2 = outputPath_ + "/" + params_->get("xva", "nettingSetCubeFile");
        bool hyperCube2 = false;
        if (params_->has("xva", "hyperNettingSetCube"))
            hyperCube2 = parseBool(params_->get("xva", "hyperNettingSetCube"));

        if (hyperCube2)
            nettingSetCube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
        else
            nettingSetCube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
        LOG("Load netting set cube from file " << cubeFile2);
        nettingSetCube_ = ore::analytics::loadCube(cubeFile2);
        LOG("Cube loading done: ids=" << nettingSetCube_->numIds() << " dates=" << nettingSetCube_->numDates()
                                      << " samples=" << nettingSetCube_->samples()
                                      << " depth=" << nettingSetCube_->depth());
    }

    // load additional cube on counterparty level (if specified)

    if (params_->has("xva", "cptyCubeFile") && params_->get("xva", "cptyCubeFile") != "") {
        string cubeFile3 = outputPath_ + "/" + params_->get("xva", "cptyCubeFile");
        cptyCube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
        LOG("Load counterparty cube from file " << cubeFile3);
        cptyCube_ = ore::analytics::loadCube(cubeFile3);
        LOG("Cube loading done: ids=" << cptyCube_->numIds() << " dates=" << cptyCube_->numDates()
                                      << " samples=" << cptyCube_->samples()
                                      << " depth=" << cptyCube_->depth());
    }
}

boost::shared_ptr<NettingSetManager> OREApp::initNettingSetManager() {
    string csaFile = inputPath_ + "/" + params_->get("xva", "csaFile");
    boost::shared_ptr<NettingSetManager> netting = boost::make_shared<NettingSetManager>();
    netting->fromFile(csaFile);
    return netting;
}

void OREApp::runPostProcessor() {
    boost::shared_ptr<NettingSetManager> netting = initNettingSetManager();
    map<string, bool> analytics;
    analytics["exerciseNextBreak"] = parseBool(params_->get("xva", "exerciseNextBreak"));
    analytics["cva"] = parseBool(params_->get("xva", "cva"));
    analytics["dva"] = parseBool(params_->get("xva", "dva"));
    analytics["fva"] = parseBool(params_->get("xva", "fva"));
    analytics["colva"] = parseBool(params_->get("xva", "colva"));
    analytics["collateralFloor"] = parseBool(params_->get("xva", "collateralFloor"));
    if (params_->has("xva", "kva"))
        analytics["kva"] = parseBool(params_->get("xva", "kva"));
    else
        analytics["kva"] = false;
    if (params_->has("xva", "mva"))
        analytics["mva"] = parseBool(params_->get("xva", "mva"));
    else
        analytics["mva"] = false;
    if (params_->has("xva", "dim"))
        analytics["dim"] = parseBool(params_->get("xva", "dim"));
    else
        analytics["dim"] = false;
    if (params_->has("xva", "dynamicCredit"))
        analytics["dynamicCredit"] = parseBool(params_->get("xva", "dynamicCredit"));
    else
        analytics["dynamicCredit"] = false;
    if (params_->has("xva", "cvaSensi"))
        analytics["cvaSensi"] = parseBool(params_->get("xva", "cvaSensi"));
    else
        analytics["cvaSensi"] = false;

    string baseCurrency = params_->get("xva", "baseCurrency");
    string calculationType = params_->get("xva", "calculationType");
    string allocationMethod = params_->get("xva", "allocationMethod");
    Real marginalAllocationLimit = parseReal(params_->get("xva", "marginalAllocationLimit"));
    Real quantile = parseReal(params_->get("xva", "quantile"));
    string dvaName = params_->get("xva", "dvaName");
    string fvaLendingCurve = params_->get("xva", "fvaLendingCurve");
    string fvaBorrowingCurve = params_->get("xva", "fvaBorrowingCurve");

    Real dimQuantile = 0.99;
    Size dimHorizonCalendarDays = 14;
    Size dimRegressionOrder = 0;
    vector<string> dimRegressors;
    // Warning dimScaling set but not used.
    // Real dimScaling = 1.0;
    Size dimLocalRegressionEvaluations = 0;
    Real dimLocalRegressionBandwidth = 0.25;

    Real kvaCapitalDiscountRate = 0.10;
    Real kvaAlpha = 1.4;
    Real kvaRegAdjustment = 12.5;
    Real kvaCapitalHurdle = 0.012;
    Real kvaOurPdFloor = 0.03;
    Real kvaTheirPdFloor = 0.03;
    Real kvaOurCvaRiskWeight = 0.05;
    Real kvaTheirCvaRiskWeight = 0.05;
    if (analytics["kva"]) {
        kvaCapitalDiscountRate = parseReal(params_->get("xva", "kvaCapitalDiscountRate"));
        kvaAlpha = parseReal(params_->get("xva", "kvaAlpha"));
        kvaRegAdjustment = parseReal(params_->get("xva", "kvaRegAdjustment"));
        kvaCapitalHurdle = parseReal(params_->get("xva", "kvaCapitalHurdle"));
        kvaOurPdFloor = parseReal(params_->get("xva", "kvaOurPdFloor"));
        kvaTheirPdFloor = parseReal(params_->get("xva", "kvaTheirPdFloor"));
        kvaOurCvaRiskWeight = parseReal(params_->get("xva", "kvaOurCvaRiskWeight"));
        kvaTheirCvaRiskWeight = parseReal(params_->get("xva", "kvaTheirCvaRiskWeight"));
    }

    if (analytics["mva"] || analytics["dim"]) {
        dimQuantile = parseReal(params_->get("xva", "dimQuantile"));
        dimHorizonCalendarDays = parseInteger(params_->get("xva", "dimHorizonCalendarDays"));
        dimRegressionOrder = parseInteger(params_->get("xva", "dimRegressionOrder"));
        string dimRegressorsString = params_->get("xva", "dimRegressors");
        dimRegressors = parseListOfValues(dimRegressorsString);
        // dimScaling = parseReal(params_->get("xva", "dimScaling"));
        dimLocalRegressionEvaluations = parseInteger(params_->get("xva", "dimLocalRegressionEvaluations"));
        dimLocalRegressionBandwidth = parseReal(params_->get("xva", "dimLocalRegressionBandwidth"));
    }

    string marketConfiguration = params_->get("markets", "simulation");

    bool fullInitialCollateralisation = false;
    if (params_->has("xva", "fullInitialCollateralisation")) {
        fullInitialCollateralisation = parseBool(params_->get("xva", "fullInitialCollateralisation"));
    }
    analytics["flipViewXVA"] = false;
    if (params_->has("xva", "flipViewXVA")) {
        analytics["flipViewXVA"] = parseBool(params_->get("xva", "flipViewXVA"));
    }

    // FIXME: Needs the "simulation" section in ore.xml with consistent simulation.xml
    if (!cubeInterpreter_) {
        boost::shared_ptr<ScenarioGeneratorData> sgd = getScenarioGeneratorData();
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

    std::vector<Period> cvaSensiGrid;
    if (params_->has("xva", "cvaSensiGrid"))
        cvaSensiGrid = parseListOfValues<Period>(params_->get("xva", "cvaSensiGrid"), &parsePeriod);
    Real cvaSensiShiftSize = 0.0;
    if (params_->has("xva", "cvaSensiShiftSize"))
        cvaSensiShiftSize = parseReal(params_->get("xva", "cvaSensiShiftSize"));

    string flipViewBorrowingCurvePostfix = "_BORROW";
    string flipViewLendingCurvePostfix = "_LEND";
    if (analytics["flipViewXVA"]) {
        flipViewBorrowingCurvePostfix = params_->get("xva", "flipViewBorrowingCurvePostfix");
        flipViewLendingCurvePostfix = params_->get("xva", "flipViewLendingCurvePostfix");
    }

    postProcess_ = boost::make_shared<PostProcess>(
        portfolio_, netting, market_, marketConfiguration, cube_, scenarioData_, analytics, baseCurrency,
        allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix);
}

void OREApp::writeXVAReports() {

    MEM_LOG;
    LOG("Writing XVA reports");

    if (params_->has("xva", "exposureProfilesByTrade")) {
        if (parseBool(params_->get("xva", "exposureProfilesByTrade"))) {
            for (const auto& [t,_] : postProcess_->tradeIds()) {
                ostringstream o;
                o << outputPath_ << "/exposure_trade_" << t << ".csv";
                string tradeExposureFile = o.str();
                CSVFileReport tradeExposureReport(tradeExposureFile);
                getReportWriter()->writeTradeExposures(tradeExposureReport, postProcess_, t);
            }
        }
    }
    if (params_->has("xva", "exposureProfiles")) {
        if (parseBool(params_->get("xva", "exposureProfiles"))) {
            for (const auto& [n, _] : postProcess_->nettingSetIds()) {
                ostringstream o1;
                o1 << outputPath_ << "/exposure_nettingset_" << n << ".csv";
                string nettingSetExposureFile = o1.str();
                CSVFileReport nettingSetExposureReport(nettingSetExposureFile);
                getReportWriter()->writeNettingSetExposures(nettingSetExposureReport, postProcess_, n);

                ostringstream o2;
                o2 << outputPath_ << "/colva_nettingset_" << n << ".csv";
                string nettingSetColvaFile = o2.str();
                CSVFileReport nettingSetColvaReport(nettingSetColvaFile);
                getReportWriter()->writeNettingSetColva(nettingSetColvaReport, postProcess_, n);

                ostringstream o3;
                o3 << outputPath_ << "/cva_sensitivity_nettingset_" << n << ".csv";
                string nettingSetCvaSensiFile = o3.str();
                CSVFileReport nettingSetCvaSensitivityReport(nettingSetCvaSensiFile);
                getReportWriter()->writeNettingSetCvaSensitivities(nettingSetCvaSensitivityReport, postProcess_, n);
            }   
        }
    }
    string XvaFile = outputPath_ + "/xva.csv";
    CSVFileReport xvaReport(XvaFile);
    getReportWriter()->writeXVA(xvaReport, params_->get("xva", "allocationMethod"), portfolio_, postProcess_);

    map<string, string> nettingSetMap = portfolio_->nettingSetMap();
    string rawCubeOutputFile = params_->get("xva", "rawCubeOutputFile");
    if (rawCubeOutputFile != "") {
        CubeWriter cw1(outputPath_ + "/" + rawCubeOutputFile);
        cw1.write(postProcess_->cube(), nettingSetMap);
    }
    
    string netCubeOutputFile = params_->get("xva", "netCubeOutputFile");
    if (netCubeOutputFile != "") {
        CubeWriter cw2(outputPath_ + "/" + netCubeOutputFile);
        cw2.write(postProcess_->netCube(), nettingSetMap);
    }

    LOG("XVA reports written");
    MEM_LOG;
}

void OREApp::writeDIMReport() {
    string dimFile1 = outputPath_ + "/" + params_->get("xva", "dimEvolutionFile");
    vector<string> dimFiles2;
    for (auto f : parseListOfValues(params_->get("xva", "dimRegressionFiles")))
        dimFiles2.push_back(outputPath_ + "/" + f);
    string nettingSet = params_->get("xva", "dimOutputNettingSet");
    std::vector<Size> dimOutputGridPoints =
        parseListOfValues<Size>(params_->get("xva", "dimOutputGridPoints"), &parseInteger);
    ore::data::CSVFileReport dimEvolutionReport(dimFile1);
    postProcess_->exportDimEvolution(dimEvolutionReport);
    vector<boost::shared_ptr<ore::data::Report>> reportVec;
    for (Size i = 0; i < dimOutputGridPoints.size(); ++i)
        reportVec.push_back(boost::make_shared<ore::data::CSVFileReport>(dimFiles2[i]));
    postProcess_->exportDimRegression(nettingSet, dimOutputGridPoints, reportVec);
}

void OREApp::buildMarket(const std::string& todaysMarketXML, const std::string& curveConfigXML,
                         const std::string& conventionsXML, const std::vector<string>& marketData,
                         const std::vector<string>& fixingData) {
    MEM_LOG;
    LOG("Building today's market");

    if (conventionsXML == "")
        getConventions();
    else
        conventions_->fromXMLString(conventionsXML);

    if (todaysMarketXML == "")
        getMarketParameters();
    else
        marketParameters_->fromXMLString(todaysMarketXML);

    if (curveConfigXML != "")
        curveConfigs_->fromXMLString(curveConfigXML);
    else if (params_->has("setup", "curveConfigFile") && params_->get("setup", "curveConfigFile") != "") {
        CONSOLEW("Curve configuration...");
        string inputPath = params_->get("setup", "inputPath");
        string curveConfigFile = inputPath + "/" + params_->get("setup", "curveConfigFile");
        LOG("Load curve configurations from file");
        curveConfigs_->fromFile(curveConfigFile);
        CONSOLE("OK");
    } else {
        WLOG("No curve configurations loaded");
    }

    string implyTodaysFixingsString = params_->get("setup", "implyTodaysFixings");
    bool implyTodaysFixings = parseBool(implyTodaysFixingsString);

    boost::shared_ptr<Loader> loader;
    if (marketData.size() == 0 || fixingData.size() == 0) {
        /*******************************
         * Market and fixing data loader
         */
        if (params_->has("setup", "marketDataFile") && params_->get("setup", "marketDataFile") != "") {
            CONSOLEW("Market data loader... ");
            string marketFileString = params_->get("setup", "marketDataFile");
            vector<string> marketFiles = getFileNames(marketFileString, inputPath_);
            string fixingFileString = params_->get("setup", "fixingDataFile");
            vector<string> fixingFiles = getFileNames(fixingFileString, inputPath_);
            vector<string> dividendFiles = {};
            if (params_->has("setup", "dividendDataFile")) {
                string dividendFileString = params_->get("setup", "dividendDataFile");
                dividendFiles = getFileNames(dividendFileString, inputPath_);
            }
            loader = boost::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings);
            CONSOLE("OK");
        } else {
            WLOG("No market data loaded from file");
        }
    } else {
        LOG("Load market and fixing data from string vectors");
        loader = boost::make_shared<InMemoryLoader>();
        loadDataFromBuffers(*boost::static_pointer_cast<InMemoryLoader>(loader), marketData, fixingData,
                            implyTodaysFixings);
    }

    // add generated data to the loader (implied bond spreads, ...)
    boost::shared_ptr<Loader> jointLoader;
    auto generatedData = generateMarketData(loader);
    if (generatedData != nullptr) {
        jointLoader = boost::make_shared<CompositeLoader>(loader, generatedData);
    } else {
        jointLoader = loader;
    }

    // build market
    CONSOLEW("Market... ");
    market_ = boost::make_shared<TodaysMarket>(asof_, marketParameters_, jointLoader, curveConfigs_,
                                               continueOnError_, true, lazyMarketBuilding_, referenceData_, false,
                                               iborFallbackConfig_);
    CONSOLE("OK");

    LOG("Today's market built");
    MEM_LOG;


}

boost::shared_ptr<MarketImpl> OREApp::getMarket() const {
    QL_REQUIRE(market_ != nullptr, "OREApp::getMarket(): original market is null");
    return boost::dynamic_pointer_cast<MarketImpl>(market_);
}

boost::shared_ptr<EngineFactory> OREApp::buildEngineFactoryFromXMLString(const boost::shared_ptr<Market>& market,
                                                                         const std::string& pricingEngineXML,
                                                                         const bool generateAdditionalResults) {
    DLOG("OREApp::buildEngineFactoryFromXMLString called");

    if (pricingEngineXML == "")
        return buildEngineFactory(market, "setup", generateAdditionalResults);
    else {
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->fromXMLString(pricingEngineXML);
        engineData->globalParameters()["GenerateAdditionalResults"] = generateAdditionalResults ? "true" : "false";
        map<MarketContext, string> configurations;
        configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
        configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
        configurations[MarketContext::pricing] = params_->get("markets", "pricing");
        boost::shared_ptr<EngineFactory> factory =
            boost::make_shared<EngineFactory>(engineData, market, configurations, referenceData_, iborFallbackConfig_);
        return factory;
    }
}

} // namespace analytics
} // namespace ore
