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

/*! \file orea/app/inputparameters.hpp
    \brief Input Parameters
*/

#pragma once

#include <orea/app/parameters.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <boost/filesystem/path.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;

class InputParameters {
public:
    InputParameters() {}
    virtual ~InputParameters() {}

    /***************************
     * Getters for general setup
     ***************************/

    const QuantLib::Date& asof() { return asof_; }
    const boost::filesystem::path& resultsPath() const { return resultsPath_; }
    const std::string& baseCurrency() { return baseCurrency_; }
    bool continueOnError() { return continueOnError_; }
    bool lazyMarketBuilding() { return lazyMarketBuilding_; }
    bool buildFailedTrades() { return buildFailedTrades_; }
    const std::string& observationModel() { return observationModel_; }
    bool implyTodaysFixings() { return implyTodaysFixings_; }
    const std::map<std::string, std::string>&  marketConfigs() { return marketConfigs_; }
    const std::string& marketConfig(const std::string& context);
    const boost::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager() { return refDataManager_; }
    const boost::shared_ptr<ore::data::Conventions>& conventions() { return conventions_; }
    const boost::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig() const { return iborFallbackConfig_; }
    const std::vector<boost::shared_ptr<ore::data::CurveConfigurations>>& curveConfigs() { return curveConfigs_; }
    const boost::shared_ptr<ore::data::EngineData>& pricingEngine() { return pricingEngine_; }
    const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams() { return todaysMarketParams_; }
    const boost::shared_ptr<ore::data::Portfolio>& portfolio() { return portfolio_; }

    QuantLib::Size nThreads() const { return nThreads_; }
    bool entireMarket() { return entireMarket_; }
    bool allFixings() { return allFixings_; }
    bool eomInflationFixings() { return eomInflationFixings_; }
    bool useMarketDataFixings() { return useMarketDataFixings_; }
    bool iborFallbackOverride() { return iborFallbackOverride_; }
    const std::string& reportNaString() { return reportNaString_; }
    char csvQuoteChar() const { return csvQuoteChar_; }
    bool dryRun() const { return dryRun_; }
    QuantLib::Size mporDays() { return mporDays_; }
    const QuantLib::Calendar mporCalendar() {
        if (mporCalendar_.empty()) {
            QL_REQUIRE(!baseCurrency_.empty(), "mpor calendar or baseCurrency must be provided";);
            return parseCalendar(baseCurrency_);
        } else
            return mporCalendar_;
    }
    bool mporForward() { return mporForward_; }
    bool includeMporExpired() const { return includeMporExpired_; };

    /***************************
     * Getters for npv analytics
     ***************************/

    bool outputAdditionalResults() const { return outputAdditionalResults_; };

    /***********************
     * Getters for cashflows
     ***********************/

    bool includePastCashflows() { return includePastCashflows_; }

    /****************************
     * Getters for curves/markets
     ****************************/

    bool outputCurves() const { return outputCurves_; };
    bool outputTodaysMarketCalibration() const { return outputTodaysMarketCalibration_; };
    const std::string& curvesMarketConfig() { return curvesMarketConfig_; }
    const std::string& curvesGrid() const { return curvesGrid_; }

    /*****************************
     * Getters for sensi analytics
     *****************************/
    
    bool xbsParConversion() { return xbsParConversion_; }
    bool analyticFxSensis() { return analyticFxSensis_; }
    bool parSensi() const { return parSensi_; };
    bool alignPillars() const { return alignPillars_; };
    bool outputJacobi() const { return outputJacobi_; };
    bool useSensiSpreadedTermStructures() { return useSensiSpreadedTermStructures_; }
    QuantLib::Real sensiThreshold() const { return sensiThreshold_; }
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& sensiSimMarketParams() { return sensiSimMarketParams_; }
    const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData() { return sensiScenarioData_; }
    const boost::shared_ptr<ore::data::EngineData>& sensiPricingEngine() { return sensiPricingEngine_; }
    // const boost::shared_ptr<ore::data::TodaysMarketParameters>& sensiTodaysMarketParams() { return sensiTodaysMarketParams_; }

    /****************************
     * Getters for stress testing
     ****************************/

    QuantLib::Real stressThreshold() { return stressThreshold_; }
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& stressSimMarketParams() { return stressSimMarketParams_; }
    const boost::shared_ptr<ore::analytics::StressTestScenarioData>& stressScenarioData() { return stressScenarioData_; }
    const boost::shared_ptr<ore::data::EngineData>& stressPricingEngine() { return stressPricingEngine_; }

    /*****************
     * Getters for VaR
     *****************/

    bool salvageCovariance() { return salvageCovariance_; }
    const std::vector<Real>& varQuantiles() { return varQuantiles_; }
    bool varBreakDown() { return varBreakDown_; }
    const std::string& portfolioFilter() { return portfolioFilter_; }
    const std::string& varMethod() { return varMethod_; }
    Size mcVarSamples() { return mcVarSamples_; }
    long mcVarSeed() { return mcVarSeed_; }
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& covarianceData() { return covarianceData_; }
    const boost::shared_ptr<SensitivityStream>& sensitivityStream() { return sensitivityStream_; }
    
    /*********************************
     * Getters for exposure simulation 
     *********************************/
    
    bool amc() { return amc_; }
    const std::vector<std::string>& amcExcludeTradeTypes() { return amcExcludeTradeTypes_; }
    const std::string& exposureBaseCurrency() { return exposureBaseCurrency_; }
    const std::string& exposureObservationModel() { return exposureObservationModel_; }
    const std::string& nettingSetId() { return nettingSetId_; }
    const std::string& scenarioGenType() { return scenarioGenType_; }
    bool storeFlows() { return storeFlows_; }
    bool storeSurvivalProbabilities() { return storeSurvivalProbabilities_; }
    bool writeCube() { return writeCube_; }
    bool writeScenarios() { return writeScenarios_; }
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& exposureSimMarketParams() { return exposureSimMarketParams_; }
    const boost::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData() { return scenarioGeneratorData_; }
    const boost::shared_ptr<CrossAssetModelData>& crossAssetModelData() { return crossAssetModelData_; }
    const boost::shared_ptr<ore::data::EngineData>& simulationPricingEngine() { return simulationPricingEngine_; }
    const boost::shared_ptr<ore::data::EngineData>& amcPricingEngine() { return amcPricingEngine_; }
    const boost::shared_ptr<ore::data::NettingSetManager>& nettingSetManager() { return nettingSetManager_; }
    // const boost::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager() { return counterpartyManager_; }
    // const boost::shared_ptr<ore::data::CollateralBalances>& collateralBalances() { return collateralBalances_; }

    /*****************
     * Getters for xva
     *****************/

    const std::string& xvaBaseCurrency() { return xvaBaseCurrency_; }
    bool loadCube() { return loadCube_; }
    boost::shared_ptr<NPVCube> cube() { return cube_; }
    boost::shared_ptr<NPVCube> nettingSetCube() { return nettingSetCube_; }
    boost::shared_ptr<NPVCube> cptyCube() { return cptyCube_; }
    boost::shared_ptr<AggregationScenarioData> mktCube() { return mktCube_; }
    bool flipViewXVA() { return flipViewXVA_; }
    bool fullInitialCollateralisation() { return fullInitialCollateralisation_; }
    bool exposureProfiles() { return exposureProfiles_; }
    bool exposureProfilesByTrade() { return exposureProfilesByTrade_; }
    Real pfeQuantile() { return pfeQuantile_; }
    const std::string&  collateralCalculationType() { return collateralCalculationType_; }
    const std::string& exposureAllocationMethod() { return exposureAllocationMethod_; }
    Real marginalAllocationLimit() { return marginalAllocationLimit_; }
    bool exerciseNextBreak() { return exerciseNextBreak_; }
    bool cvaAnalytic() { return cvaAnalytic_; }
    bool dvaAnalytic() { return dvaAnalytic_; }
    bool fvaAnalytic() { return fvaAnalytic_; }
    bool colvaAnalytic() { return colvaAnalytic_; }
    bool collateralFloorAnalytic() { return collateralFloorAnalytic_; }
    bool dimAnalytic() { return dimAnalytic_; }
    bool mvaAnalytic() { return mvaAnalytic_; }
    bool kvaAnalytic() { return kvaAnalytic_; }
    bool dynamicCredit() { return dynamicCredit_; }
    bool cvaSensi() { return cvaSensi_; }
    const std::vector<Period>& cvaSensiGrid() { return cvaSensiGrid_; }
    Real cvaSensiShiftSize() { return cvaSensiShiftSize_; }
    const std::string& dvaName() { return dvaName_; }
    bool rawCubeOutput() { return rawCubeOutput_; }
    bool netCubeOutput() { return netCubeOutput_; }
    const std::string& rawCubeOutputFile() { return rawCubeOutputFile_; }
    const std::string& netCubeOutputFile() { return netCubeOutputFile_; }
    // funding value adjustment details
    const std::string& fvaBorrowingCurve() { return fvaBorrowingCurve_; }
    const std::string& fvaLendingCurve() { return fvaLendingCurve_; }
    const std::string& flipViewBorrowingCurvePostfix() { return flipViewBorrowingCurvePostfix_; }
    const std::string& flipViewLendingCurvePostfix() { return flipViewLendingCurvePostfix_; }
    // dynamic initial margin details
    Real dimQuantile() { return dimQuantile_; }
    Size dimHorizonCalendarDays() { return dimHorizonCalendarDays_; }
    Size dimRegressionOrder() { return dimRegressionOrder_; }
    const std::vector<std::string>& dimRegressors() { return dimRegressors_; }
    const std::vector<Size>& dimOutputGridPoints() { return dimOutputGridPoints_; }
    const std::string& dimOutputNettingSet() { return dimOutputNettingSet_; }
    Size dimLocalRegressionEvaluations() { return dimLocalRegressionEvaluations_; }
    Real dimLocalRegressionBandwidth() { return dimLocalRegressionBandwidth_; }
    // capital value adjustment details
    Real kvaCapitalDiscountRate() { return kvaCapitalDiscountRate_; } 
    Real kvaAlpha() { return kvaAlpha_; }
    Real kvaRegAdjustment() { return kvaRegAdjustment_; }
    Real kvaCapitalHurdle() { return kvaCapitalHurdle_; }
    Real kvaOurPdFloor() { return kvaOurPdFloor_; }
    Real kvaTheirPdFloor() { return kvaTheirPdFloor_; }
    Real kvaOurCvaRiskWeight() { return kvaOurCvaRiskWeight_; }
    Real kvaTheirCvaRiskWeight() { return kvaTheirCvaRiskWeight_; }
    
    /**************************************************
     * Getters for cashflow npv and dynamic backtesting
     **************************************************/
    
    const QuantLib::Date& cashflowHorizon() const { return cashflowHorizon_; };
    const QuantLib::Date& portfolioFilterDate() const { return portfolioFilterDate_; }
    
    /*************************************
     * List of analytics that shall be run
     *************************************/

    const std::vector<std::string>& runTypes() { return runTypes_; }

    virtual void loadParameters() = 0;
    virtual void writeOutParameters() = 0;

protected:

    // List of analytics that shall be run, including
    // - NPV
    // - CASHFLOW
    // - CASHFLOWNPV
    // - SENSITIVITY
    // - STRESS
    // - VAR
    // - EXPOSURE
    // - XVA
    // Each analytic type comes with additional input requirements, see below
    std::vector<std::string> runTypes_;

    /***********************************
     * Basic setup, across all run types
     ***********************************/
    QuantLib::Date asof_;
    boost::filesystem::path resultsPath_;
    std::string baseCurrency_;
    bool continueOnError_ = true;
    bool lazyMarketBuilding_ = true;
    bool buildFailedTrades_ = false;
    std::string observationModel_ = "None";
    bool implyTodaysFixings_ = false;
    std::map<std::string, std::string> marketConfigs_;
    boost::shared_ptr<ore::data::BasicReferenceDataManager> refDataManager_;
    boost::shared_ptr<ore::data::Conventions> conventions_;
    boost::shared_ptr<ore::data::IborFallbackConfig> iborFallbackConfig_;
    std::vector<boost::shared_ptr<ore::data::CurveConfigurations>> curveConfigs_;
    boost::shared_ptr<ore::data::EngineData> pricingEngine_;
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::Size nThreads_ = 1;
   
    bool entireMarket_ = false; 
    bool allFixings_ = false; 
    bool eomInflationFixings_ = true;
    bool useMarketDataFixings_ = true;
    bool iborFallbackOverride_ = false;
    std::string reportNaString_;
    char csvQuoteChar_ = '\0';
    bool dryRun_ = false;
    QuantLib::Size mporDays_ = 10;
    QuantLib::Calendar mporCalendar_;
    bool mporForward_ = true;
    bool includeMporExpired_ = true;

    /**************
     * NPV analytic
     *************/
    bool outputAdditionalResults_ = false;
    bool outputCurves_ = false;
    std::string curvesMarketConfig_ = "";
    std::string curvesGrid_ = "240,1M";
    bool outputTodaysMarketCalibration_ = false;

    /***********************************
     * CASHFLOW and CASHFLOWNPV analytic
     ***********************************/
    bool includePastCashflows_ = false;
    QuantLib::Date cashflowHorizon_;
    QuantLib::Date portfolioFilterDate_;

    /**********************
     * SENSITIVITY analytic
     **********************/
    bool xbsParConversion_ = false;
    bool analyticFxSensis_ = true;
    bool parSensi_ = false;
    bool outputJacobi_ = false;
    bool alignPillars_ = false;
    bool useSensiSpreadedTermStructures_ = true;
    QuantLib::Real sensiThreshold_ = 0.0;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> sensiSimMarketParams_;
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    boost::shared_ptr<ore::data::EngineData> sensiPricingEngine_;
    // boost::shared_ptr<ore::data::TodaysMarketParameters> sensiTodaysMarketParams_;

    /*****************
     * STRESS analytic
     *****************/
    QuantLib::Real stressThreshold_ = 0.0;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> stressSimMarketParams_;
    boost::shared_ptr<ore::analytics::StressTestScenarioData> stressScenarioData_;
    boost::shared_ptr<ore::data::EngineData> stressPricingEngine_;

    /*****************
     * VaR analytics
     *****************/
    bool salvageCovariance_ = false;
    std::vector<Real> varQuantiles_;
    bool varBreakDown_ = false;
    std::string portfolioFilter_;
    // Delta, DeltaGammaNormal, MonteCarlo, Cornish-Fisher, Saddlepoint 
    std::string varMethod_;
    Size mcVarSamples_ = 0;
    long mcVarSeed_ = 0;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarianceData_;
    boost::shared_ptr<SensitivityStream> sensitivityStream_;
    
    /*******************
     * EXPOSURE analytic
     *******************/
    // bool simulation_ = false;
    bool amc_ = false;
    std::vector<std::string> amcExcludeTradeTypes_;
    std::string exposureBaseCurrency_ = "";
    std::string exposureObservationModel_ = "Disable";
    std::string nettingSetId_ = "";
    std::string scenarioGenType_ = "";
    bool storeFlows_ = false;
    bool storeSurvivalProbabilities_ = false;
    bool writeCube_ = false;
    bool writeScenarios_ = false;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> exposureSimMarketParams_;
    boost::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    boost::shared_ptr<CrossAssetModelData> crossAssetModelData_;
    boost::shared_ptr<ore::data::EngineData> simulationPricingEngine_;
    boost::shared_ptr<ore::data::EngineData> amcPricingEngine_;
    boost::shared_ptr<ore::data::NettingSetManager> nettingSetManager_;
    bool exposureProfiles_ = true;
    bool exposureProfilesByTrade_ = true;
    Real pfeQuantile_ = 0.95;
    bool fullInitialCollateralisation_ = false;
    std::string collateralCalculationType_ = "Symmetric";
    std::string exposureAllocationMethod_ = "None";
    Real marginalAllocationLimit_ = 1.0;
    // intermediate results of the exposure simulation, before aggregation
    boost::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_;
    boost::shared_ptr<AggregationScenarioData> mktCube_;

    /**************
     * XVA analytic
     **************/
    std::string xvaBaseCurrency_ = "";
    bool loadCube_ = false;
    bool hyperCube_ = false;
    bool hyperNettingSetCube_ = false;
    bool hyperCptyCube_ = false;
    bool flipViewXVA_ = false;
    bool exerciseNextBreak_ = false;
    bool cvaAnalytic_ = true;
    bool dvaAnalytic_ = false;
    bool fvaAnalytic_ = false;
    bool colvaAnalytic_ = false;
    bool collateralFloorAnalytic_ = false;
    bool dimAnalytic_ = false;
    bool mvaAnalytic_= false;
    bool kvaAnalytic_= false;
    bool dynamicCredit_ = false;
    bool cvaSensi_ = false;
    std::vector<Period> cvaSensiGrid_;
    Real cvaSensiShiftSize_ = 0.0001;
    std::string dvaName_ = "";
    bool rawCubeOutput_ = false;
    bool netCubeOutput_ = false;
    std::string rawCubeOutputFile_ = "";
    std::string netCubeOutputFile_ = "";
    // funding value adjustment details
    std::string fvaBorrowingCurve_ = "";
    std::string fvaLendingCurve_ = "";    
    std::string flipViewBorrowingCurvePostfix_ = "_BORROW";
    std::string flipViewLendingCurvePostfix_ = "_LEND";
    // dynamic initial margin details
    Real dimQuantile_ = 0.99;
    Size dimHorizonCalendarDays_ = 14;
    Size dimRegressionOrder_ = 0;
    vector<string> dimRegressors_;
    vector<Size> dimOutputGridPoints_;
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

};

inline const std::string& InputParameters::marketConfig(const std::string& context) {
    auto it = marketConfigs_.find(context);
    return (it != marketConfigs_.end() ? it->second : Market::defaultConfiguration);
}

class OREAppInputParameters : public InputParameters {
public:
    OREAppInputParameters(const boost::shared_ptr<Parameters>& params) : params_(params) {
        loadParameters();
    }

    void loadParameters() override;
    void writeOutParameters() override;

    const boost::shared_ptr<CSVLoader>& csvLoader() { return csvLoader_; }

    //! map internal report name to the configured external file name
    std::string outputFileName(const std::string& internalName, const std::string& suffix);
    
private:
    boost::shared_ptr<Parameters> params_;
    boost::shared_ptr<CSVLoader> csvLoader_;

    std::map<std::string, std::string> fileNameMap_;
    
    std::string npvOutputFileName_;
    std::string cashflowOutputFileName_;
    std::string curvesOutputFileName_;
    std::string scenarioDumpFileName_;
    std::string cubeFileName_;
    std::string mktCubeFileName_;
    std::string rawCubeFileName_;
    std::string netCubeFileName_;
    std::string dimEvolutionFileName_;
    std::vector<std::string> dimRegressionFileNames_;
    std::string sensitivityFileName_;
    std::string sensitivityScenarioFileName_;
    std::string parSensitivityFileName_;
    std::string jacobiFileName_;
    std::string jacobiInverseFileName_;
    std::string stressTestFileName_;
    std::string varFileName_;
};

} // namespace analytics
} // namespace ore
