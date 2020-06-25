/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/app/reportwriter.hpp>
#include <orea/app/sensitivityrunner.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <ored/report/csvreport.hpp>
#include <ored/utilities/log.hpp>

using namespace std;
using namespace ore::data;

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

void SensitivityRunner::runSensitivityAnalysis(boost::shared_ptr<Market> market, Conventions& conventions,
                                               const CurveConfigurations& curveConfigs,
                                               const TodaysMarketParameters& todaysMarketParams) {

    MEM_LOG;
    LOG("Running sensitivity analysis");

    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    boost::shared_ptr<SensitivityScenarioData> sensiData(new SensitivityScenarioData);
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    boost::shared_ptr<Portfolio> sensiPortfolio = boost::make_shared<Portfolio>();
    string marketConfiguration = params_->get("markets", "sensitivity");

    sensiInputInitialize(simMarketData, sensiData, engineData, sensiPortfolio);

    bool recalibrateModels =
        params_->has("sensitivity", "recalibrateModels") && parseBool(params_->get("sensitivity", "recalibrateModels"));

    boost::shared_ptr<SensitivityAnalysis> sensiAnalysis = boost::make_shared<SensitivityAnalysis>(
        sensiPortfolio, market, marketConfiguration, engineData, simMarketData, sensiData, conventions,
        recalibrateModels, curveConfigs, todaysMarketParams, false, extraEngineBuilders_, extraLegBuilders_,
        referenceData_, continueOnError_);
    sensiAnalysis->generateSensitivities();

    sensiOutputReports(sensiAnalysis);

    LOG("Sensitivity analysis completed");
    MEM_LOG;
}

void SensitivityRunner::sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                             boost::shared_ptr<SensitivityScenarioData>& sensiData,
                                             boost::shared_ptr<EngineData>& engineData,
                                             boost::shared_ptr<Portfolio>& sensiPortfolio) {

    DLOG("sensiInputInitialize called");

    LOG("Get Simulation Market Parameters");
    string inputPath = params_->get("setup", "inputPath");
    string marketConfigFile = inputPath + "/" + params_->get("sensitivity", "marketConfigFile");
    simMarketData->fromFile(marketConfigFile);

    LOG("Get Sensitivity Parameters");
    string sensitivityConfigFile = inputPath + "/" + params_->get("sensitivity", "sensitivityConfigFile");
    sensiData->fromFile(sensitivityConfigFile);

    LOG("Get Engine Data");
    string sensiPricingEnginesFile = inputPath + "/" + params_->get("sensitivity", "pricingEnginesFile");
    engineData->fromFile(sensiPricingEnginesFile);

    LOG("Get Portfolio");
    string portfoliosString = params_->get("setup", "portfolioFile");
    vector<string> portfolioFiles = getFilenames(portfoliosString, inputPath);
    // Just load here. We build the portfolio in SensitivityAnalysis, after building SimMarket.
    for (auto portfolioFile : portfolioFiles) {
        sensiPortfolio->load(portfolioFile, tradeFactory_);
    }

    DLOG("sensiInputInitialize done");
}

void SensitivityRunner::sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis) {

    string outputPath = params_->get("setup", "outputPath");
    Real sensiThreshold = parseReal(params_->get("sensitivity", "outputSensitivityThreshold"));

    string outputFile = outputPath + "/" + params_->get("sensitivity", "scenarioOutputFile");
    CSVFileReport scenReport(outputFile);
    ReportWriter().writeScenarioReport(scenReport, sensiAnalysis->sensiCube(), sensiThreshold);

    // Create a stream from the sensitivity cube
    auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
    auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), baseCurrency);

    outputFile = outputPath + "/" + params_->get("sensitivity", "sensitivityOutputFile");
    CSVFileReport sensiReport(outputFile);
    ReportWriter().writeSensitivityReport(sensiReport, ss, sensiThreshold);
}

} // namespace analytics
} // namespace ore
