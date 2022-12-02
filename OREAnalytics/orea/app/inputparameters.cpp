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

    LOG("OREAppInputParameters starting");

    QL_REQUIRE(params_->hasGroup("setup"), "parameter group 'setup' missing");

    std::string inputPath = params_->get("setup", "inputPath");
    std::string outputPath = params_->get("setup", "outputPath");

    // FIXME
    bool continueOnError_ = false;
    if (params_->has("setup", "continueOnError"))
        continueOnError_ = parseBool(params_->get("setup", "continueOnError"));

    // FIXME
    bool lazyMarketBuilding_ = true;
    if (params_->has("setup", "lazyMarketBuilding"))
        lazyMarketBuilding_ = parseBool(params_->get("setup", "lazyMarketBuilding"));

    // FIXME
    bool buildFailedTrades_ = false;
    if (params_->has("setup", "buildFailedTrades"))
        buildFailedTrades_ = parseBool(params_->get("setup", "buildFailedTrades"));

    if (params_->has("setup", "observationModel")) {
        string om = params_->get("setup", "observationModel");
        ObservationMode::instance().setMode(om);
        LOG("Observation Mode is " << om);
    }

    // TODO ?
    // Load calendar adjustments
    if (params_->has("setup", "calendarAdjustment") && params_->get("setup", "calendarAdjustment") != "") {
        CalendarAdjustmentConfig calendarAdjustments;
        string calendarAdjustmentFile = inputPath + "/" + params_->get("setup", "calendarAdjustment");
        LOG("Loading calendar adjustments from file: " << calendarAdjustmentFile);
        calendarAdjustments.fromFile(calendarAdjustmentFile);
    } else {
        WLOG("Calendar adjustments not found, using defaults");
    }

    // TODO ?
    // Load currency configs
    if (params_->has("setup", "currencyConfiguration") && params_->get("setup", "currencyConfiguration") != "") {
        CurrencyConfig currencyConfig;
        string currencyConfigFile = inputPath + "/" + params_->get("setup", "currencyConfiguration");
        LOG("Loading currency configurations from file: " << currencyConfigFile);
        currencyConfig.fromFile(currencyConfigFile);
    } else {
        WLOG("Currency configurations not found, using defaults");
    }

    asof_ = parseDate(params_->get("setup", "asofDate"));

    resultsPath_ = outputPath;

    // FIXME: allow different base currencies by analytic in OREApp
    baseCurrency_ = params_->get("npv", "baseCurrency");

    // FIXME
    discountIndex_ = "";

    // FIXME
    entireMarket_ = false;
    allFixings_ = false;
    eomInflationFixings_ = false; 
    useMarketDataFixings_ = false;
    validateIdentifiers_ = false;
    filterNettingSets_ = false;
    marketDataFilter_ = "";

    // Load reference data
    if (params_->has("setup", "referenceDataFile") && params_->get("setup", "referenceDataFile") != "") {
        string refDataFile = inputPath + "/" + params_->get("setup", "referenceDataFile");
        LOG("Loading reference data from file: " << refDataFile);
        refDataManager_ = boost::make_shared<BasicReferenceDataManager>(refDataFile);
    } else {
        WLOG("Reference data not found");
    }

    // Load conventions
    conventions_ = boost::make_shared<Conventions>();
    if (params_->has("setup", "conventionsFile") && params_->get("setup", "conventionsFile") != "") {
        string conventionsFile = inputPath + "/" + params_->get("setup", "conventionsFile");
        LOG("Loading conventions from file: " << conventionsFile);
        conventions_->fromFile(conventionsFile);
    } else {
        ALOG("Conventions not found");
    }

    // Load IBOR fallbacks
    iborFallbackConfig_ = boost::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig());
    if (params_->has("setup", "iborFallbackConfig") && params_->get("setup", "iborFallbackConfig") != "") {
        std::string tmp = inputPath + "/" + params_->get("setup", "iborFallbackConfig");
        LOG("Loading Ibor fallback config from file: " << tmp);
        iborFallbackConfig_->fromFile(tmp);
    } else {
        WLOG("Using default Ibor fallback config");
    }

    iborFallbackOverride_ = false;

    // Load portfolio
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

    // Load curve configs
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
    
    // Load pricing engine configs
    pricingEngine_ = boost::make_shared<EngineData>();
    if (params_->has("setup", "pricingEnginesFile") && params_->get("setup", "pricingEnginesFile") != "") {
        string pricingEnginesFile = inputPath + "/" + params_->get("setup", "pricingEnginesFile");
        LOG("Load pricing engine data from file: " << pricingEnginesFile);
        pricingEngine_->fromFile(pricingEnginesFile);
    } else {
        ALOG("Pricing engine data not found");
    }
    
    xbsParConversion_ = false;
    useSensiSpreadedTermStructures_ = true;

    // Load today's market parameters
    todaysMarketParams_ = boost::make_shared<TodaysMarketParameters>();
    if (params_->has("setup", "marketConfigFile") && params_->get("setup", "marketConfigFile") != "") {
        string marketConfigFile = inputPath + "/" + params_->get("setup", "marketConfigFile");
        LOG("Loading today's market parameters from file" << marketConfigFile);
        todaysMarketParams_->fromFile(marketConfigFile);
    } else {
        ALOG("Today's market parameters not found");
    }
    QL_REQUIRE(todaysMarketParams_, "Today's market parameters not set");
    
    // Load netting and CSA details
    nettingSetManager_ = boost::make_shared<NettingSetManager>();
    if (params_->hasGroup("xva") &&
        params_->has("xva", "csaFile") &&
        params_->get("xva", "csaFile") != "") {
        string csaFile = inputPath + "/" + params_->get("xva", "csaFile");
        LOG("Loading netting and csa data from file" << csaFile);
        nettingSetManager_->fromFile(csaFile);
    } else {
        // FIXME: log level depending on chosen analytics
        WLOG("Netting/CSA data not found");
    }

    sensiSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    if (params_->hasGroup("sensitivity") &&
        params_->has("sensitivity", "marketConfigFile") &&
        params_->get("sensitivity", "marketConfigFile") != "") {
        string marketConfigFile = inputPath + "/" + params_->get("sensitivity", "marketConfigFile");
        LOG("Loading scenario sim market parameters from file" << marketConfigFile);
        sensiSimMarketParams_->fromFile(marketConfigFile);
    } else {
        WLOG("ScenarioSimMarket parameters not loaded");
    }
    
    sensiScenarioData_ = boost::make_shared<SensitivityScenarioData>();
    if (params_->hasGroup("sensitivity") &&
        params_->has("sensitivity", "sensitivityConfigFile") &&
        params_->get("sensitivity", "sensitivityConfigFile") != "") {
        string sensitivityConfigFile = inputPath + "/" + params_->get("sensitivity", "sensitivityConfigFile");
        LOG("Load sensitivity scenario data from file" << sensitivityConfigFile);
        sensiScenarioData_->fromFile(sensitivityConfigFile);
    } else {
        WLOG("Sensitivity scenario data not found");
    }

    sensiThreshold_ = 0;
    if (params_->hasGroup("sensitivity") &&
        params_->has("sensitivity", "outputSensitivityThreshold"))
        sensiThreshold_ = parseReal(params_->get("sensitivity", "outputSensitivityThreshold"));
        
    bool implyTodaysFixings = false;
    if (params_->has("setup", "implyTodaysFixings")) {
        implyTodaysFixings = ore::data::parseBool(params_->get("setup", "implyTodaysFixings"));
    } else {
        WLOG("implyTodaysFixings not found, using default false");
    }
    
    exposureSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    crossAssetModelData_ = boost::make_shared<CrossAssetModelData>();
    scenarioGeneratorData_ = boost::make_shared<ScenarioGeneratorData>();
    if (params_->hasGroup("simulation") &&
        params_->has("simulation", "simulationConfigFile") &&
        params_->get("simulation", "simulationConfigFile") != "") {
        string simulationConfigFile = inputPath + "/" + params_->get("simulation", "simulationConfigFile");
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
    if (params_->hasGroup("simulation") &&
        params_->has("simulation", "pricingEnginesFile") &&
        params_->get("simulation", "pricingEnginesFile") != "") {
        string pricingEnginesFile = inputPath + "/" + params_->get("simulation", "pricingEnginesFile");
        LOG("Load simulation pricing engine data from file: " << pricingEnginesFile);
        simulationPricingEngine_->fromFile(pricingEnginesFile);
    } else {
        WLOG("Simulation pricing engine data not found, using standard pricing engines");
        simulationPricingEngine_ = pricingEngine_;
    }

    storeSurvivalProbabilities_ = false;
    if (params_->hasGroup("simulation") &&
        params_->has("simulation", "storeSurvivalProbabilities") &&
        params_->get("simulation", "storeSurvivalProbabilities") == "Y") {
        storeSurvivalProbabilities_ = true;
    }

    exposureBaseCurrency_ = baseCurrency_;
    if (params_->hasGroup("simulation") &&
        params_->has("simulation", "baseCurrency") &&
        params_->get("simulation", "baseCurrency") != "")
        exposureBaseCurrency_ = params_->get("simulation", "baseCurrency");

    flipViewXVA_ = false;
    if (params_->hasGroup("xva") &&
        params_->has("xva", "flipViewXVA") &&
        params_->get("xva", "flipViewXVA") != "")
        flipViewXVA_ = parseBool(params_->get("xva", "flipViewXVA"));

    dvaName_ = "";
    if (params_->hasGroup("xva") &&
        params_->has("xva", "dvaName") &&
        params_->get("xva", "dvaName") != "")
        dvaName_ = params_->get("xva", "dvaName");

    /**********************
     * Build the CSV Loader
     */
    vector<string> marketFiles = {};
    if (params_->has("setup", "marketDataFile") && params_->get("setup", "marketDataFile") != "") {
        //out_ << setw(tab_) << left << "Market data loader... " << flush;
        string marketFileString = params_->get("setup", "marketDataFile");
        marketFiles = getFilenames(marketFileString, inputPath);
    } else {
        ALOG("market data file not found");
    }

    vector<string> fixingFiles = {};
    if (params_->has("setup", "fixingDataFile") && params_->get("setup", "fixingDataFile") != "") {
        string fixingFileString = params_->get("setup", "fixingDataFile");
        fixingFiles = getFilenames(fixingFileString, inputPath);
    } else {
        ALOG("fixing data file not found");
    }
    
    vector<string> dividendFiles = {};
    if (params_->has("setup", "dividendDataFile")) {
        string dividendFileString = params_->get("setup", "dividendDataFile");
        dividendFiles = getFilenames(dividendFileString, inputPath);
    } else {
        WLOG("dividend data file not found");
    }

    csvLoader_ = boost::make_shared<CSVLoader>(marketFiles, fixingFiles, dividendFiles, implyTodaysFixings);

    /*****************************
     * Collect requested run types
     */
    runTypes_.clear();
    
    if (params_->hasGroup("npv") && params_->get("npv", "active") == "Y")
        runTypes_.push_back("NPV");
    
    if (params_->hasGroup("cashflow") && params_->get("cashflow", "active") == "Y")
        runTypes_.push_back("CASHFLOW");

    if (params_->hasGroup("simulation") && params_->get("simulation", "active") == "Y")
        runTypes_.push_back("EXPOSURE");

    if (params_->hasGroup("xva") && params_->get("xva", "active") == "Y")
        runTypes_.push_back("XVA");

    if (params_->hasGroup("sensitivity") && params_->get("sensitivity", "active") == "Y")
        runTypes_.push_back("SENSITIVITY");

    if (params_->hasGroup("parametricVar") && params_->get("parametricVar", "active") == "Y")
        runTypes_.push_back("VAR");

    if (outputAdditionalResults_)
        runTypes_.push_back("ADDITIONAL_RESULTS");

    LOG("OREAppInputParameters complete");
}

void OREAppInputParameters::writeOutParameters() {
    QL_FAIL("OREAppInputParameters::writeOutParameters() not implmented");    
}

} // namespace analytics
} // namespace ore
