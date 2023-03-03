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

vector<string> getFileNames(const string& fileString, const string& path) {
    vector<string> fileNames;
    boost::split(fileNames, fileString, boost::is_any_of(",;"), boost::token_compress_on);
    for (auto it = fileNames.begin(); it < fileNames.end(); it++) {
        boost::trim(*it);
        *it = path + "/" + *it;
    }
    return fileNames;
}

InputParameters::InputParameters() {
    iborFallbackConfig_ = boost::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig());
    loadParameters();
}

void InputParameters::setAsOfDate(const std::string& s) {
    asof_ = parseDate(s);
    Settings::instance().evaluationDate() = asof_;
}

void InputParameters::setMarketConfig(const std::string& config, const std::string& context) {
    auto it = marketConfigs_.find(context);
    QL_REQUIRE(it == marketConfigs_.end(),
               "market config " << it->second << " already set for context " << it->first);
    marketConfigs_[context] = config;
}

void InputParameters::setRefDataManager(const std::string& xml) {
    refDataManager_ = boost::make_shared<BasicReferenceDataManager>();
    refDataManager_->fromXMLString(xml);
}

void InputParameters::setRefDataManagerFromFile(const std::string& fileName) {
    refDataManager_ = boost::make_shared<BasicReferenceDataManager>(fileName);
}

void InputParameters::setConventions(const std::string& xml) {
    conventions_ = boost::make_shared<Conventions>();
    conventions_->fromXMLString(xml);
}
    
void InputParameters::setConventionsFromFile(const std::string& fileName) {
    conventions_ = boost::make_shared<Conventions>();
    conventions_->fromFile(fileName);
}

void InputParameters::setCurveConfigs(const std::string& xml) {
    auto curveConfig = boost::make_shared<CurveConfigurations>();
    curveConfig->fromXMLString(xml);
    curveConfigs_.push_back(curveConfig);
}

void InputParameters::setCurveConfigsFromFile(const std::string& fileName) {
    auto curveConfig = boost::make_shared<CurveConfigurations>();
    curveConfig->fromFile(fileName);
    curveConfigs_.push_back(curveConfig);
}

void InputParameters::setIborFallbackConfig(const std::string& xml) {
    iborFallbackConfig_= boost::make_shared<IborFallbackConfig>();
    iborFallbackConfig_->fromXMLString(xml);
}

void InputParameters::setIborFallbackConfigFromFile(const std::string& fileName) {
    iborFallbackConfig_= boost::make_shared<IborFallbackConfig>();
    iborFallbackConfig_->fromFile(fileName);
}

void InputParameters::setPricingEngine(const std::string& xml) {
    pricingEngine_ = boost::make_shared<EngineData>();
    pricingEngine_->fromXMLString(xml);
}

void InputParameters::setPricingEngineFromFile(const std::string& fileName) {
    pricingEngine_ = boost::make_shared<EngineData>();
    pricingEngine_->fromFile(fileName);
}

void InputParameters::setTodaysMarketParams(const std::string& xml) {
    todaysMarketParams_ = boost::make_shared<TodaysMarketParameters>();
    todaysMarketParams_->fromXMLString(xml);
}

void InputParameters::setTodaysMarketParamsFromFile(const std::string& fileName) {
    todaysMarketParams_ = boost::make_shared<TodaysMarketParameters>();
    todaysMarketParams_->fromFile(fileName);
}

void InputParameters::setPortfolio(const std::string& xml) {
    portfolio_ = boost::make_shared<Portfolio>(buildFailedTrades_);
    portfolio_->fromXMLString(xml);
}

void InputParameters::setPortfolioFromFile(const std::string& fileNameString, const std::string& inputPath) {
    vector<string> files = getFileNames(fileNameString, inputPath);
    portfolio_ = boost::make_shared<Portfolio>(buildFailedTrades_);
    for (auto file : files) {
        LOG("Loading portfolio from file: " << file);
        portfolio_->fromFile(file);
    }
}

void InputParameters::setMarketConfigs(const std::map<std::string, std::string>& m) {
    marketConfigs_ = m;
}
    
void InputParameters::setMporCalendar(const std::string& s) {
    mporCalendar_ = parseCalendar(s);
}

void InputParameters::setSensiSimMarketParams(const std::string& xml) {
    sensiSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    sensiSimMarketParams_->fromXMLString(xml);
}

void InputParameters::setSensiSimMarketParamsFromFile(const std::string& fileName) {
    sensiSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    sensiSimMarketParams_->fromFile(fileName);
}
    
void InputParameters::setSensiScenarioData(const std::string& xml) {
    sensiScenarioData_ = boost::make_shared<SensitivityScenarioData>();
    sensiScenarioData_->fromXMLString(xml);
}

void InputParameters::setSensiScenarioDataFromFile(const std::string& fileName) {
    sensiScenarioData_ = boost::make_shared<SensitivityScenarioData>();
    sensiScenarioData_->fromFile(fileName);
}

void InputParameters::setSensiPricingEngine(const std::string& xml) {
    sensiPricingEngine_ = boost::make_shared<EngineData>();
    sensiPricingEngine_->fromXMLString(xml);
}

void InputParameters::setSensiPricingEngineFromFile(const std::string& fileName) {
    sensiPricingEngine_ = boost::make_shared<EngineData>();
    sensiPricingEngine_->fromFile(fileName);
}

void InputParameters::setStressSimMarketParams(const std::string& xml) {
    stressSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    stressSimMarketParams_->fromXMLString(xml);
}
    
void InputParameters::setStressSimMarketParamsFromFile(const std::string& fileName) {
    stressSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    stressSimMarketParams_->fromFile(fileName);
}
    
void InputParameters::setStressScenarioData(const std::string& xml) {
    stressScenarioData_ = boost::make_shared<StressTestScenarioData>();
    stressScenarioData_->fromXMLString(xml);
}
    
void InputParameters::setStressScenarioDataFromFile(const std::string& fileName) {
    stressScenarioData_ = boost::make_shared<StressTestScenarioData>();
    stressScenarioData_->fromFile(fileName);
}
    
void InputParameters::setStressPricingEngine(const std::string& xml) {
    stressPricingEngine_ = boost::make_shared<EngineData>();
    stressPricingEngine_->fromXMLString(xml);
}

void InputParameters::setStressPricingEngineFromFile(const std::string& fileName) {
    stressPricingEngine_ = boost::make_shared<EngineData>();
    stressPricingEngine_->fromFile(fileName);
}

void InputParameters::setExposureSimMarketParams(const std::string& xml) {
    exposureSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    exposureSimMarketParams_->fromXMLString(xml);
}
    
void InputParameters::setExposureSimMarketParamsFromFile(const std::string& fileName) {
    exposureSimMarketParams_ = boost::make_shared<ScenarioSimMarketParameters>();
    exposureSimMarketParams_->fromFile(fileName);
}

void InputParameters::setScenarioGeneratorData(const std::string& xml) {
    scenarioGeneratorData_ = boost::make_shared<ScenarioGeneratorData>();
    scenarioGeneratorData_->fromXMLString(xml);
}

void InputParameters::setScenarioGeneratorDataFromFile(const std::string& fileName) {
    scenarioGeneratorData_ = boost::make_shared<ScenarioGeneratorData>();
    scenarioGeneratorData_->fromFile(fileName);
}

void InputParameters::setCrossAssetModelData(const std::string& xml) {
    crossAssetModelData_ = boost::make_shared<CrossAssetModelData>();
    crossAssetModelData_->fromXMLString(xml);
}

void InputParameters::setCrossAssetModelDataFromFile(const std::string& fileName) {
    crossAssetModelData_ = boost::make_shared<CrossAssetModelData>();
    crossAssetModelData_->fromFile(fileName);
}

void InputParameters::setSimulationPricingEngine(const std::string& xml) {
    simulationPricingEngine_ = boost::make_shared<EngineData>();
    simulationPricingEngine_->fromXMLString(xml);
}

void InputParameters::setSimulationPricingEngineFromFile(const std::string& fileName) {
    simulationPricingEngine_ = boost::make_shared<EngineData>();
    simulationPricingEngine_->fromFile(fileName);
}

void InputParameters::setAmcPricingEngine(const std::string& xml) {
    amcPricingEngine_ = boost::make_shared<EngineData>();
    amcPricingEngine_->fromXMLString(xml);
}

void InputParameters::setAmcPricingEngineFromFile(const std::string& fileName) {
    amcPricingEngine_ = boost::make_shared<EngineData>();
    amcPricingEngine_->fromFile(fileName);
}

void InputParameters::setNettingSetManager(const std::string& xml) {
    nettingSetManager_ = boost::make_shared<NettingSetManager>();
    nettingSetManager_->fromXMLString(xml);
}

void InputParameters::setNettingSetManagerFromFile(const std::string& fileName) {
    nettingSetManager_ = boost::make_shared<NettingSetManager>();
    nettingSetManager_->fromFile(fileName);
}

void InputParameters::setCubeFromFile(const std::string& file) {
    cube_ = ore::analytics::loadCube(file);
}

void InputParameters::setNettingSetCubeFromFile(const std::string& file) {
    nettingSetCube_ = ore::analytics::loadCube(file);
}

void InputParameters::setCptyCubeFromFile(const std::string& file) {
    cptyCube_ = ore::analytics::loadCube(file);
}

void InputParameters::setMarketCubeFromFile(const std::string& file) {
    mktCube_ = loadAggregationScenarioData(file);
}

void InputParameters::setVarQuantiles(const std::string& s) {
    // parse to vector<Real>
    varQuantiles_ = parseListOfValues<Real>(s, &parseReal);
}

void InputParameters::setCovarianceDataFromFile(const std::string& fileName) {
    ore::data::CSVFileReader reader(fileName, false);
    std::vector<std::string> dummy;
    while (reader.next()) {
        covarianceData_[std::make_pair(*parseRiskFactorKey(reader.get(0), dummy),
                                       *parseRiskFactorKey(reader.get(1), dummy))] =
            ore::data::parseReal(reader.get(2));
    }
    LOG("Read " << covarianceData_.size() << " valid covariance data lines from " << fileName);
}

void InputParameters::setSensitivityStreamFromFile(const std::string& fileName) {
    sensitivityStream_ = boost::make_shared<SensitivityFileStream>(fileName);
}

void InputParameters::setAmcTradeTypes(const std::string& s) {
    // parse to set<string>
    auto v = parseListOfValues(s);
    amcTradeTypes_ = std::set<std::string>(v.begin(), v.end());
}
    
void InputParameters::setCvaSensiGrid(const std::string& s) {
    // parse to vector<Period>
    cvaSensiGrid_ = parseListOfValues<Period>(s, &parsePeriod);
}
    
void InputParameters::setDimRegressors(const std::string& s) {
    // parse to vector<string>
    dimRegressors_ = parseListOfValues(s);
}
    
void InputParameters::setDimOutputGridPoints(const std::string& s) {
    // parse to vector<Size>
    dimOutputGridPoints_ = parseListOfValues<Size>(s, &parseInteger);
}
    
void InputParameters::setCashflowHorizon(const std::string& s) {
    // parse to Date
    cashflowHorizon_ = parseDate(s);
}
    
void InputParameters::setPortfolioFilterDate(const std::string& s) {
    // parse to Date
    portfolioFilterDate_ = parseDate(s);
}
    
void InputParameters::setAnalytics(const std::string& s) {
    // parse to set<string>
    auto v = parseListOfValues(s);
    analytics_ = std::set<std::string>(v.begin(), v.end());
}
    
void InputParameters::insertAnalytic(const std::string& s) {
    analytics_.insert(s);
}
    
OutputParameters::OutputParameters(const boost::shared_ptr<Parameters>& params) {
    LOG("OutputFileNameMap called");
    npvOutputFileName_ = params->get("npv", "outputFileName", false);
    cashflowOutputFileName_ = params->get("cashflow", "outputFileName", false);
    curvesOutputFileName_ = params->get("curves", "outputFileName", false);
    scenarioDumpFileName_ = params->get("simulation", "scenariodump", false);
    cubeFileName_ = params->get("simulation", "cubeFile", false);
    mktCubeFileName_ = params->get("simulation", "aggregationScenarioDataFileName", false);
    rawCubeFileName_ = params->get("xva", "rawCubeOutputFile", false);
    netCubeFileName_ = params->get("xva", "netCubeOutputFile", false);
    dimEvolutionFileName_ = params->get("xva", "dimEvolutionFile", false);    
    std::string tmp = params->get("xva", "dimRegressionFiles", false);
    if (tmp != "")
        dimRegressionFileNames_ = parseListOfValues(tmp);
    sensitivityFileName_ = params->get("sensitivity", "sensitivityOutputFile", false);    
    parSensitivityFileName_ = params->get("sensitivity", "parSensitivityOutputFile", false);    
    jacobiFileName_ = params->get("sensitivity", "jacobiOutputFile", false);    
    jacobiInverseFileName_ = params->get("sensitivity", "jacobiInverseOutputFile", false);    
    sensitivityScenarioFileName_ = params->get("sensitivity", "scenarioOutputFile", false);    
    stressTestFileName_ = params->get("stress", "scenarioOutputFile", false);    
    varFileName_ = params->get("parametricVar", "outputFile", false);
    
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
    
    vector<Size> dimOutputGridPoints;
    tmp = params->get("xva", "dimOutputGridPoints", false);
    if (tmp != "")
        dimOutputGridPoints = parseListOfValues<Size>(tmp, parseInteger);
    QL_REQUIRE(dimOutputGridPoints.size() == dimRegressionFileNames_.size(),
               "dim regression output grid points size (" << dimOutputGridPoints.size() << ") "
               << "and file names size (" << dimRegressionFileNames_.size() << ") do not match");
    for (Size i = 0; i < dimRegressionFileNames_.size(); ++i)
        fileNameMap_["dim_regression_" + to_string(i)] = dimRegressionFileNames_[i];
    
    LOG("OutputFileNameMap complete");
}
    
std::string OutputParameters::outputFileName(const std::string& internalName, const std::string& suffix) {
    auto it = fileNameMap_.find(internalName);
    if (it == fileNameMap_.end() || it->second == "")
        return internalName + "." + suffix;
    else        
        return it->second; // contains suffix
}

} // namespace analytics
} // namespace ore
