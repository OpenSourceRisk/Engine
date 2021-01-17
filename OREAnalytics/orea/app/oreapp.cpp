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
#include <boost/timer/timer.hpp>

#ifdef BOOST_MSVC
// disable warning C4503: '__LINE__Var': decorated name length exceeded, name was truncated
// This pragma statement needs to be at the top of the file - lower and it will not work:
// http://stackoverflow.com/questions/9673504/is-it-possible-to-disable-compiler-warning-c4503
// http://boost.2283326.n4.nabble.com/General-Warnings-and-pragmas-in-MSVC-td2587449.html
#pragma warning(disable : 4503)
#endif

#include <boost/filesystem.hpp>

#include <orea/orea.hpp>
#include <ored/ored.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>

#include <orea/app/oreapp.hpp>

using namespace std;
using namespace ore::data;
using namespace ore::analytics;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace {

vector<string> getFilenames(const string& fileString, const string& path) {
    vector<string> fileNames;
    boost::split(fileNames, fileString, boost::is_any_of(",;"), boost::token_compress_on);
    for (auto it = fileNames.begin(); it < fileNames.end(); it++) {
        boost::trim(*it);
        *it = path + "/" + *it;
    }
    return fileNames;
}

} // anonymous namespace

namespace ore {
namespace analytics {

OREApp::OREApp(boost::shared_ptr<Parameters> params, ostream& out)
    : tab_(40), progressBarWidth_(72 - std::min<Size>(tab_, 67)), params_(params),
      asof_(parseDate(params_->get("setup", "asofDate"))), out_(out), cubeDepth_(0) {

    // Set global evaluation date
    Settings::instance().evaluationDate() = asof_;

    // Set up logging
    setupLog();

    readSetup();
}

OREApp::~OREApp() {
    // Close logs
    closeLog();
}

int OREApp::run() {

    cpu_timer timer;

    try {
        out_ << "ORE starting" << std::endl;
        LOG("ORE starting");
        // readSetup();

        /*********
         * Build Markets
         */
        out_ << setw(tab_) << left << "Market... " << flush;
        buildMarket();
        out_ << "OK" << endl;

        /*********
         * Load Reference Data
         */
        out_ << setw(tab_) << left << "Reference... " << flush;
        getReferenceData();
        out_ << "OK" << endl;

        /************************
         *Build Pricing Engine Factory
         */
        out_ << setw(tab_) << left << "Engine factory... " << flush;
        engineFactory_ = buildEngineFactory(market_);
        out_ << "OK" << endl;

        /******************************
         * Load and Build the Portfolio
         */
        out_ << setw(tab_) << left << "Portfolio... " << flush;
        portfolio_ = buildPortfolio(engineFactory_);
        out_ << "OK" << endl;

        /******************************
         * Write initial reports
         */
        out_ << setw(tab_) << left << "Write Reports... " << flush;
        writeInitialReports();
        out_ << "OK" << endl;

        /**************************
         * Write base scenario file
         */
        out_ << setw(tab_) << left << "Write Base Scenario... " << flush;
        if (writeBaseScenario_) {
            writeBaseScenario();
            out_ << "OK" << endl;
        } else {
            LOG("skip base scenario");
            out_ << "SKIP" << endl;
        }

        /**********************
         * Sensitivity analysis
         */
        if (sensitivity_) {
            out_ << setw(tab_) << left << "Sensitivity Report... " << flush;

            // We reset this here because the date grid building in sensitivity analysis depends on it.
            Settings::instance().evaluationDate() = asof_;
            getSensitivityRunner()->runSensitivityAnalysis(market_, conventions_, curveConfigs_, marketParameters_);
            out_ << "OK" << endl;
        } else {
            LOG("skip sensitivity analysis");
            out_ << setw(tab_) << left << "Sensitivity... ";
            out_ << "SKIP" << endl;
        }

        /****************
         * Stress testing
         */
        if (stress_) {
            runStressTest();
        } else {
            LOG("skip stress test");
            out_ << setw(tab_) << left << "Stress testing... ";
            out_ << "SKIP" << endl;
        }

        /****************
         * Parametric VaR
         */
        if (parametricVar_) {
            runParametricVar();
        } else {
            LOG("skip parametric var");
            out_ << setw(tab_) << left << "Parametric VaR... ";
            out_ << "SKIP" << endl;
        }

        /******************************************
         * Simulation: Scenario and Cube Generation
         */

        if (simulate_) {
            generateNPVCube();
        } else {
            LOG("skip simulation");
            out_ << setw(tab_) << left << "Simulation... ";
            out_ << "SKIP" << endl;
        }

        /*****************************
         * Aggregation and XVA Reports
         */
        out_ << setw(tab_) << left << "Aggregation and XVA Reports... " << flush;
        if (xva_) {

            // We reset this here because the date grid building below depends on it.
            Settings::instance().evaluationDate() = asof_;

            // Use pre-generated cube
            if (!cube_)
                loadCube();

            QL_REQUIRE(cube_->numIds() == portfolio_->size(),
                       "cube x dimension (" << cube_->numIds() << ") does not match portfolio size ("
                                            << portfolio_->size() << ")");

            // Use pre-generared scenarios
            if (!scenarioData_)
                loadScenarioData();

            QL_REQUIRE(scenarioData_->dimDates() == cube_->dates().size(),
                       "scenario dates do not match cube grid size");
            QL_REQUIRE(scenarioData_->dimSamples() == cube_->samples(),
                       "scenario sample size does not match cube sample size");

            runPostProcessor();
            out_ << "OK" << endl;
            out_ << setw(tab_) << left << "Write Reports... " << flush;
            writeXVAReports();
            if (writeDIMReport_)
                writeDIMReport();
            out_ << "OK" << endl;
        } else {
            LOG("skip XVA reports");
            out_ << "SKIP" << endl;
        }

    } catch (std::exception& e) {
        ALOG("Error: " << e.what());
        out_ << "Error: " << e.what() << endl;
        return 1;
    }

    timer.stop();
    out_ << "run time: " << setprecision(2) << timer.format(default_places, "%w") << " sec" << endl;
    out_ << "ORE done." << endl;

    LOG("ORE done.");
    return 0;
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
        CalendarAdjustments::instance().setConfig(calendarAdjustments);
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
}

void OREApp::setupLog() {
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
        conventions_.fromFile(conventionsFile);
    } else {
        WLOG("No conventions file loaded");
    }
}

void OREApp::getMarketParameters() {
    if (params_->has("setup", "marketConfigFile") && params_->get("setup", "marketConfigFile") != "") {
        string marketConfigFile = inputPath_ + "/" + params_->get("setup", "marketConfigFile");
        marketParameters_.fromFile(marketConfigFile);
    } else {
        WLOG("No market parameters loaded");
    }
}

boost::shared_ptr<EngineFactory> OREApp::buildEngineFactory(const boost::shared_ptr<Market>& market,
                                                            const string& groupName) const {
    MEM_LOG;
    LOG("Building an engine factory")

    map<MarketContext, string> configurations;
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    string pricingEnginesFile = inputPath_ + "/" + params_->get(groupName, "pricingEnginesFile");
    if (params_->get(groupName, "pricingEnginesFile") != "")
        engineData->fromFile(pricingEnginesFile);
    configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
    configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
    configurations[MarketContext::pricing] = params_->get("markets", "pricing");
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(
        engineData, market, configurations, getExtraEngineBuilders(), getExtraLegBuilders(), referenceData_);

    LOG("Engine factory built");
    MEM_LOG;

    return factory;
}

boost::shared_ptr<TradeFactory> OREApp::buildTradeFactory() const {
    boost::shared_ptr<TradeFactory> tf = boost::make_shared<TradeFactory>();
    tf->addExtraBuilders(getExtraTradeBuilders(tf));
    return tf;
}

boost::shared_ptr<Portfolio> OREApp::buildPortfolio(const boost::shared_ptr<EngineFactory>& factory) {
    MEM_LOG;
    LOG("Building portfolio");
    boost::shared_ptr<Portfolio> portfolio = loadPortfolio();
    portfolio->build(factory);
    LOG("Portfolio built");
    MEM_LOG;
    return portfolio;
}

boost::shared_ptr<Portfolio> OREApp::loadPortfolio() {
    string portfoliosString = params_->get("setup", "portfolioFile");
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    if (params_->get("setup", "portfolioFile") == "")
        return portfolio;
    vector<string> portfolioFiles = getFilenames(portfoliosString, inputPath_);
    for (auto portfolioFile : portfolioFiles) {
        portfolio->load(portfolioFile, buildTradeFactory());
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
    string simulationMarketStr = Market::defaultConfiguration;
    if (params_->has("markets", "simulation"))
        simulationMarketStr = params_->get("markets", "simulation");

    CrossAssetModelBuilder modelBuilder(market, modelData, lgmCalibrationMarketStr, fxCalibrationMarketStr,
                                        eqCalibrationMarketStr, infCalibrationMarketStr, simulationMarketStr,
                                        ActualActual(), false, continueOnCalibrationError);
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
    out_ << endl << setw(tab_) << left << "Curve Report... " << flush;
    if (params_->hasGroup("curves") && params_->get("curves", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("curves", "outputFileName");
        CSVFileReport curvesReport(fileName);
        DateGrid grid(params_->get("curves", "grid"));
        getReportWriter()->writeCurves(curvesReport, params_->get("curves", "configuration"), grid, marketParameters_,
                                       market_, continueOnError_);
        out_ << "OK" << endl;
    } else {
        LOG("skip curve report");
        out_ << "SKIP" << endl;
    }

    /*********************
     * Portfolio valuation
     */
    out_ << setw(tab_) << left << "NPV Report... " << flush;
    if (params_->hasGroup("npv") && params_->get("npv", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("npv", "outputFileName");
        CSVFileReport npvReport(fileName);
        getReportWriter()->writeNpv(npvReport, params_->get("npv", "baseCurrency"), market_,
                                    params_->get("markets", "pricing"), portfolio_);
        out_ << "OK" << endl;
    } else {
        LOG("skip portfolio valuation");
        out_ << "SKIP" << endl;
    }

    /**********************
     * Cash flow generation
     */
    out_ << setw(tab_) << left << "Cashflow Report... " << flush;
    if (params_->hasGroup("cashflow") && params_->get("cashflow", "active") == "Y") {
        string fileName = outputPath_ + "/" + params_->get("cashflow", "outputFileName");
        CSVFileReport cashflowReport(fileName);
        getReportWriter()->writeCashflow(cashflowReport, portfolio_, market_);
        out_ << "OK" << endl;
    } else {
        LOG("skip cashflow generation");
        out_ << "SKIP" << endl;
    }

    LOG("Initial reports written");
    MEM_LOG;
}

boost::shared_ptr<ReportWriter> OREApp::getReportWriter() const {
    return boost::shared_ptr<ReportWriter>(getReportWriterImpl());
}

boost::shared_ptr<SensitivityRunner> OREApp::getSensitivityRunner() {
    return boost::make_shared<SensitivityRunner>(params_, buildTradeFactory(), getExtraEngineBuilders(),
                                                 getExtraLegBuilders(), referenceData_, continueOnError_);
}

void OREApp::runStressTest() {

    MEM_LOG;
    LOG("Running stress test");

    out_ << setw(tab_) << left << "Stress Test Report... " << flush;
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
    string portfolioFile = inputPath_ + "/" + params_->get("setup", "portfolioFile");
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    // Just load here. We build the portfolio in SensitivityAnalysis, after building SimMarket.
    portfolio->load(portfolioFile);

    LOG("Build Stress Test");
    string marketConfiguration = params_->get("markets", "pricing");
    boost::shared_ptr<StressTest> stressTest =
        boost::make_shared<StressTest>(portfolio, market_, marketConfiguration, engineData, simMarketData, stressData,
                                       conventions_, curveConfigs_, marketParameters_);

    string outputFile = outputPath_ + "/" + params_->get("stress", "scenarioOutputFile");
    Real threshold = parseReal(params_->get("stress", "outputThreshold"));
    boost::shared_ptr<Report> stressReport = boost::make_shared<CSVFileReport>(outputFile);
    stressTest->writeReport(stressReport, threshold);

    out_ << "OK" << endl;

    LOG("Stress test completed");
    MEM_LOG;
}

void OREApp::runParametricVar() {

    MEM_LOG;
    LOG("Running parametric VaR");

    out_ << setw(tab_) << left << "Parametric VaR Report... " << flush;

    LOG("Get sensitivity data");
    string sensiFile = inputPath_ + "/" + params_->get("parametricVar", "sensitivityInputFile");
    auto ss = boost::make_shared<SensitivityFileStream>(sensiFile);

    LOG("Build trade to portfolio id mapping");
    map<string, set<string>> tradePortfolio;
    for (auto const& t : portfolio_->trades()) {
        tradePortfolio[t->id()].insert(t->portfolioIds().begin(), t->portfolioIds().end());
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
    out_ << "OK" << endl;

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

    auto simMarket = boost::make_shared<ScenarioSimMarket>(market_, simMarketData, conventions_, marketConfiguration,
                                                           curveConfigs_, marketParameters_, continueOnError_);
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
    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(grid_->size(), samples_);
}

void OREApp::initCube(boost::shared_ptr<NPVCube>& cube, const std::vector<std::string>& ids) {
    if (cubeDepth_ == 1)
        cube = boost::make_shared<SinglePrecisionInMemoryCube>(asof_, ids, grid_->dates(), samples_);
    else if (cubeDepth_ == 2)
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(asof_, ids, grid_->dates(), samples_, cubeDepth_);
    else {
        QL_FAIL("cube depth 1 or 2 expected");
    }
}

void OREApp::buildNPVCube() {
    LOG("Build valuation cube engine");
    // Valuation calculators
    string baseCurrency = params_->get("simulation", "baseCurrency");
    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(boost::make_shared<NPVCalculator>(baseCurrency));
    if (cubeDepth_ > 1)
        calculators.push_back(boost::make_shared<CashflowCalculator>(baseCurrency, asof_, grid_, 1));
    LOG("Build cube");
    ValuationEngine engine(asof_, grid_, simMarket_);
    ostringstream o;
    o.str("");
    o << "Build Cube " << simPortfolio_->size() << " x " << grid_->size() << " x " << samples_ << "... ";

    auto progressBar = boost::make_shared<SimpleProgressBar>(o.str(), tab_, progressBarWidth_);
    auto progressLog = boost::make_shared<ProgressLog>("Building cube...");
    engine.registerProgressIndicator(progressBar);
    engine.registerProgressIndicator(progressLog);
    engine.buildCube(simPortfolio_, cube_, calculators);
    out_ << "OK" << endl;
}

void OREApp::initialiseNPVCubeGeneration(boost::shared_ptr<Portfolio> portfolio) {
    out_ << setw(tab_) << left << "Simulation Setup... ";
    LOG("Load Simulation Market Parameters");
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData = getSimMarketData();
    boost::shared_ptr<ScenarioGeneratorData> sgd = getScenarioGeneratorData();
    grid_ = sgd->grid();
    samples_ = sgd->samples();

    if (buildSimMarket_) {
        LOG("Build Simulation Market");

        simMarket_ = boost::make_shared<ScenarioSimMarket>(market_, simMarketData, conventions_, getFixingManager(),
                                                           params_->get("markets", "simulation"), curveConfigs_,
                                                           marketParameters_, continueOnError_);
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
        portfolio->build(simFactory);
        simPortfolio_ = portfolio;
        if (simPortfolio_->size() != n) {
            ALOG("There were errors during the sim portfolio building - check the sim market setup? Could build "
                 << simPortfolio_->size() << " trades out of " << n);
        }
        out_ << "OK" << endl;
    }

    if (params_->has("simulation", "storeFlows") && params_->get("simulation", "storeFlows") == "Y")
        cubeDepth_ = 2; // NPV and FLOW
    else
        cubeDepth_ = 1; // NPV only

    ostringstream o;
    o << "Aggregation Scenario Data " << grid_->size() << " x " << samples_ << "... ";
    out_ << setw(tab_) << o.str() << flush;

    initAggregationScenarioData();
    // Set AggregationScenarioData
    simMarket_->aggregationScenarioData() = scenarioData_;
    out_ << "OK" << endl;

    initCube(cube_, simPortfolio_->ids());
}

void OREApp::generateNPVCube() {
    MEM_LOG;
    LOG("Running NPV cube generation");

    boost::shared_ptr<Portfolio> portfolio = loadPortfolio();
    initialiseNPVCubeGeneration(portfolio);
    buildNPVCube();
    writeCube(cube_);
    writeScenarioData();

    LOG("NPV cube generation completed");
    MEM_LOG;
}

void OREApp::writeCube(boost::shared_ptr<NPVCube> cube) {
    out_ << endl << setw(tab_) << left << "Write Cube... " << flush;
    LOG("Write cube");
    if (params_->has("simulation", "cubeFile")) {
        string cubeFileName = outputPath_ + "/" + params_->get("simulation", "cubeFile");
        cube->save(cubeFileName);
        out_ << "OK" << endl;
    } else
        out_ << "SKIP" << endl;
}

void OREApp::writeScenarioData() {
    out_ << endl << setw(tab_) << left << "Write Aggregation Scenario Data... " << flush;
    LOG("Write scenario data");
    bool skipped = true;
    if (params_->has("simulation", "aggregationScenarioDataFileName")) {
        // binary output
        string outputFileNameAddScenData =
            outputPath_ + "/" + params_->get("simulation", "aggregationScenarioDataFileName");
        scenarioData_->save(outputFileNameAddScenData);
        out_ << "OK" << endl;
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
    if (skipped)
        out_ << "SKIP" << endl;
}

void OREApp::loadScenarioData() {
    string scenarioFile = outputPath_ + "/" + params_->get("xva", "scenarioFile");
    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>();
    scenarioData_->load(scenarioFile);
}

void OREApp::loadCube() {
    string cubeFile = outputPath_ + "/" + params_->get("xva", "cubeFile");
    cubeDepth_ = 1;
    if (params_->has("xva", "hyperCube"))
        cubeDepth_ = parseBool(params_->get("xva", "hyperCube")) ? 2 : 1;

    if (cubeDepth_ > 1)
        cube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
    else
        cube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
    LOG("Load cube from file " << cubeFile);
    cube_->load(cubeFile);
    LOG("Cube loading done");
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
    analytics["exposureProfiles"] = parseBool(params_->get("xva", "exposureProfiles"));
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
    Real dimScaling = 1.0;
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
        dimScaling = parseReal(params_->get("xva", "dimScaling"));
        dimLocalRegressionEvaluations = parseInteger(params_->get("xva", "dimLocalRegressionEvaluations"));
        dimLocalRegressionBandwidth = parseReal(params_->get("xva", "dimLocalRegressionBandwidth"));
    }

    string marketConfiguration = params_->get("markets", "simulation");

    bool fullInitialCollateralisation = false;
    if (params_->has("xva", "fullInitialCollateralisation")) {
        fullInitialCollateralisation = parseBool(params_->get("xva", "fullInitialCollateralisation"));
    }
    analytics["flipViewXVA"] = parseBool(params_->get("xva", "flipViewXVA"));
    string flipViewBorrowingCurvePostfix = "_BORROW";
    string flipViewLendingCurvePostfix = "_LEND";
    if (analytics["flipViewXVA"]) {
        flipViewBorrowingCurvePostfix = params_->get("xva", "flipViewBorrowingCurvePostfix");
        flipViewLendingCurvePostfix = params_->get("xva", "flipViewLendingCurvePostfix");
    }

    postProcess_ = boost::make_shared<PostProcess>(
        portfolio_, netting, market_, marketConfiguration, cube_, scenarioData_, analytics, baseCurrency,
        allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimQuantile, dimHorizonCalendarDays, dimRegressionOrder, dimRegressors,
        dimLocalRegressionEvaluations, dimLocalRegressionBandwidth, dimScaling, fullInitialCollateralisation,
        kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor, kvaTheirPdFloor,
        kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, flipViewBorrowingCurvePostfix, flipViewLendingCurvePostfix);
}

void OREApp::writeXVAReports() {

    MEM_LOG;
    LOG("Writing XVA reports");

    bool exposureByTrade = true;
    if (params_->has("xva", "exposureProfilesByTrade"))
        exposureByTrade = parseBool(params_->get("xva", "exposureProfilesByTrade"));
    if (exposureByTrade) {
        for (auto t : postProcess_->tradeIds()) {
            ostringstream o;
            o << outputPath_ << "/exposure_trade_" << t << ".csv";
            string tradeExposureFile = o.str();
            CSVFileReport tradeExposureReport(tradeExposureFile);
            getReportWriter()->writeTradeExposures(tradeExposureReport, postProcess_, t);
        }
    }
    for (auto n : postProcess_->nettingSetIds()) {
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
    }

    string XvaFile = outputPath_ + "/xva.csv";
    CSVFileReport xvaReport(XvaFile);
    getReportWriter()->writeXVA(xvaReport, params_->get("xva", "allocationMethod"), portfolio_, postProcess_);

    string rawCubeOutputFile = params_->get("xva", "rawCubeOutputFile");
    CubeWriter cw1(outputPath_ + "/" + rawCubeOutputFile);
    map<string, string> nettingSetMap = portfolio_->nettingSetMap();
    cw1.write(cube_, nettingSetMap);

    string netCubeOutputFile = params_->get("xva", "netCubeOutputFile");
    CubeWriter cw2(outputPath_ + "/" + netCubeOutputFile);
    cw2.write(postProcess_->netCube(), nettingSetMap);

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
        conventions_.fromXMLString(conventionsXML);

    if (todaysMarketXML == "")
        getMarketParameters();
    else
        marketParameters_.fromXMLString(todaysMarketXML);

    if (curveConfigXML != "")
        curveConfigs_.fromXMLString(curveConfigXML);
    else if (params_->has("setup", "curveConfigFile") && params_->get("setup", "curveConfigFile") != "") {
        out_ << endl << setw(tab_) << left << "Curve configuration... " << flush;
        string inputPath = params_->get("setup", "inputPath");
        string curveConfigFile = inputPath + "/" + params_->get("setup", "curveConfigFile");
        LOG("Load curve configurations from file");
        curveConfigs_.fromFile(curveConfigFile);
        out_ << "OK" << endl;
    } else {
        WLOG("No curve configurations loaded");
    }

    string implyTodaysFixingsString = params_->get("setup", "implyTodaysFixings");
    bool implyTodaysFixings = parseBool(implyTodaysFixingsString);

    if (marketData.size() == 0 || fixingData.size() == 0) {
        /*******************************
         * Market and fixing data loader
         */
        if (params_->has("setup", "marketDataFile") && params_->get("setup", "marketDataFile") != "") {
            out_ << setw(tab_) << left << "Market data loader... " << flush;
            string marketFileString = params_->get("setup", "marketDataFile");
            vector<string> marketFiles = getFilenames(marketFileString, inputPath_);
            string fixingFileString = params_->get("setup", "fixingDataFile");
            vector<string> fixingFiles = getFilenames(fixingFileString, inputPath_);
            vector<string> dividendFiles = {};
            if (params_->has("setup", "dividendDataFile")) {
                string dividendFileString = params_->get("setup", "dividendDataFile");
                dividendFiles = getFilenames(dividendFileString, inputPath_);
            }
            CSVLoader loader(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings);
            out_ << "OK" << endl;
            market_ = boost::make_shared<TodaysMarket>(asof_, marketParameters_, loader, curveConfigs_, conventions_,
                                                       continueOnError_, true, referenceData_);
        } else {
            WLOG("No market data loaded from file");
        }
    } else {
        LOG("Load market and fixing data from string vectors");
        InMemoryLoader loader;
        loadDataFromBuffers(loader, marketData, fixingData, implyTodaysFixings);
        market_ = boost::make_shared<TodaysMarket>(asof_, marketParameters_, loader, curveConfigs_, conventions_,
                                                   continueOnError_, true, referenceData_);
    }
    LOG("Today's market built");
    MEM_LOG;
}

boost::shared_ptr<MarketImpl> OREApp::getMarket() const {
    QL_REQUIRE(market_ != nullptr, "OREApp::getMarket(): original market is null");
    return boost::dynamic_pointer_cast<MarketImpl>(market_);
}

boost::shared_ptr<EngineFactory> OREApp::buildEngineFactoryFromXMLString(const boost::shared_ptr<Market>& market,
                                                                         const std::string& pricingEngineXML) {
    DLOG("OREApp::buildEngineFactoryFromXMLString called");

    if (pricingEngineXML == "")
        return buildEngineFactory(market);
    else {
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->fromXMLString(pricingEngineXML);

        map<MarketContext, string> configurations;
        configurations[MarketContext::irCalibration] = params_->get("markets", "lgmcalibration");
        configurations[MarketContext::fxCalibration] = params_->get("markets", "fxcalibration");
        configurations[MarketContext::pricing] = params_->get("markets", "pricing");
        boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(
            engineData, market, configurations, getExtraEngineBuilders(), getExtraLegBuilders());
        return factory;
    }
}

} // namespace analytics
} // namespace ore
