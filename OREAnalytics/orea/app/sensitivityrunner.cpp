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

void SensitivityRunner::runSensitivityAnalysis(QuantLib::ext::shared_ptr<Market> market,
                                               const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs,
                                               const QuantLib::ext::shared_ptr<TodaysMarketParameters>& todaysMarketParams) {

    MEM_LOG;
    LOG("Running sensitivity analysis");

    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
    sensiData_ = QuantLib::ext::make_shared<SensitivityScenarioData>();
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    QuantLib::ext::shared_ptr<Portfolio> sensiPortfolio = QuantLib::ext::make_shared<Portfolio>();
    string marketConfiguration = params_->get("markets", "sensitivity");

    sensiInputInitialize(simMarketData, sensiData_, engineData, sensiPortfolio);

    bool recalibrateModels =
        params_->has("sensitivity", "recalibrateModels") && parseBool(params_->get("sensitivity", "recalibrateModels"));

    bool analyticFxSensis = false;
    if (params_->has("sensitivity", "analyticFxSensis")) {
        analyticFxSensis = parseBool(params_->get("sensitivity", "analyticFxSensis"));
    }

    QuantLib::ext::shared_ptr<SensitivityAnalysis> sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
        sensiPortfolio, market, marketConfiguration, engineData, simMarketData, sensiData_, recalibrateModels,
        curveConfigs, todaysMarketParams, false, referenceData_, iborFallbackConfig_, continueOnError_,
        analyticFxSensis);
    sensiAnalysis->generateSensitivities();

    simMarket_ = sensiAnalysis->simMarket();

    sensiOutputReports(sensiAnalysis);

    LOG("Sensitivity analysis completed");
    MEM_LOG;
}

void SensitivityRunner::sensiInputInitialize(QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                             QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensiData,
                                             QuantLib::ext::shared_ptr<EngineData>& engineData,
                                             QuantLib::ext::shared_ptr<Portfolio>& sensiPortfolio) {

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
        sensiPortfolio->fromFile(portfolioFile);
    }

    DLOG("sensiInputInitialize done");
}

void SensitivityRunner::sensiOutputReports(const QuantLib::ext::shared_ptr<SensitivityAnalysis>& sensiAnalysis) {

    string outputPath = params_->get("setup", "outputPath");
    Real sensiThreshold = parseReal(params_->get("sensitivity", "outputSensitivityThreshold"));

    string outputFile = outputPath + "/" + params_->get("sensitivity", "scenarioOutputFile");
    CSVFileReport scenReport(outputFile);
    ReportWriter().writeScenarioReport(scenReport, sensiAnalysis->sensiCubes(), sensiThreshold);

    // Create a stream from the sensitivity cube
    auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
    auto ss = QuantLib::ext::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), baseCurrency);

    Size outputPrecision = 2;
    if (params_->has("sensitivity", "outputPrecision")) {
        outputPrecision = parseInteger(params_->get("sensitivity", "outputPrecision"));
    }

    outputFile = outputPath + "/" + params_->get("sensitivity", "sensitivityOutputFile");
    CSVFileReport sensiReport(outputFile);
    ReportWriter().writeSensitivityReport(sensiReport, ss, sensiThreshold, outputPrecision);

    CSVFileReport pricingStatsReport(params_->get("setup", "outputPath") + "/pricingstats_sensi.csv");
    ore::analytics::ReportWriter().writePricingStats(pricingStatsReport, sensiAnalysis->portfolio());
}

} // namespace analytics
} // namespace ore
