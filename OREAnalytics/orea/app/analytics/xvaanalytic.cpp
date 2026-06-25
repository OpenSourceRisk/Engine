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

#include <qle/math/gpuqrsolve.hpp>

#include <orea/aggregation/dimflatcalculator.hpp>
#include <orea/aggregation/dimdirectcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/aggregation/dimhelper.hpp>
#include <orea/aggregation/dynamicdeltavarcalculator.hpp>
#include <orea/aggregation/dynamicsimmcalculator.hpp>
#include <orea/aggregation/simmhelper.hpp>
#include <orea/app/analytics/utilities.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/inputparameters.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/overlaynpvcube.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/cube/jointnpvcube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/sparsenpvcube.hpp>
#include <orea/cube/inmemorycube.hpp>
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
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/filteredscenarioreader.hpp>
#include <orea/app/analytics/correlationanalytic.hpp>
#include <orea/app/analytics/utilities.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <qle/methods/pathgeneratorfactory.hpp>

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

    // Opt-in: route AMC regression QR through GPU (cuSOLVER).
    {
        bool tmp = false;
        inputs->loadParameter<bool>(tmp, "simulation", "amcUseGpuRegression", false, parseBool);
        QuantExt::setUseGpuRegression(tmp);
    }

    scenarioReader_ = inputs->loadScenarioReader("simulation", "scenarioFile");
    inputs->loadParameterXML<EngineData>(simulationPricingEngine_, "simulation", "pricingEnginesFile");
    if (!simulationPricingEngine_)
        simulationPricingEngine_ = inputs->setupVariables().pricingEngine_;
    inputs->loadParameterXML<EngineData>(amcPricingEngine_, "simulation", "amcPricingEnginesFile");
    if (!amcPricingEngine_)
        amcPricingEngine_ = inputs->setupVariables().pricingEngine_;
    inputs->loadParameterXML<EngineData>(amcCgPricingEngine_, "simulation", "amcCgPricingEnginesFile");
    if (!amcCgPricingEngine_)
        amcCgPricingEngine_ = inputs->setupVariables().pricingEngine_;

    applyEngineDataOverride(inputs, simulationPricingEngine_, "simulation", "pricingEnginesOverride");
    applyEngineDataOverride(inputs, amcPricingEngine_, "simulation", "amcPricingEnginesOverride");
    applyEngineDataOverride(inputs, amcCgPricingEngine_, "simulation", "amcCgPricingEnginesOverride");

    inputs->loadParameterXML<ScenarioSimMarketParameters>(exposureSimMarketParams_, "simulation", "simulationConfigFile");
    inputs->loadParameterXML<CrossAssetModelData>(crossAssetModelData_, "simulation", "crossAssetModelData");
    if (!crossAssetModelData_)
        // load default if not provided
        inputs->loadParameterXML<CrossAssetModelData>(crossAssetModelData_, "simulation", "simulationConfigFile");
    inputs->loadParameterXML<ScenarioGeneratorData>(scenarioGeneratorData_, "simulation", "scenarioGeneratorData");
    if (!scenarioGeneratorData_) {
        LOG("ScenarioGenerator data not found")
        inputs->loadParameterXML<ScenarioGeneratorData>(scenarioGeneratorData_, "simulation", "simulationConfigFile");
    }
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
    inputs->loadParameter<vector<QuantExt::RiskFactorKey::KeyType>>(filterRiskKeys_, "simulation", "filterRiskKeys",
                                                                    false, parseListOfRiskFactorKeyValues);
    if (!writeScenarios.empty())
        writeScenarios_ = true;
    if (!writeCube_)
        inputs->loadParameter<bool>(writeCube_, "simulation", "writeCube", false, parseBool);
    if (!writeScenarios_)
        inputs->loadParameter<bool>(writeScenarios_, "simulation", "writeScenarios", false, parseBool);
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
    inputs->loadParameter<Size>(xvaCgRegressionCacheSize_, "simulation", "xvaCgRegressionCacheSize", false, parseInteger);
    inputs->loadParameter<bool>(xvaCgEnableCgOptimization_, "simulation", "xvaCgEnableCgOptimization", false, parseBool);
    inputs->loadParameter<bool>(cubeNpvOverlay_, "simulation", "cubeNpvOverlay", false, parseBool);

    /**********************
     * XVA specifically
     **********************/

    inputs->loadParameter<bool>(generateCorrelations_, "xva", "generateCorrelations", false, parseBool);
    inputs->loadParameter<bool>(outputCrossAssetModelData_, "xva", "outputCrossAssetModelData", false, parseBool);
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

    inputs->loadParameter<bool>(rawCubeOutput_, "xva", "rawCubeOutput", false, parseBool);
    inputs->loadParameter<string>(rawCubeOutputFile_, pfeAnalytics, "rawCubeOutputFile", false);
    if (!rawCubeOutputFile_.empty())
        rawCubeOutput_ = true;

    inputs->loadParameter<bool>(netCubeOutput_, "xva", "netCubeOutput", false, parseBool);
    inputs->loadParameter<string>(netCubeOutputFile_, pfeAnalytics, "netCubeOutputFile", false);
    if (!netCubeOutputFile_.empty())
        netCubeOutput_ = true;

    inputs->loadParameter<string>(timeAveragedNettedExposureOutputFile_, "xva", "timeAveragedNettedExposureOutputFile", false);
    if (!timeAveragedNettedExposureOutputFile_.empty())
        timeAveragedNettedExposureOutput_ = true;

    // FVA
    inputs->loadParameter<string>(borrowingCurve_, "xva", vector<string>({"borrowingCurve", "fvaBorrowingCurve"}), false);
    inputs->loadParameter<string>(lendingCurve_, "xva", vector<string>({"lendingCurve", "fvaLendingCurve"}), false);
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
    inputs->loadParameter<Real>(dimScaling_, "xva", "dimScaling", false, parseReal);
    string dimModel;
    inputs->loadParameter<string>(dimModel, "xva", "dimModel", false);
    if (!dimModel.empty() && dimAnalytic_) {
        dimModel_ = dimModel;
        QL_REQUIRE(
            dimModel_ == "Regression" || dimModel_ == "Flat" || dimModel_ == "DeltaVaR" ||
                dimModel_ == "DeltaGammaNormalVaR" || dimModel_ == "DeltaGammaVaR" || dimModel_ == "DynamicIM" ||
                dimModel_ == "SimmAnalytic",
            "DIM model "
                << dimModel_ << " not supported, "
                << "expected Flat, Regression, DeltaVaR, DeltaGammaNormalVaR, DeltaGammaVaR, DynamicIM, SimmAnalytic");
    }

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

    string marketConfiguration = inputs_->marketConfig("simulation");

    bool fullInitialCollateralisation = xvaVars->fullInitialCollateralisation_;
    bool firstMporCollateralAdjustment = xvaVars->firstMporCollateralAdjustment_;
    checkConfigurations(analytic()->portfolio());
    applyConfigurationFallback(analytic()->portfolio());


    if (!dimCalculator_ && (analytics["mva"] || analytics["dim"])) {
        LOG("dim calculator not set, create one");
	    std::map<std::string, Real> currentIM;
        Real dimScaling = xvaVars->dimScaling_;
        if (dimScaling == QuantLib::Null<Real>() && xvaVars->collateralBalances_) {
            for (auto const& [n, b] : xvaVars->collateralBalances_->collateralBalances()) {
                Real im = b->initialMargin();
                QL_REQUIRE(im != QuantLib::Null<Real>() && im > 0.0,
                          "DIM: collateral balance initial margin for netting set '"
                              << n.nettingSetId()
                              << "' is zero or not set. "
                                 "Provide a valid IM or set dimScaling explicitly in the xva analytic.");
                currentIM[n.nettingSetId()] =
                   im *
                   (b->currency() == baseCurrency
                        ? 1.0
                        : analytic()->market()->fxRate(b->currency() + baseCurrency, marketConfiguration)->value());
            }
        }

        DLOG("Create a '" << xvaVars->dimModel_ << "' Dynamic Initial Margin Calculator");

        if (xvaVars->dimModel_ == "Regression" || xvaVars->dimModel_ == "DeltaVaR" ||
            xvaVars->dimModel_ == "DeltaGammaNormalVaR" || xvaVars->dimModel_ == "DeltaGammaVaR") {
            if (dimScaling == QuantLib::Null<Real>()) {
                for (auto const& n : getNettingSetIds(analytic()->portfolio())) {
                    QL_REQUIRE(currentIM.count(n) > 0,
                               "DIM: dimScaling is not set and netting set '"
                                   << n << "' has no entry in collateralBalancesFile. "
                                   << "Provide dimScaling explicitly in the XVA analytic or supply a "
                                   << "collateralBalancesFile with valid initial margins for each netting set.");
                }
            }
        }

        if (xvaVars->dimModel_ == "Regression") {
            dimCalculator_ = QuantLib::ext::make_shared<RegressionDynamicInitialMarginCalculator>(
                analytic()->portfolio(), cube_, cubeInterpreter_, scenarioData_, dimQuantile,
                dimHorizonCalendarDays, dimRegressionOrder, dimRegressors, dimLocalRegressionEvaluations,
                dimLocalRegressionBandwidth, currentIM,
                xvaVars->deterministicInitialMargin_, dimScaling);
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
                dimHorizonCalendarDays, dimHelper, ddvOrder, currentIM, dimScaling);
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

    auto market = analytic()->offsetScenario() == nullptr ? analytic()->market() : offsetSimMarket_;

    postProcess_ = QuantLib::ext::make_shared<PostProcess>(
        analytic()->portfolio(), netting, balances, market, marketConfiguration, cube_, scenarioData_, analytics,
        baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName, borrowingCurve,
        lendingCurve, dimCalculator_, cubeInterpreter_, fullInitialCollateralisation, cvaSensiGrid,
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
    QL_REQUIRE(analytic()->offsetScenario() == nullptr || analytic()->offsetSimMarketParams() != nullptr,
               "Need offsetScenario and corresponding simMarketParameter");

    SavedSettings settings;

    if (xvaVars->exposureIncludeTodaysCashFlows_) {
        LOG("Exposure IncludeTodaysCashFlows is set to "
            << (*xvaVars->exposureIncludeTodaysCashFlows_ ? "true" : "false"));
        Settings::instance().includeTodaysCashFlows() = *xvaVars->exposureIncludeTodaysCashFlows_;
    }

    if (xvaVars->exposureIncludeReferenceDateEvents_) {
        LOG("Simulation IncludeReferenceDateEvents is set to "
            << (xvaVars->exposureIncludeReferenceDateEvents_ ? "true" : "false"));
        Settings::instance().includeReferenceDateEvents() = *xvaVars->exposureIncludeReferenceDateEvents_;
    }

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
            xvaVars->xvaCgUsePythonIntegrationDynamicIm_, xvaVars->xvaCgRegressionCacheSize_,
            xvaVars->xvaCgEnableCgOptimization_, true, true, true, inputs_->useAtParCouponsCurves(),
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
         * We may have two non-empty portfolios to be merged for post processing
         ***************************************************/

        LOG("Classic portfolio size " << classicPortfolio_->size());
        LOG("AMC portfolio size " << amcPortfolio_->size());
        auto newPortfolio = QuantLib::ext::make_shared<Portfolio>();
        for (const auto& [tradeId, tradeIdCubePos] : classicPortfolio_->trades())
            newPortfolio->add(tradeId);
        for (const auto& [tradeId, tradeIdCubePos] : amcPortfolio_->trades())
            newPortfolio->add(tradeId);
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
        auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->setupVariables().reportBufferSize());
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

    // Output CrossAssetModelData XML if requested
    if (xvaVars->outputCrossAssetModelData_ && analytic()->configurations().crossAssetModelData) {
        string camXml = analytic()->configurations().crossAssetModelData->toXMLString();
        DLOG("CrossAssetModel XML:\n" << camXml);
        std::filesystem::path camXmlPath = inputs_->resultsPath() / "crossassetmodel_xva.xml";
        std::ofstream camFile(camXmlPath.string());
        if (camFile.is_open()) {
            camFile << camXml;
            camFile.close();
            LOG("Written CrossAssetModelData XML to " << camXmlPath.string());
        }
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
