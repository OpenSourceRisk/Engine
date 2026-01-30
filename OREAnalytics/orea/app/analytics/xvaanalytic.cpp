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

#include <orea/aggregation/dimflatcalculator.hpp>
#include <orea/aggregation/dimdirectcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/aggregation/dimhelper.hpp>
#include <orea/aggregation/dynamicdeltavarcalculator.hpp>
#include <orea/aggregation/dynamicsimmcalculator.hpp>
#include <orea/aggregation/simmhelper.hpp>
#include <orea/app/analytics/utilities.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/overlaynpvcube.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/sparsenpvcube.hpp>
#include <orea/engine/amcvaluationengine.hpp>
#include <orea/engine/cptycalculator.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/sensitivitycalculator.hpp>
#include <orea/engine/simmsensitivitystoragemanager.hpp>
#include <orea/engine/multistatenpvcalculator.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/app/analytics/correlationanalytic.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace std::filesystem;

namespace ore {
namespace analytics {

void XvaVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    inputs->loadParameter<optional<bool>>(exposureIncludeTodaysCashFlows_, "simulation", "includeTodaysCashFlows", false,
                                parseBool);
    vector<string> pfeAnalytics = {"xva", "pfe"};

    if (!exposureIncludeTodaysCashFlows_) {
        // use the global setting if available
        optional<bool> inc = Settings::instance().includeTodaysCashFlows();
        if (inc)
            exposureIncludeTodaysCashFlows_ = *inc;
    }
    
    inputs->loadParameter<optional<bool>>(exposureIncludeReferenceDateEvents_, "simulation", "includeReferenceDateEvents",
                                          false, parseBool);
    if (!exposureIncludeReferenceDateEvents_)
        // use the global setting if available
        exposureIncludeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();

    inputs->loadParameter<bool>(amc_, "simulation", "amc", false, parseBool);
    inputs->loadParameter<XvaEngineCG::Mode>(amcCg_, "simulation", "amcCg", false, parseXvaEngineCgMode);
    inputs->loadParameterXML<SensitivityScenarioData>(xvaCgSensiScenarioData_, "simulation", "xvaCgSensitivityConfigFile");
    inputs->loadParameter<std::set<std::string>>(amcTradeTypes_, "simulation", "amcTradeTypes", false, parseListOfValuesToSet);

    inputs->loadParameter<string>(amcPathDataInput_, "simulation", "amcPathDataInput", false);
    inputs->loadParameter<string>(amcPathDataOutput_, "simulation", "amcPathDataOutput", false);
    inputs->loadParameter<bool>(amcIndividualTrainingInput_, "xsimulationva", "amcIndividualTrainingInput", false, parseBool);
    inputs->loadParameter<bool>(amcIndividualTrainingOutput_, "simulation", "amcIndividualTrainingOutput", false, parseBool);

    string scenarioFile;
    inputs->loadParameter<string>(scenarioFile, "simulation", "scenarioFile", false);
    if (!scenarioFile.empty())
        scenarioReader_ = loadScenarioReader(scenarioFile, inputs->setupVariables().inputPath_);

    simulationPricingEngine_ = amcPricingEngine_ = amcCgPricingEngine_ = inputs->setupVariables().pricingEngine_;
    inputs->loadParameterXML<EngineData>(simulationPricingEngine_, "simulation", "pricingEnginesFile");
    inputs->loadParameterXML<EngineData>(amcPricingEngine_, "simulation", "amcPricingEnginesFile");
    inputs->loadParameterXML<EngineData>(amcCgPricingEngine_, "simulation", "amcCgPricingEnginesFile");
    inputs->loadParameterXML<ScenarioSimMarketParameters>(exposureSimMarketParams_, "simulation", "simulationConfigFile");
    inputs->loadParameterXML<CrossAssetModelData>(crossAssetModelData_, "simulation", "crossAssetModelData");
    if (!crossAssetModelData_)
        // load default if not provided
        inputs->loadParameterXML<CrossAssetModelData>(crossAssetModelData_, "simulation", "simulationConfigFile");
    inputs->loadParameterXML<ScenarioGeneratorData>(scenarioGeneratorData_, "simulation", "scenarioGeneratorData");
    if (!scenarioGeneratorData_)
        inputs->loadParameterXML<ScenarioGeneratorData>(scenarioGeneratorData_, "simulation", "simulationConfigFile");
    inputs->loadParameter<Size>(maxScenario_, "simulation", "maxScenario", false, parseInteger);
    if (scenarioGeneratorData_ && maxScenario_ != QuantLib::Null<QuantLib::Size>() &&
        scenarioGeneratorData_->samples() > maxScenario_) {
        scenarioGeneratorData_->samples() = maxScenario_;
    }

    exposureBaseCurrency_ = inputs->setupVariables().baseCurrency_;
    inputs->loadParameter<string>(exposureBaseCurrency_, "simulation", "baseCurrency", false);

    exposureObservationModel_ = inputs->setupVariables().observationModel_;
    inputs->loadParameter<string>(exposureObservationModel_, "simulation", "observationModel", false);

    inputs->loadParameter<bool>(storeFlows_, "simulation", "storeFlows", false, parseBool);
    inputs->loadParameter<bool>(storeExerciseValues_, "simulation", "storeExerciseValues", false, parseBool);
    inputs->loadParameter<bool>(storeSensis_, "simulation", "storeSensis", false, parseBool);
    inputs->loadParameter<bool>(allowPartialScenarios_, "simulation", "allowPartialScenarios", false, parseBool);
    inputs->loadParameter<Size>(storeCreditStateNPVs_, "simulation", "storeCreditStateNPVs", false, parseInteger);
    inputs->loadParameter<bool>(storeSurvivalProbabilities_, "simulation", "storeSurvivalProbabilities", false, parseBool);
    string writeCube, writeScenarios;
    inputs->loadParameter<string>(writeCube, "simulation", "cubeFile", false);
    if (!writeCube.empty())
        writeCube_ = true;
    inputs->loadParameter<string>(writeScenarios, "simulation", "scenariodump", false);
    if (!writeScenarios.empty())
        writeScenarios_ = true;
    inputs->loadParameter<bool>(xvaCgBumpSensis_, "simulation", "xvaCgBumpSensis", false, parseBool);
    inputs->loadParameter<bool>(xvaCgUseExternalComputeDevice_, "simulation", "xvaCgUseExternalComputeDevice", false, parseBool);
    inputs->loadParameter<string>(xvaCgExternalComputeDevice_, "simulation", "xvaCgExternalComputeDevice", false);
    inputs->loadParameter<bool>(xvaCgExternalDeviceCompatibilityMode_, "simulation", "xvaCgExternalDeviceCompatibilityMode", false, parseBool);
    inputs->loadParameter<bool>(xvaCgUseDoublePrecisionForExternalCalculation_, "simulation", "xvaCgUseDoublePrecisionForExternalCalculation", false, parseBool);
    inputs->loadParameter<bool>(xvaCgUsePythonIntegration_, "simulation", "xvaCgUsePythonIntegration", false, parseBool);
    inputs->loadParameter<bool>(xvaCgUsePythonIntegrationDynamicIm_, "simulation", "xvaCgUsePythonIntegrationDynamicIm", false, parseBool);
    inputs->loadParameter<vector<double>>(curveSensiGrid_, "simulation", "curveSensiGrid", false, parseListOfRealValues);
    inputs->loadParameter<vector<double>>(vegaSensiGrid_, "simulation", "vegaSensiGrid", false, parseListOfRealValues);
    inputs->loadParameter<string>(nettingSetId_, "simulation", "nettingSetId", false);
    inputs->loadParameter<bool>(xvaCgDynamicIM_, "simulation", "xvaCgDynamicIM", false, parseBool);
    inputs->loadParameter<bool>(xvaCgUsePythonIntegrationDynamicIm_, "simulation", "xvaCgUsePythonIntegrationDynamicIm", false, parseBool);
    inputs->loadParameter<Size>(xvaCgDynamicIMStepSize_, "simulation", "xvaCgDynamicIMStepSize", false, parseInteger);
    inputs->loadParameter<Size>(xvaCgRegressionOrder_, "simulation", "xvaCgRegressionOrder", false, parseInteger);
    inputs->loadParameter<double>(xvaCgRegressionVarianceCutoff_, "simulation", "xvaCgRegressionVarianceCutoff", false, parseReal);
    inputs->loadParameter<Size>(xvaCgRegressionOrderDynamicIm_, "simulation", "xvaCgRegressionOrderDynamicIm", false, parseInteger);
    inputs->loadParameter<double>(xvaCgRegressionVarianceCutoffDynamicIm_, "simulation", "xvaCgRegressionVarianceCutoffDynamicIm", false, parseReal);
    inputs->loadParameter<bool>(xvaCgTradeLevelBreakdown_, "simulation", "xvaCgTradeLevelBreakDown", false, parseBool);
    inputs->loadParameter<vector<Size>>(xvaCgRegressionReportTimeStepsDynamicIM_, "simulation", "xvaCgRegressionReportTimeStepsDynamicIM", false, parseListOfIntegerValues);
    inputs->loadParameter<bool>(xvaCgUseRedBlocks_, "simulation", "xvaCgUseRedBlocks", false, parseBool);
    inputs->loadParameter<bool>(cubeNpvOverlay_, "simulation", "cubeNpvOverlay", false, parseBool);

    /**********************
     * XVA specifically
     **********************/

    inputs->loadParameter<bool>(generateCorrelations_, "xva", "generateCorrelations", false, parseBool);
    inputs->loadParameter<bool>(xvaUseDoublePrecisionCubes_, "xva", "useDoublePrecisionCubes", false, parseBool);
    xvaBaseCurrency_ = inputs->setupVariables().baseCurrency_;
    inputs->loadParameter<string>(xvaBaseCurrency_, pfeAnalytics, "baseCurrency", false);
    inputs->loadParameter<bool>(flipViewXVA_, "xva", "flipViewXVA", false, parseBool);
    inputs->loadParameter<MporCashFlowMode>(mporCashFlowMode_, "xva", "mporCashFlowMode", false, parseMporCashFlowMode);
    inputs->loadParameter<bool>(fullInitialCollateralisation_, "xva", "fullInitialCollateralisation", false, parseBool);
    inputs->loadParameter<bool>(exposureProfilesByTrade_, pfeAnalytics, "exposureProfilesByTrade", false, parseBool);
    inputs->loadParameter<bool>(exposureProfiles_, pfeAnalytics, "exposureProfiles", false, parseBool);
    inputs->loadParameter<bool>(exposureProfilesUseCloseOutValues_, pfeAnalytics, "exposureProfilesUseCloseOutValues", false, parseBool);
    inputs->loadParameter<bool>(writeIndividualExposureReports_, pfeAnalytics, "writeIndividualExposureReports", false, parseBool);
    inputs->loadParameter<string>(collateralCalculationType_, pfeAnalytics, "calculationType", false);
    inputs->loadParameter<string>(exposureAllocationMethod_, pfeAnalytics, "allocationMethod", false);
    inputs->loadParameter<Real>(marginalAllocationLimit_, pfeAnalytics, "marginalAllocationLimit", false, parseReal);
    inputs->loadParameter<Real>(pfeQuantile_, pfeAnalytics, "quantile", false, parseReal);
    inputs->loadParameter<bool>(exerciseNextBreak_, pfeAnalytics, "exerciseNextBreak", false, parseBool);
    inputs->loadParameter<bool>(cvaAnalytic_, "xva", "cva", false, parseBool);
    inputs->loadParameter<bool>(dvaAnalytic_, "xva", "dva", false, parseBool);
    inputs->loadParameter<bool>(fvaAnalytic_, "xva", "fva", false, parseBool);
    inputs->loadParameter<bool>(colvaAnalytic_, "xva", "colva", false, parseBool);
    inputs->loadParameter<bool>(collateralFloorAnalytic_, "xva", "collateralFloor", false, parseBool);
    inputs->loadParameter<bool>(dimAnalytic_, "xva", "dim", false, parseBool);
    inputs->loadParameter<bool>(mvaAnalytic_, "xva", "mva", false, parseBool);
    inputs->loadParameter<bool>(kvaAnalytic_, "xva", "kva", false, parseBool);
    inputs->loadParameter<bool>(dynamicCredit_, "xva", "dynamicCredit", false, parseBool);
    inputs->loadParameter<bool>(cvaSensi_, "xva", "cvaSensi", false, parseBool);
    inputs->loadParameter<vector<Period>>(cvaSensiGrid_, "xva", "cvaSensiGrid", false, parseListOfPeriodValues);
    inputs->loadParameter<Real>(cvaSensiShiftSize_, "xva", "cvaSensiShiftSize", false, parseReal);
    inputs->loadParameter<string>(dvaName_, "xva", "dvaName", false);
    
    inputs->loadParameter<string>(rawCubeOutputFile_, pfeAnalytics, "rawCubeOutputFile", false);
    if (!rawCubeOutputFile_.empty())
        rawCubeOutput_ = true;
        
    inputs->loadParameter<string>(netCubeOutputFile_, pfeAnalytics, "netCubeOutputFile", false);
    if (!netCubeOutputFile_.empty())
        netCubeOutput_ = true;
    
    inputs->loadParameter<string>(timeAveragedNettedExposureOutputFile_, "xva", "timeAveragedNettedExposureOutputFile", false);
    if (!timeAveragedNettedExposureOutputFile_.empty())
        timeAveragedNettedExposureOutput_ = true;

    // FVA
    inputs->loadParameter<string>(fvaBorrowingCurve_, "xva", "fvaBorrowingCurve", false);
    inputs->loadParameter<string>(fvaLendingCurve_, "xva", "fvaLendingCurve", false);
    inputs->loadParameter<string>(flipViewBorrowingCurvePostfix_, "xva", "flipViewBorrowingCurvePostfix", false);
    inputs->loadParameter<string>(flipViewLendingCurvePostfix_, "xva", "flipViewLendingCurvePostfix", false);

    // DIM
    inputs->loadParameter<Real>(dimQuantile_, "xva", "dimQuantile", false, parseReal);
    inputs->loadParameter<Size>(dimHorizonCalendarDays_, "xva", "dimHorizonCalendarDays", false, parseInteger);
    inputs->loadParameter<Size>(dimRegressionOrder_, "xva", "dimRegressionOrder", false, parseInteger);
    inputs->loadParameter<vector<string>>(dimRegressors_, "xva", "dimRegressors", false, parseListOfStringValues);
    inputs->loadParameter<vector<Size>>(dimOutputGridPoints_, "xva", "dimOutputGridPoints", false,
                                        parseListOfIntegerValues);
    
    string dimDistributionCoveredStdDevs;
    inputs->loadParameter<string>(dimDistributionCoveredStdDevs, "xva", "dimDistributionCoveredStdDevs", false);
    if (!dimDistributionCoveredStdDevs.empty()) {
        if (dimDistributionCoveredStdDevs == "inf")
            dimDistributionCoveredStdDevs_ = Null<Real>();
        else
            dimDistributionCoveredStdDevs = parseReal(dimDistributionCoveredStdDevs);
    }

    inputs->loadParameter<Size>(dimDistributionGridSize_, "xva", "dimDistributionGridSize", false, parseInteger);
    inputs->loadParameter<string>(dimOutputNettingSet_, "xva", "dimOutputNettingSet", false);
    inputs->loadParameter<Size>(dimLocalRegressionEvaluations_, "xva", "dimLocalRegressionEvaluations", false, parseInteger);
    inputs->loadParameter<Real>(dimLocalRegressionBandwidth_, "xva", "dimLocalRegressionBandwidth", false, parseReal);
    inputs->loadParameter<string>(dimModel_, "xva", "dimModel", false);
    QL_REQUIRE(
        dimModel_ == "Regression" || dimModel_ == "Flat" || dimModel_ == "DeltaVaR" ||
            dimModel_ == "DeltaGammaNormalVaR" || dimModel_ == "DeltaGammaVaR" || dimModel_ == "DynamicIM" ||
            dimModel_ == "SimmAnalytic",
        "DIM model "
            << dimModel_ << " not supported, "
            << "expected Flat, Regression, DeltaVaR, DeltaGammaNormalVaR, DeltaGammaVaR, DynamicIM, SimmAnalytic");

    string deterministicInitialMarginFile;
    inputs->loadParameter<string>(deterministicInitialMarginFile, "xva", "deterministicInitialMarginFile", false);
    if (!deterministicInitialMarginFile.empty())
        deterministicInitialMargin_ = loadDeterministicInitialMarginFromFile(
            (inputs->setupVariables().inputPath_ / deterministicInitialMarginFile).generic_string());

    // KVA
    inputs->loadParameter<Real>(kvaCapitalDiscountRate_, "xva", "kvaCapitalDiscountRate", false, parseReal);
    inputs->loadParameter<Real>(kvaAlpha_, "xva", "kvaAlpha", false, parseReal);
    inputs->loadParameter<Real>(kvaRegAdjustment_, "xva", "kvaRegAdjustment", false, parseReal);
    inputs->loadParameter<Real>(kvaCapitalHurdle_, "xva", "kvaCapitalHurdle", false, parseReal);
    inputs->loadParameter<Real>(kvaOurPdFloor_, "xva", "kvaOurPdFloor", false, parseReal);
    inputs->loadParameter<Real>(kvaTheirPdFloor_, "xva", "kvaTheirPdFloor", false, parseReal);
    inputs->loadParameter<Real>(kvaOurCvaRiskWeight_, "xva", "kvaOurCvaRiskWeight", false, parseReal);
    inputs->loadParameter<Real>(kvaTheirCvaRiskWeight_, "xva", "kvaTheirCvaRiskWeight", false, parseReal);

    // credit simulation
    inputs->loadParameter<bool>(creditMigrationAnalytic_, "xva", "creditMigration", false, parseBool);
    inputs->loadParameter<vector<Real>>(creditMigrationDistributionGrid_, "xvs", "creditMigrationDistributionGrid", false, parseListOfRealValues);
    inputs->loadParameter<vector<Size>>(creditMigrationTimeSteps_, "xvs", "creditMigrationTimeSteps", false, parseListOfIntegerValues);
    inputs->loadParameter<bool>(firstMporCollateralAdjustment_, "xva", "firstMporCollateralAdjustment", false, parseBool);
    inputs->loadParameter<string>(creditMigrationOutputFiles_, "xva", "creditMigrationOutputFiles", false);
    inputs->loadParameterXML<CreditSimulationParameters>(creditSimulationParameters_, "xva", "creditMigrationConfig");
    inputs->loadParameterXML<NettingSetManager>(nettingSetManager_, pfeAnalytics, "csaFile");
    inputs->loadParameterXML<CollateralBalances>(collateralBalances_, pfeAnalytics, "collateralBalancesFile");
    
    string correlationInputFile;
    inputs->loadParameter<string>(correlationInputFile, "xva", "correlationInputFile", false);
    if (!correlationInputFile.empty())
        correlationData_ = loadCorrelationDataFromFile((inputs->setupVariables().inputPath_ / correlationInputFile).generic_string());
        
}

void XvaVariables::loadCube(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
    vector<string> pfeAnalytics = {"xva", "pfe"};
    bool loadedCube = inputs->loadFromParameters<QuantLib::ext::shared_ptr<NPVCube>>(cube_, pfeAnalytics, "cubeFile");
    if (!loadedCube) {
        string cubeFile;
        inputs->loadParameter<string>(cubeFile, pfeAnalytics, "cubeFile", false);
        if (!cubeFile.empty()) {
            LOG("Load cube from file " << cubeFile);
            auto r = ore::analytics::loadCube((inputs->setupVariables().inputPath_ / cubeFile).generic_string());
            cube_ = r->cube();
            if (r->scenarioGeneratorData())
                scenarioGeneratorData_ = r->scenarioGeneratorData();
            if (r->storeFlows())
                storeFlows_ = *r->storeFlows();
            if (r->storeCreditStateNPVs())
                storeCreditStateNPVs_ = *r->storeCreditStateNPVs();
            LOG("Cube loading done: ids=" << cube_->numIds() << " dates=" << cube_->numDates()
                                          << " samples=" << cube_->samples() << " depth=" << cube_->depth());
        }
    }

    bool loadedAggData = inputs->loadFromParameters<QuantLib::ext::shared_ptr<AggregationScenarioData>>(
        mktCube_, pfeAnalytics, "scenarioFile");
    if (!loadedAggData) {
        string scenarioFile;
        inputs->loadParameter<string>(scenarioFile, "xva", "scenarioFile", false);
        if (!scenarioFile.empty()) {
            LOG("Load agg scen data from file " << scenarioFile);
            mktCube_ =
                loadAggregationScenarioData((inputs->setupVariables().inputPath_ / scenarioFile).generic_string());
            LOG("MktCube loading done");
        }
    }

    string nettingCubeFile;
    inputs->loadParameter<string>(nettingCubeFile, "xva", "nettingSetCubeFile", false);
    if (!nettingCubeFile.empty()) {
        LOG("Load nettingset cube from file " << nettingCubeFile);
        nettingSetCube_ =
            ore::analytics::loadCube((inputs->setupVariables().inputPath_ / nettingCubeFile).generic_string())->cube();
        DLOG("NettingSetCube loading done: ids="
             << nettingSetCube_->numIds() << " dates=" << nettingSetCube_->numDates()
             << " samples=" << nettingSetCube_->samples() << " depth=" << nettingSetCube_->depth());
    }

    string cptyCubeFile;
    inputs->loadParameter<string>(cptyCubeFile, "xva", "cptyCubeFile", false);
    if (!cptyCubeFile.empty()) {
        LOG("Load cpty cube from file " << cptyCubeFile);
        cptyCube_ =
            ore::analytics::loadCube((inputs->setupVariables().inputPath_ / cptyCubeFile).generic_string())->cube();
        DLOG("CptyCube loading done: ids=" << cptyCube_->numIds() << " dates=" << cptyCube_->numDates()
                                           << " samples=" << cptyCube_->samples() << " depth=" << cptyCube_->depth());
    }
}

std::string XvaAnalyticImpl::mapRiskFactorToAssetType(RiskFactorKey::KeyType keyF) {
    std::vector<std::string> ir = {"DiscountCurve", "IndexCurve", "OptionletVolatility",
                                    "SwaptionVolatility", "YieldVolatility"};
    std::vector<std::string> fx = {"FXSpot", "FXVolatility"};
    std::vector<std::string> inf = {"CPIIndex", "ZeroInflationCurve","YoYInflationCurve",
                                    "YoYInflationCapFloorVolatility", "YoYInflationCapFloorVolatility"};
    std::vector<std::string> cr = {"SurvivalProbability", "SurvivalWeight", "RecoveryRate",
                                   "CDSVolatility", "SecuritySpread", "CPR"};
    std::vector<std::string> eq = {"EquitySpot", "EquityVolatility", "DividendYield"};
    std::vector<std::string> com = {"CommodityCurve", "CommodityVolatility"};
    std::vector<std::string> crstate = {"CreditState"};

    std::unordered_map<std::string, std::vector<std::string>> mapping;
    mapping["IR"] = ir;
    mapping["FX"] = fx;
    mapping["INF"] = inf;
    mapping["CR"] = cr;
    mapping["EQ"] = eq;
    mapping["COM"] = com;
    mapping["CrState"] = crstate;

    for (const auto& pair : mapping) {
        const auto& keyMap = pair.first;
        const auto& vec = pair.second;
        if (std::find(vec.begin(), vec.end(), ore::data::to_string(keyF)) != vec.end()) {
            return keyMap;
        }
    }
    return "";
}

/******************************************************************************
 * XVA Analytic: EXPOSURE, CVA, DVA, FVA, KVA, COLVA, COLLATERALFLOOR, DIM, MVA
 ******************************************************************************/
void XvaAnalyticImpl::buildDependencies() {
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    if (xvaVars->generateCorrelations_) {
        auto correlationAnalytic =
                AnalyticFactory::instance().build("CORRELATION", inputs_, analytic()->analyticsManager(), false);
            if (correlationAnalytic.second)
                addDependentAnalytic(corrLookupKey, correlationAnalytic.second);
    }
    if (inputs_->cubeNpvOverlay()) {
        if (auto pricingAnalytic =
                AnalyticFactory::instance().build("PRICING", inputs_, analytic()->analyticsManager(), false);
            pricingAnalytic.second) {
            addDependentAnalytic("PRICING", pricingAnalytic.second);
        }
    }
}

void XvaAnalyticImpl::feedCorrelationToCAM(const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& corrData) {
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    DLOG("Parse Correlation Matrix as Cross Asset Model Data Instantaneous Correlation.");
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> correlationData;
    correlationData = !corrData.empty() ? corrData : xvaVars->correlationData_;

    QL_REQUIRE(correlationData.size()>0," No Correlations.");
    // Instantaneous Correlation is a pair of smth "IR:USD, IR:GBP, EQ:SP5 etc.
    std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>> mapInstantaneousCor;
    std::vector<std::string> vecAssetType = {"DiscountCurve", "FXSpot", "EquitySpot", "SurvivalProbability", "ZeroInflationCurve", "CommodityCurve"};
    for (auto const& cor : correlationData) {
        RiskFactorKey pair1 = cor.first.first;
        RiskFactorKey pair2 = cor.first.second;
        //We filter the RiskFactorKey because the instantaneous correlation only have one IR, FX, INF etc
        if (std::find(vecAssetType.begin(), vecAssetType.end(), ore::data::to_string(pair1.keytype)) !=
                vecAssetType.end() &&
            std::find(vecAssetType.begin(), vecAssetType.end(), ore::data::to_string(pair2.keytype)) !=
                vecAssetType.end()) {
            //We want to exclude the combination type DiscountCurve/USD/0 and DiscountCurve/USD/1
            //We select only those riskfactor to be mapped to an asset type
            if (!((pair1.name == pair2.name) &&
                    (ore::data::to_string(pair1.keytype) == ore::data::to_string(pair2.keytype)))) {
                string asset1 = mapRiskFactorToAssetType(pair1.keytype);
                string asset2 = mapRiskFactorToAssetType(pair2.keytype);
                CorrelationFactor corrFactor1{parseCamAssetType(asset1), pair1.name, pair1.index};
                CorrelationFactor corrFactor2{parseCamAssetType(asset2), pair2.name, pair2.index};
                std::pair<CorrelationFactor, CorrelationFactor> correlationKey =
                    std::make_pair(corrFactor1, corrFactor2);
                mapInstantaneousCor[correlationKey] =
                    QuantLib::Handle<QuantLib::Quote>(QuantLib::ext::make_shared<SimpleQuote>(cor.second));
                TLOG("Replaced correlation: (" << corrFactor1 << "," << corrFactor2 << ") = " << cor.second << ".");
            }
        }
    }
    QuantLib::ext::shared_ptr<InstantaneousCorrelations> instantaneousCorrelation = ext::make_shared<InstantaneousCorrelations>(mapInstantaneousCor);
    analytic()->configurations().crossAssetModelData->setCorrelations(instantaneousCorrelation);
}

void XvaAnalyticImpl::setUpConfigurations() {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    LOG("XvaAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = xvaVars->exposureSimMarketParams_;
    analytic()->configurations().scenarioGeneratorData = xvaVars->scenarioGeneratorData_;
    analytic()->configurations().crossAssetModelData = xvaVars->crossAssetModelData_;

    if (analytic()->configurations().crossAssetModelData != nullptr && xvaVars->correlationData_.size() > 0) {
        feedCorrelationToCAM();
    }
}

void XvaAnalyticImpl::checkConfigurations(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    // find the unique nettingset keys in portfolio
    std::map<std::string, std::string> nettingSetMap = portfolio->nettingSetMap();
    std::set<std::string> nettingSetKeys;
    for (std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it)
        nettingSetKeys.insert(it->second);
    // controls on calcType and grid type, if netting-set has an active CSA in place
    for (auto const& key : nettingSetKeys) {
        if (!xvaVars->nettingSetManager_->has(key)) {
            StructuredAnalyticsWarningMessage(
                "XvaAnalytic", "Netting set definition not found",
                "Definition for netting set " + key + " is not found. "
                "Configuration check is not performed on this netting set.")
                .log();
            continue;
        }
        LOG("For netting-set " << key << "CSA flag is " << xvaVars->nettingSetManager_->get(key)->activeCsaFlag());
        if (xvaVars->nettingSetManager_->get(key)->activeCsaFlag()) {
            string calculationType = xvaVars->collateralCalculationType_;
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()) {
                QL_REQUIRE(calculationType == "NoLag",
                           "For nettingSetID " << key
                                               << ", CSA is active and a close-out grid is configured in the "
                                                  "simulation.xml. Therefore, calculation type "
                                               << calculationType << " is not admissable. It must be set to NoLag!");
                LOG("For netting-set " << key << ", calculation type is " << calculationType);
            } else {
                QL_REQUIRE(
                    calculationType != "NoLag",
                    "For nettingSetID "
                        << key
                        << ", CSA is active and a close-out grid is not configured in the simulation.xml. Therefore, "
                           "calculation type "
                        << calculationType
                        << " is not admissable. It must be set to either Symmetric or AsymmerticCVA or AsymmetricDVA!");
                LOG("For netting-set " << key << ", calculation type is " << calculationType);
            }
            if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag() &&
                analytic()->configurations().scenarioGeneratorData->closeOutLag() != 0 * Days) {
                Period mpor_simulation = analytic()->configurations().scenarioGeneratorData->closeOutLag();
                Period mpor_netting = xvaVars->nettingSetManager_->get(key)->csaDetails()->marginPeriodOfRisk();
                if (mpor_simulation != mpor_netting)
                    StructuredAnalyticsWarningMessage(
                        "XvaAnalytic", "Inconsistent MPoR period",
                        "For netting set " + key + ", close-out lag is not consistent with the netting-set's mpor ")
                        .log();
            }
        }
    }
}

void XvaAnalyticImpl::applyConfigurationFallback(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    // find the unique undefined nettingset keys in portfolio
    std::map<std::string, std::string> nettingSetMap = portfolio->nettingSetMap();
    std::set<std::string> nettingSetKeys;
    for (std::map<std::string, std::string>::iterator it = nettingSetMap.begin(); it != nettingSetMap.end(); ++it) {
        if (!xvaVars->nettingSetManager_->has(it->second)) {
            StructuredTradeErrorMessage(
                it->first, portfolio->get(it->first)->tradeType(),
                "Netting set definition is not found.",
                "Definition for netting set " + it->second + " is not found. "
                "A fallback of 'uncollateralised' netting set will be used, results for this netting set may be invalid.")
                .log();
                nettingSetKeys.insert(it->second);
        }
    }
    // add fallback netting set definitions
    for (auto const& key : nettingSetKeys) {
        StructuredAnalyticsErrorMessage(
            "XvaAnalytic", "Netting set definition not found",
            "Definition for netting set " + key + " is not found. "
            "A fallback of 'uncollateralised' netting set will be used, results for this netting set may be invalid.")
            .log();
        xvaVars->nettingSetManager_->add(QuantLib::ext::make_shared<NettingSetDefinition>(key));
    }
}

QuantLib::ext::shared_ptr<EngineFactory> XvaAnalyticImpl::engineFactory() {
    LOG("XvaAnalytic::engineFactory() called");

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    QuantLib::ext::shared_ptr<EngineData> edCopy =
        QuantLib::ext::make_shared<EngineData>(*xvaVars->simulationPricingEngine_);
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    edCopy->globalParameters()["McType"] = "Classic";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    // configurations[MarketContext::simulation] = inputs_->marketConfig("simulation");
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders;
    std::vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders;

    if (runSimulation_) {
        // link to the sim market here
        QL_REQUIRE(simMarket_, "Simulaton market not set");
        engineFactory_ = QuantLib::ext::make_shared<EngineFactory>(
            edCopy, simMarket_, configurations, inputs_->refDataManager(), inputs_->iborFallbackConfig());
    } else {
        // we just link to today's market if simulation is not required
        engineFactory_ = QuantLib::ext::make_shared<EngineFactory>(
            edCopy, analytic()->market(), configurations, inputs_->refDataManager(), inputs_->iborFallbackConfig());
    }
    return engineFactory_;
}

void XvaAnalyticImpl::buildScenarioSimMarket() {
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams,
        QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
        *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), false, true,
        xvaVars->allowPartialScenarios_, inputs_->iborFallbackConfig(), false, offsetScenario_);

    if (offsetScenario_ == nullptr) {
        simMarketCalibration_ = simMarket_;
        offsetSimMarket_ = simMarket_;
    } else {
        // set useSpreadedTermstructure to true, yield better results in calibration of the CAM
        simMarketCalibration_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), offsetSimMarketParams_,
            QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(), 
            *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), true, true, 
            xvaVars->allowPartialScenarios_, inputs_->iborFallbackConfig(), false, offsetScenario_);

        // Create a third market used for AMC and Postprocessor, holds a larger simmarket, e.g. default curves
        offsetSimMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), offsetSimMarketParams_, QuantLib::ext::make_shared<FixingManager>(inputs_->asof()),
            configuration, *inputs_->curveConfigs().get(), *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), true, true, xvaVars->allowPartialScenarios_, inputs_->iborFallbackConfig(),
            false, offsetScenario_);

        TLOG("XvaAnalytic: Offset Scenario used in building SimMarket");
        TLOG("XvaAnalytic: Offset scenario is absolute = " << offsetScenario_->isAbsolute());
        TLOG("RfKey,OffsetScenarioValue");
        for (const auto& key : offsetScenario_->keys()) {
            TLOG(key << " : " << offsetScenario_->get(key));
        }
    }

    TLOG("XvaAnalytic:Finished building Scenario SimMarket");
    TLOG("RfKey,BaseScenarioValue,BaseScenarioAbsValue");
    for (const auto& key : simMarket_->baseScenario()->keys()) {
        TLOG(key << "," << simMarket_->baseScenario()->get(key) << "," << simMarket_->baseScenarioAbsolute()->get(key));
    }
    TLOG("XvaAnalytic: Finished building Scenario SimMarket for model calibration (useSpreadedTermStructure)");
    TLOG("RfKey,BaseScenarioValue,BaseScenarioAbsValue");
    for (const auto& key : simMarketCalibration_->baseScenario()->keys()) {
        TLOG(key << "," << simMarketCalibration_->baseScenario()->get(key) << ","
                 << simMarketCalibration_->baseScenarioAbsolute()->get(key));
    }
}

void XvaAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError, const bool allowModelFallbacks) {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    if (xvaVars->scenarioReader_ && !xvaVars->generateCorrelations_) {
        auto loader = QuantLib::ext::make_shared<SimpleScenarioLoader>(xvaVars->scenarioReader_);
        auto slg = QuantLib::ext::make_shared<ScenarioLoaderPathGenerator>(loader, inputs_->asof(), grid_->dates(),
                                                                       grid_->timeGrid());
        scenarioGenerator_ = slg;
    } else {
        if (!model_)
            buildCrossAssetModel(continueOnCalibrationError, allowModelFallbacks);
        ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
        QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
        string config = inputs_->marketConfig("simulation");
        auto market = offsetScenario_ == nullptr ? analytic()->market() : simMarketCalibration_;
        scenarioGenerator_ =
            sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), market, config,
                      QuantLib::ext::make_shared<MultiPathGeneratorFactory>(), xvaVars->amcPathDataOutput_);
        QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator");
    }
    samples_ = analytic()->configurations().scenarioGeneratorData->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));
    if (xvaVars->writeScenarios_) {
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        analytic()->addReport(LABEL, "scenario", report);
        scenarioGenerator_ =
            QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report, std::vector<RiskFactorKey>{}, false);
    }
}

void XvaAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError, const bool allowModelFallbacks) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ", allowModelFallbacks = " << allowModelFallbacks << ")");
    ext::shared_ptr<Market> market = offsetScenario_ != nullptr ? simMarketCalibration_ : analytic()->market();
    QL_REQUIRE(market != nullptr, "Internal error, buildCrossAssetModel needs to be called after the market is built.");

    CrossAssetModelBuilder modelBuilder(market, analytic()->configurations().crossAssetModelData,
                                        inputs_->marketConfig("lgmcalibration"), inputs_->marketConfig("fxcalibration"),
                                        inputs_->marketConfig("eqcalibration"), inputs_->marketConfig("infcalibration"),
                                        inputs_->marketConfig("crcalibration"), inputs_->marketConfig("simulation"),
                                        false, continueOnCalibrationError, "", "xva cam building", false,
                                        allowModelFallbacks);

    model_ = *modelBuilder.model();
}

void XvaAnalyticImpl::initCubeDepth() {
    if (cubeDepth_ == 0) {
        LOG("XVA: Set cube depth");
        cubeDepth_ = cubeInterpreter_->requiredNpvCubeDepth();
        LOG("XVA: Cube depth set to: " << cubeDepth_);
    }
}

void XvaAnalyticImpl::initCube(QuantLib::ext::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids,
                               Size cubeDepth) {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    LOG("Init cube with depth " << cubeDepth);

    for (Size i = 0; i < grid_->valuationDates().size(); ++i)
        DLOG("initCube: grid[" << i << "]=" << io::iso_date(grid_->valuationDates()[i]));

    if (xvaVars->xvaUseDoublePrecisionCubes_)
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                   samples_, cubeDepth, 0.0f);
    else
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(inputs_->asof(), ids, grid_->valuationDates(),
                                                                  samples_, cubeDepth, 0.0f);
}

std::set<std::string> XvaAnalyticImpl::getNettingSetIds(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) const {
    // collect netting set ids from portfolio
    std::set<std::string> nettingSetIds;
    for (auto const& [tradeId,trade] : portfolio->trades())
        nettingSetIds.insert(trade->envelope().nettingSetId());
    return nettingSetIds;
}

void XvaAnalyticImpl::initClassicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA: initClassicRun");
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);

    initCubeDepth();

    // May have been set already
    if (scenarioData_ == nullptr) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ =
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
    }

    // We can skip the cube initialization if the mt val engine is used, since it builds its own cubes
    if (inputs_->nThreads() == 1) {
        if (portfolio->size() > 0)
            initCube(cube_, portfolio->ids(), cubeDepth_);
	
	    // not required by any calculators in ore at the moment
        nettingSetCube_ = nullptr;
	    // except in this case with a classic single-threaded run, left here for validation purposes:
        if (xvaVars->storeSensis_) {
            // Create the sensitivity storage manager
            // FIXME: Does the storage manager check consistency with the sensis provided by the delta/gamma engines?
            bool sensitivities2ndOrder = false;
            vector<Real> curveSensitivityGrid = xvaVars->curveSensiGrid_;
            vector<Real> vegaOptSensitivityGrid = xvaVars->vegaSensiGrid_;
            vector<Real> vegaUndSensitivityGrid = xvaVars->vegaSensiGrid_;
            vector<Real> fxVegaSensitivityGrid = xvaVars->vegaSensiGrid_;
            Size n = curveSensitivityGrid.size();
            Size u = vegaOptSensitivityGrid.size();
            Size v = vegaUndSensitivityGrid.size();
            Size w = fxVegaSensitivityGrid.size();
	        QL_REQUIRE(n + u + v + w > 0, "store sensis chosen, but sensitivity grids not set"); 
            // first cube index can be set to 0, since at the moment we only use the netting-set cube for sensi storage
            if (xvaVars->dimModel_ == "SimmAnalytic")
                sensitivityStorageManager_ = QuantLib::ext::make_shared<ore::analytics::SimmSensitivityStorageManager>(
                    analytic()->configurations().crossAssetModelData->currencies(), 0);
            else
                sensitivityStorageManager_ = QuantLib::ext::make_shared<ore::analytics::CamSensitivityStorageManager>(
                    analytic()->configurations().crossAssetModelData->currencies(), n, u, v, w, 0,
                    sensitivities2ndOrder);
            QL_REQUIRE(sensitivityStorageManager_, "creating sensitivity storage manager failed");

            // Create the netting set cube 
	        Size samples = analytic()->configurations().scenarioGeneratorData->samples();
            vector<Date> dates = analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();
            std::set<std::string> nettingSets = getNettingSetIds(portfolio);
	        LOG("Initialise netting set cube for " << nettingSets.size() << " netting sets, " << dates.size() << " valuation dates, " << samples
		        << " samples, " << sensitivityStorageManager_->getRequiredSize() << " sensitivities to store");
            nettingSetCube_ = QuantLib::ext::make_shared<SinglePrecisionSparseNpvCube>(
                inputs_->asof(), nettingSets, dates, samples, sensitivityStorageManager_->getRequiredSize(), 0.0f);
        }

        // Init counterparty cube for the storage of survival probabilities
        if (xvaVars->storeSurvivalProbabilities_) {
            // Use full list of counterparties, not just those in the sub-portflio
            auto counterparties = inputs_->portfolio()->counterparties();
            counterparties.insert(xvaVars->dvaName_);
            initCube(cptyCube_, counterparties, 1);
        } else {
            cptyCube_ = nullptr;
        }
    }

    LOG("XVA: initClassicRun completed");
}

QuantLib::ext::shared_ptr<Portfolio>
XvaAnalyticImpl::classicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
    LOG("XVA: classicRun");

    Size n = portfolio->size();
    // Create a new empty portfolio, fill it and link it to the simulation market
    // We don't use Analytic::buildPortfolio() here because we are possibly dealing with a sub-portfolio only.
    LOG("XVA: Build classic portfolio of size " << n << " linked to the simulation market");
    const string msg = "XVA: Build Portfolio";
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    classicPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());
    portfolio->reset();
    for (const auto& [tradeId, trade] : portfolio->trades())
        classicPortfolio_->add(trade);
    QL_REQUIRE(analytic()->market(), "today's market not set");
    QuantLib::ext::shared_ptr<EngineFactory> factory = engineFactory();
    classicPortfolio_->build(factory, "analytic/" + label(), true, inputs_->useAtParCouponsTrades());
    Date maturityDate = inputs_->asof();
    if (inputs_->portfolioFilterDate() != Null<Date>())
        maturityDate = inputs_->portfolioFilterDate();
    LOG("Filter trades that expire before " << maturityDate);
    classicPortfolio_->removeMatured(maturityDate);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    // Allocate cubes for the sub-portfolio we are processing here
    initClassicRun(classicPortfolio_);

    // This is where the valuation work is done
    buildClassicCube(classicPortfolio_);

    LOG("XVA: classicRun completed");

    return classicPortfolio_;
}

void XvaAnalyticImpl::buildClassicCube(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("XVA::buildCube");

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    // set up valuation calculator factory
    auto calculators = [this, xvaVars]() {
        vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
        if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag()) {
            QuantLib::ext::shared_ptr<NPVCalculator> npvCalc =
                QuantLib::ext::make_shared<NPVCalculator>(xvaVars->exposureBaseCurrency_);
            calculators.push_back(QuantLib::ext::make_shared<MPORCalculator>(
                npvCalc, cubeInterpreter_->defaultDateNpvIndex(), cubeInterpreter_->closeOutDateNpvIndex()));
        } else
            calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(xvaVars->exposureBaseCurrency_));
        if (xvaVars->storeFlows_)
            calculators.push_back(QuantLib::ext::make_shared<CashflowCalculator>(
                xvaVars->exposureBaseCurrency_, inputs_->asof(), grid_, cubeInterpreter_->mporFlowsIndex()));
	    // Ensure that the NPV calculator is executed before the exercise calculator
        if (xvaVars->storeExerciseValues_)
            calculators.push_back(QuantLib::ext::make_shared<ExerciseCalculator>(
                xvaVars->exposureBaseCurrency_, cubeInterpreter_->exerciseValueIndex()));
        if (xvaVars->storeCreditStateNPVs_ > 0) {
            calculators.push_back(QuantLib::ext::make_shared<MultiStateNPVCalculator>(
                xvaVars->exposureBaseCurrency_, cubeInterpreter_->creditStateNPVsIndex(),
                xvaVars->storeCreditStateNPVs_));
        }
        if (xvaVars->storeSensis_) {
	    LOG("CamSensitivityStorageManager: store sensis true");
	    QL_REQUIRE(sensitivityStorageManager_, "sensitivity storage manager not set");
            calculators.push_back(QuantLib::ext::make_shared<SensitivityCalculator>(sensitivityStorageManager_));
        }
        return calculators;
    };

    // set up cpty calculator factory

    auto cptyCalculators = [this, xvaVars]() {
        vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>> cptyCalculators;
        if (xvaVars->storeSurvivalProbabilities_) {
            string configuration = inputs_->marketConfig("simulation");
            cptyCalculators.push_back(QuantLib::ext::make_shared<SurvivalProbabilityCalculator>(configuration));
        }
        return cptyCalculators;
    };

    // log message

    ostringstream o;
    o << "XVA: Build Cube " << portfolio->size() << " x " << grid_->valuationDates().size() << " x " << samples_;
    CONSOLEW(o.str());
    LOG(o.str());

    // set up progress indicators

    auto progressBar = QuantLib::ext::make_shared<SimpleProgressBar>(o.str(), ConsoleLog::instance().width(),
                                                                     ConsoleLog::instance().progressBarWidth());
    auto progressLog = QuantLib::ext::make_shared<ProgressLog>("XVA: Building cube", 100, oreSeverity::notice);

    if (inputs_->nThreads() == 1) {

        // single-threaded engine run

        ValuationEngine engine(inputs_->asof(), grid_, simMarket_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.buildCube(portfolio, cube_, calculators(), ValuationEngine::ErrorPolicy::RemoveAll,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), nettingSetCube_,
                         cptyCube_, cptyCalculators());
    } else {

        // multi-threaded engine run

        /* TODO we assume no netting output cube is needed. Currently there are no valuation calculators in ore that
         * require this cube. */

        auto cubeFactory = [this, xvaVars](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                  const std::vector<QuantLib::Date>& dates,
                                  const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
            if (xvaVars->xvaUseDoublePrecisionCubes_)
                return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, cubeDepth_, 0.0);
            else
                return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, cubeDepth_, 0.0);
        };

        std::function<QuantLib::ext::shared_ptr<NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                         const std::vector<QuantLib::Date>&, const QuantLib::Size)>
            cptyCubeFactory;
        if (xvaVars->storeSurvivalProbabilities_) {
            cptyCubeFactory = [xvaVars](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                     const std::vector<QuantLib::Date>& dates,
                                     const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
                if (xvaVars->xvaUseDoublePrecisionCubes_)
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, 0.0f);
                else
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, 0.0f);
            };
        } else {
            cptyCubeFactory = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                 const std::vector<QuantLib::Date>& dates,
                                 const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> { return nullptr; };
        }

        MultiThreadedValuationEngine engine(
            inputs_->nThreads(), inputs_->asof(), grid_, samples_, analytic()->loader(), scenarioGenerator_,
            xvaVars->simulationPricingEngine_, inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, inputs_->marketConfig("simulation"),
            analytic()->configurations().simMarketParams, false, false, QuantLib::ext::make_shared<ScenarioFilter>(),
            inputs_->refDataManager(), inputs_->iborFallbackConfig(), true, false, false, cubeFactory, {},
            cptyCubeFactory, "xva-simulation", offsetScenario_, inputs_->useAtParCouponsCurves(),
            inputs_->useAtParCouponsTrades());

        engine.setAggregationScenarioData(scenarioData_);
        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);

        engine.buildCube(portfolio, calculators, ValuationEngine::ErrorPolicy::RemoveAll, cptyCalculators,
                         analytic()->configurations().scenarioGeneratorData->withMporStickyDate());

        cube_ = QuantLib::ext::make_shared<JointNPVCube>(engine.outputCubes(), portfolio->ids());

        if (xvaVars->storeSurvivalProbabilities_)
            cptyCube_ = QuantLib::ext::make_shared<JointNPVCube>(
                engine.outputCptyCubes(), portfolio->counterparties(), false,
                [](Real a, Real x) { return std::max(a, x); }, 0.0);
    }

    CONSOLE("OK");

    LOG("XVA::buildCube done");

    Settings::instance().evaluationDate() = inputs_->asof();
}

QuantLib::ext::shared_ptr<EngineFactory>
XvaAnalyticImpl::amcEngineFactory(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                  const std::vector<Date>& simDates, const std::vector<Date>& stickyCloseOutDates) {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    LOG("XvaAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*xvaVars->amcPricingEngine_);
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    edCopy->globalParameters()["McType"] = "American";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    ext::shared_ptr<ore::data::Market> market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;
    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, market, configurations, inputs_->refDataManager(), inputs_->iborFallbackConfig(),
        EngineBuilderFactory::instance().generateAmcEngineBuilders(cam, simDates, stickyCloseOutDates));
    return factory;
}

void XvaAnalyticImpl::buildAmcPortfolio() {

    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    LOG("XVA: buildAmcPortfolio");
    const string msg = "XVA: Build AMC portfolio";
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();

    std::vector<Date> simDates, stickyCloseOutDates;
    if (analytic()->configurations().scenarioGeneratorData->withCloseOutLag() &&
        analytic()->configurations().scenarioGeneratorData->withMporStickyDate()) {
        simDates = analytic()->configurations().scenarioGeneratorData->getGrid()->valuationDates();
        stickyCloseOutDates = analytic()->configurations().scenarioGeneratorData->getGrid()->closeOutDates();
    } else {
        simDates = analytic()->configurations().scenarioGeneratorData->getGrid()->dates();
    }

    LOG("buildAmcPortfolio: Register additional engine builders");
    auto factory = amcEngineFactory(model_, simDates, stickyCloseOutDates);

    LOG("buildAmcPortfolio: Load Portfolio");
    QuantLib::ext::shared_ptr<Portfolio> portfolio = inputs_->portfolio();

    LOG("Build Portfolio with AMC Engine factory and select amc-enabled trades")
    amcPortfolio_ = QuantLib::ext::make_shared<Portfolio>();
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        if (xvaVars->amcTradeTypes_.find(trade->tradeType()) != xvaVars->amcTradeTypes_.end()) {
            if (xvaVars->amcCg_ != XvaEngineCG::Mode::CubeGeneration) {
                auto t = trade;
                auto [ft, success] =
                    buildTrade(t, factory, "analytic/" + label(), false, true, true, inputs_->useAtParCouponsTrades());
                if (success)
                    amcPortfolio_->add(trade);
                else
                    amcPortfolio_->add(ft);
            } else {
                // the amc-cg engine will build the trades itself
                trade->reset();
                amcPortfolio_->add(trade);
            }
            DLOG("trade " << tradeId << " is added to amc portfolio");
        }
    }

    LOG("AMC portfolio built, size is " << amcPortfolio_->size());

    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    LOG("XVA: buildAmcPortfolio completed");
}

void XvaAnalyticImpl::amcRun(bool doClassicRun, bool continueOnCalibrationError, bool allowModelFallbacks) {

    LOG("XVA: amcRun");
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);

    if (scenarioData_ == nullptr) {
        LOG("XVA: Create asd " << grid_->valuationDates().size() << " x " << samples_);
        scenarioData_ =
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(grid_->valuationDates().size(), samples_);
        simMarket_->aggregationScenarioData() = scenarioData_;
    }

    initCubeDepth();

    std::string message = "XVA: Build AMC Cube " + std::to_string(amcPortfolio_->size()) + " x " +
                          std::to_string(grid_->valuationDates().size()) + " x " + std::to_string(samples_) + "... ";
    CONSOLEW(message);
    auto progressBar = QuantLib::ext::make_shared<SimpleProgressBar>(message, ConsoleLog::instance().width(),
                                                                     ConsoleLog::instance().progressBarWidth());
    auto progressLog = QuantLib::ext::make_shared<ProgressLog>("XVA: Build AMC Cube...", 100, oreSeverity::notice);

    if (xvaVars->amcCg_ == XvaEngineCG::Mode::CubeGeneration) {

        // cube generation with amc-cg engine

        initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);

        if (xvaVars->xvaCgDynamicIM_) {
            // cube storing dynamic IM per netting set (total margin, delta, vega, curvature), i.e. depth 4
            Size imCubeDepth = 4;
            nettingSetCube_ = QuantLib::ext::make_shared<SinglePrecisionSparseNpvCube>(
                inputs_->asof(), getNettingSetIds(amcPortfolio_), grid_->valuationDates(), samples_, imCubeDepth, 0.0f);
        }

        XvaEngineCG engine(
            xvaVars->amcCg_, inputs_->nThreads(), inputs_->asof(), analytic()->loader(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
            xvaVars->amcCgPricingEngine_, xvaVars->crossAssetModelData_, xvaVars->scenarioGeneratorData_,
            amcPortfolio_, inputs_->marketConfig("simulation"), inputs_->marketConfig("lgmcalibration"),
            xvaVars->xvaCgSensiScenarioData_, inputs_->refDataManager(), inputs_->iborFallbackConfig(),
            xvaVars->xvaCgBumpSensis_, xvaVars->xvaCgDynamicIM_, xvaVars->xvaCgDynamicIMStepSize_,
            xvaVars->xvaCgRegressionOrder_, xvaVars->xvaCgRegressionVarianceCutoff_,
            xvaVars->xvaCgRegressionOrderDynamicIm_, xvaVars->xvaCgRegressionVarianceCutoffDynamicIm_,
            xvaVars->xvaCgTradeLevelBreakdown_, xvaVars->xvaCgRegressionReportTimeStepsDynamicIM_,
            xvaVars->xvaCgUseRedBlocks_, xvaVars->xvaCgUseExternalComputeDevice_,
            xvaVars->xvaCgExternalDeviceCompatibilityMode_, xvaVars->xvaCgUseDoublePrecisionForExternalCalculation_,
            xvaVars->xvaCgExternalComputeDevice_, xvaVars->xvaCgUsePythonIntegration_,
            xvaVars->xvaCgUsePythonIntegrationDynamicIm_, true, true, true, inputs_->useAtParCouponsCurves(),
            inputs_->useAtParCouponsTrades(), "xva analytic");

        engine.registerProgressIndicator(progressBar);
        engine.registerProgressIndicator(progressLog);
        engine.setAggregationScenarioData(scenarioData_);
        engine.setOffsetScenario(offsetScenario_);
        engine.setNpvOutputCube(amcCube_);
        if (xvaVars->xvaCgDynamicIM_) {
            engine.setDynamicIMOutputCube(nettingSetCube_);
        }
        engine.run();

        if (engine.dynamicImRegressionReport())
            analytic()->addReport(LABEL, "xvacg-regression", engine.dynamicImRegressionReport());

    } else {

        // cube generation with amc engine

        if (inputs_->nThreads() == 1) {
            initCube(amcCube_, amcPortfolio_->ids(), cubeDepth_);
            ext::shared_ptr<ore::data::Market> market =
                offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

            AMCValuationEngine amcEngine(
                model_, xvaVars->scenarioGeneratorData_, market,
                xvaVars->exposureSimMarketParams_->additionalScenarioDataIndices(),
                xvaVars->exposureSimMarketParams_->additionalScenarioDataCcys(),
                xvaVars->exposureSimMarketParams_->additionalScenarioDataNumberOfCreditStates(),
                xvaVars->amcPathDataInput_, xvaVars->amcPathDataOutput_, xvaVars->amcIndividualTrainingInput_,
                xvaVars->amcIndividualTrainingOutput_);
            amcEngine.registerProgressIndicator(progressBar);
            amcEngine.registerProgressIndicator(progressLog);
            amcEngine.aggregationScenarioData() = scenarioData_;
            amcEngine.buildCube(amcPortfolio_, amcCube_);
        } else {
            auto cubeFactory = [this, xvaVars](const QuantLib::Date& asof, const std::set<std::string>& ids,
                                      const std::vector<QuantLib::Date>& dates,
                                      const Size samples) -> QuantLib::ext::shared_ptr<NPVCube> {
                if (xvaVars->xvaUseDoublePrecisionCubes_)
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, cubeDepth_,
                                                                               0.0);
                else
                    return QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, cubeDepth_,
                                                                              0.0);
            };

            auto simMarketParams =
                offsetScenario_ == nullptr ? analytic()->configurations().simMarketParams : offsetSimMarketParams_;

            AMCValuationEngine amcEngine(
                inputs_->nThreads(), inputs_->asof(), samples_, analytic()->loader(), xvaVars->scenarioGeneratorData_,
                xvaVars->exposureSimMarketParams_->additionalScenarioDataIndices(),
                xvaVars->exposureSimMarketParams_->additionalScenarioDataCcys(),
                xvaVars->exposureSimMarketParams_->additionalScenarioDataNumberOfCreditStates(),
                xvaVars->crossAssetModelData_, xvaVars->amcPricingEngine_, inputs_->curveConfigs().get(),
                analytic()->configurations().todaysMarketParams, inputs_->marketConfig("lgmcalibration"),
                inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
                inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
                inputs_->marketConfig("simulation"), xvaVars->amcPathDataInput_, xvaVars->amcPathDataOutput_,
                xvaVars->amcIndividualTrainingInput_, xvaVars->amcIndividualTrainingOutput_,
                inputs_->refDataManager(), inputs_->iborFallbackConfig(), true, cubeFactory, offsetScenario_,
                simMarketParams, continueOnCalibrationError, allowModelFallbacks, inputs_->useAtParCouponsCurves(),
                inputs_->useAtParCouponsTrades());

            amcEngine.registerProgressIndicator(progressBar);
            amcEngine.registerProgressIndicator(progressLog);
            amcEngine.aggregationScenarioData() = scenarioData_;
            amcEngine.buildCube(amcPortfolio_);
            amcCube_ = QuantLib::ext::make_shared<JointNPVCube>(amcEngine.outputCubes());
        }
    }

    CONSOLE("OK");
    LOG("XVA: amcRun completed");
}

void XvaAnalyticImpl::runPostProcessor() {
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    QuantLib::ext::shared_ptr<NettingSetManager> netting = xvaVars->nettingSetManager_;
    QuantLib::ext::shared_ptr<CollateralBalances> balances = xvaVars->collateralBalances_;
    map<string, bool> analytics;
    analytics["exerciseNextBreak"] = xvaVars->exerciseNextBreak_;
    analytics["cva"] = xvaVars->cvaAnalytic_;
    analytics["dva"] = xvaVars->dvaAnalytic_;
    analytics["fva"] = xvaVars->fvaAnalytic_;
    analytics["colva"] = xvaVars->colvaAnalytic_;
    analytics["collateralFloor"] = xvaVars->collateralFloorAnalytic_;
    analytics["mva"] = xvaVars->mvaAnalytic_;
    analytics["kva"] = xvaVars->kvaAnalytic_;
    analytics["dim"] = xvaVars->dimAnalytic_;
    analytics["dynamicCredit"] = xvaVars->dynamicCredit_;
    analytics["cvaSensi"] = xvaVars->cvaSensi_;
    analytics["flipViewXVA"] = xvaVars->flipViewXVA_;
    analytics["creditMigration"] = xvaVars->creditMigrationAnalytic_;
    analytics["exposureProfilesUseCloseOutValues"] = xvaVars->exposureProfilesUseCloseOutValues_;

    string baseCurrency = xvaVars->xvaBaseCurrency_;
    string calculationType = xvaVars->collateralCalculationType_;
    string allocationMethod = xvaVars->exposureAllocationMethod_;
    Real marginalAllocationLimit = xvaVars->marginalAllocationLimit_;
    Real quantile = xvaVars->pfeQuantile_;
    string dvaName = xvaVars->dvaName_;
    string fvaLendingCurve = xvaVars->fvaLendingCurve_;
    string fvaBorrowingCurve = xvaVars->fvaBorrowingCurve_;

    Real dimQuantile = xvaVars->dimQuantile_;
    Size dimHorizonCalendarDays = xvaVars->dimHorizonCalendarDays_;
    Size dimRegressionOrder = xvaVars->dimRegressionOrder_;
    vector<string> dimRegressors = xvaVars->dimRegressors_;
    Size dimLocalRegressionEvaluations = xvaVars->dimLocalRegressionEvaluations_;
    Real dimLocalRegressionBandwidth = xvaVars->dimLocalRegressionBandwidth_;

    Real kvaCapitalDiscountRate = xvaVars->kvaCapitalDiscountRate_;
    Real kvaAlpha = xvaVars->kvaAlpha_;
    Real kvaRegAdjustment = xvaVars->kvaRegAdjustment_;
    Real kvaCapitalHurdle = xvaVars->kvaCapitalHurdle_;
    Real kvaOurPdFloor = xvaVars->kvaOurPdFloor_;
    Real kvaTheirPdFloor = xvaVars->kvaTheirPdFloor_;
    Real kvaOurCvaRiskWeight = xvaVars->kvaOurCvaRiskWeight_;
    Real kvaTheirCvaRiskWeight = xvaVars->kvaTheirCvaRiskWeight_;

    string marketConfiguration = inputs_->marketConfig("simulation");

    bool fullInitialCollateralisation = xvaVars->fullInitialCollateralisation_;
    bool firstMporCollateralAdjustment = xvaVars->firstMporCollateralAdjustment_;
    checkConfigurations(analytic()->portfolio());
    applyConfigurationFallback(analytic()->portfolio());

    if (!dimCalculator_ && (analytics["mva"] || analytics["dim"])) {
        LOG("dim calculator not set, create one");
	    std::map<std::string, Real> currentIM;
        if (xvaVars->collateralBalances_) {
                for (auto const& [n, b] : xvaVars->collateralBalances_->collateralBalances()) {
                currentIM[n.nettingSetId()] =
                    b->initialMargin() *
                    (b->currency() == baseCurrency
                         ? 1.0
                         : analytic()->market()->fxRate(b->currency() + baseCurrency, marketConfiguration)->value());
            }
        }

        DLOG("Create a '" << xvaVars->dimModel_ << "' Dynamic Initial Margin Calculator");

        if (xvaVars->dimModel_ == "Regression") {
            dimCalculator_ = QuantLib::ext::make_shared<RegressionDynamicInitialMarginCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimRegressionOrder, dimRegressors, dimLocalRegressionEvaluations,
                dimLocalRegressionBandwidth, currentIM);
        } else if (xvaVars->dimModel_ == "DeltaVaR" ||
		   xvaVars->dimModel_ == "DeltaGammaNormalVaR" ||
                   xvaVars->dimModel_ == "DeltaGammaVaR") {
            QL_REQUIRE(nettingSetCube_ && sensitivityStorageManager_,
                       "netting set cube or sensitivity storage manager not set - "
                           << "is this a single-threaded classic run storing sensis?");
            // delta 1, delta-gamma-normal 2, delta-gamma 3
            Size ddvOrder;
            if (xvaVars->dimModel_ == "DeltaVaR")
                ddvOrder = 1;
            else if (xvaVars->dimModel_ == "DeltaGammaNormalVaR")
                ddvOrder = 2;
            else
                ddvOrder = 3;
            QuantLib::ext::shared_ptr<DimHelper> dimHelper = QuantLib::ext::make_shared<DimHelper>(
                model_, nettingSetCube_, sensitivityStorageManager_, xvaVars->curveSensiGrid_, dimHorizonCalendarDays);
            dimCalculator_ = QuantLib::ext::make_shared<DynamicDeltaVaRCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimHelper, ddvOrder, currentIM);
        } else if (xvaVars->dimModel_ == "SimmAnalytic") {
            QL_REQUIRE(nettingSetCube_ && sensitivityStorageManager_,
                       "netting set cube or sensitivity storage manager not set - "
                           << "is this a single-threaded classic run storing sensis?");
            QuantLib::ext::shared_ptr<SimmHelper> simmHelper = QuantLib::ext::make_shared<SimmHelper>(
                analytic()->configurations().crossAssetModelData->currencies(),
		nettingSetCube_, scenarioData_, sensitivityStorageManager_, analytic()->market());
            Size imCubeDepth = 6; // allow for total, delta, vega and curvature margin at depths 0-3, fx delta and ir delta at depths 4-5
            dimCalculator_ = QuantLib::ext::make_shared<DynamicSimmCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, simmHelper, dimQuantile,
                dimHorizonCalendarDays, currentIM, imCubeDepth);
        } else if (xvaVars->dimModel_ == "DynamicIM") {
            QL_REQUIRE(nettingSetCube_ && xvaVars->xvaCgDynamicIM_ &&
                           xvaVars->amcCg_ == XvaEngineCG::Mode::CubeGeneration,
                       "dim model is set to DynamicIM, this requires amcCg=CubeGeneration, xvaCgDynamicIM=true");
            dimCalculator_ = QuantLib::ext::make_shared<DirectDynamicInitialMarginCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, nettingSetCube_, currentIM);
        } else {
            WLOG("dim model not specified, create FlatDynamicInitialMarginCalculator");
            dimCalculator_ = QuantLib::ext::make_shared<FlatDynamicInitialMarginCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, xvaVars->collateralBalances_);
        }
    }

    std::vector<Period> cvaSensiGrid = xvaVars->cvaSensiGrid_;
    Real cvaSensiShiftSize = xvaVars->cvaSensiShiftSize_;

    string flipViewBorrowingCurvePostfix = xvaVars->flipViewBorrowingCurvePostfix_;
    string flipViewLendingCurvePostfix = xvaVars->flipViewLendingCurvePostfix_;

    LOG("baseCurrency " << baseCurrency);

    auto market = offsetScenario_ == nullptr ? analytic()->market() : offsetSimMarket_;

    postProcess_ = QuantLib::ext::make_shared<PostProcess>(
        analytic()->portfolio(), netting, balances, market, marketConfiguration, cube_, scenarioData_, analytics,
        baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, fvaBorrowingCurve,
        fvaLendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
        cvaSensiShiftSize, kvaCapitalDiscountRate, kvaAlpha, kvaRegAdjustment, kvaCapitalHurdle, kvaOurPdFloor,
        kvaTheirPdFloor, kvaOurCvaRiskWeight, kvaTheirCvaRiskWeight, cptyCube_, flipViewBorrowingCurvePostfix,
        flipViewLendingCurvePostfix, xvaVars->creditSimulationParameters_, xvaVars->creditMigrationDistributionGrid_,
        xvaVars->creditMigrationTimeSteps_, creditStateCorrelationMatrix(),
        analytic()->configurations().scenarioGeneratorData->withMporStickyDate(), xvaVars->mporCashFlowMode_,
        firstMporCollateralAdjustment, inputs_->continueOnError(), xvaVars->xvaUseDoublePrecisionCubes_);
    LOG("post done");
}

void XvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::set<std::string>& runTypes) {
    
    auto xvaVars = ext::dynamic_pointer_cast<XvaVariables>(inputVariables_);
    LOG("XVA analytic is running with amc cg mode '" << xvaVars->amcCg_ << "'.");

    QL_REQUIRE(!((offsetScenario_ == nullptr) ^ (offsetSimMarketParams_ == nullptr)),
               "Need offsetScenario and corresponding simMarketParameter");

    SavedSettings settings;

    Settings::instance().includeTodaysCashFlows() = *xvaVars->exposureIncludeTodaysCashFlows_;
    LOG("Simulation IncludeTodaysCashFlows is defined: " << (xvaVars->exposureIncludeTodaysCashFlows_ ? "true"
                                                                                                    : "false"));
    if (xvaVars->exposureIncludeTodaysCashFlows_) {
        LOG("Exposure IncludeTodaysCashFlows is set to "
            << (*xvaVars->exposureIncludeTodaysCashFlows_ ? "true" : "false"));
    }

    Settings::instance().includeReferenceDateEvents() = *xvaVars->exposureIncludeReferenceDateEvents_;
    LOG("Simulation IncludeReferenceDateEvents is set to "
        << (xvaVars->exposureIncludeReferenceDateEvents_ ? "true" : "false"));

    std::map<std::string, double> cubeNpvOverlay;
    if (xvaVars->cubeNpvOverlay_) {
        auto pricingAnalytic = dependentAnalytic("PRICING");
        static_cast<PricingAnalyticImpl*>(pricingAnalytic->impl().get())
            ->overwriteResultCurrency(analytic()->configurations().simMarketParams->baseCcy());
        pricingAnalytic->runAnalytic(loader,{"NPV"});
        auto npvReport = pricingAnalytic->reports().at("NPV").at("npv");
        std::size_t colTradeId = npvReport->columnPosition("TradeId");
        std::size_t colNpvBase = npvReport->columnPosition("NPV(Base)");
        for (Size r = 0; r < npvReport->rows(); ++r) {
            cubeNpvOverlay[boost::get<std::string>(npvReport->data(colTradeId, r))] =
                boost::get<double>(npvReport->data(colNpvBase, r));
        }
    }

    if(xvaVars->generateCorrelations_){
        auto corrAnalytic = dependentAnalytic(corrLookupKey);
        corrAnalytic->runAnalytic(loader,{"CORRELATION"});
        auto cai = static_cast<CorrelationAnalyticImpl*>(corrAnalytic->impl().get());
        auto corrReportObject = cai->correlationReport();
        const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& corrData = corrReportObject->correlationData();
        QL_REQUIRE(!corrData.empty(),"generateCorrelations returned empty Correlations");
        feedCorrelationToCAM(corrData);
        auto report = corrAnalytic->reports().at("CORRELATION").at("correlation");
        analytic()->addReport(LABEL,"correlation",report);
    }

    LOG("XVA analytic called with asof " << io::iso_date(inputs_->asof()));
    ProgressMessage("Running XVA Analytic", 0, 1).log();

    if (runTypes.find("EXPOSURE") != runTypes.end() || runTypes.empty())
        runSimulation_ = true;

    if (runTypes.find("XVA") != runTypes.end() || runTypes.empty())
        runXva_ = true;

    if (runTypes.find("PFE") != runTypes.end() || runTypes.empty())
        runPFE_ = true;

    if (!runSimulation_ && (runXva_ || runPFE_))
        xvaVars->loadCube(inputs_);

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(xvaVars->exposureObservationModel_);

    const string msg = "XVA: Build Today's Market";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    if (xvaVars->amcCg_ == XvaEngineCG::Mode::Full) {
        // note: market configs both set to simulation, see note in xvaenginecg, we'd need inccy config
        // in sim market there...
        XvaEngineCG engine(
            xvaVars->amcCg_, inputs_->nThreads(), inputs_->asof(), analytic()->loader(), inputs_->curveConfigs().get(),
            analytic()->configurations().todaysMarketParams, analytic()->configurations().simMarketParams,
            xvaVars->amcCgPricingEngine_, xvaVars->crossAssetModelData_, xvaVars->scenarioGeneratorData_,
            inputs_->portfolio(), inputs_->marketConfig("simulation"), inputs_->marketConfig("simulation"),
            xvaVars->xvaCgSensiScenarioData_, inputs_->refDataManager(), inputs_->iborFallbackConfig(),
            xvaVars->xvaCgBumpSensis_, xvaVars->xvaCgDynamicIM_, xvaVars->xvaCgDynamicIMStepSize_,
            xvaVars->xvaCgRegressionOrder_, xvaVars->xvaCgRegressionVarianceCutoff_,
            xvaVars->xvaCgRegressionOrderDynamicIm_, xvaVars->xvaCgRegressionVarianceCutoffDynamicIm_,
            xvaVars->xvaCgTradeLevelBreakdown_, xvaVars->xvaCgRegressionReportTimeStepsDynamicIM_,
            xvaVars->xvaCgUseRedBlocks_, xvaVars->xvaCgUseExternalComputeDevice_,
            xvaVars->xvaCgExternalDeviceCompatibilityMode_, xvaVars->xvaCgUseDoublePrecisionForExternalCalculation_,
            xvaVars->xvaCgExternalComputeDevice_, xvaVars->xvaCgUsePythonIntegration_,
            xvaVars->xvaCgUsePythonIntegrationDynamicIm_, true, true, true, inputs_->useAtParCouponsCurves(),
            inputs_->useAtParCouponsTrades(), "xva analytic");

        engine.run();

        analytic()->addReport(LABEL, "xvacg-exposure", engine.exposureReport());
        if (xvaVars->xvaCgSensiScenarioData_)
            analytic()->addReport(LABEL, "xvacg-cva-sensi-scenario", engine.sensiReport());
        if(engine.dynamicImRegressionReport())
            analytic()->addReport(LABEL, "xvacg-regression", engine.dynamicImRegressionReport());
        return;
    }

    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();
    cubeInterpreter_ = QuantLib::ext::make_shared<CubeInterpretation>(
        xvaVars->storeFlows_, analytic()->configurations().scenarioGeneratorData->withCloseOutLag(),
        xvaVars->storeExerciseValues_, grid_, xvaVars->storeCreditStateNPVs_, xvaVars->flipViewXVA_);

    if (runSimulation_) {
        LOG("XVA: Build simulation market");
        buildScenarioSimMarket();

        LOG("XVA: Build Scenario Generator");
        auto continueOnErr = false;
        auto allowModelFallbacks = false;
        auto globalParams = xvaVars->simulationPricingEngine_->globalParameters();
        if (auto c = globalParams.find("ContinueOnCalibrationError"); c != globalParams.end())
            continueOnErr = parseBool(c->second);
        if (auto c = globalParams.find("AllowModelFallbacks"); c != globalParams.end())
            allowModelFallbacks = parseBool(c->second);
        buildScenarioGenerator(continueOnErr, allowModelFallbacks);

        LOG("XVA: Attach Scenario Generator to ScenarioSimMarket");
        simMarket_->scenarioGenerator() = scenarioGenerator_;

        // We may have to build two cubes below for complementary sub-portfolios, a classical cube and an AMC cube
        bool doClassicRun = true;
        bool doAmcRun = false;

        // Initialize the residual "classical" portfolio that we do not process using AMC
        auto residualPortfolio = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        if (xvaVars->amc_ || xvaVars->amcCg_ == XvaEngineCG::Mode::CubeGeneration) {
            // Build a separate sub-portfolio for the AMC cube generation and perform its training
            buildAmcPortfolio();

            // Build the residual portfolio for the classic cube generation, i.e. strip out the AMC part
            for (auto const& [tradeId, trade] : inputs_->portfolio()->trades()) {
                if (xvaVars->amcTradeTypes_.find(trade->tradeType()) == xvaVars->amcTradeTypes_.end())
                    residualPortfolio->add(trade);
            }

            LOG("AMC portfolio size " << amcPortfolio_->size());
            LOG("Residual portfolio size " << residualPortfolio->size());

            doAmcRun = !amcPortfolio_->trades().empty();
            doClassicRun = !residualPortfolio->trades().empty();
        } else {
            for (const auto& [tradeId, trade] : inputs_->portfolio()->trades())
                residualPortfolio->add(trade);
        }

        /********************************************************************************
         * This is where we build cubes and the "classic" valuation work is done
         * The bulk of the AMC work is done before in the AMC portfolio building/training
         ********************************************************************************/

        if (doAmcRun)
            amcRun(doClassicRun, continueOnErr, allowModelFallbacks);
        else
            amcPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        if (doClassicRun)
            classicPortfolio_ = classicRun(residualPortfolio);
        else
            classicPortfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs_->buildFailedTrades());

        /***************************************************
         * We may have two cubes now that need to be merged
         ***************************************************/

        if (doClassicRun && doAmcRun) {
            LOG("Joining classical and AMC cube");
            cube_ = QuantLib::ext::make_shared<JointNPVCube>(cube_, amcCube_);
        } else if (!doClassicRun && doAmcRun) {
            LOG("We have generated an AMC cube only");
            cube_ = amcCube_;
        } else {
            LOG("We have generated a classic cube only");
        }

        LOG("NPV cube generation completed");

        /************************************************************
         * Apply correction pricing t0 npv - sim t0 npv if requested
         ************************************************************/

        if (!cubeNpvOverlay.empty()) {
            cube_ = QuantLib::ext::make_shared<OverlayNPVCube>(cube_, cubeNpvOverlay);
        }

        /***********************************************************************
         * We may have two non-empty portfolios to be merged for post processing
         ***********************************************************************/

        LOG("Classic portfolio size " << classicPortfolio_->size());
        LOG("AMC portfolio size " << amcPortfolio_->size());
        auto newPortfolio = QuantLib::ext::make_shared<Portfolio>();
        for (const auto& [tradeId, trade] : classicPortfolio_->trades())
            newPortfolio->add(trade);
        for (const auto& [tradeId, trade] : amcPortfolio_->trades())
            newPortfolio->add(trade);
        LOG("Total portfolio size " << newPortfolio->size());
        if (newPortfolio->size() < inputs_->portfolio()->size()) {
            ALOG("input portfolio size is " << inputs_->portfolio()->size() << ", but we have built only "
                                            << newPortfolio->size() << " trades");
        }
        analytic()->setPortfolio(newPortfolio);
    } else { // runSimulation_

        // build the portfolio linked to today's market
        //
        // during simulation stage, trades may be built using amc engine factory
        // instead of classic engine factory, resulting in trade errors from the following buildPortfolio()
        //
        // when buildFailedTrades is set to False, trade errors are emitted in structured log, because
        // the trades will be removed from the portfolio and do NOT participate in the post-processing.
        // we have a genuine interest in such errors
        //
        // when buildFailedTrades is set to True, trade errors are NOT emitted in structured log, because
        // the trades will NOT be removed from the portfolio and DO participate in the post-processing.
        // any genuine error should have been reported during simulation stage
        analytic()->buildPortfolio(!inputs_->buildFailedTrades());

        // ... and load a pre-built cube for post-processing

        LOG("Skip cube generation, load input cubes for XVA");
        const string msg = "XVA: Load Cubes";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        QL_REQUIRE(xvaVars->cube_, "XVA without EXPOSURE requires an NPV cube as input");
        cube_ = xvaVars->cube_;
        QL_REQUIRE(xvaVars->mktCube_, "XVA without EXPOSURE requires a market cube as input");
        scenarioData_ = xvaVars->mktCube_;
        if (xvaVars->nettingSetCube_)
            nettingSetCube_ = xvaVars->nettingSetCube_;
        if (xvaVars->cptyCube_)
            cptyCube_ = xvaVars->cptyCube_;
        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();
    }

    MEM_LOG;

    // Return the cubes to serialalize
    if (xvaVars->writeCube_) {
        analytic()->npvCubes()[LABEL]["cube"] = ext::make_shared<NPVCubeWithMetaData>(cube_, xvaVars->scenarioGeneratorData_, xvaVars->storeFlows_, xvaVars->storeCreditStateNPVs_);
        analytic()->mktCubes()[LABEL]["scenariodata"] = scenarioData_;
        if (nettingSetCube_)
            analytic()->npvCubes()[LABEL]["nettingsetcube"] = ext::make_shared<NPVCubeWithMetaData>(
                nettingSetCube_, xvaVars->scenarioGeneratorData_, xvaVars->storeFlows_, xvaVars->storeCreditStateNPVs_);
        if (cptyCube_) {
            analytic()->npvCubes()[LABEL]["cptycube"] = ext::make_shared<NPVCubeWithMetaData>(
                cptyCube_, xvaVars->scenarioGeneratorData_, xvaVars->storeFlows_, xvaVars->storeCreditStateNPVs_);
        }
    }

    // Generate cube reports to inspect
    if (xvaVars->rawCubeOutput_) {
        map<string, string> nettingSetMap = analytic()->portfolio()->nettingSetMap();
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->setupVariables().reportBufferSize_);
        ReportWriter(inputs_->reportNaString()).writeCube(*report, cube_, nettingSetMap);
        analytic()->addReport(LABEL, "rawcube", report);
    }

    if (runXva_ || runPFE_) {

        /*********************************************************************
         * This is where the aggregation work is done: call the post-processor
         *********************************************************************/

        string runStr = "";
        if (runXva_ && runPFE_) {
            runStr = "XVA and PFE";
        } else if (!runXva_ && runPFE_) {
            runStr = "PFE";
        } else if (runXva_ && !runPFE_) {
            runStr = "XVA";
        }

        string msg = runStr + ": Aggregation";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        runPostProcessor();
        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();

        /******************************************************
         * Finally generate various (in-memory) reports/outputs
         ******************************************************/

        msg = runStr + ": Reports";
        CONSOLEW(msg);
        ProgressMessage(msg, 0, 1).log();
        LOG("Generating " + runStr + " reports and cube outputs");

        // By default, will write all exposure reports individually (one report per trade, nettingset, etc.), but when 
        // writeIndividualExposureReports is set to false, it will combine the reports of the same type into a single file.

        if (xvaVars->exposureProfilesByTrade_) {
            if (xvaVars->writeIndividualExposureReports_) {
                for (const auto& [tradeId, tradeIdCubePos] : postProcess_->tradeIds()) {
                    auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    try {
                        ReportWriter(inputs_->reportNaString()).writeTradeExposures(*report, postProcess_, tradeId);
                        analytic()->addReport(LABEL, "exposure_trade_" + tradeId, report);
                    } catch (const std::exception& e) {
                        QuantLib::ext::shared_ptr<Trade> failedTrade =
                            postProcess_->portfolio()->trades().find(tradeId)->second;
                        map<string, string> subfields;
                        subfields.insert({"tradeId", tradeId});
                        subfields.insert({"tradeType", failedTrade->tradeType()});
                        StructuredAnalyticsErrorMessage("Trade Exposure Report", "Error processing trade.", e.what(),
                                                        subfields)
                            .log();
                    }
                }
            } else {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                try {
                    ReportWriter(inputs_->reportNaString()).writeTradeExposures(*report, postProcess_);
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Trade Exposure Report", "Error processing report.", e.what()).log();
                }
                analytic()->addReport(LABEL, "exposure_trade", report);
            }
        }

        if (xvaVars->exposureProfiles_ || runPFE_) {
            if (xvaVars->writeIndividualExposureReports_) {
                for (auto [nettingSet, nettingSetPosInCube] : postProcess_->nettingSetIds()) {
                    auto exposureReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    try {
                        ReportWriter(inputs_->reportNaString())
                            .writeNettingSetExposures(*exposureReport, postProcess_, nettingSet);
                        analytic()->addReport(LABEL, "exposure_nettingset_" + nettingSet, exposureReport);
                    } catch (const std::exception& e) {
                        StructuredAnalyticsErrorMessage("Netting Set Exposure Report", "Error processing netting set.",
                                                        e.what(), {{"nettingSetId", nettingSet}})
                            .log();
                    }
                    if (runXva_) {
                        auto colvaReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                        try {
                            ReportWriter(inputs_->reportNaString())
                                .writeNettingSetColva(*colvaReport, postProcess_, nettingSet);
                            analytic()->addReport(LABEL, "colva_nettingset_" + nettingSet, colvaReport);
                        } catch (const std::exception& e) {
                            StructuredAnalyticsErrorMessage("Netting Set Colva Report", "Error processing netting set.",
                                                            e.what(), {{"nettingSetId", nettingSet}})
                                .log();
                        }

                        auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                        try {
                            ReportWriter(inputs_->reportNaString())
                                .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_, nettingSet);
                            analytic()->addReport(LABEL, "cva_sensitivity_nettingset_" + nettingSet, cvaSensiReport);
                        } catch (const std::exception& e) {
                            StructuredAnalyticsErrorMessage("Cva Sensi Report", "Error processing netting set.",
                                                            e.what(), {{"nettingSetId", nettingSet}})
                                .log();
                        }
                    }
                }
            } else {
                auto exposureReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                auto colvaReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());

                try {
                    ReportWriter(inputs_->reportNaString())
                        .writeNettingSetExposures(*exposureReport, postProcess_);
                } catch (const std::exception& e) {
                    StructuredAnalyticsErrorMessage("Netting Set Exposure Report", "Error processing netting set.",
                                                    e.what()).log();
                }
                if (runXva_) {
                    try {
                        ReportWriter(inputs_->reportNaString())
                            .writeNettingSetColva(*colvaReport, postProcess_);
                    } catch (const std::exception& e) {
                        StructuredAnalyticsErrorMessage("Netting Set Colva Report", "Error processing netting set.",
                                                        e.what()).log();
                    }
                    try {
                        ReportWriter(inputs_->reportNaString())
                            .writeNettingSetCvaSensitivities(*cvaSensiReport, postProcess_);
                    } catch (const std::exception& e) {
                        StructuredAnalyticsErrorMessage("Cva Sensi Report", "Error processing netting set.",
                                                        e.what()).log();
                    }
                }

                analytic()->addReport(LABEL, "exposure_nettingset", exposureReport);
                if (runXva_) {
                    analytic()->addReport(LABEL, "colva_nettingset", colvaReport);
                    analytic()->addReport(LABEL, "cva_sensitivity_nettingset", cvaSensiReport);
                }

            }
        }

        if (runXva_) {
            auto xvaReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
            ReportWriter(inputs_->reportNaString())
                .writeXVA(*xvaReport, xvaVars->exposureAllocationMethod_, analytic()->portfolio(), postProcess_);
            analytic()->addReport(LABEL, "xva", xvaReport);

            if (xvaVars->netCubeOutput_) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                ReportWriter(inputs_->reportNaString()).writeCube(*report, postProcess_->netCube());
                analytic()->addReport(LABEL, "netcube", report);
            }

            if (xvaVars->timeAveragedNettedExposureOutput_) {
                auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                ReportWriter(inputs_->reportNaString())
                    .writeTimeAveragedNettedExposure(*report, postProcess_->timeAveragedNettedExposure());
                analytic()->addReport(LABEL, "timeAveragedNettedExposure", report);
            }

            if (xvaVars->dimAnalytic_ || xvaVars->mvaAnalytic_) {
                // Generate DIM evolution report
                auto dimEvolutionReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimEvolution(*dimEvolutionReport);
                analytic()->addReport(LABEL, "dim_evolution", dimEvolutionReport);

                // Generate DIM distribution report
                auto dimDistributionReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimDistribution(*dimDistributionReport, xvaVars->dimDistributionGridSize_,
                                                    xvaVars->dimDistributionCoveredStdDevs_);
                analytic()->addReport(LABEL, "dim_distribution", dimDistributionReport);

                auto dimCubeReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                postProcess_->exportDimCube(*dimCubeReport);
                analytic()->addReport(LABEL, "dim_cube", dimCubeReport);

                // Generate DIM regression reports
                vector<QuantLib::ext::shared_ptr<ore::data::Report>> dimRegReports;
                for (Size i = 0; i < xvaVars->dimOutputGridPoints_.size(); ++i) {
                    auto rep = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    dimRegReports.push_back(rep);
                    analytic()->addReport(LABEL, "dim_regression_" + std::to_string(i), rep);
                }
                postProcess_->exportDimRegression(xvaVars->dimOutputNettingSet_, xvaVars->dimOutputGridPoints_,
                                                  dimRegReports);
            }

            if (xvaVars->creditMigrationAnalytic_) {
                QL_REQUIRE(
                    postProcess_->creditMigrationPdf().size() == xvaVars->creditMigrationTimeSteps_.size(),
                    "XvaAnalyticImpl::runAnalytic(): inconsistent post process results for credit migration pdf / cdf ("
                        << postProcess_->creditMigrationPdf().size() << ") and input credit migration time steps ("
                        << xvaVars->creditMigrationTimeSteps_.size() << ")");
                for (Size i = 0; i < postProcess_->creditMigrationPdf().size(); ++i) {
                    auto rep = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
                    analytic()->addReport(
                        "XVA", "credit_migration_" + std::to_string(xvaVars->creditMigrationTimeSteps_[i]), rep);
                    (*rep)
                        .addColumn("upperBucketBound", double(), 6)
                        .addColumn("pdf", double(), 8)
                        .addColumn("cdf", double(), 8);
                    for (Size j = 0; j < postProcess_->creditMigrationPdf()[i].size(); ++j) {
                        (*rep)
                            .next()
                            .add(postProcess_->creditMigrationUpperBucketBounds()[j])
                            .add(postProcess_->creditMigrationPdf()[i][j])
                            .add(postProcess_->creditMigrationCdf()[i][j]);
                    }
                    rep->end();
                }
            }
        }

        CONSOLE("OK");
        ProgressMessage(msg, 1, 1).log();
    }

    // reset that mode
    ObservationMode::instance().setMode(inputs_->observationModel());

    ProgressMessage("Running XVA Analytic", 1, 1).log();
}

Matrix XvaAnalyticImpl::creditStateCorrelationMatrix() const {

    CorrelationMatrixBuilder cmb;
    for (auto const& [pair, value] : analytic()->configurations().crossAssetModelData->correlations()) {
        cmb.addCorrelation(pair.first, pair.second, value);
    }

    CorrelationMatrixBuilder::ProcessInfo processInfo;
    processInfo[CrossAssetModel::AssetType::CrState] = {
        {"CrState", analytic()->configurations().simMarketParams->numberOfCreditStates()}};

    return cmb.correlationMatrix(processInfo);
}

} // namespace analytics
} // namespace ore
