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

/*! \file orea/app/analytic.hpp
    \brief ORE Analytics Manager
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/inputvariables.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/sensitivitystoragemanager.hpp>
#include <orea/engine/xvaenginecg.hpp>

namespace ore {
namespace analytics {

class InputParameters;

struct XvaVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
    void loadCube(const QuantLib::ext::shared_ptr<InputParameters>& inputs);

    /*******************
     * EXPOSURE analytic
     *******************/
    bool amc_ = false;
    XvaEngineCG::Mode amcCg_ = XvaEngineCG::Mode::Disabled;
    bool xvaCgDynamicIM_ = false;
    Size xvaCgDynamicIMStepSize_ = 1;
    Size xvaCgRegressionOrder_ = 4;
    double xvaCgRegressionVarianceCutoff_ = Null<Real>();
    Size xvaCgRegressionOrderDynamicIm_ = 4;
    double xvaCgRegressionVarianceCutoffDynamicIm_ = Null<Real>();
    bool xvaCgTradeLevelBreakdown_ = true;
    std::vector<Size> xvaCgRegressionReportTimeStepsDynamicIM_;
    bool xvaCgUseRedBlocks_ = true;
    bool xvaCgBumpSensis_ = false;
    bool xvaCgUseExternalComputeDevice_ = false;
    bool xvaCgExternalDeviceCompatibilityMode_ = false;
    bool xvaCgUseDoublePrecisionForExternalCalculation_ = false;
    string xvaCgExternalComputeDevice_;
    bool xvaCgUsePythonIntegration_ = false;
    bool xvaCgUsePythonIntegrationDynamicIm_ = false;
    QuantLib::ext::shared_ptr<SensitivityScenarioData> xvaCgSensiScenarioData_;
    std::set<std::string> amcTradeTypes_;
    std::string amcPathDataInput_, amcPathDataOutput_;
    bool amcIndividualTrainingInput_ = false, amcIndividualTrainingOutput_ = false;
    std::string exposureBaseCurrency_;
    std::string exposureObservationModel_ = "Disable";
    std::string nettingSetId_;
    bool storeFlows_ = false;
    bool storeExerciseValues_ = false;
    bool storeSensis_ = false;
    bool allowPartialScenarios_ = false;
    vector<Real> curveSensiGrid_;
    vector<Real> vegaSensiGrid_;
    Size storeCreditStateNPVs_ = 0;
    bool storeSurvivalProbabilities_ = false;
    bool writeCube_ = false;
    bool writeScenarios_ = false;
    bool generateCorrelations_ = false;
    bool cubeNpvOverlay_ = false;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> exposureSimMarketParams_;
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::shared_ptr<CrossAssetModelData> crossAssetModelData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> simulationPricingEngine_, amcPricingEngine_, amcCgPricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::NettingSetManager> nettingSetManager_;
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances_;
    bool exposureProfiles_ = true;
    bool exposureProfilesByTrade_ = true;
    bool exposureProfilesUseCloseOutValues_ = false;
    Real pfeQuantile_ = 0.95;
    bool fullInitialCollateralisation_ = false;
    std::string collateralCalculationType_ = "NoLag";
    std::string exposureAllocationMethod_ = "None";
    QuantLib::Size maxScenario_ = QuantLib::Null<QuantLib::Size>();
    Real marginalAllocationLimit_ = 1.0;
    // intermediate results of the exposure simulation, before aggregation
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> mktCube_;
    Real simulationBootstrapTolerance_ = 0.0001;
    optional<bool> exposureIncludeTodaysCashFlows_;
    optional<bool> exposureIncludeReferenceDateEvents_ = false;
    QuantLib::ext::shared_ptr<ScenarioReader> scenarioReader_;

    /**************
     * XVA analytic
     **************/
    bool xvaUseDoublePrecisionCubes_ = false;
    std::string xvaBaseCurrency_;
    bool flipViewXVA_ = false;
    MporCashFlowMode mporCashFlowMode_ = MporCashFlowMode::Unspecified;
    bool exerciseNextBreak_ = false;
    bool cvaAnalytic_ = true;
    bool dvaAnalytic_ = false;
    bool fvaAnalytic_ = false;
    bool colvaAnalytic_ = false;
    bool collateralFloorAnalytic_ = false;
    bool dimAnalytic_ = false;
    std::string dimModel_ = "Regression";
    bool mvaAnalytic_ = false;
    bool kvaAnalytic_ = false;
    bool dynamicCredit_ = false;
    bool cvaSensi_ = false;
    std::vector<Period> cvaSensiGrid_;
    Real cvaSensiShiftSize_ = 0.0001;
    std::string dvaName_ = "";
    bool rawCubeOutput_ = false;
    bool netCubeOutput_ = false;
    bool cubeOutput_ = false;
    bool timeAveragedNettedExposureOutput_ = false;
    std::string rawCubeOutputFile_, netCubeOutputFile_, timeAveragedNettedExposureOutputFile_;
    // funding value adjustment details
    std::string fvaBorrowingCurve_, fvaLendingCurve_;
    std::string flipViewBorrowingCurvePostfix_ = "_BORROW";
    std::string flipViewLendingCurvePostfix_ = "_LEND";
    // deterministic initial margin by netting set
    std::map<std::string, TimeSeries<Real>> deterministicInitialMargin_;
    // dynamic initial margin details
    Real dimQuantile_ = 0.99;
    Size dimHorizonCalendarDays_ = 14;
    Size dimRegressionOrder_ = 0;
    vector<string> dimRegressors_;
    vector<Size> dimOutputGridPoints_;
    Real dimDistributionCoveredStdDevs_ = 5.0;
    Size dimDistributionGridSize_ = 50;
    string dimOutputNettingSet_;
    Size dimLocalRegressionEvaluations_ = 0;
    Real dimLocalRegressionBandwidth_ = 0.25;
    // capital value adjustment details
    Real kvaCapitalDiscountRate_ = 0.10;
    Real kvaAlpha_ = 1.4;
    Real kvaRegAdjustment_ = 12.5;
    Real kvaCapitalHurdle_ = 0.012;
    Real kvaOurPdFloor_ = 0.03;
    Real kvaTheirPdFloor_ = 0.03;
    Real kvaOurCvaRiskWeight_ = 0.05;
    Real kvaTheirCvaRiskWeight_ = 0.05;
    // credit simulation details
    bool creditMigrationAnalytic_ = false;
    std::vector<Real> creditMigrationDistributionGrid_;
    std::vector<Size> creditMigrationTimeSteps_;
    QuantLib::ext::shared_ptr<CreditSimulationParameters> creditSimulationParameters_;
    std::string creditMigrationOutputFiles_;
    bool firstMporCollateralAdjustment_ = false;
    bool writeIndividualExposureReports_ = true;

    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> correlationData_;
};

class XvaAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA";
    static constexpr const char* corrLookupKey = "CORRELATION";

    explicit XvaAnalyticImpl(
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        const QuantLib::ext::shared_ptr<Scenario>& offsetScenario = nullptr,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : Analytic::Impl(inputs, QuantLib::ext::make_shared<XvaVariables>()), offsetScenario_(offsetScenario),
          offsetSimMarketParams_(offsetSimMarketParams) {
        setLabel(LABEL);;
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void reset() override;

    void checkConfigurations(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);

    void applyConfigurationFallback(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);

    void setOffsetScenario(const QuantLib::ext::shared_ptr<Scenario>& offsetScenario) {
        offsetScenario_ = offsetScenario;
    }
        
    void setOffsetSimMarketParams(
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams) {
        offsetSimMarketParams_ = offsetSimMarketParams;
    }
    void buildDependencies() override;

protected:
    QuantLib::ext::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError, bool allowModelFallbacks);
    void buildScenarioGenerator(bool continueOnError, bool allowModelFallbacks);

    void initCubeDepth();
    void initCube(QuantLib::ext::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth);
    std::set<std::string> getNettingSetIds(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) const;

    void initClassicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    void buildClassicCube(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    QuantLib::ext::shared_ptr<Portfolio> classicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);

    QuantLib::ext::shared_ptr<EngineFactory>
    amcEngineFactory(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam, const std::vector<Date>& simDates,
                     const std::vector<Date>& stickyCloseOutDates);
    void buildAmcPortfolio();
    void amcRun(bool doClassicRun, bool continueOnCalibrationError, bool allowModelFallbacks);

    void runPostProcessor();

    Matrix creditStateCorrelationMatrix() const;
    std::string mapRiskFactorToAssetType(RiskFactorKey::KeyType keyF);
    void feedCorrelationToCAM(const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& corrData = {});

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarketCalibration_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> offsetSimMarket_;
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    QuantLib::ext::shared_ptr<Portfolio> amcPortfolio_, classicPortfolio_;
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_, amcCube_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> scenarioData_;
    QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpreter_;
    QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    QuantLib::ext::shared_ptr<PostProcess> postProcess_;
    QuantLib::ext::shared_ptr<Scenario> offsetScenario_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> offsetSimMarketParams_;
    QuantLib::ext::shared_ptr<SensitivityStorageManager> sensitivityStorageManager_;
    Size cubeDepth_ = 0;
    QuantLib::ext::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;

    bool runSimulation_ = false;
    bool runXva_ = false;
    bool runPFE_ = false;
};

static const std::set<std::string> xvaAnalyticSubAnalytics{"XVA", "EXPOSURE", "PFE"};

class XvaAnalytic : public Analytic {
public:
    explicit XvaAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                         const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
                         const QuantLib::ext::shared_ptr<Scenario>& offSetScenario = nullptr,
                         const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : Analytic(std::make_unique<XvaAnalyticImpl>(inputs, offSetScenario, offsetSimMarketParams),
                   xvaAnalyticSubAnalytics, inputs, analyticsManager, false, false, false, false) {}
};

} // namespace analytics
} // namespace ore
