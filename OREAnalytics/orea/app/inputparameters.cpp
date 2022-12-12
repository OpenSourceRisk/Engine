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

#include <orea/app/inputparameters.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/observationmode.hpp>
#include <ored/ored.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/utilities/currencyconfig.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace analytics {

vector<string> getFilenames(const string& fileString, const string& path) {
    vector<string> fileNames;
    boost::split(fileNames, fileString, boost::is_any_of(",;"), boost::token_compress_on);
    for (auto it = fileNames.begin(); it < fileNames.end(); it++) {
        boost::trim(*it);
        *it = path + "/" + *it;
    }
    return fileNames;
}

void OREAppInputParameters::loadParameters() {

    LOG("OREAppInputParameters::loadParameters starting");

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

    asof_ = parseDate(params_->get("setup", "asofDate"));

    // Set it immediately, otherwise the scenario generator grid will be based on today's date
    Settings::instance().evaluationDate() = asof_;

    resultsPath_ = outputPath;

    baseCurrency_ = params_->get("npv", "baseCurrency");

    tmp = params_->get("setup", "useMarketDataFixings", false);
    if (tmp != "")
        useMarketDataFixings_ = parseBool(tmp);

    tmp = params_->get("setup", "dryRun", false);
    if (tmp != "")
        dryRun_ = parseBool(tmp);

    tmp = params_->get("setup", "reportNaString", false);
    if (tmp != "")
        reportNaString_ = tmp;

    tmp = params_->get("setup", "eomInflationFixings", false);
    if (tmp != "")
        eomInflationFixings_ = parseBool(tmp);

    tmp = params_->get("setup", "nThreads", false);
    if (tmp != "")
        nThreads_ = parseInteger(tmp);

    tmp = params_->get("setup", "entireMarket", false);
    if (tmp != "")
        entireMarket_ = parseBool(tmp);

    tmp = params_->get("setup", "iborFallbackOverride", false);
    if (tmp != "")
        iborFallbackOverride_ = parseBool(tmp);

    tmp = params_->get("setup", "continueOnError", false);
    if (tmp != "")
        continueOnError_ = parseBool(tmp);

    tmp = params_->get("setup", "lazyMarketBuilding", false);
    if (tmp != "")
        lazyMarketBuilding_ = parseBool(tmp);

    tmp = params_->get("setup", "buildFailedTrades", false);
    if (tmp != "")
        buildFailedTrades_ = parseBool(tmp);

    tmp = params_->get("setup", "observationModel", false);
    if (tmp != "") {
        observationModel_ = tmp;
        ObservationMode::instance().setMode(observationModel_);
        LOG("Observation Mode is " << observationModel_);
    }

    tmp = params_->get("setup", "implyTodaysFixings", false);
    if (tmp != "")
        implyTodaysFixings_ = ore::data::parseBool(tmp);

    tmp = params_->get("setup", "referenceDataFile", false);
    if (tmp != "") {
        string refDataFile = inputPath + "/" + tmp;
        LOG("Loading reference data from file: " << refDataFile);
        refDataManager_ = boost::make_shared<BasicReferenceDataManager>(refDataFile);
    } else {
        WLOG("Reference data not found");
    }

    conventions_ = boost::make_shared<Conventions>();
    if (params_->has("setup", "conventionsFile") && params_->get("setup", "conventionsFile") != "") {
        string conventionsFile = inputPath + "/" + params_->get("setup", "conventionsFile");
        LOG("Loading conventions from file: " << conventionsFile);
        conventions_->fromFile(conventionsFile);
    } else {
        ALOG("Conventions not found");
    }

    iborFallbackConfig_ = boost::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig());
    if (params_->has("setup", "iborFallbackConfig") && params_->get("setup", "iborFallbackConfig") != "") {
        std::string tmp = inputPath + "/" + params_->get("setup", "iborFallbackConfig");
        LOG("Loading Ibor fallback config from file: " << tmp);
        iborFallbackConfig_->fromFile(tmp);
    } else {
        WLOG("Using default Ibor fallback config");
    }

    auto curveConfig = boost::make_shared<ore::data::CurveConfigurations>();
    if (params_->has("setup", "curveConfigFile") && params_->get("setup", "curveConfigFile") != "") {
        //out_ << setw(tab_) << left << "Curve configuration... " << flush;
        string curveConfigFile = inputPath + "/" + params_->get("setup", "curveConfigFile");
        LOG("Load curve configurations from file: ");
        curveConfig->fromFile(curveConfigFile);
        curveConfigs_.push_back(curveConfig);
        //out_ << "OK" << endl;
    } else {
        ALOG("no curve configs loaded");
    }
    
    pricingEngine_ = boost::make_shared<EngineData>();
    tmp = params_->get("setup", "pricingEnginesFile", false);
    if (tmp != "") {
        string pricingEnginesFile = inputPath + "/" + tmp;
        LOG("Load pricing engine data from file: " << pricingEnginesFile);
        pricingEngine_->fromFile(pricingEnginesFile);
    } else {
        ALOG("Pricing engine data not found");
    }
    
    todaysMarketParams_ = boost::make_shared<TodaysMarketParameters>();
    tmp = params_->get("setup", "marketConfigFile", false);
    if (tmp != "") {
        string marketConfigFile = inputPath + "/" + tmp;
        LOG("Loading today's market parameters from file" << marketConfigFile);
        todaysMarketParams_->fromFile(marketConfigFile);
    } else {
        ALOG("Today's market parameters not found");
    }

    bool buildFailedTrades = false;
    if (params_->has("setup", "buildFailedTrades"))
        buildFailedTrades = parseBool(params_->get("setup", "buildFailedTrades"));
    portfolio_ = boost::make_shared<Portfolio>(buildFailedTrades);
    std::string portfolioFileString = params_->get("setup", "portfolioFile");
    if (portfolioFileString != "") {
        vector<string> portfolioFiles = getFilenames(portfolioFileString, inputPath);
        boost::shared_ptr<TradeFactory> tf = boost::make_shared<TradeFactory>();
        for (auto portfolioFile : portfolioFiles) {
            LOG("Loading portfolio from file: " << portfolioFile);
            portfolio_->load(portfolioFile, tf);
        }
    } else {
        ALOG("Portfolio data not found");
    }

    if (params_->hasGroup("Markets"))
        marketConfig_ = params_->data("Markets");
    
    /*************
     * NPV
     *************/

    tmp = params_->get("npv", "additionalResults", false);
    if (tmp != "")
        outputAdditionalResults_ = parseBool(tmp);

    /*************
     * Curves
     *************/

    tmp = params_->get("curves", "outputTodaysMarketCalibration", false);
    if (tmp != "")
        outputTodaysMarketCalibration_ = parseBool(tmp);
    
    /*************
     * Sensitivity
     *************/

    // FIXME: To be loaded from params or removed from the base class
    // xbsParConversion_ = false;
    // analyticFxSensis_ = true;
    // outputJacobi_ = false;
    // useSensiSpreadedTermStructures_ = true;
    
    sensiSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    tmp = params_->get("sensitivity", "marketConfigFile", false);
    if (tmp != "") {
        string marketConfigFile = inputPath + "/" + tmp;
        LOG("Loading scenario sim market parameters from file" << marketConfigFile);
        sensiSimMarketParams_->fromFile(marketConfigFile);
    } else {
        WLOG("ScenarioSimMarket parameters not loaded");
    }
    
    sensiScenarioData_ = boost::make_shared<SensitivityScenarioData>();
    tmp = params_->get("sensitivity", "sensitivityConfigFile", false);
    if (tmp != "") {
        string sensitivityConfigFile = inputPath + "/" + tmp;
        LOG("Load sensitivity scenario data from file" << sensitivityConfigFile);
        sensiScenarioData_->fromFile(sensitivityConfigFile);
    } else {
        WLOG("Sensitivity scenario data not found");
    }

    tmp = params_->get("sensitivity", "outputSensitivityThreshold", false); 
    if (tmp != "")
        sensiThreshold_ = parseReal(tmp);
        
    /************
     * Simulation
     ************/
    
    tmp = params_->get("simulation", "active", false);
    if (tmp == "Y")
        simulation_ = true;

    exposureSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    crossAssetModelData_ = boost::make_shared<CrossAssetModelData>();
    scenarioGeneratorData_ = boost::make_shared<ScenarioGeneratorData>();
    tmp = params_->get("simulation", "simulationConfigFile", false) ;
    if (tmp != "") {
        string simulationConfigFile = inputPath + "/" + tmp;
        LOG("Loading simulation config from file" << simulationConfigFile);
        exposureSimMarketParams_->fromFile(simulationConfigFile);
        crossAssetModelData_->fromFile(simulationConfigFile);
        scenarioGeneratorData_->fromFile(simulationConfigFile);
        auto grid = scenarioGeneratorData_->getGrid();
        LOG("grid size=" << grid->size() << ", dates=" << grid->dates().size()
            << ", valuationDates=" << grid->valuationDates().size()
            << ", closeOutDates=" << grid->closeOutDates().size());
    } else {
        WLOG("Simulation market, model and scenario generator data not loaded");
    }

    simulationPricingEngine_ = boost::make_shared<EngineData>();
    tmp = params_->get("simulation", "pricingEnginesFile", false);
    if (tmp != "") {
        string pricingEnginesFile = inputPath + "/" + tmp;
        LOG("Load simulation pricing engine data from file: " << pricingEnginesFile);
        simulationPricingEngine_->fromFile(pricingEnginesFile);
    } else {
        WLOG("Simulation pricing engine data not found, using standard pricing engines");
        simulationPricingEngine_ = pricingEngine_;
    }

    nettingSetManager_ = boost::make_shared<NettingSetManager>();
    tmp = params_->get("xva", "csaFile", false);
    if (tmp != "") {
        string csaFile = inputPath + "/" + tmp;
        LOG("Loading netting and csa data from file" << csaFile);
        nettingSetManager_->fromFile(csaFile);
    } else {
        // FIXME: log level depending on chosen analytics
        WLOG("Netting/CSA data not found");
    }

    exposureBaseCurrency_ = baseCurrency_;
    tmp = params_->get("simulation", "baseCurrency", false);
    if (tmp != "")
        exposureBaseCurrency_ = tmp;

    tmp = params_->get("simulation", "observationModel", false);
    if (tmp != "")
        exposureObservationModel_ = tmp;
    else
        exposureObservationModel_ = observationModel_;
        
    tmp = params_->get("simulation", "storeFlows", false);
    if (tmp == "Y")
        storeFlows_ = true;

    tmp = params_->get("simulation", "storeSurvivalProbabilities", false);
    if (tmp == "Y")
        storeSurvivalProbabilities_ = true;

    tmp = params_->get("simulation", "nettingSetId", false);
    if (tmp != "")
        nettingSetId_ = tmp;

    tmp = params_->get("simulation", "cubeFile", false);
    if (tmp != "") {
        writeCube_ = true;
    }

    tmp = params_->get("simulation", "scenariodump", false);
    if (tmp != "") {
        writeScenarios_ = true;
    }

    /**********************
     * XVA
     **********************/
    
    tmp = params_->get("xva", "baseCurrency", false);
    if (tmp != "")
        xvaBaseCurrency_ = tmp;
    else
        xvaBaseCurrency_ = exposureBaseCurrency_;
        
    tmp = params_->get("simulation", "active", false);
    if (tmp != "Y")
        loadCube_ = true;

    tmp = params_->get("xva", "cubeFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        tmp = params_->get("xva", "hyperCube", false);
        if (tmp != "")
            hyperCube_ = parseBool(tmp);        
        if (hyperCube_)
            cube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
        else
            cube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
        LOG("Load cube from file " << cubeFile);
        cube_->load(cubeFile);
        LOG("Cube loading done: ids=" << cube_->numIds() << " dates=" << cube_->numDates()
            << " samples=" << cube_->samples() << " depth=" << cube_->depth());
    }

    tmp = params_->get("xva", "nettingSetCubeFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        tmp = params_->get("xva", "nettingSetHyperCube", false);
        if (tmp != "")
            hyperNettingSetCube_ = parseBool(tmp);        
        if (hyperNettingSetCube_)
            nettingSetCube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
        else
            nettingSetCube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
        LOG("Load nettingset cube from file " << cubeFile);
        nettingSetCube_->load(cubeFile);
        LOG("NettingSetCube loading done: ids=" << nettingSetCube_->numIds() << " dates=" << nettingSetCube_->numDates()
            << " samples=" << nettingSetCube_->samples() << " depth=" << nettingSetCube_->depth());
    }

    tmp = params_->get("xva", "cptyCubeFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        tmp = params_->get("xva", "cptyHyperCube", false);
        if (tmp != "")
            hyperCptyCube_ = parseBool(tmp);        
        if (hyperCptyCube_)
            cptyCube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>();
        else
            cptyCube_ = boost::make_shared<SinglePrecisionInMemoryCube>();
        LOG("Load cpty cube from file " << cubeFile);
        cptyCube_->load(cubeFile);
        LOG("CptyCube loading done: ids=" << cptyCube_->numIds() << " dates=" << cptyCube_->numDates()
            << " samples=" << cptyCube_->samples() << " depth=" << cptyCube_->depth());
    }

    tmp = params_->get("xva", "scenarioFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        mktCube_ = boost::make_shared<InMemoryAggregationScenarioData>();
        mktCube_->load(cubeFile);
        LOG("MktCube loading done");
    }
    
    tmp = params_->get("xva", "flipViewXVA", false);
    if (tmp != "")
        flipViewXVA_ = parseBool(tmp);

    tmp = params_->get("xva", "fullInitialCollateralisation", false);
    if (tmp != "")
        fullInitialCollateralisation_ = parseBool(tmp);

    tmp = params_->get("xva", "exposureProfilesByTrade", false);
    if (tmp != "")
        exposureProfilesByTrade_ = parseBool(tmp);

    tmp = params_->get("xva", "exposureProfiles", false);
    if (tmp != "")
        exposureProfiles_ = parseBool(tmp);

    tmp = params_->get("xva", "quantile", false);
    if (tmp != "")
        pfeQuantile_ = parseReal(tmp);

    tmp = params_->get("xva", "collateralCalculationType", false);
    if (tmp != "")
        collateralCalculationType_ = tmp;

    tmp = params_->get("xva", "exposureAllocationMethod", false);
    if (tmp != "")
        exposureAllocationMethod_ = tmp;

    tmp = params_->get("xva", "marginalAllocationLimit", false);
    if (tmp != "")
        marginalAllocationLimit_ = parseReal(tmp);

    tmp = params_->get("xva", "exerciseNextBreak", false);
    if (tmp != "")
        exerciseNextBreak_ = parseBool(tmp);

    tmp = params_->get("xva", "cva", false);
    if (tmp != "")
        cvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "dva", false);
    if (tmp != "")
        dvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "fva", false);
    if (tmp != "")
        fvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "colva", false);
    if (tmp != "")
        colvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "collateralFloor", false);
    if (tmp != "")
        collateralFloorAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "dim", false);
    if (tmp != "")
        dimAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "mva", false);
    if (tmp != "")
        mvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "kva", false);
    if (tmp != "")
        kvaAnalytic_ = parseBool(tmp);

    tmp = params_->get("xva", "dynamicCredit", false);
    if (tmp != "")
        dynamicCredit_ = parseBool(tmp);

    tmp = params_->get("xva", "cvaSensi", false);
    if (tmp != "")
        cvaSensi_ = parseBool(tmp);

    tmp = params_->get("xva", "cvaSensiGrid", false);
    if (tmp != "")
        cvaSensiGrid_ = parseListOfValues<Period>(tmp, &parsePeriod);

    tmp = params_->get("xva", "cvaSensiShiftSize", false);
    if (tmp != "")
        cvaSensiShiftSize_ = parseReal(tmp);

    tmp = params_->get("xva", "dvaName", false);
    if (tmp != "")
        dvaName_ = tmp;

    tmp = params_->get("xva", "rawCubeOutputFile", false);
    if (tmp != "") {
        rawCubeOutputFile_ = tmp;
        rawCubeOutput_= true;
    }

    tmp = params_->get("xva", "netCubeOutputFile", false);
    if (tmp != "") {
        netCubeOutputFile_ = tmp;
        netCubeOutput_ = true;
    }

    // FVA

    tmp = params_->get("xva", "fvaBorrowingCurve", false);
    if (tmp != "")
        fvaBorrowingCurve_ = tmp;

    tmp = params_->get("xva", "fvaLendingCurve", false);
    if (tmp != "")
        fvaLendingCurve_ = tmp;

    tmp = params_->get("xva", "flipViewBorrowingCurvePostfix", false);
    if (tmp != "")
        flipViewBorrowingCurvePostfix_ = tmp;

    tmp = params_->get("xva", "flipViewLendingCurvePostfix", false);
    if (tmp != "")
        flipViewLendingCurvePostfix_ = tmp;

    // DIM

    tmp = params_->get("xva", "dimQuantile", false);
    if (tmp != "")
        dimQuantile_ = parseReal(tmp);

    tmp = params_->get("xva", "dimHorizonCalendarDays", false);
    if (tmp != "")
        dimHorizonCalendarDays_ = parseInteger(tmp);

    tmp = params_->get("xva", "dimRegressionOrder", false);
    if (tmp != "")
        dimRegressionOrder_ = parseInteger(tmp);

    tmp = params_->get("xva", "dimRegressors", false);
    if (tmp != "")
        dimRegressors_ = parseListOfValues(tmp);

    tmp = params_->get("xva", "dimLocalRegressionEvaluations", false);
    if (tmp != "")
        dimLocalRegressionEvaluations_ = parseInteger(tmp);

    tmp = params_->get("xva", "dimLocalRegressionBandwidth", false);
    if (tmp != "")
        dimLocalRegressionBandwidth_ = parseReal(tmp);

    // KVA

    tmp = params_->get("xva", "kvaCapitalDiscountRate", false);
    if (tmp != "")
        kvaCapitalDiscountRate_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaAlpha", false);
    if (tmp != "")
        kvaAlpha_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaRegAdjustment", false);
    if (tmp != "")
        kvaRegAdjustment_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaCapitalHurdle", false);
    if (tmp != "")
        kvaCapitalHurdle_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaOurPdFloor", false);
    if (tmp != "")
        kvaOurPdFloor_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaTheirPdFloor", false);
    if (tmp != "")
        kvaTheirPdFloor_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaOurCvaRiskWeight", false);
    if (tmp != "")
        kvaOurCvaRiskWeight_ = parseReal(tmp);

    tmp = params_->get("xva", "kvaTheirCvaRiskWeight", false);
    if (tmp != "")
        kvaTheirCvaRiskWeight_ = parseReal(tmp);

    // cashflow npv and dynamic backtesting

    tmp = params_->get("cashflow", "cashFlowHorizon", false);
    if (tmp != "")
        cashflowHorizon_ = parseDate(tmp);

    tmp = params_->get("cashflow", "portfolioFilterDate", false);
    if (tmp != "")
        portfolioFilterDate_ = parseDate(tmp);
    
    /**********************
     * Build the CSV Loader
     */
    vector<string> marketFiles = {};
    tmp = params_->get("setup", "marketDataFile", false);
    if (tmp != "") {
        marketFiles = getFilenames(tmp, inputPath);
    } else {
        ALOG("market data file not found");
    }

    vector<string> fixingFiles = {};
    tmp = params_->get("setup", "fixingDataFile", false);
    if (tmp != "") {
        fixingFiles = getFilenames(tmp, inputPath);
    } else {
        ALOG("fixing data file not found");
    }
    
    vector<string> dividendFiles = {};
    tmp = params_->get("setup", "dividendDataFile", false);
    if (tmp != "") {
        dividendFiles = getFilenames(tmp, inputPath);
    } else {
        WLOG("dividend data file not found");
    }

    csvLoader_ = boost::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings_);

    /*****************************
     * Collect requested run types
     */
    runTypes_.clear();
    
    if (params_->get("npv", "active", false) == "Y")
        runTypes_.push_back("NPV");
    
    if (params_->get("cashflow", "active", false) == "Y")
        runTypes_.push_back("CASHFLOW");

    if (params_->get("sensitivity", "active", false) == "Y")
        runTypes_.push_back("SENSITIVITY");

    if (params_->get("parametricVar", "active", false) == "Y")
        runTypes_.push_back("VAR");

    if (simulation_)
        runTypes_.push_back("EXPOSURE");

    if (params_->get("xva", "active", false) == "Y") {
        if (cvaAnalytic_)
            runTypes_.push_back("CVA");
        if (dvaAnalytic_)
            runTypes_.push_back("DVA");
        if (fvaAnalytic_)
            runTypes_.push_back("FVA");
        if (colvaAnalytic_)
            runTypes_.push_back("COLVA");
        if (collateralFloorAnalytic_)
            runTypes_.push_back("COLLATERALFLOOR");
        if (dimAnalytic_)
            runTypes_.push_back("DIM");
        if (mvaAnalytic_)
            runTypes_.push_back("MVA");
        if (kvaAnalytic_)
            runTypes_.push_back("KVA");
    }
    
    /***************************
     * Collect output file names
     */

    npvOutputFileName_ = params_->get("npv", "outputFileName", false);
    cashflowOutputFileName_ = params_->get("cashflow", "outputFileName", false);
    curvesOutputFileName_ = params_->get("curves", "outputFileName", false);
    scenarioDumpFileName_ = params_->get("simulation", "scenariodump", false);
    cubeFileName_ = params_->get("simulation", "cubeFile", false);
    mktCubeFileName_ = params_->get("simulation", "aggregationScenarioDataFileName", false);
    rawCubeFileName_ = params_->get("xva", "rawCubeOutputFile", false);
    netCubeFileName_ = params_->get("xva", "netCubeOutputFile", false);

    // map internal key to output file name
    fileNameMap_["npv"] = npvOutputFileName_;
    fileNameMap_["cashflow"] = cashflowOutputFileName_;
    fileNameMap_["curves"] = curvesOutputFileName_;
    fileNameMap_["cube"] = cubeFileName_;
    fileNameMap_["scenariodata"] = mktCubeFileName_;
    fileNameMap_["rawcube"] = rawCubeFileName_;
    fileNameMap_["netcube"] = netCubeFileName_;
    
    LOG("OREAppInputParameters complete");
}

std::string OREAppInputParameters::outputFileName(const std::string& internalName, const std::string& suffix) {
    auto it = fileNameMap_.find(internalName);
    if (it == fileNameMap_.end() || it->second == "")
        return internalName + "." + suffix;
    else        
        return it->second; // contains suffix
}

void OREAppInputParameters::writeOutParameters() {
    QL_FAIL("OREAppInputParameters::writeOutParameters() not implmented");    
}

} // namespace analytics
} // namespace ore
