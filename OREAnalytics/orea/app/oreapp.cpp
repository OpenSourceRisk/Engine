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
#include <orea/app/marketdatabinaryloader.hpp>
#include <orea/app/marketdatacsvloader.hpp>
#include <orea/app/marketdatainmemoryloader.hpp>
#include <orea/app/oreapp.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>

#include <ored/configuration/currencyconfig.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>

#include <qle/version.hpp>
#include <qle/gitversion.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>

#include <boost/algorithm/string.hpp>
#include <filesystem>
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
                return b.second->cube();
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

std::vector<std::string> OREApp::getErrors() { return errorMessages_; }

Real OREApp::getRunTime() {
    boost::chrono::duration<double> seconds = boost::chrono::nanoseconds(runTimer_.elapsed().wall);
    return seconds.count();
}

QuantLib::ext::shared_ptr<BufferLogger> OREApp::getLogger(const std::string& name) {
    QuantLib::ext::shared_ptr<Logger> log = Log::instance().logger(name);
    QuantLib::ext::shared_ptr<BufferLogger> bufferLog = QuantLib::ext::dynamic_pointer_cast<BufferLogger>(log);
    if (bufferLog)
        return bufferLog;
    else
        QL_FAIL("No Buffer Logger found.");
}

std::vector<std::string>& OREApp::getProgressLog() {
    QuantLib::ext::shared_ptr<IndependentLogger> log = Log::instance().independentLogger("ProgressLogger");
    return log->messages();
}

QuantLib::ext::shared_ptr<CSVLoader> OREApp::buildCsvLoader(const QuantLib::ext::shared_ptr<Parameters>& params) {
    bool implyTodaysFixings = false;
    vector<string> marketFiles = {};
    vector<string> fixingFiles = {};
    vector<string> dividendFiles = {};

    filesystem::path inputPath = params_->getString("setup", "inputPath");

    std::string tmp = params_->getString("setup", "implyTodaysFixings", false);
    if (tmp != "")
        implyTodaysFixings = ore::data::parseBool(tmp);

    tmp = params->getString("setup", "marketDataFile", false);
    if (tmp != "")
        marketFiles = getFileNames(tmp, inputPath);
    else {
        ALOG("market data file not found");
    }

    tmp = params->getString("setup", "fixingDataFile", false);
    if (tmp != "")
        fixingFiles = getFileNames(tmp, inputPath);
    else {
        ALOG("fixing data file not found");
    }

    tmp = params->getString("setup", "dividendDataFile", false);
    if (tmp != "")
        dividendFiles = getFileNames(tmp, inputPath);
    else {
        WLOG("dividend data file not found");
    }

    tmp = params->getString("setup", "fixingCutoff", false);
    Date cutoff = Date();
    if (tmp != "")
        cutoff = parseDate(tmp);
    else {
        WLOG("fixing cutoff date not set");
    }
    
    auto loader = QuantLib::ext::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings, cutoff);

    return loader;
}

void OREApp::analytics() {

    try {
        LOG("ORE analytics starting");
        MEM_LOG_USING_LEVEL(ORE_WARNING, "Starting OREApp::analytics()");

        QL_REQUIRE(params_, "ORE input parameters not set");

        Settings::instance().evaluationDate() = inputs_->asof();

        if (inputs_->pricingEngine()) {
            GlobalPseudoCurrencyMarketParameters::instance().set(inputs_->pricingEngine()->globalParameters());
        }

        // Initialize the global conventions
        InstrumentConventions::instance().setConventions(inputs_->conventions());

        // Create a market data loader that reads market data, fixings, dividends from csv files
        QuantLib::ext::shared_ptr<MarketDataLoader> loader;
        if (!inputs_->marketDataLoaderInput().empty()) {
            loader = QuantLib::ext::make_shared<MarketDataBinaryLoader>(inputs_, inputs_->marketDataLoaderInput());
        } else {
            auto csvLoader = buildCsvLoader(params_);
            loader = QuantLib::ext::make_shared<MarketDataCsvLoader>(inputs_, csvLoader);
        }
        // Create the analytics manager
        analyticsManager_ = QuantLib::ext::make_shared<AnalyticsManager>(inputs_, loader);
        analyticsManager_->initialise();
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        std::vector<QuantLib::ext::shared_ptr<MarketCalibrationReportBase>> mcr;
        if (inputs_->outputTodaysMarketCalibration()) {
            mcr.push_back(QuantLib::ext::make_shared<MarketCalibrationReport>(
                string(), QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize()),
                QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize()),
                inputs_->todaysMarketCalibrationPrecision()));
        }

        // Run the requested analytics
        analyticsManager_->runAnalytics(mcr);

        CONSOLEW("Writing reports...");

        // Write reports to files in the results path
        Analytic::analytic_reports reports = analyticsManager_->reports();
        analyticsManager_->toFile(reports, inputs_->resultsPath().string(), outputs_->fileNameMap(),
                                  inputs_->csvSeparator(), inputs_->csvCommentCharacter(), inputs_->csvQuoteChar(),
                                  inputs_->reportNaString());

        CONSOLE("OK");
        CONSOLEW("Writing cubes...");

        // Write npv cube(s)
        for (auto a : analyticsManager_->npvCubes()) {
            for (auto b : a.second) {
                LOG("write npv cube " << b.first);
                string reportName = b.first;
                std::string fileName =
                    inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "csv.gz");
                LOG("write npv cube " << reportName << " to file " << fileName);
                saveCube(fileName, *b.second);
            }
        }

        // Write market cube(s)
        for (auto a : analyticsManager_->mktCubes()) {
            for (auto b : a.second) {
                string reportName = b.first;
                std::string fileName =
                    inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "csv.gz");
                LOG("write market cube " << reportName << " to file " << fileName);
                saveAggregationScenarioData(fileName, *b.second);
            }
        }

        for (auto a : analyticsManager_->stressTests()) {
            for (auto b : a.second) {
                string reportName = b.first;
                std::string fileName =
                    inputs_->resultsPath().string() + "/" + outputs_->outputFileName(reportName, "xml");
                LOG("write converted stress test scenario definition " << reportName << " to file " << fileName);
                b.second->toFile(fileName);
            }
        }
        
        if (analyticsManager_->failedAnalytics().size() > 0)
            QL_FAIL("Failed to run analytics " + boost::algorithm::join(analyticsManager_->failedAnalytics(), ","));

        CONSOLE("OK");
    } catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        ALOG(oss.str());
        MEM_LOG_USING_LEVEL(ORE_WARNING, "Finishing OREApp::analytics()");
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
    }

    MEM_LOG_USING_LEVEL(ORE_WARNING, "Finishing OREApp::analytics()");
    LOG("ORE analytics done");
}

void OREApp::initFromParams() {
    if (console_) {
        ConsoleLog::instance().switchOn();
    }

    outputPath_ = params_->getString("setup", "outputPath");
    logFile_ = outputPath_ + "/" + params_->getString("setup", "logFile");
    logMask_ = 15;
    // Get log mask if available
    if (params_->has("setup", "logMask")) {
        logMask_ = static_cast<Size>(parseInteger(params_->getString("setup", "logMask")));
    }

    progressLogRotationSize_ = 0;
    progressLogToConsole_ = false;
    structuredLogRotationSize_ = 0;

    if (params_->hasGroup("logging")) {
        string tmp = params_->getString("logging", "logFile", false);
        if (!tmp.empty()) {
            logFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->getString("logging", "logMask", false);
        if (!tmp.empty()) {
            logMask_ = static_cast<Size>(parseInteger(tmp));
        }
        tmp = params_->getString("logging", "progressLogFile", false);
        if (!tmp.empty()) {
            progressLogFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->getString("logging", "progressLogRotationSize", false);
        if (!tmp.empty()) {
            progressLogRotationSize_ = static_cast<Size>(parseInteger(tmp));
        }
        tmp = params_->getString("logging", "progressLogToConsole", false);
        if (!tmp.empty()) {
            progressLogToConsole_ = ore::data::parseBool(tmp);
        }
        tmp = params_->getString("logging", "structuredLogFile", false);
        if (!tmp.empty()) {
            structuredLogFile_ = outputPath_ + '/' + tmp;
        }
        tmp = params_->getString("logging", "structuredLogRotationSize", false);
        if (!tmp.empty()) {
            structuredLogRotationSize_ = static_cast<Size>(parseInteger(tmp));
        }
    }

    setupLog(logMask_, outputPath_, logFile_, logRootPath_, progressLogFile_, progressLogRotationSize_,
             progressLogToConsole_, structuredLogFile_, structuredLogRotationSize_);

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
    if (inputs_->mporConventions() && inputs_->mporDate() != Date())
        InstrumentConventions::instance().setConventions(inputs_->mporConventions(), inputs_->mporDate());
    if (inputs_->scriptLibraryData())
        ScriptLibraryStorage::instance().set(*inputs_->scriptLibraryData());
    
    if (inputs_->currencyConfigs() != nullptr)
        inputs_->currencyConfigs()->addCurrencies();
    if (inputs_->calendarAdjustmentConfigs() != nullptr)
        inputs_->calendarAdjustmentConfigs()->addCalendars();

    if (console_) {
        ConsoleLog::instance().switchOn();
    }

    outputPath_ = inputs_->resultsPath().string();
    if (clearLog_)
        setupLog(logMask_, outputPath_, logFile_, logRootPath_, progressLogFile_, progressLogRotationSize_,
                 progressLogToConsole_, structuredLogFile_, structuredLogRotationSize_);
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
        CleanUpLogSingleton cleanupLogSingleton(clearLog_, true);
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

    ext::optional<bool> inc = Settings::instance().includeTodaysCashFlows();
    LOG("Global IncludeTodaysCashFlows is set " << (inc ? "true" : "false")
                                                << ", value: " << (inc ? (*inc ? "true" : "false") : "na"));

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

void OREApp::run(const std::vector<std::string>& marketData, const std::vector<std::string>& fixingData) {
    QL_REQUIRE(inputs_, "No InputParameters set");
    // Create a market data loader that takes input from the provided vectors
    auto loader = QuantLib::ext::make_shared<MarketDataInMemoryLoader>(inputs_, marketData, fixingData);
    run(loader);
}

void OREApp::run(const QuantLib::ext::shared_ptr<MarketDataLoader> loader) {

    // Only one thread at a time should call run
    static std::mutex _s_mutex;
    std::lock_guard<std::mutex> lock(_s_mutex);

    // Clean start, but leave Singletons intact after run is completed
    {
        CleanUpThreadLocalSingletons cleanupThreadLocalSingletons;
        CleanUpThreadGlobalSingletons cleanupThreadGloablSingletons;
        CleanUpLogSingleton cleanupLogSingleton(clearLog_, true);
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
        MEM_LOG_USING_LEVEL(ORE_WARNING, "Starting OREApp::run()")

        QL_REQUIRE(inputs_, "ORE input parameters not set");

        // Set global evaluation date, though already set in the OREAppInputParameters c'tor
        Settings::instance().evaluationDate() = inputs_->asof();

        // FIXME
        // QL_REQUIRE(inputs_->pricingEngine(), "pricingEngine not set");
        if (inputs_->pricingEngine()) {
        GlobalPseudoCurrencyMarketParameters::instance().set(inputs_->pricingEngine()->globalParameters());
        }

        // Initialize the global conventions
        QL_REQUIRE(inputs_->conventions(), "conventions not set");
        InstrumentConventions::instance().setConventions(inputs_->conventions());

        // Create the analytics manager
        analyticsManager_ = QuantLib::ext::make_shared<AnalyticsManager>(inputs_, loader);
        analyticsManager_->initialise();
        LOG("Available analytics: " << to_string(analyticsManager_->validAnalytics()));
        CONSOLEW("Requested analytics:");
        CONSOLE(to_string(inputs_->analytics()));
        LOG("Requested analytics: " << to_string(inputs_->analytics()));

        std::vector<QuantLib::ext::shared_ptr<MarketCalibrationReportBase>> mcr;
        if (inputs_->outputTodaysMarketCalibration()) {
            mcr.push_back(QuantLib::ext::make_shared<MarketCalibrationReport>(
                string(), QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize()),
                QuantLib::ext::make_shared<ore::data::InMemoryReport>(inputs_->reportBufferSize()),
                inputs_->todaysMarketCalibrationPrecision()));
        }

        // Run the requested analytics
        analyticsManager_->runAnalytics(mcr);

        MEM_LOG_USING_LEVEL(ORE_WARNING, "Finishing OREApp::run()");
        // Leave any report writing to the calling aplication
    } catch (std::exception& e) {
        ostringstream oss;
        oss << "Error in ORE analytics: " << e.what();
        StructuredAnalyticsWarningMessage("OREApp::run()", oss.str(), e.what()).log();
        MEM_LOG_USING_LEVEL(ORE_WARNING, "Finishing OREApp::run()");
        CONSOLE(oss.str());
        QL_FAIL(oss.str());
        return;
    }

    runTimer_.stop();

    LOG("ORE analytics done");
}

void OREApp::setupLog(Size mask, const std::string& path, const std::string& file,
                      const std::filesystem::path& logRootPath, const std::string& progressLogFile,
                      Size progressLogRotationSize, bool progressLogToConsole, const std::string& structuredLogFile,
                      Size structuredLogRotationSize) {
    closeLog();

    if (file == "" && path == "") {
        Log::instance().registerLogger(QuantLib::ext::make_shared<BufferLogger>(mask));
        Log::instance().switchOn();
        auto progressLogger = QuantLib::ext::make_shared<ProgressLogger>(progressLogToConsole);
        Log::instance().registerIndependentLogger(progressLogger);
        structuredLogger_ = QuantLib::ext::make_shared<StructuredLogger>();
        Log::instance().registerIndependentLogger(structuredLogger_);

        return;
    }

    std::filesystem::path p{path};
    if (!std::filesystem::exists(p)) {
        std::filesystem::create_directories(p);
    }
    QL_REQUIRE(std::filesystem::is_directory(p), "output path '" << path << "' is not a directory.");

    Log::instance().registerLogger(QuantLib::ext::make_shared<FileLogger>(file));
    std::filesystem::path oreRootPath =
        logRootPath.empty() ? std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
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

std::string OREApp::gitHash() { 
    std::string hashStr;
#ifdef GIT_HASH
    hashStr = std::string(GIT_HASH);
#endif
    return hashStr;
}

std::string OREAppInputParameters::loadParameterString(const std::string& analytic, const std::string& param, bool mandatory) {
    return params_->getString(analytic, param, mandatory);
}

std::vector<std::string> OREAppInputParameters::loadParameterXMLString(const std::string& rawString) {
    if (rawString.empty())
        return {rawString};
    vector<string> fileNames;
    boost::split(fileNames, rawString, boost::is_any_of(",;"), boost::token_compress_on);
    for (auto it = fileNames.begin(); it < fileNames.end(); it++) {
        boost::trim(*it);
        *it = (setupVariables_.inputPath_ / *it).generic_string();
    }
    std::vector<std::string> result;
    for (auto file : fileNames) {
        XMLDocument doc(file);
        result.push_back(doc.toString());
    }
    return result;
}

void OREAppInputParameters::loadParameters() {
    LOG("load OREAppInputParameters called");

    // switch default for backward compatibility
    setEntireMarket(true);
    setAllFixings(true);
    setEomInflationFixings(false);
    setBuildFailedTrades(false);

    QL_REQUIRE(params_->hasGroup("setup"), "parameter group 'setup' missing");

    if (params_->hasGroup("markets")) {
        setMarketConfigs(params_->markets());
        for (auto m : marketConfigs())
            LOG("MarketContext::" << m.first << " = " << m.second);
    }

    InputParameters::loadParameters();

    std::string tmp;

    /*************
     * NPV
     *************/
    tmp = params_->getString("npv", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("NPV");

    /*************
     * CASHFLOW
     *************/
    tmp = params_->getString("cashflow", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("CASHFLOW");

    tmp = params_->getString("curves", "outputTodaysMarketCalibration", false);
    if (tmp != "")
        setOutputTodaysMarketCalibration(parseBool(tmp));

    tmp = params_->getString("curves", "todaysMarketCalibrationPrecision", false);
    if (tmp != "")
        setTodaysMarketCalibrationPrecision(parseInteger(tmp));

    /*************
     * SENSITIVITY
     *************/

    // FIXME: The following are not loaded from params so far, relying on defaults
    // xbsParConversion_ = false;
    // analyticFxSensis_ = true;
    // useSensiSpreadedTermStructures_ = true;

    tmp = params_->getString("sensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("SENSITIVITY");

        tmp = params_->getString("sensitivity", "parSensitivity", false);
        if (tmp != "")
            setParSensi(parseBool(tmp));

        tmp = params_->getString("sensitivity", "optimiseRiskFactors", false);
        if (tmp != "")
            setOptimiseRiskFactors(parseBool(tmp));

        tmp = params_->getString("sensitivity", "outputJacobi", false);
        if (tmp != "")
            setOutputJacobi(parseBool(tmp));

        tmp = params_->getString("sensitivity", "alignPillars", false);
        if (tmp != "")
            setAlignPillars(parseBool(tmp));
        else
            setAlignPillars(parSensi());

        tmp = params_->getString("sensitivity", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading sensitivity scenario sim market parameters from file" << file);
            setSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }

        tmp = params_->getString("sensitivity", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setSensiScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }

        tmp = params_->getString("sensitivity", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setSensiPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for sensitivity analysis, using global");
            // FIXME
            setSensiPricingEngine(pricingEngine());
        }

        tmp = params_->getString("sensitivity", "outputSensitivityThreshold", false);
        if (tmp != "")
            setSensiThreshold(parseReal(tmp));

        tmp = params_->getString("sensitivity", "recalibrateModels", false);
        if (tmp != "")
            setSensiRecalibrateModels(parseBool(tmp));

        tmp = params_->getString("sensitivity", "laxFxConversion", false);
        if (tmp != "")
            setSensiLaxFxConversion(parseBool(tmp));

        tmp = params_->getString("sensitivity", "decomposeIndexSensitivities", false);
        if (tmp != "")
            setSensiDecomposition(parseBool(tmp));

        tmp = params_->getString("sensitivity", "outputPrecision", false);
        if (tmp != "")
            setSensiOutputPrecision(parseInteger(tmp));
    }

    /************
     * SCENARIO
     ************/

    tmp = params_->getString("scenario", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("SCENARIO");

        tmp = params_->getString("scenario", "simulationConfigFile", false);
        if (tmp != "") {
            string simulationConfigFile = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading scenario simulation config from file" << simulationConfigFile);
            setScenarioSimMarketParamsFromFile(simulationConfigFile);
        } else {
            ALOG("Scenario Simulation market data not loaded");
        }

        tmp = params_->getString("scenario", "scenarioOutputFile", false);
        if (tmp != "")
            setScenarioOutputFile(tmp);
    }

    /****************
     * STRESS
     ****************/

    tmp = params_->getString("stress", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("STRESS");
        setStressPricingEngine(pricingEngine());
        tmp = params_->getString("stress", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading stress test scenario sim market parameters from file" << file);
            setStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for stress testing not loaded");
        }

        tmp = params_->getString("stress", "scenarioFile", false);
        if (tmp != "") {
            std::string scenarioFile = (setupVariables_.inputPath_ / tmp).generic_string();
            setScenarioReader(scenarioFile);
        }

        tmp = params_->getString("stress", "stressConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load stress test scenario data from file" << file);
            setStressScenarioDataFromFile(file);
        } else {
            WLOG("Stress scenario data not loaded");
        }

        tmp = params_->getString("stress", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setStressPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }

        tmp = params_->getString("stress", "outputThreshold", false);
        if (tmp != "")
            setStressThreshold(parseReal(tmp));

        tmp = params_->getString("stress", "optimiseRiskFactors", false);
        if (tmp != "")
            setStressOptimiseRiskFactors(parseBool(tmp));

        tmp = params_->getString("stress", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }

        tmp = params_->getString("stress", "lowerBoundCapVols", false);
        if (tmp != "") {
            setStressLowerBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->getString("stress", "upperBoundCapVols", false);
        if (tmp != "") {
            setStressUpperBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->getString("stress", "lowerBoundDiscountFactors", false);
        if (tmp != "") {
            setStressLowerBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->getString("stress", "upperBoundDiscountFactors", false);
        if (tmp != "") {
            setStressUpperBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->getString("stress", "lowerBoundSurvivalProb", false);
        if (tmp != "") {
            setStressLowerBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->getString("stress", "upperBoundSurvivalProb", false);
        if (tmp != "") {
            setStressUpperBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->getString("stress", "accuracy", false);
        if (tmp != "") {
            setStressAccurary(parseReal(tmp));
        }
        tmp = params_->getString("stress", "precision", false);
        if (tmp != "") {
            setStressPrecision((Size)parseReal(tmp));
        }
        tmp = params_->getString("stress", "generateCashflows", false);
        if (tmp != "") {
            setStressGenerateCashflows(parseBool(tmp));
        }
    }

    /****************
     * PAR STRESS CONVERSION
     ****************/

    tmp = params_->getString("parStressConversion", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PARSTRESSCONVERSION");
        setParStressPricingEngine(pricingEngine());
        tmp = params_->getString("parStressConversion", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading parStressConversion test scenario sim market parameters from file" << file);
            setParStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for par stress conversion testing not loaded");
        }

        tmp = params_->getString("parStressConversion", "stressConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load stress test scenario data from file" << file);
            setParStressScenarioDataFromFile(file);
        } else {
            WLOG("Stress scenario data not loaded");
        }

        tmp = params_->getString("parStressConversion", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setParStressPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }

        tmp = params_->getString("parStressConversion", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setParStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }

        tmp = params_->getString("parStressConversion", "lowerBoundCapVols", false);
        if (tmp != "") {
            setParStressLowerBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "upperBoundCapVols", false);
        if (tmp != "") {
            setParStressUpperBoundCapFloorVolatility(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "lowerBoundDiscountFactors", false);
        if (tmp != "") {
            setParStressLowerBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "upperBoundDiscountFactors", false);
        if (tmp != "") {
            setParStressUpperBoundRatesDiscountFactor(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "lowerBoundSurvivalProb", false);
        if (tmp != "") {
            setParStressLowerBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "upperBoundSurvivalProb", false);
        if (tmp != "") {
            setParStressUpperBoundSurvivalProb(parseReal(tmp));
        }
        tmp = params_->getString("parStressConversion", "accuracy", false);
        if (tmp != "") {
            setParStressAccurary(parseReal(tmp));
        }
    }

    /****************
     * ZERO TO PAR SHIFT CONVERSION
     *****************/

    tmp = params_->getString("zeroToParShift", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("ZEROTOPARSHIFT");
        setZeroToParShiftPricingEngine(pricingEngine());
        tmp = params_->getString("zeroToParShift", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading zero to par shift conversion scenario sim market parameters from file " << file);
            setZeroToParShiftSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for zero to par shift conversion not loaded");
        }

        tmp = params_->getString("zeroToParShift", "stressConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load zero to par shift conversion scenario data from file" << file);
            setZeroToParShiftScenarioDataFromFile(file);
        } else {
            WLOG("Zero to par shift conversion scenario data not loaded");
        }

        tmp = params_->getString("zeroToParShift", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setZeroToParShiftPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for Zero to par shift conversion, using global");
        }

        tmp = params_->getString("zeroToParShift", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setZeroToParShiftSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded for zero to par shift conversion");
        }
    }

    /********************
     * VaR - Parametric
     ********************/

    tmp = params_->getString("parametricVar", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("PARAMETRIC_VAR");

    /********************
     * VaR - Historical Simulation
     ********************/

    tmp = params_->getString("historicalSimulationVar", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("HISTSIM_VAR");

    /*************
     * P&L
     *************/

    tmp = params_->getString("pnl", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("PNL");

    /****************
     * PNL Explain
     ****************/
    tmp = params_->getString("pnlExplain", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("PNL_EXPLAIN");

    /****************
     * SIMM and IM Schedule
     ****************/

    LOG("SIMM");
    tmp = params_->getString("simm", "active", false);
    bool doSimm = !tmp.empty() ? parseBool(tmp) : false;
    if (doSimm) {
        insertAnalytic("SIMM");

        tmp = params_->getString("simm", "version", false);
        if (tmp != "")
            setSimmVersion(tmp);

        tmp = params_->getString("simm", "mporDays", false);
        if (tmp != "")
            setMporDays(static_cast<Size>(parseInteger(tmp)));

        tmp = params_->getString("simm", "simmCalibration", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            if (std::filesystem::exists(file))
                setSimmCalibrationDataFromFile(file);
        }

        tmp = params_->getString("simm", "crif", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            setCrifFromFile(file, csvEolChar(), csvSeparator(), '\"', csvEscapeChar());
        }
	    else {
            // If an external CRIF is not provided we need to generate CRIF
            // using the CRIF analytic settings below
            tmp = params_->getString("crif", "marketConfigFile", false);
            if (tmp != "") {
                string file = (setupVariables_.inputPath_ / tmp).generic_string();
                LOG("Loading sensitivity scenario sim market parameters from file" << file);
                setSensiSimMarketParamsFromFile(file);
            } else {
                WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
            }

            tmp = params_->getString("crif", "sensitivityConfigFile", false);
            if (tmp != "") {
                string file = (setupVariables_.inputPath_ / tmp).generic_string();
                LOG("Load sensitivity scenario data from file" << file);
                setSensiScenarioDataFromFile(file);
            } else {
                WLOG("Sensitivity scenario data not loaded");
            }

            auto nameMapper = QuantLib::ext::make_shared<SimmBasicNameMapper>();
            tmp = params_->getString("crif", "nameMappingInputFile", false);
            if (tmp != "") {
                string fileName = (setupVariables_.inputPath_ / tmp).generic_string();
                LOG("simmNameMapper file name: " << fileName);
                nameMapper->fromFile(fileName);
            }
            simmNameMapper_ = nameMapper;

            auto bucketMapper = QuantLib::ext::make_shared<SimmBucketMapperBase>();
            tmp = params_->getString("crif", "bucketMappingInputFile", false);
            if (tmp != "") {
                string fileName = (setupVariables_.inputPath_ / tmp).generic_string();
                LOG("simmBucketMapper file name: " << fileName);
                bucketMapper->fromFile(fileName);
            }
            simmBucketMapper_ = bucketMapper;
        }

        tmp = params_->getString("simm", "calculationCurrency", false);
        if (tmp != "") {
            setSimmCalculationCurrencyCall(tmp);
            setSimmCalculationCurrencyPost(tmp);
        } else {
            QL_REQUIRE(baseCurrency() != "", "either base currency or calculation currency is required");
        }

        tmp = params_->getString("simm", "calculationCurrencyCall", false);
        if (tmp != "") {
            setSimmCalculationCurrencyCall(tmp);
        }

        tmp = params_->getString("simm", "calculationCurrencyPost", false);
        if (tmp != "") {
            setSimmCalculationCurrencyPost(tmp);
        }

        tmp = params_->getString("simm", "resultCurrency", false);
        if (tmp != "")
            setSimmResultCurrency(tmp);
        else
            setSimmResultCurrency(simmCalculationCurrencyCall());

        tmp = params_->getString("simm", "reportingCurrency", false);
        if (tmp != "")
            setSimmReportingCurrency(tmp);

        tmp = params_->getString("simm", "enforceIMRegulations", false);
        if (tmp != "")
            setEnforceIMRegulations(parseBool(tmp));

        tmp = params_->getString("simm", "writeIntermediateReports", false);
        if (tmp != "")
            setWriteSimmIntermediateReports(parseBool(tmp));
    }

    LOG("IM SCHEDULE");
    tmp = params_->getString("imschedule", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("IM_SCHEDULE");

        tmp = params_->getString("imschedule", "version", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "version", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "version for imschedule and simm should match");
            setSimmVersion(tmp);
        } else if (simmVersion() == "") {
            LOG("set SIMM version for IM Schedule to 2.6, required to load CRIF")
            setSimmVersion("2.6");
        }

        tmp = params_->getString("imschedule", "crif", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "crif", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "crif files for imschedule and simm should match");
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            setCrifFromFile(file, csvEolChar(), csvSeparator(), '\"', csvEscapeChar());
        }

        tmp = params_->getString("imschedule", "calculationCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "calculationCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for for imschedule and simm should match");
            setSimmCalculationCurrencyCall(tmp);
            setSimmCalculationCurrencyPost(tmp);
        } else {
            QL_REQUIRE(baseCurrency() != "", "either base currency or calculation currency is required");
        }

        tmp = params_->getString("imschedule", "calculationCurrencyCall", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "calculationCurrencyCall", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for imschedule and simm should match");
            setSimmCalculationCurrencyCall(tmp);
        }

        tmp = params_->getString("imschedule", "calculationCurrencyPost", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "calculationCurrencyPost", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "calculation currency for imschedule and simm should match");
            setSimmCalculationCurrencyPost(tmp);
        }

        tmp = params_->getString("imschedule", "resultCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "resultCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "result currency for imschedule and simm should match");
            setSimmResultCurrency(tmp);
        } else
            setSimmResultCurrency(simmCalculationCurrencyCall());

        tmp = params_->getString("imschedule", "reportingCurrency", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "reportingCurrency", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "reporting currency for imschedule and simm should match");
            setSimmReportingCurrency(tmp);
        }

        tmp = params_->getString("imschedule", "enforceIMRegulations", false);
        if (tmp != "") {
            string tmpSimm = params_->getString("simm", "enforceIMRegulations", false);
            QL_REQUIRE(!doSimm || tmp == tmpSimm, "enforceIMRegulations for imschedule and simm should match");
            setEnforceIMRegulations(parseBool(tmp));
        }
    }

    /*************
     * Calibration
     *************/

    tmp = params_->getString("calibration", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("CALIBRATION");
    }

    /*************
     * Correlation
     *************/
    tmp = params_->getString("correlation", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("CORRELATION");    

    /************
     * Simulation
     ************/

    tmp = params_->getString("simulation", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("EXPOSURE");
    }    

    // check this here because we need to know further below when checking for EXPOSURE or XVA analytic
    tmp = params_->getString("xva", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA");

    tmp = params_->getString("pfe", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("PFE");

    tmp = params_->getString("xvaStress", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA_STRESS");
    
    tmp = params_->getString("sensitivityStress", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("SENSITIVITY_STRESS");

    tmp = params_->getString("xvaSensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA_SENSITIVITY");

    tmp = params_->getString("xvaExplain", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("XVA_EXPLAIN");

    /*************
     * XVA Stress
     *************/

    if (analytics().find("XVA_STRESS") != analytics().end()) {
        tmp = params_->getString("xvaStress", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading xva stress test scenario sim market parameters from file" << file);
            setXvaStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for xva stress testing not loaded");
        }

        tmp = params_->getString("xvaStress", "stressConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load xav stress test scenario data from file" << file);
            setXvaStressScenarioDataFromFile(file);
        } else {
            WLOG("Xva Stress scenario data not loaded");
        }

        tmp = params_->getString("xvaStress", "writeCubes", false);
        if (!tmp.empty()) {
            bool writeCubes = false;
            bool success = tryParse<bool>(tmp, writeCubes, parseBool);
            if (success) {
                setXvaStressWriteCubes(writeCubes);
            }
        }

        tmp = params_->getString("xvaStress", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setXvaStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }
    }

    /*************
     * Sensitivity Stress
     *************/

     if (analytics().find("SENSITIVITY_STRESS") != analytics().end()) {
        tmp = params_->getString("sensitivityStress", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading sensitivity stress test scenario sim market parameters from file" << file);
            setSensitivityStressSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity stress testing not loaded");
        }

        tmp = params_->getString("sensitivityStress", "stressConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity stress test scenario data from file" << file);
            setSensitivityStressScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity Stress scenario data not loaded");
        }

        tmp = params_->getString("sensitivityStress", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setSensitivityStressSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded, don't support par stress tests");
        }

        tmp = params_->getString("sensitivityStress", "calcBaseScenario", false);
        if (!tmp.empty()) {
            bool calcBaseScenario = false;
            bool success = tryParse<bool>(tmp, calcBaseScenario, parseBool);
            if (success) {
                setSensitivityStressCalculateBaseScenario(calcBaseScenario);
            }
        }
    }

    /*************
     * XVA Sensi
     *************/

    if (analytics().find("XVA_SENSITIVITY") != analytics().end() ||
	analytics().find("SA_CVA") != analytics().end()) {
        tmp = params_->getString("xvaSensitivity", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading xva sensitivity scenario sim market parameters from file" << file);
            setXvaSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for xva sensitivity not loaded");
        }

        tmp = params_->getString("xvaSensitivity", "sensitivityConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load xva sensitivity scenario data from file" << file);
            setXvaSensiScenarioDataFromFile(file);
        } else {
            WLOG("Xva sensitivity scenario data not loaded");
        }

        tmp = params_->getString("xvaSensitivity", "parSensitivity", false);
        if (tmp != "")
            setXvaSensiParSensi(parseBool(tmp));

        tmp = params_->getString("xvaSensitivity", "outputJacobi", false);
        if (tmp != "")
            setXvaSensiOutputJacobi(parseBool(tmp));

        tmp = params_->getString("xvaSensitivity", "outputSensitivityThreshold", false);
        if (tmp != "")
            setXvaSensiThreshold(parseReal(tmp));

	tmp = params_->getString("xvaSensitivity", "outputPrecision", false);
        if (tmp != "")
            setXvaSensiOutputPrecision(parseInteger(tmp));
    }

    /*************
     * XVA Explain
     *************/

    if (analytics().find("XVA_EXPLAIN") != analytics().end()) {
        tmp = params_->getString("xvaExplain", "marketConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading xva explain scenario sim market parameters from file" << file);
            setXvaExplainSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for xvaExplain not loaded");
        }

        tmp = params_->getString("xvaExplain", "sensitivityConfigFile", false);
        if (!tmp.empty()) {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load xvaExplain sensitivity scenario data from file" << file);
            setXvaExplainSensitivityScenarioDataFromFile(file);
        } else {
            WLOG("xvaExplain scenario data not loaded");
        }

        tmp = params_->getString("xvaExplain", "shiftThreshold", false);
        if (!tmp.empty()) {
            setXvaExplainShiftThreshold(parseReal(tmp));
        } else {
            setXvaExplainShiftThreshold(0.0);
        }

        tmp = params_->getString("xvaExplain", "mporDate", false);
        if (tmp != "")
            setMporDate(parseDate(tmp));

        tmp = params_->getString("xvaExplain", "mporDays", false);
        if (tmp != "")
            setMporDays(parseInteger(tmp));

        tmp = params_->getString("xvaExplain", "mporCalendar", false);
        if (tmp != "")
            setMporCalendar(tmp);
    }

     /*********************
      * CVA Capital: SA-CVA
      *********************/

     tmp = params_->getString("sacva", "active", false);
     if (!tmp.empty() && parseBool(tmp)) {
         insertAnalytic("SA_CVA");

         tmp = params_->getString("sacva", "saCvaNetSensitivitiesFile", false);
         if (!tmp.empty()) {
             string file = (setupVariables_.inputPath_ / tmp).generic_string();
             LOG("Loading aggregated SA-CVA sensitivity input from file" << file);
             setSaCvaNetSensitivitiesFromFile(file);
         } else {
             tmp = params_->getString("sacva", "cvaSensitivitiesFile", false);
             if (!tmp.empty()) {
                 string file = (setupVariables_.inputPath_ / tmp).generic_string();
                 LOG("Loading granular cva sensitivity input from file" << file);
                 setCvaSensitivitiesFromFile(file);
             }
	 }

	 // if both above failed: run the sub-analytic
	 if (saCvaNetSensitivities().size() == 0) {
	     // Ensure that we have the XVA Sensitivity analytic configured, see above
	     QL_REQUIRE(analytics().find("XVA_SENSITIVITY") != analytics().end(),
			"SA-CVA needs the XVA Sensitivity analytic configured unless sensitivities are provided as "
			"an input");
	     // XVA Sensitivity will be run as a sub analytic of SA-CVA so that we can drop it here
	     removeAnalytic("XVA_SENSITIVITY");
         }
     }

     /*********************
      * CCR Capital: SA-CCR
      *********************/

     tmp = params_->getString("saccr", "active", false);
     if (!tmp.empty() && parseBool(tmp)) {
         insertAnalytic("SA_CCR");

         // Commodity asset class uses SIMM name and bucket mapping for hedging set definitions
	     // Note that Equities use reference data for that purpose
         tmp = params_->getString("saccr", "simmVersion", false);
         if (tmp != "")
             setSimmVersion(tmp);
         else if (simmVersion_ == "") {
             setSimmVersion("2.6");
             WLOG("Setting SIMM version to 2.6 for SACCR");
         }

         tmp = params_->getString("saccr", "simmNameMapping", false);
         if (tmp != "") {
             string nameMappingFile = (setupVariables_.inputPath_ / tmp).generic_string();
             setSimmNameMapperFromFile(nameMappingFile);
             LOG("Loading SIMM bucket mapping from file " << nameMappingFile);
         }

         tmp = params_->getString("saccr", "simmBucketMapping", false);
         if (tmp != "") {
             string bucketMappingFile = (setupVariables_.inputPath_ / tmp).generic_string();
             setSimmBucketMapperFromFile(bucketMappingFile);
             LOG("Loading SIMM bucket mapping from file " << bucketMappingFile);
         }
     }

     /*********************
      * CVA Capital: BA-CVA
      *********************/

     tmp = params_->getString("bacva", "active", false);
     if (!tmp.empty() && parseBool(tmp))
         insertAnalytic("BA_CVA");
     
     /******************
      * MR Capital: SMRC
      ******************/

     tmp = params_->getString("smrc", "active", false);
     if (!tmp.empty() && parseBool(tmp))
         insertAnalytic("SMRC");

     /*************
      * cashflow npv and dynamic backtesting
      *************/

     tmp = params_->getString("cashflow", "cashFlowHorizon", false);
     if (tmp != "")
         setCashflowHorizon(tmp);

     tmp = params_->getString("cashflow", "portfolioFilterDate", false);
     if (tmp != "")
         setPortfolioFilterDate(tmp);

     /*************
      * ZERO TO PAR SENSI CONVERSION
      *************/

     tmp = params_->getString("zeroToParSensiConversion", "active", false);
     if (!tmp.empty() && parseBool(tmp)) {
        insertAnalytic("PARCONVERSION");

        tmp = params_->getString("zeroToParSensiConversion", "sensitivityInputFile", false);
        if (tmp != "") {
            setParConversionInputFile((setupVariables_.inputPath_ / tmp).generic_string());
        }

        tmp = params_->getString("zeroToParSensiConversion", "idColumn", false);
        if (tmp != "") {
            setParConversionInputIdColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "riskFactorColumn", false);
        if (tmp != "") {
            setParConversionInputRiskFactorColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "deltaColumn", false);
        if (tmp != "") {
            setParConversionInputDeltaColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "currencyColumn", false);
        if (tmp != "") {
            setParConversionInputCurrencyColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "baseNpvColumn", false);
        if (tmp != "") {
            setParConversionInputBaseNpvColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "shiftSizeColumn", false);
        if (tmp != "") {
            setParConversionInputShiftSizeColumn(tmp);
        }

        tmp = params_->getString("zeroToParSensiConversion", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading par converions scenario sim market parameters from file" << file);
            setParConversionSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for par conversion testing not loaded");
        }

        tmp = params_->getString("zeroToParSensiConversion", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load par conversion scenario data from file" << file);
            setParConversionScenarioDataFromFile(file);
        } else {
            WLOG("Par conversion scenario data not loaded");
        }

        tmp = params_->getString("zeroToParSensiConversion", "pricingEnginesFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load pricing engine data from file: " << file);
            setParConversionPricingEngineFromFile(file);
        } else {
            WLOG("Pricing engine data not found for par conversion, using global");
        }

        tmp = params_->getString("zeroToParSensiConversion", "outputThreshold", false);
        if (tmp != "")
            setParConversionThreshold(parseReal(tmp));

        tmp = params_->getString("zeroToParSensiConversion", "outputJacobi", false);
        if (tmp != "")
            setParConversionOutputJacobi(parseBool(tmp));
    } 

    tmp = params_->getString("scenarioGeneration", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("SCENARIO_GENERATION");

    tmp = params_->getString("portfolioDetails", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        insertAnalytic("PORTFOLIO_DETAILS");

    /*****************
     * CRIF Generation
     *****************/

    tmp = params_->getString("crif", "active", false);
    bool doCrif = !tmp.empty() && parseBool(tmp);
    // tmp = params_->getString("simm", "active", false);
    // bool doSimm = !tmp.empty() && parseBool(tmp);
    if (doCrif) {
        insertAnalytic("CRIF");

	// Use the same market and sensi configs as the the sensitivity analytic
	// FIXME: Warn if we override previous settings
        tmp = params_->getString("crif", "marketConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Loading sensitivity scenario sim market parameters from file" << file);
            setSensiSimMarketParamsFromFile(file);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }

        tmp = params_->getString("crif", "sensitivityConfigFile", false);
        if (tmp != "") {
            string file = (setupVariables_.inputPath_ / tmp).generic_string();
            LOG("Load sensitivity scenario data from file" << file);
            setSensiScenarioDataFromFile(file);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }

	    tmp = params_->getString("crif", "simmVersion", false);
        if (tmp != "") {
            setSimmVersion(tmp);
        } else {
            LOG("set SIMM version for CRIF generation to 2.6")
            setSimmVersion("2.6");
        }

	    auto nameMapper = QuantLib::ext::make_shared<SimmBasicNameMapper>();
	    tmp = params_->getString("crif", "nameMappingInputFile", false);
	    if (tmp != "") {
	       string fileName = (setupVariables_.inputPath_ / tmp).generic_string();
	       LOG("simmNameMapper file name: " << fileName);
	       nameMapper->fromFile(fileName);
	    }
	    simmNameMapper_ = nameMapper;

	    auto bucketMapper = QuantLib::ext::make_shared<SimmBucketMapperBase>();
	    tmp = params_->getString("crif", "bucketMappingInputFile", false);
	    if (tmp != "") {
	       string fileName = (setupVariables_.inputPath_ / tmp).generic_string();
	       LOG("simmBucketMapper file name: " << fileName);
	       bucketMapper->fromFile(fileName);
	    }
	    simmBucketMapper_ = bucketMapper;
    }

    if (analytics().size() == 0) {
        insertAnalytic("MARKETDATA");
        setOutputTodaysMarketCalibration(true);
        if (lazyMarketBuilding())
            LOG("Lazy market build being overridden to \"false\" for MARKETDATA analytic.")
        setLazyMarketBuilding(false);
    }

    LOG("analytics: " << analytics().size());
    for (auto a : analytics())
        LOG("analytic: " << a);

    LOG("buildInputParameters done");
}

} // namespace analytics
} // namespace ore
