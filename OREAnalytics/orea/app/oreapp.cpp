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

#include <orea/app/cleanupsingletons.hpp>
#include <orea/app/marketdatacsvloader.hpp>
#include <orea/app/marketdatainmemoryloader.hpp>
#include <orea/app/oreapp.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/observationmode.hpp>

#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/configuration/currencyconfig.hpp>
#include <ored/portfolio/collateralbalance.hpp>

#include <qle/version.hpp>


#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/timer/timer.hpp>

#include <mutex>

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

const QuantLib::ext::shared_ptr<Analytic>& OREApp::getAnalytic(std::string type) {
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

QuantLib::ext::shared_ptr<PlainInMemoryReport> OREApp::getReport(std::string reportName) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    for (const auto& rep : analyticsManager_->reports()) {
        for (auto b : rep.second) {
            if (reportName == b.first)
                return QuantLib::ext::make_shared<PlainInMemoryReport>(b.second);
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
    
QuantLib::ext::shared_ptr<NPVCube> OREApp::getCube(std::string cubeName) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    for (const auto& c : analyticsManager_->npvCubes()) {
        for (auto b : c.second) {
            if (cubeName == b.first)
                return b.second;
        }
    }
    QL_FAIL("npv cube " << cubeName << " not found in results");
}

std::set<std::string> OREApp::getMarketCubeNames() {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    std::set<std::string> names;
    for (const auto& c : analyticsManager_->mktCubes()) {
        for (auto b : c.second) {
            string cubeName = b.first;
            if (names.find(cubeName) == names.end())
                names.insert(cubeName);
            else {
                ALOG("market cube name " << cubeName
                     << " occurs more than once, will retrieve the first cube with that name only");
            }
        }
    }
    return names;
}
    
QuantLib::ext::shared_ptr<AggregationScenarioData> OREApp::getMarketCube(std::string cubeName) {
    QL_REQUIRE(analyticsManager_, "analyticsManager_ not set yet, call analytics first");
    for (const auto& c : analyticsManager_->mktCubes()) {
        for (auto b : c.second) {
            if (cubeName == b.first)
                return b.second;
        }
    }
    QL_FAIL("market cube " << cubeName << " not found in results");
}

std::vector<std::string> OREApp::getErrors() {
    return errorMessages_;
}

Real OREApp::getRunTime() {
    boost::chrono::duration<double> seconds = boost::chrono::nanoseconds(runTimer_.elapsed().wall);
    return seconds.count();
}
    
QuantLib::ext::shared_ptr<CSVLoader> OREApp::buildCsvLoader(const QuantLib::ext::shared_ptr<Parameters>& params) {
    bool implyTodaysFixings = false;
    vector<string> marketFiles = {};
    vector<string> fixingFiles = {};
    vector<string> dividendFiles = {};

    filesystem::path inputPath = params_->get("setup", "inputPath");

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

    tmp = params->get("setup", "fixingCutoff", false);
    Date cutoff = Date();
    if (tmp != "")
        cutoff = parseDate(tmp);
    else {
        WLOG("fixing cutoff date not set");
    }
    
    auto loader = boost::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings, cutoff);

    return loader;
}

void OREApp::analytics() {

    try {
        LOG("ORE analytics starting");
        MEM_LOG_USING_LEVEL(ORE_WARNING)

        QL_REQUIRE(params_, "ORE input parameters not set");
                
        Settings::instance().evaluationDate() = inputs_->asof();

        GlobalPseudoCurrencyMarketParameters::instance().set(inputs_->pricingEngine()->globalParameters());

        // Initialize the global conventions 
        InstrumentConventions::instance().setConventions(inputs_->conventions());

        // Create a market data loader that reads market data, fixings, dividends from csv files
        auto csvLoader = buildCsvLoader(params_);
        auto loader = QuantLib::ext::make_shared<MarketDataCsvLoader>(inputs_, csvLoader);

        // Create the analytics manager
        analyticsManager_ = QuantLib::ext::make_shared<AnalyticsManager>(inputs_, loader);
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        QuantLib::ext::shared_ptr<MarketCalibrationReportBase> mcr;
        if (inputs_->outputTodaysMarketCalibration()) {
            auto marketCalibrationReport = QuantLib::ext::make_shared<ore::data::InMemoryReport>();
            mcr = QuantLib::ext::make_shared<MarketCalibrationReport>(string(), marketCalibrationReport);
        }

        // Run the requested analytics
        analyticsManager_->runAnalytics(mcr);

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
                std::string fileName = inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "csv.gz");
                LOG("write npv cube " << reportName << " to file " << fileName);
                NPVCubeWithMetaData r;
                r.cube = b.second;
                if (b.first == "cube") {
                    // store meta data together with npv cube
                    r.scenarioGeneratorData = inputs_->scenarioGeneratorData();
                    r.storeFlows = inputs_->storeFlows();
                    r.storeCreditStateNPVs = inputs_->storeCreditStateNPVs();
                }
                saveCube(fileName, r);
            }
        }
        
        // Write market cube(s)
        for (auto a : analyticsManager_->mktCubes()) {
            for (auto b : a.second) {
                string reportName = b.first;
                std::string fileName = inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "csv.gz");
                LOG("write market cube " << reportName << " to file " << fileName);
                saveAggregationScenarioData(fileName, *b.second);
            }
        }

        for (auto a: analyticsManager_->stressTests()){
            for(auto b: a.second){
                string reportName = b.first;
                std::string fileName =
                    inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "xml");
                LOG("write converted stress test scenario definition " << reportName << " to file " << fileName);
                b.second->toFile(fileName);
            }
        }

    }
    catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        ALOG(oss.str());
        MEM_LOG_USING_LEVEL(ORE_WARNING)
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
    }

    MEM_LOG_USING_LEVEL(ORE_WARNING)
    LOG("ORE analytics done");
}


void OREApp::initFromParams() {
    if (console_) {
        ConsoleLog::instance().switchOn();
    }

    outputPath_ = params_->get("setup", "outputPath");
    logFile_ = outputPath_ + "/" + params_->get("setup", "logFile");
    logMask_ = 15;
    // Get log mask if available
    if (params_->has("setup", "logMask")) {
        logMask_ = static_cast<Size>(parseInteger(params_->get("setup", "logMask")));
    }

    progressLogRotationSize_ = 0;
    progressLogToConsole_ = false;
    structuredLogRotationSize_ = 0;
    
    if (params_->hasGroup("logging")) {
        string tmp = params_->get("logging", "logFile", false);
        if (!tmp.empty()) {
            logFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->get("logging", "logMask", false);
        if (!tmp.empty()) {
            logMask_ = static_cast<Size>(parseInteger(tmp));
        }
        tmp = params_->get("logging", "progressLogFile", false);
        if (!tmp.empty()) {
            progressLogFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->get("logging", "progressLogRotationSize", false);
        if (!tmp.empty()) {
            progressLogRotationSize_ = static_cast<Size>(parseInteger(tmp));
        }
        tmp = params_->get("logging", "progressLogToConsole", false);
        if (!tmp.empty()) {
            progressLogToConsole_ = ore::data::parseBool(tmp);
        }
        tmp = params_->get("logging", "structuredLogFile", false);
        if (!tmp.empty()) {
            structuredLogFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->get("logging", "structuredLogRotationSize", false);
        if (!tmp.empty()) {
            structuredLogRotationSize_ = static_cast<Size>(parseInteger(tmp));
        }
    }
    
    setupLog(outputPath_, logFile_, logMask_, logRootPath_, progressLogFile_, progressLogRotationSize_, progressLogToConsole_,
             structuredLogFile_, structuredLogRotationSize_);

    // Log the input parameters
    params_->log();

    // Read all inputs from params and files referenced in params
    CONSOLEW("Loading inputs");
    inputs_ = QuantLib::ext::make_shared<OREAppInputParameters>(params_);
    inputs_->loadParameters();
    outputs_ = QuantLib::ext::make_shared<OutputParameters>(params_);
    CONSOLE("OK");

    Settings::instance().evaluationDate() = inputs_->asof();
    LOG("initFromParameters done, requested analytics:" << to_string(inputs_->analytics()));
} 
  
void OREApp::initFromInputs() {
    // Initialise Singletons
    Settings::instance().evaluationDate() = inputs_->asof();
    InstrumentConventions::instance().setConventions(inputs_->conventions());
    if (console_) {
        ConsoleLog::instance().switchOn();
    }

    outputPath_ = inputs_->resultsPath().string();
    setupLog(outputPath_, logFile_, logMask_, logRootPath_, progressLogFile_, progressLogRotationSize_, progressLogToConsole_,
             structuredLogFile_, structuredLogRotationSize_);
    LOG("initFromInputs done, requested analytics:" << to_string(inputs_->analytics()));
}

OREApp::~OREApp() {
    // Close logs
    closeLog();
}

void OREApp::run() {

    // Only one thread at a time should call run
    static std::mutex _s_mutex;
    std::lock_guard<std::mutex> lock(_s_mutex);

    // Clean start, but leave Singletons intact after run is completed
    {
      CleanUpThreadLocalSingletons cleanupThreadLocalSingletons;
      CleanUpThreadGlobalSingletons cleanupThreadGloablSingletons;
      CleanUpLogSingleton cleanupLogSingleton(true, true);
    }

    // Use inputs when available, otherwise try params
    if (inputs_ != nullptr)
        initFromInputs();
    else if (params_ != nullptr)
        initFromParams();
    else {
        ALOG("both inputs are empty");
	return;
    }

    runTimer_.start();
    
    try {
        structuredLogger_->clear();
        analytics();
    } catch (std::exception& e) {
        StructuredAnalyticsWarningMessage("OREApp::run()", "Error", e.what()).log();
        CONSOLE("Error: " << e.what());
        return;
    }

    runTimer_.stop();

    // cache the error messages because we reset the loggers 
    errorMessages_ = structuredLogger_->messages();

    CONSOLE("run time: " << runTimer_.format(default_places, "%w") << " sec");
    CONSOLE("ORE done.");
    LOG("ORE done.");
}

void OREApp::run(const std::vector<std::string>& marketData,
                 const std::vector<std::string>& fixingData) {

    // Only one thread at a time should call run
    static std::mutex _s_mutex;
    std::lock_guard<std::mutex> lock(_s_mutex);

    // Clean start, but leave Singletons intact after run is completed
    {
      CleanUpThreadLocalSingletons cleanupThreadLocalSingletons;
      CleanUpThreadGlobalSingletons cleanupThreadGloablSingletons;
      CleanUpLogSingleton cleanupLogSingleton(true, true);
    }

    // Use inputs when available, otherwise try params
    if (inputs_ != nullptr)
        initFromInputs();
    else if (params_ != nullptr)
        initFromParams();
    else {
        ALOG("both inputs are empty");
	return;
    }

    runTimer_.start();

    try {
        LOG("ORE analytics starting");
        structuredLogger_->clear();
        MEM_LOG_USING_LEVEL(ORE_WARNING)

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
        auto loader = QuantLib::ext::make_shared<MarketDataInMemoryLoader>(inputs_, marketData, fixingData);

        // Create the analytics manager
        analyticsManager_ = QuantLib::ext::make_shared<AnalyticsManager>(inputs_, loader);
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics:");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        QuantLib::ext::shared_ptr<MarketCalibrationReportBase> mcr;
        if (inputs_->outputTodaysMarketCalibration()) {
            auto marketCalibrationReport = QuantLib::ext::make_shared<ore::data::InMemoryReport>();
            mcr = QuantLib::ext::make_shared<MarketCalibrationReport>(string(), marketCalibrationReport);
        }

        // Run the requested analytics
        analyticsManager_->runAnalytics(mcr);

        MEM_LOG_USING_LEVEL(ORE_WARNING)
        // Leave any report writing to the calling aplication
    }
    catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        StructuredAnalyticsWarningMessage("OREApp::run()", oss.str(), e.what()).log();
        MEM_LOG_USING_LEVEL(ORE_WARNING)
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
        return;
    }

    runTimer_.stop();
    
    LOG("ORE analytics done");
}

void OREApp::setupLog(const std::string& path, const std::string& file, Size mask,
                      const boost::filesystem::path& logRootPath, const std::string& progressLogFile,
                      Size progressLogRotationSize, bool progressLogToConsole, const std::string& structuredLogFile,
                      Size structuredLogRotationSize) {
    closeLog();
    
    boost::filesystem::path p{path};
    if (!boost::filesystem::exists(p)) {
        boost::filesystem::create_directories(p);
    }
    QL_REQUIRE(boost::filesystem::is_directory(p), "output path '" << path << "' is not a directory.");

    Log::instance().registerLogger(QuantLib::ext::make_shared<FileLogger>(file));
    boost::filesystem::path oreRootPath =
        logRootPath.empty() ? boost::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                            : logRootPath;
    Log::instance().setRootPath(oreRootPath);
    Log::instance().setMask(mask);
    Log::instance().switchOn();

    // Progress logger
    auto progressLogger = QuantLib::ext::make_shared<ProgressLogger>();
    string progressLogFilePath = progressLogFile.empty() ? path + "/log_progress.json" : progressLogFile;
    progressLogger->setFileLog(progressLogFilePath, path, progressLogRotationSize);
    progressLogger->setCoutLog(progressLogToConsole);
    Log::instance().registerIndependentLogger(progressLogger);

    // Structured message logger
    structuredLogger_ = QuantLib::ext::make_shared<StructuredLogger>();
    string structuredLogFilePath = structuredLogFile.empty() ? path + "/log_structured.json" : structuredLogFile;
    structuredLogger_->setFileLog(structuredLogFilePath, path, structuredLogRotationSize);
    Log::instance().registerIndependentLogger(structuredLogger_);

    // Event message logger
    auto eventLogger = QuantLib::ext::make_shared<EventLogger>();
    eventLogger->setFileLog(path + "/log_event_");
    ore::data::Log::instance().registerIndependentLogger(eventLogger);
}

void OREApp::closeLog() { Log::instance().removeAllLoggers(); }

std::string OREApp::version() { return std::string(OPEN_SOURCE_RISK_VERSION); }

void OREAppInputParameters::loadParameters() {
    LOG("load OREAppInputParameters called");

    // switch default for backward compatibility
    setEntireMarket(false);
    setAllFixings(true);
    setEomInflationFixings(false);
    setUseMarketDataFixings(false);
    setBuildFailedTrades(false);

    QL_REQUIRE(params_->hasGroup("setup"), "parameter group 'setup' missing");

    filesystem::path inputPath = params_->get("setup", "inputPath");
    std::string outputPath = params_->get("setup", "outputPath");

    // Load calendar adjustments
    std::string tmp = params_->get("setup", "calendarAdjustment", false);
    if (tmp != "") {
        CalendarAdjustmentConfig calendarAdjustments;
        filesystem::path calendarAdjustmentFile = inputPath / tmp;
        LOG("Loading calendar adjustments from file: " << calendarAdjustmentFile);
        calendarAdjustments.fromFile(calendarAdjustmentFile.generic_string());
    } else {
        WLOG("Calendar adjustments not found, using defaults");
    }

    // Load currency configs
    tmp = params_->get("setup", "currencyConfiguration", false);
    if (tmp != "") {
        CurrencyConfig currencyConfig;
        filesystem::path currencyConfigFile = inputPath / tmp;
        LOG("Loading currency configurations from file: " << currencyConfigFile);
        currencyConfig.fromFile(currencyConfigFile.generic_string());
    } else {
        WLOG("Currency configurations not found, using defaults");
    }

    setAsOfDate(params_->get("setup", "asofDate"));

    // Set it immediately, otherwise the scenario generator grid below will be based on today's date
    Settings::instance().evaluationDate() = asof();

    setResultsPath(outputPath);

    // first look for baseCurrency in setUp, and then in NPV node
    tmp = params_->get("setup", "baseCurrency", false);
    if (tmp != "")
        setBaseCurrency(tmp);
    else if (params_->hasGroup("npv"))
        setBaseCurrency(params_->get("npv", "baseCurrency"));
    else {
        WLOG("Base currency not set");
    }

    tmp = params_->get("setup", "useMarketDataFixings", false);
    if (tmp != "")
        setUseMarketDataFixings(parseBool(tmp));

    tmp = params_->get("setup", "dryRun", false);
    if (tmp != "")
        setDryRun(parseBool(tmp));

    tmp = params_->get("setup", "reportNaString", false);
    if (tmp != "")
        setReportNaString(tmp);

    tmp = params_->get("setup", "eomInflationFixings", false);
    if (tmp != "")
        setEomInflationFixings(parseBool(tmp));

    tmp = params_->get("setup", "nThreads", false);
    if (tmp != "")
        setThreads(parseInteger(tmp));

    tmp = params_->get("setup", "entireMarket", false);
    if (tmp != "")
        setEntireMarket(parseBool(tmp));

    tmp = params_->get("setup", "iborFallbackOverride", false);
    if (tmp != "")
        setIborFallbackOverride(parseBool(tmp));

    tmp = params_->get("setup", "continueOnError", false);
    if (tmp != "")
        setContinueOnError(parseBool(tmp));

    tmp = params_->get("setup", "lazyMarketBuilding", false);
    if (tmp != "")
        setLazyMarketBuilding(parseBool(tmp));

    tmp = params_->get("setup", "buildFailedTrades", false);
    if (tmp != "")
        setBuildFailedTrades(parseBool(tmp));

    tmp = params_->get("setup", "observationModel", false);
    if (tmp != "") {
        setObservationModel(tmp);
        ObservationMode::instance().setMode(observationModel());
        LOG("Observation Mode is " << observationModel());
    }

    tmp = params_->get("setup", "implyTodaysFixings", false);
    if (tmp != "")
        setImplyTodaysFixings(ore::data::parseBool(tmp));

    tmp = params_->get("setup", "referenceDataFile", false);
    if (tmp != "") {
        filesystem::path refDataFile = inputPath / tmp;
        LOG("Loading reference data from file: " << refDataFile);
        setRefDataManagerFromFile(refDataFile.generic_string());
    } else {
        WLOG("Reference data not found");
    }

    tmp = params_->get("setup", "scriptLibrary", false);
    if (tmp != "") {
        filesystem::path scriptFile = inputPath / tmp;
        LOG("Loading script library from file: " << scriptFile);
        setScriptLibraryFromFile(scriptFile.generic_string());        
    }
    else {
        WLOG("Script library not loaded");
    }

    if (params_->has("setup", "conventionsFile") && params_->get("setup", "conventionsFile") != "") {
        filesystem::path conventionsFile = inputPath / params_->get("setup", "conventionsFile");
        LOG("Loading conventions from file: " << conventionsFile);
        setConventionsFromFile(conventionsFile.generic_string());
    } else {
        ALOG("Conventions not found");
    }

    if (params_->has("setup", "iborFallbackConfig") && params_->get("setup", "iborFallbackConfig") != "") {
        filesystem::path tmp = inputPath / params_->get("setup", "iborFallbackConfig");
        LOG("Loading Ibor fallback config from file: " << tmp);
        setIborFallbackConfigFromFile(tmp.generic_string());
    } else {
        WLOG("Using default Ibor fallback config");
    }

    if (params_->has("setup", "curveConfigFile") && params_->get("setup", "curveConfigFile") != "") {
        filesystem::path curveConfigFile = inputPath / params_->get("setup", "curveConfigFile");
        LOG("Load curve configurations from file: ");
        setCurveConfigsFromFile(curveConfigFile.generic_string());
    } else {
        ALOG("no curve configs loaded");
    }

    tmp = params_->get("setup", "pricingEnginesFile", false);
    if (tmp != "") {
        filesystem::path pricingEnginesFile = inputPath / tmp;
        LOG("Load pricing engine data from file: " << pricingEnginesFile);
        setPricingEngineFromFile(pricingEnginesFile.generic_string());
    } else {
        ALOG("Pricing engine data not found");
    }

    tmp = params_->get("setup", "marketConfigFile", false);
    if (tmp != "") {
        filesystem::path marketConfigFile = inputPath / tmp;
        LOG("Loading today's market parameters from file" << marketConfigFile);
        setTodaysMarketParamsFromFile(marketConfigFile.generic_string());
    } else {
        ALOG("Today's market parameters not found");
    }

    if (params_->has("setup", "buildFailedTrades"))
        setBuildFailedTrades(parseBool(params_->get("setup", "buildFailedTrades")));

    tmp = params_->get("setup", "portfolioFile", false);
    if (tmp != "") {
        setPortfolioFromFile(tmp, inputPath);
    } else {
        WLOG("Portfolio data not provided");
    }

    if (params_->hasGroup("markets")) {
        setMarketConfigs(params_->markets());
        for (auto m : marketConfigs())
            LOG("MarketContext::" << m.first << " = " << m.second);
    }

    if (params_->has("setup", "csvCommentReportHeader"))
        setCsvCommentCharacter(parseBool(params_->get("setup", "csvCommentReportHeader")));

    if (params_->has("setup", "csvSeparator")) {
        tmp = params_->get("setup", "csvSeparator");
        QL_REQUIRE(tmp.size() == 1, "csvSeparator must be exactly one character");
        setCsvSeparator(tmp[0]);
    }

    /*************
     * NPV
     *************/

    tmp = params_->get("npv", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("NPV");

    tmp = params_->get("npv", "additionalResults", false);
    if (tmp != "")
        setOutputAdditionalResults(parseBool(tmp));

    tmp = params_->get("npv", "additionalResultsReportPrecision", false);
    if (tmp != "")
        setAdditionalResultsReportPrecision(parseInteger(tmp));

    /*************
     * P&L
     *************/
    
    tmp = params_->get("pnl", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PNL");

	tmp = params_->get("pnl", "mporDate", false);
	if (tmp != "")
	    setMporDate(parseDate(tmp));

	tmp = params_->get("pnl", "mporDays", false);
	if (tmp != "")
	    setMporDays(parseInteger(tmp));

	tmp = params_->get("pnl", "mporCalendar", false);
	if (tmp != "")
	    setMporCalendar(tmp);

	tmp = params_->get("pnl", "simulationConfigFile", false);
	if (tmp != "") {
	    string simulationConfigFile = (inputPath / tmp).generic_string();
	    LOG("Loading scenario simulation config from file" << simulationConfigFile);
	    // Should we check whether other analytics are setting this, too?
	    setScenarioSimMarketParamsFromFile(simulationConfigFile);
	} else {
	    ALOG("Scenario Simulation market data not loaded");
	}
    }

    /*************
     * CASHFLOW
     *************/

    tmp = params_->get("cashflow", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("CASHFLOW");

    tmp = params_->get("cashflow", "includePastCashflows", false);
    if (tmp != "")
        setIncludePastCashflows(parseBool(tmp));

    /*************
     * Curves
     *************/

    tmp = params_->get("curves", "active", false);
    if (tmp != "") {
        bool mkt = parseBool(tmp);
        setOutputCurves(mkt);
    }

    tmp = params_->get("curves", "grid", false);
    if (tmp != "")
        setCurvesGrid(tmp);

    tmp = params_->get("curves", "configuration", false);
    if (tmp != "")
        setCurvesMarketConfig(tmp);

    tmp = params_->get("curves", "outputTodaysMarketCalibration", false);
    if (tmp != "")
        setOutputTodaysMarketCalibration(parseBool(tmp));

    /*************
     * SENSITIVITY
     *************/

    // FIXME: The following are not loaded from params so far, relying on defaults
    // xbsParConversion_ = false;
    // analyticFxSensis_ = true;
    // useSensiSpreadedTermStructures_ = true;

    tmp = params_->get("sensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("SENSITIVITY");

        tmp = params_->get("sensitivity", "parSensitivity", false);
        if (tmp != "")
            setParSensi(parseBool(tmp));

        tmp = params_->get("sensitivity", "optimiseRiskFactors", false);
        if (tmp != "")
            setOptimiseRiskFactors(parseBool(tmp));

        tmp = params_->get("sensitivity", "outputJacobi", false);
        if (tmp != "")
            setOutputJacobi(parseBool(tmp));

        tmp = params_->get("sensitivity", "alignPillars", false);
        if (tmp != "")
            setAlignPillars(parseBool(tmp));
        else
            setAlignPillars(parSensi());

        tmp = params_->get("sensitivity", "marketConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading sensitivity scenario sim market parameters from file" << file);
            setSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }

        tmp = params_->get("sensitivity", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setSensiScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }

        tmp = params_->get("sensitivity", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setSensiPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for sensitivity analysis, using global");
            // FIXME
            setSensiPricingEngine(pricingEngine());
        }

        tmp = params_->get("sensitivity", "outputSensitivityThreshold", false);
        if (tmp != "")
            setSensiThreshold(parseReal(tmp));

        tmp = params_->get("sensitivity", "recalibrateModels", false);
        if (tmp != "")
            setSensiRecalibrateModels(parseBool(tmp));
    }

    /************
     * SCENARIO
     ************/

    tmp = params_->get("scenario", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("SCENARIO");

        tmp = params_->get("scenario", "simulationConfigFile", false);
        if (tmp != "") {
            string simulationConfigFile = (inputPath / tmp).generic_string();
            LOG("Loading scenario simulation config from file" << simulationConfigFile);
            setScenarioSimMarketParamsFromFile(simulationConfigFile);
        } else {
            ALOG("Scenario Simulation market data not loaded");
        }

        tmp = params_->get("scenario", "scenarioOutputFile", false);
        if (tmp != "")
            setScenarioOutputFile(tmp);
    }

    /****************
     * STRESS
     ****************/

    tmp = params_->get("stress", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("STRESS");
        setStressPricingEngine(pricingEngine());
        tmp = params_->get("stress", "marketConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading stress test scenario sim market parameters from file" << file);
            setStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for stress testing not loaded");
        }

        tmp = params_->get("stress", "stressConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load stress test scenario data from file" << file);
            setStressScenarioDataFromFile(file);
        } else {
            WLOG("Stress scenario data not loaded");
        }

        tmp = params_->get("stress", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setStressPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }

        tmp = params_->get("stress", "outputThreshold", false);
        if (tmp != "")
            setStressThreshold(parseReal(tmp));

        tmp = params_->get("stress", "optimiseRiskFactors", false);
        if (tmp != "")
            setStressOptimiseRiskFactors(parseBool(tmp));

        tmp = params_->get("stress", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }

        tmp = params_->get("stress", "lowerBoundCapVols", false);
        if (tmp != "") {
            setStressLowerBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->get("stress", "upperBoundCapVols", false);
        if (tmp != "") {
            setStressUpperBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->get("stress", "lowerBoundDiscountFactors", false);
        if (tmp != "") {
            setStressLowerBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->get("stress", "upperBoundDiscountFactors", false);
        if (tmp != "") {
            setStressUpperBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->get("stress", "lowerBoundSurvivalProb", false);
        if (tmp != "") {
            setStressLowerBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->get("stress", "upperBoundSurvivalProb", false);
        if (tmp != "") {
            setStressUpperBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->get("stress", "accuracy", false);
        if (tmp != "") {
            setStressAccurary(parseReal(tmp));
        }
    }

    /****************
     * PAR STRESS CONVERSION
     ****************/

    tmp = params_->get("parStressConversion", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PARSTRESSCONVERSION");
        setParStressPricingEngine(pricingEngine());
        tmp = params_->get("parStressConversion", "marketConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading parStressConversion test scenario sim market parameters from file" << file);
            setParStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for par stress conversion testing not loaded");
        }

        tmp = params_->get("parStressConversion", "stressConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load stress test scenario data from file" << file);
            setParStressScenarioDataFromFile(file);
        } else {
            WLOG("Stress scenario data not loaded");
        }

        tmp = params_->get("parStressConversion", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setParStressPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }

        tmp = params_->get("parStressConversion", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setParStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }

        tmp = params_->get("parStressConversion", "lowerBoundCapVols", false);
        if (tmp != "") {
            setParStressLowerBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "upperBoundCapVols", false);
        if (tmp != "") {
            setParStressUpperBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "lowerBoundDiscountFactors", false);
        if (tmp != "") {
            setParStressLowerBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "upperBoundDiscountFactors", false);
        if (tmp != "") {
            setParStressUpperBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "lowerBoundSurvivalProb", false);
        if (tmp != "") {
            setParStressLowerBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "upperBoundSurvivalProb", false);
        if (tmp != "") {
            setParStressUpperBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->get("parStressConversion", "accuracy", false);
        if (tmp != "") {
            setParStressAccurary(parseReal(tmp));
        }
    }

    /****************
     * ZERO TO PAR SHIFT CONVERSION
     ****************/

    tmp = params_->get("zeroToParShift", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("ZEROTOPARSHIFT");
        setZeroToParShiftPricingEngine(pricingEngine());
        tmp = params_->get("zeroToParShift", "marketConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading zero to par shift conversion scenario sim market parameters from file " << file);
            setZeroToParShiftSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for zero to par shift conversion not loaded");
        }

        tmp = params_->get("zeroToParShift", "stressConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load zero to par shift conversion scenario data from file" << file);
            setZeroToParShiftScenarioDataFromFile(file);
        } else {
            WLOG("Zero to par shift conversion scenario data not loaded");
        }

        tmp = params_->get("zeroToParShift", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setZeroToParShiftPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for Zero to par shift conversion, using global");
        }

        tmp = params_->get("zeroToParShift", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setZeroToParShiftSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded for zero to par shift conversion");
        }
    }

    /********************
     * VaR - Parametric
     ********************/

    tmp = params_->get("parametricVar", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PARAMETRIC_VAR");

        tmp = params_->get("parametricVar", "salvageCovarianceMatrix", false);
        if (tmp != "")
            setSalvageCovariance(parseBool(tmp));

        tmp = params_->get("parametricVar", "quantiles", false);
        if (tmp != "")
            setVarQuantiles(tmp);

        tmp = params_->get("parametricVar", "breakdown", false);
        if (tmp != "")
            setVarBreakDown(parseBool(tmp));

        tmp = params_->get("parametricVar", "portfolioFilter", false);
        if (tmp != "")
            setPortfolioFilter(tmp);

        tmp = params_->get("parametricVar", "method", false);
        if (tmp != "")
            setVarMethod(tmp);

        tmp = params_->get("parametricVar", "mcSamples", false);
        if (tmp != "")
            setMcVarSamples(parseInteger(tmp));

        tmp = params_->get("parametricVar", "mcSeed", false);
        if (tmp != "")
            setMcVarSeed(parseInteger(tmp));

        tmp = params_->get("parametricVar", "covarianceInputFile", false);
        QL_REQUIRE(tmp != "", "covarianceInputFile not provided");
        std::string covFile = (inputPath / tmp).generic_string();
        LOG("Load Covariance Data from file " << covFile);
        setCovarianceDataFromFile(covFile);

        tmp = params_->get("parametricVar", "sensitivityInputFile", false);
        QL_REQUIRE(tmp != "", "sensitivityInputFile not provided");
        std::string sensiFile = (inputPath / tmp).generic_string();
        LOG("Get sensitivity data from file " << sensiFile);
        setSensitivityStreamFromFile(sensiFile);
        
        tmp = params_->get("parametricVar", "outputHistoricalScenarios", false);
        if (tmp != "")
            setOutputHistoricalScenarios(parseBool(tmp));
    }

    /********************
     * VaR - Historical Simulation
     ********************/

    tmp = params_->get("historicalSimulationVar", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("HISTSIM_VAR");

        tmp = params_->get("historicalSimulationVar", "historicalScenarioFile", false);
        QL_REQUIRE(tmp != "", "historicalScenarioFile not provided");
        std::string scenarioFile = (inputPath / tmp).generic_string();
        setHistoricalScenarioReader(scenarioFile);

        tmp = params_->get("historicalSimulationVar", "simulationConfigFile", false);
        QL_REQUIRE(tmp != "", "simulationConfigFile not provided");
        string simulationConfigFile = (inputPath / tmp).generic_string();
        setHistVarSimMarketParamsFromFile(simulationConfigFile);

        tmp = params_->get("historicalSimulationVar", "historicalPeriod", false);
        if (tmp != "")
            setBenchmarkVarPeriod(tmp);

        tmp = params_->get("historicalSimulationVar", "mporDays", false);
        if (tmp != "")
            setMporDays(static_cast<Size>(parseInteger(tmp)));

        tmp = params_->get("historicalSimulationVar", "mporCalendar", false);
        if (tmp != "")
            setMporCalendar(tmp);

        tmp = params_->get("historicalSimulationVar", "mporOverlappingPeriods", false);
        if (tmp != "")
            setMporOverlappingPeriods(parseBool(tmp));

        tmp = params_->get("historicalSimulationVar", "quantiles", false);
        if (tmp != "")
            setVarQuantiles(tmp);

        tmp = params_->get("historicalSimulationVar", "breakdown", false);
        if (tmp != "")
            setVarBreakDown(parseBool(tmp));

        tmp = params_->get("historicalSimulationVar", "portfolioFilter", false);
        if (tmp != "")
            setPortfolioFilter(tmp);

        tmp = params_->get("historicalSimulationVar", "outputHistoricalScenarios", false);
        if (tmp != "")
            setOutputHistoricalScenarios(parseBool(tmp));
    }

    /****************
     * PNL Explain
     ****************/
    tmp = params_->get("pnlExplain", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PNL_EXPLAIN");
        
        tmp = params_->get("pnlExplain", "mporDate", false);
        if (tmp != "")
            setMporDate(parseDate(tmp));
        
        tmp = params_->get("pnlExplain", "historicalScenarioFile", false);
        if (tmp != "") {
            std::string scenarioFile = (inputPath / tmp).generic_string();
            setHistoricalScenarioReader(scenarioFile);
        }

        tmp = params_->get("pnlExplain", "simulationConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading sensitivity scenario sim market parameters from file" << file);
            setSensiSimMarketParamsFromFile(file);
            setScenarioSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }
        
        tmp = params_->get("pnlExplain", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setSensiScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }
    }
    /****************
     * SIMM and IM Schedule
     ****************/

    LOG("SIMM");
    tmp = params_->get("simm", "active", false);
    bool doSimm = !tmp.empty() ? parseBool(tmp) : false;
    if (doSimm) {
        insertAnalytic("SIMM");

        tmp = params_->get("simm", "version", false);
        if (tmp != "")
            setSimmVersion(tmp);

        tmp = params_->get("simm", "mporDays", false);
        if (tmp != "")
            setMporDays(static_cast<Size>(parseInteger(tmp)));

        tmp = params_->get("simm", "crif", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            setCrifFromFile(file, csvEolChar(), csvSeparator(), '\"', csvEscapeChar());
        }

        tmp = params_->get("simm", "simmCalibration", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            if (boost::filesystem::exists(file))
                setSimmCalibrationDataFromFile(file);
        }

        tmp = params_->get("simm", "calculationCurrency", false);
        if (tmp != "") {
            setSimmCalculationCurrencyCall(tmp);
            setSimmCalculationCurrencyPost(tmp);
        } else {
            QL_REQUIRE(baseCurrency() != "", "either base currency or calculation currency is required");
        }

        tmp = params_->get("simm", "calculationCurrencyCall", false);
        if (tmp != "") {
            setSimmCalculationCurrencyCall(tmp);
        }

        tmp = params_->get("simm", "calculationCurrencyPost", false);
        if (tmp != "") {
            setSimmCalculationCurrencyPost(tmp);
        }

        tmp = params_->get("simm", "resultCurrency", false);
        if (tmp != "")
            setSimmResultCurrency(tmp);
        else
            setSimmResultCurrency(simmCalculationCurrencyCall());

        tmp = params_->get("simm", "reportingCurrency", false);
        if (tmp != "")
            setSimmReportingCurrency(tmp);

        tmp = params_->get("simm", "enforceIMRegulations", false);
        if (tmp != "")
            setEnforceIMRegulations(parseBool(tmp));

        tmp = params_->get("simm", "writeIntermediateReports", false);
        if (tmp != "")
            setWriteSimmIntermediateReports(parseBool(tmp));
    }

    LOG("IM SCHEDULE");
    tmp = params_->get("imschedule", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("IM_SCHEDULE");

        tmp = params_->get("imschedule", "version", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "version", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "version for imschedule and simm should match");
            setSimmVersion(tmp);
        } else if (simmVersion() == "") {
            LOG("set SIMM version for IM Schedule to 2.6, required to load CRIF")
            setSimmVersion("2.6");
        }

        tmp = params_->get("imschedule", "crif", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "crif", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "crif files for imschedule and simm should match");
            string file = (inputPath / tmp).generic_string();
            setCrifFromFile(file, csvEolChar(), csvSeparator(), '\"', csvEscapeChar());
        }
        
        tmp = params_->get("imschedule", "calculationCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "calculationCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for for imschedule and simm should match");
            setSimmCalculationCurrencyCall(tmp);
            setSimmCalculationCurrencyPost(tmp);
        } else {
            QL_REQUIRE(baseCurrency() != "", "either base currency or calculation currency is required");
        }

        tmp = params_->get("imschedule", "calculationCurrencyCall", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "calculationCurrencyCall", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for imschedule and simm should match");
            setSimmCalculationCurrencyCall(tmp);
        }

        tmp = params_->get("imschedule", "calculationCurrencyPost", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "calculationCurrencyPost", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for imschedule and simm should match");
            setSimmCalculationCurrencyPost(tmp);
        }

        tmp = params_->get("imschedule", "resultCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "resultCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "result currency for imschedule and simm should match");
            setSimmResultCurrency(tmp);
        }
        else
            setSimmResultCurrency(simmCalculationCurrencyCall());

        tmp = params_->get("imschedule", "reportingCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "reportingCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "reporting currency for imschedule and simm should match");
            setSimmReportingCurrency(tmp);
        }

        tmp = params_->get("imschedule", "enforceIMRegulations", false);
        if (tmp != "") {
            string tmpSimm = params_->get("simm", "enforceIMRegulations", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "enforceIMRegulations for imschedule and simm should match");
            setEnforceIMRegulations(parseBool(tmp));
        }
    }

    /************
     * Simulation
     ************/

    tmp = params_->get("simulation", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("EXPOSURE");
    }

    // check this here because we need to know further below when checking for EXPOSURE or XVA analytic
    tmp = params_->get("xva", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA");

    tmp = params_->get("xvaStress", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA_STRESS");

    tmp = params_->get("xvaSensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA_SENSITIVITY");

    tmp = params_->get("simulation", "salvageCorrelationMatrix", false);
    if (tmp != "")
        setSalvageCorrelationMatrix(parseBool(tmp));

    tmp = params_->get("simulation", "amc", false);
    if (tmp != "")
        setAmc(parseBool(tmp));

    tmp = params_->get("simulation", "amcCg", false);
    if (tmp != "")
        setAmcCg(parseBool(tmp));

    tmp = params_->get("simulation", "xvaCgSensitivityConfigFile", false);
    if (tmp != "") {
        string file = (inputPath / tmp).generic_string();
        LOG("Load xva cg sensitivity scenario data from file" << file);
        setXvaCgSensiScenarioDataFromFile(file);
    }

    tmp = params_->get("simulation", "amcTradeTypes", false);
    if (tmp != "")
        setAmcTradeTypes(tmp);

    setSimulationPricingEngine(pricingEngine());
    setExposureObservationModel(observationModel());
    setExposureBaseCurrency(baseCurrency());

    if (analytics().find("EXPOSURE") != analytics().end() || analytics().find("XVA") != analytics().end() ||
        analytics().find("XVA_STRESS") != analytics().end() ||
        analytics().find("XVA_SENSITIVITY") != analytics().end()) {
        tmp = params_->get("simulation", "simulationConfigFile", false);
        if (tmp != "") {
            string simulationConfigFile = (inputPath / tmp).generic_string();
            LOG("Loading simulation config from file" << simulationConfigFile);
            setExposureSimMarketParamsFromFile(simulationConfigFile);
            setCrossAssetModelDataFromFile(simulationConfigFile);
            setScenarioGeneratorDataFromFile(simulationConfigFile);
            auto grid = scenarioGeneratorData()->getGrid();
            DLOG("grid size=" << grid->size() << ", dates=" << grid->dates().size() << ", valuationDates="
                              << grid->valuationDates().size() << ", closeOutDates=" << grid->closeOutDates().size());
        } else {
            ALOG("Simulation market, model and scenario generator data not loaded");
        }

        tmp = params_->get("simulation", "pricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = (inputPath / tmp).generic_string();
            LOG("Load simulation pricing engine data from file: " << pricingEnginesFile);
            setSimulationPricingEngineFromFile(pricingEnginesFile);
        } else {
            WLOG("Simulation pricing engine data not found, using standard pricing engines");
        }

        tmp = params_->get("simulation", "amcPricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = (inputPath / tmp).generic_string();            ;
            LOG("Load amc pricing engine data from file: " << pricingEnginesFile);
            setAmcPricingEngineFromFile(pricingEnginesFile);
        } else {
            WLOG("AMC pricing engine data not found, using standard pricing engines");
            setAmcPricingEngine(pricingEngine());
        }

        setExposureBaseCurrency(baseCurrency());
        tmp = params_->get("simulation", "baseCurrency", false);
        if (tmp != "")
            setExposureBaseCurrency(tmp);

        tmp = params_->get("simulation", "observationModel", false);
        if (tmp != "")
            setExposureObservationModel(tmp);
        else
            setExposureObservationModel(observationModel());

        tmp = params_->get("simulation", "storeFlows", false);
        if (tmp == "Y")
            setStoreFlows(true);

        tmp = params_->get("simulation", "storeCreditStateNPVs", false);
        if (!tmp.empty())
            setStoreCreditStateNPVs(parseInteger(tmp));

        tmp = params_->get("simulation", "storeSurvivalProbabilities", false);
        if (tmp == "Y")
            setStoreSurvivalProbabilities(true);

        tmp = params_->get("simulation", "nettingSetId", false);
        if (tmp != "")
            setNettingSetId(tmp);

        tmp = params_->get("simulation", "cubeFile", false);
        if (tmp != "")
            setWriteCube(true);

        tmp = params_->get("simulation", "scenariodump", false);
        if (tmp != "")
            setWriteScenarios(true);

        tmp = params_->get("simulation", "xvaCgBumpSensis", false);
	if (!tmp.empty())
	    setXvaCgBumpSensis(parseBool(tmp));

        tmp = params_->get("simulation", "xvaCgUseExternalComputeDevice", false);
	if (!tmp.empty())
	    setXvaCgUseExternalComputeDevice(parseBool(tmp));

        tmp = params_->get("simulation", "xvaCgExternalDeviceCompatibilityMode", false);
	if (!tmp.empty())
	    setXvaCgExternalDeviceCompatibilityMode(parseBool(tmp));

        tmp = params_->get("simulation", "xvaCgUseDoublePrecisionForExternalCalculation", false);
	if (!tmp.empty())
	    setXvaCgUseDoublePrecisionForExternalCalculation(parseBool(tmp));

        setXvaCgExternalComputeDevice(params_->get("simulation", "xvaCgExternalComputeDevice", false));

        tmp = params_->get("simulation", "xvaCgBumpSensis", false);
	if (!tmp.empty())
	    setXvaCgBumpSensis(parseBool(tmp));

    }

    /**********************
     * XVA specifically
     **********************/

    tmp = params_->get("xva", "baseCurrency", false);
    if (tmp != "")
        setXvaBaseCurrency(tmp);
    else
        setXvaBaseCurrency(exposureBaseCurrency());

    if (analytics().find("XVA") != analytics().end() && analytics().find("EXPOSURE") == analytics().end()) {
        setLoadCube(true);
        tmp = params_->get("xva", "cubeFile", false);
        if (tmp != "") {
            string cubeFile = (resultsPath() / tmp).generic_string();
            LOG("Load cube from file " << cubeFile);
            setCubeFromFile(cubeFile);
            LOG("Cube loading done: ids=" << cube()->numIds() << " dates=" << cube()->numDates()
                                          << " samples=" << cube()->samples() << " depth=" << cube()->depth());
        } else {
            ALOG("cube file name not provided");
        }
    }

    if (analytics().find("XVA") != analytics().end() || analytics().find("XVA_STRESS") != analytics().end() ||
        analytics().find("XVA_SENSITIVITY") != analytics().end()) {
        tmp = params_->get("xva", "csaFile", false);
        QL_REQUIRE(tmp != "", "Netting set manager is required for XVA");
        string csaFile = (inputPath / tmp).generic_string();
        LOG("Loading netting and csa data from file " << csaFile);
        setNettingSetManagerFromFile(csaFile);

        tmp = params_->get("xva", "collateralBalancesFile", false);
        if (tmp != "") {
            string collBalancesFile = (inputPath / tmp).generic_string();
            LOG("Loading collateral balances from file " << collBalancesFile);
            setCollateralBalancesFromFile(collBalancesFile);
        }
    }

    tmp = params_->get("xva", "nettingSetCubeFile", false);
    if (loadCube() && tmp != "") {
        string cubeFile = (resultsPath() / tmp).generic_string();
        LOG("Load nettingset cube from file " << cubeFile);
        setNettingSetCubeFromFile(cubeFile);
        DLOG("NettingSetCube loading done: ids="
             << nettingSetCube()->numIds() << " dates=" << nettingSetCube()->numDates()
             << " samples=" << nettingSetCube()->samples() << " depth=" << nettingSetCube()->depth());
    }

    tmp = params_->get("xva", "cptyCubeFile", false);
    if (loadCube() && tmp != "") {
        string cubeFile = resultsPath().string() + "/" + tmp;
        LOG("Load cpty cube from file " << cubeFile);
        setCptyCubeFromFile(cubeFile);
        DLOG("CptyCube loading done: ids=" << cptyCube()->numIds() << " dates=" << cptyCube()->numDates()
                                           << " samples=" << cptyCube()->samples() << " depth=" << cptyCube()->depth());
    }

    tmp = params_->get("xva", "scenarioFile", false);
    if (loadCube() && tmp != "") {
        string cubeFile = resultsPath().string() + "/" + tmp;
        LOG("Load agg scen data from file " << cubeFile);
        setMarketCubeFromFile(cubeFile);
        LOG("MktCube loading done");
    }

    tmp = params_->get("xva", "flipViewXVA", false);
    if (tmp != "")
        setFlipViewXVA(parseBool(tmp));

    tmp = params_->get("xva", "mporCashFlowMode", false);
    if (tmp != "")
        setMporCashFlowMode(parseMporCashFlowMode(tmp));

    tmp = params_->get("xva", "fullInitialCollateralisation", false);
    if (tmp != "")
        setFullInitialCollateralisation(parseBool(tmp));

    tmp = params_->get("xva", "exposureProfilesByTrade", false);
    if (tmp != "")
        setExposureProfilesByTrade(parseBool(tmp));

    tmp = params_->get("xva", "exposureProfiles", false);
    if (tmp != "")
        setExposureProfiles(parseBool(tmp));

    tmp = params_->get("xva", "quantile", false);
    if (tmp != "")
        setPfeQuantile(parseReal(tmp));

    tmp = params_->get("xva", "calculationType", false);
    if (tmp != "")
        setCollateralCalculationType(tmp);

    tmp = params_->get("xva", "allocationMethod", false);
    if (tmp != "")
        setExposureAllocationMethod(tmp);

    tmp = params_->get("xva", "marginalAllocationLimit", false);
    if (tmp != "")
        setMarginalAllocationLimit(parseReal(tmp));

    tmp = params_->get("xva", "exerciseNextBreak", false);
    if (tmp != "")
        setExerciseNextBreak(parseBool(tmp));

    tmp = params_->get("xva", "cva", false);
    if (tmp != "")
        setCvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dva", false);
    if (tmp != "")
        setDvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "fva", false);
    if (tmp != "")
        setFvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "colva", false);
    if (tmp != "")
        setColvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "collateralFloor", false);
    if (tmp != "")
        setCollateralFloorAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dim", false);
    if (tmp != "")
        setDimAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dimModel", false);
    if (tmp != "") {
        QL_REQUIRE(tmp == "Regression" || tmp == "Flat",
                   "DIM model " << tmp << " not supported, expected Regression or Flat");
        setDimModel(tmp);
    }
    
    tmp = params_->get("xva", "mva", false);
    if (tmp != "")
        setMvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "kva", false);
    if (tmp != "")
        setKvaAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "dynamicCredit", false);
    if (tmp != "")
        setDynamicCredit(parseBool(tmp));

    tmp = params_->get("xva", "cvaSensi", false);
    if (tmp != "")
        setCvaSensi(parseBool(tmp));

    tmp = params_->get("xva", "cvaSensiGrid", false);
    if (tmp != "")
        setCvaSensiGrid(tmp);

    tmp = params_->get("xva", "cvaSensiShiftSize", false);
    if (tmp != "")
        setCvaSensiShiftSize(parseReal(tmp));

    tmp = params_->get("xva", "dvaName", false);
    if (tmp != "")
        setDvaName(tmp);

    tmp = params_->get("xva", "rawCubeOutputFile", false);
    if (tmp != "") {
        setRawCubeOutputFile(tmp);
        setRawCubeOutput(true);
    }

    tmp = params_->get("xva", "netCubeOutputFile", false);
    if (tmp != "") {
        setNetCubeOutputFile(tmp);
        setNetCubeOutput(true);
    }

    // FVA

    tmp = params_->get("xva", "fvaBorrowingCurve", false);
    if (tmp != "")
        setFvaBorrowingCurve(tmp);

    tmp = params_->get("xva", "fvaLendingCurve", false);
    if (tmp != "")
        setFvaLendingCurve(tmp);

    tmp = params_->get("xva", "flipViewBorrowingCurvePostfix", false);
    if (tmp != "")
        setFlipViewBorrowingCurvePostfix(tmp);

    tmp = params_->get("xva", "flipViewLendingCurvePostfix", false);
    if (tmp != "")
        setFlipViewLendingCurvePostfix(tmp);

    // DIM

    tmp = params_->get("xva", "deterministicInitialMarginFile", false);
    if (tmp != "") {
        string imFile = (inputPath / tmp).generic_string();
        LOG("Load initial margin evolution from file " << tmp);
        setDeterministicInitialMarginFromFile(imFile);
    }

    tmp = params_->get("xva", "dimQuantile", false);
    if (tmp != "")
        setDimQuantile(parseReal(tmp));

    tmp = params_->get("xva", "dimHorizonCalendarDays", false);
    if (tmp != "")
        setDimHorizonCalendarDays(parseInteger(tmp));

    tmp = params_->get("xva", "dimRegressionOrder", false);
    if (tmp != "")
        setDimRegressionOrder(parseInteger(tmp));

    tmp = params_->get("xva", "dimRegressors", false);
    if (tmp != "")
        setDimRegressors(tmp);

    tmp = params_->get("xva", "dimOutputGridPoints", false);
    if (tmp != "")
        setDimOutputGridPoints(tmp);

    tmp = params_->get("xva", "dimOutputNettingSet", false);
    if (tmp != "")
        setDimOutputNettingSet(tmp);

    tmp = params_->get("xva", "dimLocalRegressionEvaluations", false);
    if (tmp != "")
        setDimLocalRegressionEvaluations(parseInteger(tmp));

    tmp = params_->get("xva", "dimLocalRegressionBandwidth", false);
    if (tmp != "")
        setDimLocalRegressionBandwidth(parseReal(tmp));

    // KVA

    tmp = params_->get("xva", "kvaCapitalDiscountRate", false);
    if (tmp != "")
        setKvaCapitalDiscountRate(parseReal(tmp));

    tmp = params_->get("xva", "kvaAlpha", false);
    if (tmp != "")
        setKvaAlpha(parseReal(tmp));

    tmp = params_->get("xva", "kvaRegAdjustment", false);
    if (tmp != "")
        setKvaRegAdjustment(parseReal(tmp));

    tmp = params_->get("xva", "kvaCapitalHurdle", false);
    if (tmp != "")
        setKvaCapitalHurdle(parseReal(tmp));

    tmp = params_->get("xva", "kvaOurPdFloor", false);
    if (tmp != "")
        setKvaOurPdFloor(parseReal(tmp));

    tmp = params_->get("xva", "kvaTheirPdFloor", false);
    if (tmp != "")
        setKvaTheirPdFloor(parseReal(tmp));

    tmp = params_->get("xva", "kvaOurCvaRiskWeight", false);
    if (tmp != "")
        setKvaOurCvaRiskWeight(parseReal(tmp));

    tmp = params_->get("xva", "kvaTheirCvaRiskWeight", false);
    if (tmp != "")
        setKvaTheirCvaRiskWeight(parseReal(tmp));

    // credit simulation

    tmp = params_->get("xva", "creditMigration", false);
    if (tmp != "")
        setCreditMigrationAnalytic(parseBool(tmp));

    tmp = params_->get("xva", "creditMigrationDistributionGrid", false);
    if (tmp != "")
        setCreditMigrationDistributionGrid(parseListOfValues<Real>(tmp, &parseReal));

    tmp = params_->get("xva", "creditMigrationTimeSteps", false);
    if (tmp != "")
        setCreditMigrationTimeSteps(parseListOfValues<Size>(tmp, &parseInteger));

    tmp = params_->get("xva", "creditMigrationConfig", false);
    if (tmp != "") {
        string file = (inputPath / tmp).generic_string();
        LOG("Loading credit migration config from file" << file);
        setCreditSimulationParametersFromFile(file);
    }

    tmp = params_->get("xva", "creditMigrationOutputFiles", false);
    if (tmp != "")
        setCreditMigrationOutputFiles(tmp);

    /*************
     * XVA Stress
     *************/

    if (analytics().find("XVA_STRESS") != analytics().end()) {
        tmp = params_->get("xvaStress", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading xva stress test scenario sim market parameters from file" << file);
            setXvaStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for xva stress testing not loaded");
        }

        tmp = params_->get("xvaStress", "stressConfigFile", false);
        if (!tmp.empty()) {
            string file = (inputPath / tmp).generic_string();
            LOG("Load xav stress test scenario data from file" << file);
            setXvaStressScenarioDataFromFile(file);
        } else {
            WLOG("Xva Stress scenario data not loaded");
        }

        tmp = params_->get("xvaStress", "writeCubes", false);
        if (!tmp.empty()) {
            bool writeCubes = false;
            bool success = tryParse<bool>(tmp, writeCubes, parseBool);
            if (success) {
                setXvaStressWriteCubes(writeCubes);
            }
        }

        tmp = params_->get("xvaStress", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setXvaStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }
    }

    /*************
     * XVA Sensi
     *************/

    if (analytics().find("XVA_SENSITIVITY") != analytics().end()) {
        tmp = params_->get("xvaSensitivity", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading xva sensitivity scenario sim market parameters from file" << file);
            setXvaSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for xva sensitivity not loaded");
        }

        tmp = params_->get("xvaSensitivity", "sensitivityConfigFile", false);
        if (!tmp.empty()) {
            string file = (inputPath / tmp).generic_string();
            LOG("Load xva sensitivity scenario data from file" << file);
            setXvaSensiScenarioDataFromFile(file);
        } else {
            WLOG("Xva sensitivity scenario data not loaded");
        }
    }

    /*************
     * cashflow npv and dynamic backtesting
     *************/

    tmp = params_->get("cashflow", "cashFlowHorizon", false);
    if (tmp != "")
        setCashflowHorizon(tmp);

    tmp = params_->get("cashflow", "portfolioFilterDate", false);
    if (tmp != "")
        setPortfolioFilterDate(tmp);

    /*************
     * ZERO TO PAR SENSI CONVERSION
     *************/

    tmp = params_->get("zeroToParSensiConversion", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PARCONVERSION");

        tmp = params_->get("zeroToParSensiConversion", "sensitivityInputFile", false);
        if (tmp != "") {
            setParConversionInputFile((inputPath / tmp).generic_string());
        }

        tmp = params_->get("zeroToParSensiConversion", "idColumn", false);
        if (tmp != "") {
            setParConversionInputIdColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "riskFactorColumn", false);
        if (tmp != "") {
            setParConversionInputRiskFactorColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "deltaColumn", false);
        if (tmp != "") {
            setParConversionInputDeltaColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "currencyColumn", false);
        if (tmp != "") {
            setParConversionInputCurrencyColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "baseNpvColumn", false);
        if (tmp != "") {
            setParConversionInputBaseNpvColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "shiftSizeColumn", false);
        if (tmp != "") {
            setParConversionInputShiftSizeColumn(tmp);
        }

        tmp = params_->get("zeroToParSensiConversion", "marketConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Loading par converions scenario sim market parameters from file" << file);
            setParConversionSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for par conversion testing not loaded");
        }

        tmp = params_->get("zeroToParSensiConversion", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load par conversion scenario data from file" << file);
            setParConversionScenarioDataFromFile(file);
        } else {
            WLOG("Par conversion scenario data not loaded");
        }

        tmp = params_->get("zeroToParSensiConversion", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (inputPath / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setParConversionPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for par conversion, using global");
        }

        tmp = params_->get("zeroToParSensiConversion", "outputThreshold", false);
        if (tmp != "")
            setParConversionThreshold(parseReal(tmp));

        tmp = params_->get("zeroToParSensiConversion", "outputJacobi", false);
        if (tmp != "")
            setParConversionOutputJacobi(parseBool(tmp));
    }

    /**********************
     * Scenario_Statistics
     **********************/

    tmp = params_->get("scenarioStatistics", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("SCENARIO_STATISTICS");
        tmp = params_->get("scenarioStatistics", "distributionBuckets", false);
        if (tmp != "")
            setScenarioDistributionSteps(parseInteger(tmp));

        tmp = params_->get("scenarioStatistics", "outputZeroRate", false);
        if (tmp != "")
            setScenarioOutputZeroRate(parseBool(tmp));

        tmp = params_->get("scenarioStatistics", "simulationConfigFile", false);
        if (tmp != "") {
            string simulationConfigFile = (inputPath / tmp).generic_string();
            LOG("Loading simulation config from file" << simulationConfigFile);
            setExposureSimMarketParamsFromFile(simulationConfigFile);
            setCrossAssetModelDataFromFile(simulationConfigFile);
            setScenarioGeneratorDataFromFile(simulationConfigFile);
            auto grid = scenarioGeneratorData()->getGrid();
            DLOG("grid size=" << grid->size() << ", dates=" << grid->dates().size() << ", valuationDates="
                              << grid->valuationDates().size() << ", closeOutDates=" << grid->closeOutDates().size());
        } else {
            ALOG("Simulation market, model and scenario generator data not loaded");
        }

        tmp = params_->get("scenarioStatistics", "scenariodump", false);
        if (tmp != "")
            setWriteScenarios(true);
    }

    if (analytics().size() == 0) {
        insertAnalytic("MARKETDATA");
        setOutputTodaysMarketCalibration(true);
        if (lazyMarketBuilding())
            LOG("Lazy market build being overridden to \"false\" for MARKETDATA analytic.")
        setLazyMarketBuilding(false);
    }

    LOG("analytics: " << analytics().size());
    for (auto a: analytics())
        LOG("analytic: " << a);

    LOG("buildInputParameters done");
}

} // namespace analytics
} // namespace ore
