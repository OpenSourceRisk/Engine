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

#include <orea/app/sensitivityrunner.hpp>
#include <ored/report/csvreport.hpp>

using namespace std;
using namespace ore::data;
using namespace ore::analytics;

namespace ore {
namespace analytics {

void SensitivityRunner::runSensitivityAnalysis(
    boost::shared_ptr<Market> market, Conventions& conventions) {

    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    boost::shared_ptr<SensitivityScenarioData> sensiData(new SensitivityScenarioData);
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    boost::shared_ptr<Portfolio> sensiPortfolio = boost::make_shared<Portfolio>();
    string marketConfiguration = params_->get("markets", "sensitivity");

    sensiInputInitialize(simMarketData, sensiData, engineData, sensiPortfolio, marketConfiguration);

    bool recalibrateModels =
        params_->has("sensitivity", "recalibrateModels") && parseBool(params_->get("sensitivity", "recalibrateModels"));

    boost::shared_ptr<SensitivityAnalysis> sensiAnalysis =
        boost::make_shared<SensitivityAnalysis>(sensiPortfolio, market, marketConfiguration, engineData, simMarketData,
                                                sensiData, conventions, recalibrateModels);
    sensiAnalysis->generateSensitivities();

    sensiOutputReports(sensiAnalysis);
}

void SensitivityRunner::sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                             boost::shared_ptr<SensitivityScenarioData>& sensiData,
                                             boost::shared_ptr<EngineData>& engineData,
                                             boost::shared_ptr<Portfolio>& sensiPortfolio, string& marketConfiguration) {

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
    string portfolioFile = inputPath + "/" + params_->get("setup", "portfolioFile");
    // Just load here. We build the portfolio in SensitivityAnalysis, after building SimMarket.
    sensiPortfolio->load(portfolioFile, boost::make_shared<TradeFactory>());

    LOG("Build Sensitivity Analysis");
    marketConfiguration = params_->get("markets", "pricing");

    DLOG("sensiInputInitialize done");
    
    return;
}

void SensitivityRunner::sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis) {

    string outputPath = params_->get("setup", "outputPath");
    string outputFile1 = outputPath + "/" + params_->get("sensitivity", "scenarioOutputFile");
    Real sensiThreshold = parseReal(params_->get("sensitivity", "outputSensitivityThreshold"));
    boost::shared_ptr<Report> scenReport = boost::make_shared<CSVFileReport>(outputFile1);
    sensiAnalysis->writeScenarioReport(scenReport, sensiThreshold);

    string outputFile2 = outputPath + "/" + params_->get("sensitivity", "sensitivityOutputFile");
    boost::shared_ptr<Report> sensiReport = boost::make_shared<CSVFileReport>(outputFile2);
    sensiAnalysis->writeSensitivityReport(sensiReport, sensiThreshold);

    string outputFile3 = outputPath + "/" + params_->get("sensitivity", "crossGammaOutputFile");
    boost::shared_ptr<Report> cgReport = boost::make_shared<CSVFileReport>(outputFile3);
    sensiAnalysis->writeCrossGammaReport(cgReport, sensiThreshold);
    return;
}

} // namespace analytics
} // namespace ore
