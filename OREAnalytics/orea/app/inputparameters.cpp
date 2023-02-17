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
#include <orea/cube/cube_io.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
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

    // Switch default values for backward compatibility
    entireMarket_ = true; 
    allFixings_ = true; 
    eomInflationFixings_ = false;
    useMarketDataFixings_ = false;
    iborFallbackOverride_ = false;
    dryRun_ = false;
    outputAdditionalResults_ = false;

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
        boost::shared_ptr<TradeFactory> tf = boost::make_shared<TradeFactory>(refDataManager_);
        for (auto portfolioFile : portfolioFiles) {
            LOG("Loading portfolio from file: " << portfolioFile);
            portfolio_->load(portfolioFile, tf);
        }
    } else {
        ALOG("Portfolio data not found");
    }

    if (params_->hasGroup("markets")) {
        marketConfigs_ = params_->markets();
        for (auto m: marketConfigs_)
        LOG("MarketContext::" << m.first << " = " << m.second);
    }
    
    /*************
     * NPV
     *************/

    tmp = params_->get("npv", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        analytics_.insert("NPV");

    tmp = params_->get("npv", "additionalResults", false);
    if (tmp != "")
        outputAdditionalResults_ = parseBool(tmp);

    /*************
     * CASHFLOW
     *************/

    tmp = params_->get("cashflow", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        analytics_.insert("CASHFLOW");

    tmp = params_->get("cashflow", "includePastCashflows", false);
    if (tmp != "")
        includePastCashflows_ = parseBool(tmp);
    
    /*************
     * Curves
     *************/

    tmp = params_->get("curves", "active", false);
    if (tmp != "") 
        outputCurves_ = parseBool(tmp);
        
    tmp = params_->get("curves", "grid", false);
    if (tmp != "") 
        curvesGrid_ = tmp;

    tmp = params_->get("curves", "configuration", false);
    if (tmp != "") 
        curvesMarketConfig_ = tmp;

    tmp = params_->get("curves", "outputTodaysMarketCalibration", false);
    if (tmp != "")
        outputTodaysMarketCalibration_ = parseBool(tmp);
    
    /*************
     * SENSITIVITY
     *************/

    // FIXME: To be loaded from params or removed from the base class
    // xbsParConversion_ = false;
    // analyticFxSensis_ = true;
    // useSensiSpreadedTermStructures_ = true;

    tmp = params_->get("sensitivity", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        analytics_.insert("SENSITIVITY");

        tmp = params_->get("sensitivity", "parSensitivity", false);
        if (tmp != "")
            parSensi_ = parseBool(tmp);

        tmp = params_->get("sensitivity", "outputJacobi", false);
        if (tmp != "")
            outputJacobi_ = parseBool(tmp);

        tmp = params_->get("sensitivity", "alignPillars", false);
        if (tmp != "")
            alignPillars_ = parseBool(tmp);

        sensiSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
        tmp = params_->get("sensitivity", "marketConfigFile", false);
        if (tmp != "") {
            string marketConfigFile = inputPath + "/" + tmp;
            LOG("Loading sensitivity scenario sim market parameters from file" << marketConfigFile);
            sensiSimMarketParams_->fromFile(marketConfigFile);
        } else {
            WLOG("ScenarioSimMarket parameters for sensitivity not loaded");
        }

        sensiScenarioData_ = boost::make_shared<SensitivityScenarioData>();
        tmp = params_->get("sensitivity", "sensitivityConfigFile", false);
        if (tmp != "") {
            string sensitivityConfigFile = inputPath + "/" + tmp;
            LOG("Load sensitivity scenario data from file" << sensitivityConfigFile);
            sensiScenarioData_->fromFile(sensitivityConfigFile);
        } else {
            WLOG("Sensitivity scenario data not loaded");
        }

        sensiPricingEngine_ = boost::make_shared<EngineData>();
        tmp = params_->get("sensitivity", "pricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = inputPath + "/" + tmp;
            LOG("Load pricing engine data from file: " << pricingEnginesFile);
            sensiPricingEngine_->fromFile(pricingEnginesFile);
        } else {
            WLOG("Pricing engine data not found for sensitivity analysis, using global");
            sensiPricingEngine_ = pricingEngine_;
        }

        tmp = params_->get("sensitivity", "outputSensitivityThreshold", false);
        if (tmp != "")
            sensiThreshold_ = parseReal(tmp);
    }

    /****************
     * STRESS
     ****************/

    tmp = params_->get("stress", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        analytics_.insert("STRESS");
        stressPricingEngine_ = pricingEngine_;
        stressSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
        tmp = params_->get("stress", "marketConfigFile", false);
        if (tmp != "") {
            string marketConfigFile = inputPath + "/" + tmp;
            LOG("Loading stress test scenario sim market parameters from file" << marketConfigFile);
            stressSimMarketParams_->fromFile(marketConfigFile);
        } else {
            WLOG("ScenarioSimMarket parameters for stress testing not loaded");
        }
        
        stressScenarioData_ = boost::make_shared<StressTestScenarioData>();
        tmp = params_->get("stress", "stressConfigFile", false);
        if (tmp != "") {
            string configFile = inputPath + "/" + tmp;
            LOG("Load stress test scenario data from file" << configFile);
            stressScenarioData_->fromFile(configFile);
        } else {
            WLOG("Stress scenario data not loaded");
        }
        
        stressPricingEngine_ = boost::make_shared<EngineData>();
        tmp = params_->get("stress", "pricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = inputPath + "/" + tmp;
            LOG("Load pricing engine data from file: " << pricingEnginesFile);
            stressPricingEngine_->fromFile(pricingEnginesFile);
        } else {
            WLOG("Pricing engine data not found for stress testing, using global");
        }
        
        tmp = params_->get("stress", "outputThreshold", false); 
        if (tmp != "")
            stressThreshold_ = parseReal(tmp);
    }
    
    /****************
     * VaR
     ****************/

    tmp = params_->get("parametricVar", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        analytics_.insert("VAR");
        
        tmp = params_->get("parametricVar", "salvageCovarianceMatrix", false);
        if (tmp != "")
            salvageCovariance_ = parseBool(tmp);
        
        tmp = params_->get("parametricVar", "quantiles", false);
        if (tmp != "")
            varQuantiles_ = parseListOfValues<Real>(tmp, &parseReal);
        
        tmp = params_->get("parametricVar", "breakdown", false);
        if (tmp != "")
            varBreakDown_ = parseBool(tmp);
        
        tmp = params_->get("parametricVar", "portfolioFilter", false);
        if (tmp != "")
            portfolioFilter_ = tmp;
        
        tmp = params_->get("parametricVar", "method", false);
        if (tmp != "")
            varMethod_ = tmp;
        
        tmp = params_->get("parametricVar", "mcSamples", false);
        if (tmp != "")
            mcVarSamples_ = parseInteger(tmp);
        
        tmp = params_->get("parametricVar", "mcSeed", false);
        if (tmp != "")
            mcVarSeed_ = parseInteger(tmp);
        
        tmp = params_->get("parametricVar", "covarianceInputFile", false);
        QL_REQUIRE(tmp != "", "covarianceInputFile not provided");
        std::string covFile = inputPath + "/" + tmp;
        LOG("Load Covariance Data from file " << covFile);
        ore::data::CSVFileReader reader(covFile, false);
        std::vector<std::string> dummy;
        while (reader.next()) {
            covarianceData_[std::make_pair(*parseRiskFactorKey(reader.get(0), dummy),
                                           *parseRiskFactorKey(reader.get(1), dummy))] =
                ore::data::parseReal(reader.get(2));
        }
        LOG("Read " << covarianceData_.size() << " valid covariance data lines from file " << covFile);
        
        tmp = params_->get("parametricVar", "sensitivityInputFile", false);
        QL_REQUIRE(tmp != "", "sensitivityInputFile not provided");
        std::string sensiFile = inputPath + "/" + tmp;
        LOG("Get sensitivity data from file " << sensiFile);
        sensitivityStream_ = boost::make_shared<SensitivityFileStream>(sensiFile);
    }

    /************
     * Simulation
     ************/

    tmp = params_->get("simulation", "active", false);
    if (!tmp.empty() && parseBool(tmp)) {
        analytics_.insert("EXPOSURE");
    }

    // check this here because we need to know 10 lines below
    tmp = params_->get("xva", "active", false);
    if (!tmp.empty() && parseBool(tmp))
        analytics_.insert("XVA");
    
    tmp = params_->get("simulation", "amc", false);
    if (tmp != "")
        amc_ = parseBool(tmp);

    tmp = params_->get("simulation", "amcTradeTypes", false);
    if (tmp != "") {
        auto v = parseListOfValues(tmp);
        amcTradeTypes_ = std::set<std::string>(v.begin(), v.end());
    }

    simulationPricingEngine_ = pricingEngine_;
    exposureObservationModel_ = observationModel_;
    exposureBaseCurrency_ = baseCurrency_;

    if (analytics_.find("EXPOSURE") != analytics_.end() ||
        analytics_.find("XVA") != analytics_.end()) {
        exposureSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
        crossAssetModelData_ = boost::make_shared<CrossAssetModelData>();
        // A bit confusing: The scenario generator data are needed for XVA post-processing
        // even if we do not simulate: the simulation grid
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
            ALOG("Simulation market, model and scenario generator data not loaded");
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

        amcPricingEngine_ = boost::make_shared<EngineData>();
        tmp = params_->get("simulation", "amcPricingEnginesFile", false);
        if (tmp != "") {
            string pricingEnginesFile = inputPath + "/" + tmp;
            LOG("Load amc pricing engine data from file: " << pricingEnginesFile);
            amcPricingEngine_->fromFile(pricingEnginesFile);
        } else {
            WLOG("AMC pricing engine data not found, using standard pricing engines");
            amcPricingEngine_ = pricingEngine_;
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
    }

    /**********************
     * XVA specifically
     **********************/

    tmp = params_->get("xva", "baseCurrency", false);
    if (tmp != "")
        xvaBaseCurrency_ = tmp;
    else
        xvaBaseCurrency_ = exposureBaseCurrency_;

    if (analytics_.find("XVA") != analytics_.end() &&
        analytics_.find("EXPOSURE") == analytics_.end()) {
        loadCube_ = true;
        tmp = params_->get("xva", "cubeFile", false);
        if (tmp != "") {
            string cubeFile = resultsPath_.string() + "/" + tmp;
            LOG("Load cube from file " << cubeFile);
            cube_ = ore::analytics::loadCube(cubeFile);
            LOG("Cube loading done: ids=" << cube_->numIds() << " dates=" << cube_->numDates()
                << " samples=" << cube_->samples() << " depth=" << cube_->depth());
        } else {
            ALOG("cube file name not provided");
        }
    }

    nettingSetManager_ = boost::make_shared<NettingSetManager>();

    if (analytics_.find("XVA") != analytics_.end() ||
        analytics_.find("EXPOSURE") != analytics_.end()) {
        tmp = params_->get("xva", "csaFile", false);
        QL_REQUIRE(tmp != "", "Netting set manager is required for XVA");
        string csaFile = inputPath + "/" + tmp;
        LOG("Loading netting and csa data from file" << csaFile);
        nettingSetManager_->fromFile(csaFile);
    }
    
    tmp = params_->get("xva", "nettingSetCubeFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        LOG("Load nettingset cube from file " << cubeFile);
        nettingSetCube_ = ore::analytics::loadCube(cubeFile);
        LOG("NettingSetCube loading done: ids=" << nettingSetCube_->numIds() << " dates=" << nettingSetCube_->numDates()
            << " samples=" << nettingSetCube_->samples() << " depth=" << nettingSetCube_->depth());
    }

    tmp = params_->get("xva", "cptyCubeFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        LOG("Load cpty cube from file " << cubeFile);
        cptyCube_ = ore::analytics::loadCube(cubeFile);
        LOG("CptyCube loading done: ids=" << cptyCube_->numIds() << " dates=" << cptyCube_->numDates()
            << " samples=" << cptyCube_->samples() << " depth=" << cptyCube_->depth());
    }

    tmp = params_->get("xva", "scenarioFile", false);
    if (loadCube_ && tmp != "") {
        string cubeFile = resultsPath_.string() + "/" + tmp;
        LOG("Load agg scen data from file " << cubeFile);
        mktCube_ = loadAggregationScenarioData(cubeFile);
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

    tmp = params_->get("xva", "calculationType", false);
    if (tmp != "")
        collateralCalculationType_ = tmp;

    tmp = params_->get("xva", "allocationMethod", false);
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

    tmp = params_->get("xva", "dimOutputGridPoints", false);
    if (tmp != "")
        dimOutputGridPoints_ = parseListOfValues<Size>(tmp, parseInteger);

    tmp = params_->get("xva", "dimOutputNettingSet", false);
    if (tmp != "")
        dimOutputNettingSet_ = tmp;
    
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
    dimEvolutionFileName_ = params_->get("xva", "dimEvolutionFile", false);    
    tmp = params_->get("xva", "dimRegressionFiles", false);
    if (tmp != "")
        dimRegressionFileNames_ = parseListOfValues(tmp);
    sensitivityFileName_ = params_->get("sensitivity", "sensitivityOutputFile", false);    
    parSensitivityFileName_ = params_->get("sensitivity", "parSensitivityOutputFile", false);    
    jacobiFileName_ = params_->get("sensitivity", "jacobiOutputFile", false);    
    jacobiInverseFileName_ = params_->get("sensitivity", "jacobiInverseOutputFile", false);    
    sensitivityScenarioFileName_ = params_->get("sensitivity", "scenarioOutputFile", false);    
    stressTestFileName_ = params_->get("stress", "scenarioOutputFile", false);    
    varFileName_ = params_->get("parametricVar", "outputFile", false);
    
    // map internal report name to output file name
    fileNameMap_["npv"] = npvOutputFileName_;
    fileNameMap_["cashflow"] = cashflowOutputFileName_;
    fileNameMap_["curves"] = curvesOutputFileName_;
    fileNameMap_["cube"] = cubeFileName_;
    fileNameMap_["scenariodata"] = mktCubeFileName_;
    fileNameMap_["scenario"] = scenarioDumpFileName_;
    fileNameMap_["rawcube"] = rawCubeFileName_;
    fileNameMap_["netcube"] = netCubeFileName_;
    fileNameMap_["dim_evolution"] = dimEvolutionFileName_;
    fileNameMap_["sensitivity"] = sensitivityFileName_;
    fileNameMap_["sensitivity_scenario"] = sensitivityScenarioFileName_;
    fileNameMap_["parSensitivity"] = parSensitivityFileName_;
    fileNameMap_["jacobi"] = jacobiFileName_;
    fileNameMap_["jacobi_inverse"] = jacobiInverseFileName_;
    fileNameMap_["stress"] = stressTestFileName_;
    fileNameMap_["var"] = varFileName_;
    
    QL_REQUIRE(dimOutputGridPoints_.size() == dimRegressionFileNames_.size(),
               "dim regression output grid points size (" << dimOutputGridPoints_.size() << ") "
               << "and file names size (" << dimRegressionFileNames_.size() << ") do not match");
    for (Size i = 0; i < dimRegressionFileNames_.size(); ++i)
        fileNameMap_["dim_regression_" + to_string(i)] = dimRegressionFileNames_[i];
    
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
