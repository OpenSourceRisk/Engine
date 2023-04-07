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

Real OREApp::getRunTime() {
    boost::chrono::duration<double> seconds = boost::chrono::nanoseconds(runTimer_.elapsed().wall);
    return seconds.count();
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

OREApp::OREApp(boost::shared_ptr<Parameters> params, bool console, 
               const boost::filesystem::path& logRootPath)
    : params_(params), inputs_(nullptr) {

    if (console) {
        ConsoleLog::instance().switchOn();
    }

    string outputPath = params_->get("setup", "outputPath");
    string logFile = outputPath + "/" + params_->get("setup", "logFile");
    Size logMask = 15;
    // Get log mask if available
    if (params_->has("setup", "logMask")) {
        logMask = static_cast<Size>(parseInteger(params_->get("setup", "logMask")));
    }
    
    setupLog(outputPath, logFile, logMask, logRootPath);

    auto conventions = boost::make_shared<Conventions>();
    InstrumentConventions::instance().setConventions(conventions);

    // Log the input parameters
    params_->log();

    // Read all inputs from params and files referenced in params
    CONSOLEW("Loading inputs");
    inputs_ = boost::make_shared<InputParameters>();
    buildInputParameters(inputs_, params_);
    outputs_ = boost::make_shared<OutputParameters>(params_);
    CONSOLE("OK");
        
    Settings::instance().evaluationDate() = inputs_->asof();
}

OREApp::OREApp(const boost::shared_ptr<InputParameters>& inputs, const std::string& logFile, Size logLevel,
               bool console, const boost::filesystem::path& logRootPath)
    : params_(nullptr), inputs_(inputs) {

    // Initialise Singletons
    Settings::instance().evaluationDate() = inputs_->asof();
    InstrumentConventions::instance().setConventions(inputs_->conventions());
    if (console) {
        ConsoleLog::instance().switchOn();
    }

    setupLog(inputs_->resultsPath().string(), logFile, logLevel, logRootPath);
}

OREApp::~OREApp() {
    // Close logs
    closeLog();
}

void OREApp::run() {

    runTimer_.start();
    
    try {
        analytics();
    } catch (std::exception& e) {
        ALOG(StructuredAnalyticsWarningMessage("OREApp::run()", "Error", e.what()));
        CONSOLE("Error: " << e.what());
        return;
    }

    runTimer_.stop();

    CONSOLE("run time: " << runTimer_.format(default_places, "%w") << " sec");
    CONSOLE("ORE done.");
    LOG("ORE done.");
}

void OREApp::run(const std::vector<std::string>& marketData,
                 const std::vector<std::string>& fixingData) {
    runTimer_.start();

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
        ALOG(StructuredAnalyticsWarningMessage("OREApp::run()", oss.str(), e.what()));
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
        return;
    }

    runTimer_.stop();
    
    LOG("ORE analytics done");
}

void OREApp::buildInputParameters(boost::shared_ptr<InputParameters> inputs,
                                  const boost::shared_ptr<Parameters>& params) {
    QL_REQUIRE(inputs, "InputParameters not created yet");

    LOG("buildInputParameters called");

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

    if (params_->hasGroup("npv"))
        inputs->setBaseCurrency(params_->get("npv", "baseCurrency"));
    else {
        ALOG("Base currency not set");
    }
        
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
    if (tmp != "") {
        bool mkt = parseBool(tmp);
        inputs->setOutputCurves(mkt);
    }
    
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
        DLOG("NettingSetCube loading done: ids=" << inputs->nettingSetCube()->numIds()
             << " dates=" << inputs->nettingSetCube()->numDates()
             << " samples=" << inputs->nettingSetCube()->samples()
             << " depth=" << inputs->nettingSetCube()->depth());
    }

    tmp = params_->get("xva", "cptyCubeFile", false);
    if (inputs->loadCube() && tmp != "") {
        string cubeFile = inputs->resultsPath().string() + "/" + tmp;
        LOG("Load cpty cube from file " << cubeFile);
        inputs->setCptyCubeFromFile(cubeFile);
        DLOG("CptyCube loading done: ids=" << inputs->cptyCube()->numIds()
             << " dates=" << inputs->cptyCube()->numDates()
             << " samples=" << inputs->cptyCube()->samples()
             << " depth=" << inputs->cptyCube()->depth());
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

    tmp = params_->get("xva", "deterministicInitialMarginFile", false);
    if (tmp != "") {
        string imFile = inputPath + "/" + tmp;
        LOG("Load initial margin evolution from file " << tmp);
        inputs->setDeterministicInitialMarginFromFile(imFile);
    }
    
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
    
    if (inputs->analytics().size() == 0)
        inputs->insertAnalytic("MARKETDATA");

    LOG("buildInputParameters done");
}

void OREApp::setupLog(const std::string& path, const std::string& file, Size mask,
                      const boost::filesystem::path& logRootPath) {
    closeLog();
    
    boost::filesystem::path p{path};
    if (!boost::filesystem::exists(p)) {
        boost::filesystem::create_directories(p);
    }
    QL_REQUIRE(boost::filesystem::is_directory(p), "output path '" << path << "' is not a directory.");

    Log::instance().registerLogger(boost::make_shared<FileLogger>(file));
    // Report StructuredErrorMessages with level WARNING, ERROR, CRITICAL, ALERT
    fbLogger_ = boost::make_shared<FilteredBufferedLoggerGuard>();
    boost::filesystem::path oreRootPath =
        logRootPath.empty() ? boost::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                            : logRootPath;
    Log::instance().setRootPath(oreRootPath);
    Log::instance().setMask(mask);
    Log::instance().switchOn();
}

void OREApp::closeLog() { Log::instance().removeAllLoggers(); }

} // namespace analytics
} // namespace ore
