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
#include <orea/aggregation/creditsimulationparameters.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <boost/filesystem/path.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;

//! Base class for input data, also exposed via SWIG
class InputParameters {
public:
    InputParameters();
    virtual ~InputParameters() {}

    /*********
     * Setters
     *********/
    
    void setAsOfDate(const std::string& s); // parse to Date
    void setResultsPath(const std::string& s) { resultsPath_ = s; }
    void setBaseCurrency(const std::string& s) { baseCurrency_ = s; }
    void setContinueOnError(bool b) { continueOnError_ = b; }
    void setLazyMarketBuilding(bool b) { lazyMarketBuilding_ = b; }
    void setBuildFailedTrades(bool b) { buildFailedTrades_ = b; }
    void setObservationModel(const std::string& s) { observationModel_ = s; }
    void setImplyTodaysFixings(bool b) { implyTodaysFixings_ = b; }
    void setMarketConfig(const std::string& config, const std::string& context);
    void setRefDataManager(const std::string& xml);
    void setRefDataManagerFromFile(const std::string& fileName);
    void setScriptLibrary(const std::string& xml);
    void setScriptLibraryFromFile(const std::string& fileName);
    void setConventions(const std::string& xml);
    void setConventionsFromFile(const std::string& fileName);
    void setIborFallbackConfig(const std::string& xml);
    void setIborFallbackConfigFromFile(const std::string& fileName);
    void setCurveConfigs(const std::string& xml);
    void setCurveConfigsFromFile(const std::string& fileName);
    void setPricingEngine(const std::string& xml);
    void setPricingEngineFromFile(const std::string& fileName);
    void setTodaysMarketParams(const std::string& xml);
    void setTodaysMarketParamsFromFile(const std::string& fileName);
    void setPortfolio(const std::string& xml); 
    void setPortfolioFromFile(const std::string& fileNameString, const std::string& inputPath); 
    void setMarketConfigs(const std::map<std::string, std::string>& m);
    void setThreads(int i) { nThreads_ = i; }
    void setEntireMarket(bool b) { entireMarket_ = b; }
    void setAllFixings(bool b) { allFixings_ = b; }
    void setEomInflationFixings(bool b) { eomInflationFixings_ = b; }
    void setUseMarketDataFixings(bool b) { useMarketDataFixings_ = b; }
    void setIborFallbackOverride(bool b) { iborFallbackOverride_ = b; }
    void setReportNaString(const std::string& s) { reportNaString_ = s; }
    void setCsvQuoteChar(const char& c){ csvQuoteChar_ = c; }
    void setCsvSeparator(const char& c) { csvSeparator_ = c; }
    void setCsvCommentCharacter(const char& c) { csvCommentCharacter_ = c; }
    void setDryRun(bool b) { dryRun_ = b; }
    void setMporDays(Size s) { mporDays_ = s; }
    void setMporDate(const QuantLib::Date& d) { mporDate_ = d; }
    void setMporCalendar(const std::string& s); 
    void setMporForward(bool b) { mporForward_ = b; }

    // Setters for npv analytics
    void setOutputAdditionalResults(bool b) { outputAdditionalResults_ = b; }

    // Setters for cashflows
    void setIncludePastCashflows(bool b) { includePastCashflows_ = b; }

    // Setters for curves/markets
    void setOutputCurves(bool b) { outputCurves_ = b; }
    void setOutputTodaysMarketCalibration(bool b) { outputTodaysMarketCalibration_ = b; }
    void setCurvesMarketConfig(const std::string& s) { curvesMarketConfig_ = s; }
    void setCurvesGrid(const std::string& s) { curvesGrid_ = s; }

    // Setters for sensi analytics
    void setXbsParConversion(bool b) { xbsParConversion_ = b; }
    void setParSensi(bool b) { parSensi_ = b; }
    void setAlignPillars(bool b) { alignPillars_ = b; }
    void setOutputJacobi(bool b) { outputJacobi_ = b; }
    void setUseSensiSpreadedTermStructures(bool b) { useSensiSpreadedTermStructures_ = b; }
    void setSensiThreshold(Real r) { sensiThreshold_ = r; }
    void setSensiSimMarketParams(const std::string& xml);
    void setSensiSimMarketParamsFromFile(const std::string& fileName);
    void setSensiScenarioData(const std::string& xml);
    void setSensiScenarioDataFromFile(const std::string& fileName);
    void setSensiPricingEngine(const std::string& xml);
    void setSensiPricingEngineFromFile(const std::string& fileName);
    void setSensiPricingEngine(const boost::shared_ptr<EngineData>& engineData) {
        sensiPricingEngine_ = engineData;
    }

    // Setters for stress testing
    void setStressThreshold(Real r) { stressThreshold_ = r; }
    void setStressSimMarketParams(const std::string& xml); 
    void setStressSimMarketParamsFromFile(const std::string& fileName); 
    void setStressScenarioData(const std::string& xml); 
    void setStressScenarioDataFromFile(const std::string& fileName); 
    void setStressPricingEngine(const std::string& xml); 
    void setStressPricingEngineFromFile(const std::string& fileName); 
    void setStressPricingEngine(const boost::shared_ptr<EngineData>& engineData) {
        stressPricingEngine_ = engineData;
    }

    // Setters for VaR
    void setSalvageCovariance(bool b) { salvageCovariance_ = b; }
    void setVarQuantiles(const std::string& s); // parse to vector<Real>
    void setVarBreakDown(bool b) { varBreakDown_ = b; }
    void setPortfolioFilter(const std::string& s) { portfolioFilter_ = s; }
    void setVarMethod(const std::string& s) { varMethod_ = s; }
    void setMcVarSamples(Size s) { mcVarSamples_ = s; }
    void setMcVarSeed(long l) { mcVarSeed_ = l; }
    void setCovarianceData(ore::data::CSVReader& reader);  
    void setCovarianceDataFromFile(const std::string& fileName);
    void setCovarianceDataFromBuffer(const std::string& xml);
    void setSensitivityStreamFromFile(const std::string& fileName);
    void setSensitivityStreamFromBuffer(const std::string& buffer);

    // Setters for exposure simulation
    void setSalvageCorrelationMatrix(bool b) { salvageCorrelationMatrix_ = b; }
    void setAmc(bool b) { amc_ = b; }
    void setAmcTradeTypes(const std::string& s); // parse to set<string>
    void setExposureBaseCurrency(const std::string& s) { exposureBaseCurrency_ = s; } 
    void setExposureObservationModel(const std::string& s) { exposureObservationModel_ = s; }
    void setNettingSetId(const std::string& s) { nettingSetId_ = s; }
    void setScenarioGenType(const std::string& s) { scenarioGenType_ = s; }
    void setStoreFlows(bool b) { storeFlows_ = b; }
    void setStoreCreditStateNPVs(Size states) { storeCreditStateNPVs_ = states; }
    void setStoreSurvivalProbabilities(bool b) { storeSurvivalProbabilities_ = b; }
    void setWriteCube(bool b) { writeCube_ = b; }
    void setWriteScenarios(bool b) { writeScenarios_ = b; }
    void setExposureSimMarketParams(const std::string& xml);
    void setExposureSimMarketParamsFromFile(const std::string& fileName);
    void setScenarioGeneratorData(const std::string& xml);
    void setScenarioGeneratorDataFromFile(const std::string& fileName);
    void setCrossAssetModelData(const std::string& xml);
    void setCrossAssetModelDataFromFile(const std::string& fileName);
    void setSimulationPricingEngine(const std::string& xml);
    void setSimulationPricingEngineFromFile(const std::string& fileName);
    void setSimulationPricingEngine(const boost::shared_ptr<EngineData>& engineData) {
        simulationPricingEngine_ = engineData;
    }
    void setAmcPricingEngine(const std::string& xml);
    void setAmcPricingEngineFromFile(const std::string& fileName);
    void setAmcPricingEngine(const boost::shared_ptr<EngineData>& engineData) {
        amcPricingEngine_ = engineData;
    }
    void setNettingSetManager(const std::string& xml);
    void setNettingSetManagerFromFile(const std::string& fileName);
    // TODO: load from XML
    // void setCounterpartyManager(const std::string& xml);
    // TODO: load from XML
    // void setCollateralBalances(const std::string& xml); 

    // Setters for xva
    void setXvaBaseCurrency(const std::string& s) { xvaBaseCurrency_ = s; }
    void setLoadCube(bool b) { loadCube_ = b; }
    // TODO: API for setting NPV and market cubes
    void setCubeFromFile(const std::string& file);
    void setNettingSetCubeFromFile(const std::string& file);
    void setCptyCubeFromFile(const std::string& file);
    void setMarketCubeFromFile(const std::string& file);
    // boost::shared_ptr<AggregationScenarioData> mktCube();
    void setFlipViewXVA(bool b) { flipViewXVA_ = b; }
    void setFullInitialCollateralisation(bool b) { fullInitialCollateralisation_ = b; }
    void setExposureProfiles(bool b) { exposureProfiles_ = b; }
    void setExposureProfilesByTrade(bool b) { exposureProfilesByTrade_ = b; }
    void setPfeQuantile(Real r) { pfeQuantile_ = r; }
    void setCollateralCalculationType(const std::string& s) { collateralCalculationType_ = s; }
    void setExposureAllocationMethod(const std::string& s) { exposureAllocationMethod_ = s; }
    void setMarginalAllocationLimit(Real r) { marginalAllocationLimit_ = r; }
    void setExerciseNextBreak(bool b) { exerciseNextBreak_ = b; }
    void setCvaAnalytic(bool b) { cvaAnalytic_ = b; }
    void setDvaAnalytic(bool b) { dvaAnalytic_ = b; }
    void setFvaAnalytic(bool b) { fvaAnalytic_ = b; }
    void setColvaAnalytic(bool b) { colvaAnalytic_ = b; }
    void setCollateralFloorAnalytic(bool b) { collateralFloorAnalytic_ = b; }
    void setDimAnalytic(bool b) { dimAnalytic_ = b; }
    void setMvaAnalytic(bool b) { mvaAnalytic_ = b; }
    void setKvaAnalytic(bool b) { kvaAnalytic_ = b; }
    void setDynamicCredit(bool b) { dynamicCredit_ = b; }
    void setCvaSensi(bool b) { cvaSensi_ = b; }
    void setCvaSensiGrid(const std::string& s); // parse to vector<Period>
    void setCvaSensiShiftSize(Real r) { cvaSensiShiftSize_ = r; }
    void setDvaName(const std::string& s) { dvaName_ = s; }
    void setRawCubeOutput(bool b) { rawCubeOutput_ = b; }
    void setNetCubeOutput(bool b) { netCubeOutput_ = b; }
    // FIXME: remove this from the base class?
    void setRawCubeOutputFile(const std::string& s) { rawCubeOutputFile_ = s; }
    void setNetCubeOutputFile(const std::string& s) { netCubeOutputFile_ = s; }
    // funding value adjustment details
    void setFvaBorrowingCurve(const std::string& s) { fvaBorrowingCurve_ = s; }
    void setFvaLendingCurve(const std::string& s) { fvaLendingCurve_ = s; }
    void setFlipViewBorrowingCurvePostfix(const std::string& s) { flipViewBorrowingCurvePostfix_ = s; }
    void setFlipViewLendingCurvePostfix(const std::string& s) { flipViewLendingCurvePostfix_ = s; }
    // deterministic initial margin input by netting set
    void setDeterministicInitialMargin(const std::string& n, TimeSeries<Real> v) { deterministicInitialMargin_[n] = v; }
    void setDeterministicInitialMarginFromFile(const std::string& fileName);
    // dynamic initial margin details
    void setDimQuantile(Real r) { dimQuantile_ = r; }
    void setDimHorizonCalendarDays(Size s) { dimHorizonCalendarDays_ = s; }
    void setDimRegressionOrder(Size s) { dimRegressionOrder_ = s; }
    void setDimRegressors(const std::string& s); // parse to vector<string>
    void setDimOutputGridPoints(const std::string& s); // parse to vector<Size>
    void setDimOutputNettingSet(const std::string& s) { dimOutputNettingSet_ = s; }
    void setDimLocalRegressionEvaluations(Size s) { dimLocalRegressionEvaluations_ = s; }
    void setDimLocalRegressionBandwidth(Real r) { dimLocalRegressionBandwidth_ = r; }
    // capital value adjustment details
    void setKvaCapitalDiscountRate(Real r) { kvaCapitalDiscountRate_ = r; } 
    void setKvaAlpha(Real r) { kvaAlpha_ = r; }
    void setKvaRegAdjustment(Real r) { kvaRegAdjustment_ = r; }
    void setKvaCapitalHurdle(Real r) { kvaCapitalHurdle_ = r; }
    void setKvaOurPdFloor(Real r) { kvaOurPdFloor_ = r; }
    void setKvaTheirPdFloor(Real r) { kvaTheirPdFloor_ = r; }
    void setKvaOurCvaRiskWeight(Real r) { kvaOurCvaRiskWeight_ = r; }
    void setKvaTheirCvaRiskWeight(Real r) { kvaTheirCvaRiskWeight_ = r; }
    // credit simulation
    void setCreditMigrationAnalytic(bool b) { creditMigrationAnalytic_ = b; }
    void setCreditMigrationDistributionGrid(const std::vector<Real>& grid) { creditMigrationDistributionGrid_ = grid; }
    void setCreditMigrationTimeSteps(const std::vector<Size>& ts) { creditMigrationTimeSteps_ = ts; }
    void setCreditSimulationParameters(const boost::shared_ptr<CreditSimulationParameters>& c) {
        creditSimulationParameters_ = c;
    }
    void setCreditSimulationParametersFromBuffer(const std::string& xml);
    void setCreditSimulationParametersFromFile(const std::string& fileName);
    void setCreditMigrationOutputFiles(const std::string& s) { creditMigrationOutputFiles_ = s; }
    // Setters for cashflow npv and dynamic backtesting
    void setCashflowHorizon(const std::string& s); // parse to Date 
    void setPortfolioFilterDate(const std::string& s); // parse to Date

    // Setters for SIMM
    void setSimmVersion(const std::string& s) { simmVersion_ = s; }
    void setCrifLoader();
    void setCrifFromFile(const std::string& fileName,
                         char eol = '\n', char delim = ',', char quoteChar = '\0', char escapeChar = '\\');
    void setCrifFromBuffer(const std::string& csvBuffer,
                           char eol = '\n', char delim = ',', char quoteChar = '\0', char escapeChar = '\\');
    void setSimmNameMapper(const boost::shared_ptr<ore::analytics::SimmBasicNameMapper>& p) { simmNameMapper_ = p; }
    void setSimmNameMapper(const std::string& xml);
    void setSimmNameMapperFromFile(const std::string& fileName);
    void setSimmBucketMapper(const boost::shared_ptr<ore::analytics::SimmBucketMapper>& p) { simmBucketMapper_ = p; }
    void setSimmBucketMapper(const std::string& xml);
    void setSimmBucketMapperFromFile(const std::string& fileName);
    void setSimmCalculationCurrency(const std::string& s) { simmCalculationCurrency_ = s; }
    void setSimmResultCurrency(const std::string& s) { simmResultCurrency_ = s; }
    void setSimmReportingCurrency(const std::string& s) { simmReportingCurrency_ = s; }
    void setEnforceIMRegulations(bool b) { enforceIMRegulations_= b; }

    // Setters for ZeroToParSensiConversion
    void setParConversionXbsParConversion(bool b) { parConversionXbsParConversion_ = b; }
    void setParConversionAlignPillars(bool b) { parConversionAlignPillars_ = b; }
    void setParConversionOutputJacobi(bool b) { parConversionOutputJacobi_ = b; }
    void setParConversionThreshold(Real r) { parConversionThreshold_ = r; }
    void setParConversionSimMarketParams(const std::string& xml);
    void setParConversionSimMarketParamsFromFile(const std::string& fileName);
    void setParConversionScenarioData(const std::string& xml);
    void setParConversionScenarioDataFromFile(const std::string& fileName);
    void setParConversionPricingEngine(const std::string& xml);
    void setParConversionPricingEngineFromFile(const std::string& fileName);
    void setParConversionPricingEngine(const boost::shared_ptr<EngineData>& engineData) {
        parConversionPricingEngine_ = engineData;
    }
    void setParConversionInputFile(const std::string& s) { parConversionInputFile_ = s; }
    void setParConversionInputIdColumn(const std::string& s) { parConversionInputIdColumn_ = s; }
    void setParConversionInputRiskFactorColumn(const std::string& s) { parConversionInputRiskFactorColumn_ = s; }
    void setParConversionInputDeltaColumn(const std::string& s) { parConversionInputDeltaColumn_ = s; }
    void setParConversionInputCurrencyColumn(const std::string& s) { parConversionInputCurrencyColumn_ = s; }
    void setParConversionInputBaseNpvColumn(const std::string& s) { parConversionInputBaseNpvColumn_ = s; }
    void setParConversionInputShiftSizeColumn(const std::string& s) { parConversionInputShiftSizeColumn_ = s; }

    // Setters for ScenarioStatistics
    void setScenarioDistributionSteps(const Size s) { scenarioDistributionSteps_ = s; }
    void setScenarioOutputZeroRate(const bool b) { scenarioOutputZeroRate_ = b; }


    // Set list of analytics that shall be run
    void setAnalytics(const std::string& s); // parse to set<string>
    void insertAnalytic(const std::string& s); 

    /***************************
     * Getters for general setup
     ***************************/
    const QuantLib::Date& asof() { return asof_; }
    const boost::filesystem::path& resultsPath() const { return resultsPath_; }
    const std::string& baseCurrency() { return baseCurrency_; }
    const std::string& resultCurrency() { return resultCurrency_; }
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
    CurveConfigurationsManager& curveConfigs() { return curveConfigs_; }
    const boost::shared_ptr<ore::data::EngineData>& pricingEngine() { return pricingEngine_; }
    const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams() { return todaysMarketParams_; }
    const boost::shared_ptr<ore::data::Portfolio>& portfolio() { return portfolio_; }
    const boost::shared_ptr<ore::data::Portfolio>& useCounterpartyOriginalPortfolio() {
        return useCounterpartyOriginalPortfolio_;
    }

    QuantLib::Size maxRetries() const { return maxRetries_; }
    QuantLib::Size nThreads() const { return nThreads_; }
    bool entireMarket() { return entireMarket_; }
    bool allFixings() { return allFixings_; }
    bool eomInflationFixings() { return eomInflationFixings_; }
    bool useMarketDataFixings() { return useMarketDataFixings_; }
    bool iborFallbackOverride() { return iborFallbackOverride_; }
    const std::string& reportNaString() { return reportNaString_; }
    char csvCommentCharacter() const { return csvCommentCharacter_; }
    char csvEolChar() const { return csvEolChar_; }
    char csvQuoteChar() const { return csvQuoteChar_; }
    char csvSeparator() const { return csvSeparator_; }
    char csvEscapeChar() const { return csvEscapeChar_; }
    bool dryRun() const { return dryRun_; }
    QuantLib::Size mporDays() { return mporDays_; }
    QuantLib::Date mporDate();
    const QuantLib::Calendar mporCalendar() {
        if (mporCalendar_.empty()) {
            QL_REQUIRE(!baseCurrency_.empty(), "mpor calendar or baseCurrency must be provided";);
            return parseCalendar(baseCurrency_);
        } else
            return mporCalendar_;
    }
    bool mporForward() { return mporForward_; }

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
    bool salvageCorrelationMatrix() { return salvageCorrelationMatrix_; }
    bool amc() { return amc_; }
    const std::set<std::string>& amcTradeTypes() { return amcTradeTypes_; }
    const std::string& exposureBaseCurrency() { return exposureBaseCurrency_; }
    const std::string& exposureObservationModel() { return exposureObservationModel_; }
    const std::string& nettingSetId() { return nettingSetId_; }
    const std::string& scenarioGenType() { return scenarioGenType_; }
    bool storeFlows() { return storeFlows_; }
    Size storeCreditStateNPVs() { return storeCreditStateNPVs_; }
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
    const boost::shared_ptr<NPVCube>& cube() { return cube_; }
    const boost::shared_ptr<NPVCube>& nettingSetCube() { return nettingSetCube_; }
    const boost::shared_ptr<NPVCube>& cptyCube() { return cptyCube_; }
    const boost::shared_ptr<AggregationScenarioData>& mktCube() { return mktCube_; }
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
    // deterministic initial margin input by nettingset
    TimeSeries<Real> deterministicInitialMargin(const std::string& n) {
        if (deterministicInitialMargin_.find(n) != deterministicInitialMargin_.end())
            return deterministicInitialMargin_.at(n);
        else
            return TimeSeries<Real>();
    }
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
    // credit simulation details
    bool creditMigrationAnalytic() const { return creditMigrationAnalytic_; }
    const std::vector<Real>& creditMigrationDistributionGrid() const { return creditMigrationDistributionGrid_; }
    std::vector<Size> creditMigrationTimeSteps() const { return creditMigrationTimeSteps_; }
    const boost::shared_ptr<CreditSimulationParameters>& creditSimulationParameters() const { return creditSimulationParameters_; }
    const std::string& creditMigrationOutputFiles() const { return creditMigrationOutputFiles_; }
    
    /**************************************************
     * Getters for cashflow npv and dynamic backtesting
     **************************************************/
    
    const QuantLib::Date& cashflowHorizon() const { return cashflowHorizon_; };
    const QuantLib::Date& portfolioFilterDate() const { return portfolioFilterDate_; }    

    /******************
     * Getters for SIMM
     ******************/
    const std::string& simmVersion() { return simmVersion_; }
    const boost::shared_ptr<ore::analytics::CrifLoader>& crifLoader() { return crifLoader_; }
    const boost::shared_ptr<ore::analytics::SimmBasicNameMapper>& simmNameMapper() { return simmNameMapper_; }
    const boost::shared_ptr<ore::analytics::SimmBucketMapper>& simmBucketMapper() { return simmBucketMapper_; }
    const std::string& simmCalculationCurrency() { return simmCalculationCurrency_; }
    const std::string& simmResultCurrency() { return simmResultCurrency_; }
    const std::string& simmReportingCurrency() { return simmReportingCurrency_; }
    bool enforceIMRegulations() { return enforceIMRegulations_; }

    /**************************************************
     * Getters for Zero to Par Sensi conversion
     **************************************************/
    bool parConversionXbsParConversion() { return parConversionXbsParConversion_; }
    bool parConversionAlignPillars() const { return parConversionAlignPillars_; };
    bool parConversionOutputJacobi() const { return parConversionOutputJacobi_; };
    QuantLib::Real parConversionThreshold() const { return parConversionThreshold_; }
    const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& parConversionSimMarketParams() {
        return parConversionSimMarketParams_;
    }
    const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& parConversionScenarioData() {
        return parConversionScenarioData_;
    }
    const boost::shared_ptr<ore::data::EngineData>& parConversionPricingEngine() { return parConversionPricingEngine_; }
    const std::string& parConversionInputFile() const { return parConversionInputFile_; }
    // Column Names in the input
    const std::string& parConversionInputIdColumn() { return parConversionInputIdColumn_; }
    const std::string& parConversionInputRiskFactorColumn() { return parConversionInputRiskFactorColumn_; }
    const std::string& parConversionInputDeltaColumn() { return parConversionInputDeltaColumn_; }
    const std::string& parConversionInputCurrencyColumn() { return parConversionInputCurrencyColumn_; }
    const std::string& parConversionInputBaseNpvColumn() { return parConversionInputBaseNpvColumn_; }
    const std::string& parConversionInputShiftSizeColumn() { return parConversionInputShiftSizeColumn_; }

    // Getters for ScenarioStatistics
    const Size& scenarioDistributionSteps() { return scenarioDistributionSteps_; }
    const bool& scenarioOutputZeroRate() { return scenarioOutputZeroRate_; }

    /*************************************
     * List of analytics that shall be run
     *************************************/
    const std::set<std::string>& analytics() { return analytics_; }

    virtual void loadParameters() {}
    virtual void writeOutParameters() {}

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
    std::set<std::string> analytics_;

    /***********************************
     * Basic setup, across all run types
     ***********************************/
    QuantLib::Date asof_;
    boost::filesystem::path resultsPath_;
    std::string baseCurrency_;
    std::string resultCurrency_;
    bool continueOnError_ = true;
    bool lazyMarketBuilding_ = true;
    bool buildFailedTrades_ = true;
    std::string observationModel_ = "None";
    bool implyTodaysFixings_ = false;
    std::map<std::string, std::string> marketConfigs_;
    boost::shared_ptr<ore::data::BasicReferenceDataManager> refDataManager_;
    boost::shared_ptr<ore::data::Conventions> conventions_;
    boost::shared_ptr<ore::data::IborFallbackConfig> iborFallbackConfig_;
    CurveConfigurationsManager curveConfigs_;
    boost::shared_ptr<ore::data::EngineData> pricingEngine_;
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    boost::shared_ptr<ore::data::Portfolio> portfolio_, useCounterpartyOriginalPortfolio_;
    QuantLib::Size maxRetries_ = 7;
    QuantLib::Size nThreads_ = 1;
   
    bool entireMarket_ = false; 
    bool allFixings_ = false; 
    bool eomInflationFixings_ = true;
    bool useMarketDataFixings_ = true;
    bool iborFallbackOverride_ = false;
    bool csvCommentCharacter_ = true;
    char csvEolChar_ = '\n';
    char csvSeparator_ = ',';
    char csvQuoteChar_ = '\0';
    char csvEscapeChar_ = '\\';
    std::string reportNaString_ = "#N/A";
    bool dryRun_ = false;
    QuantLib::Date mporDate_;
    QuantLib::Size mporDays_ = 10;
    QuantLib::Calendar mporCalendar_;
    bool mporForward_ = true;

    /**************
     * NPV analytic
     *************/
    bool outputAdditionalResults_ = false;
    bool outputCurves_ = false;
    std::string curvesMarketConfig_ = Market::defaultConfiguration;
    std::string curvesGrid_ = "240,1M";
    bool outputTodaysMarketCalibration_ = true;

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
    bool parSensi_ = false;
    bool outputJacobi_ = false;
    bool alignPillars_ = false;
    bool useSensiSpreadedTermStructures_ = true;
    QuantLib::Real sensiThreshold_ = 1e-6;
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
     * VAR analytics
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
    bool salvageCorrelationMatrix_ = false;
    bool amc_ = false;
    std::set<std::string> amcTradeTypes_;
    std::string exposureBaseCurrency_ = "";
    std::string exposureObservationModel_ = "Disable";
    std::string nettingSetId_ = "";
    std::string scenarioGenType_ = "";
    bool storeFlows_ = false;
    Size storeCreditStateNPVs_ = 0;
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
    std::string collateralCalculationType_ = "NoLag";
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
    // deterministic initial margin by netting set
    std::map<std::string,TimeSeries<Real>> deterministicInitialMargin_;
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
    // credit simulation details
    bool creditMigrationAnalytic_ = false;
    std::vector<Real> creditMigrationDistributionGrid_;
    std::vector<Size> creditMigrationTimeSteps_;
    boost::shared_ptr<CreditSimulationParameters> creditSimulationParameters_;
    std::string creditMigrationOutputFiles_;

    /***************
     * SIMM analytic
     ***************/
    std::string simmVersion_;
    boost::shared_ptr<ore::analytics::CrifLoader> crifLoader_;
    boost::shared_ptr<ore::analytics::SimmBasicNameMapper> simmNameMapper_;
    boost::shared_ptr<ore::analytics::SimmBucketMapper> simmBucketMapper_;
    std::string simmCalculationCurrency_ = "";
    std::string simmResultCurrency_ = "";
    std::string simmReportingCurrency_ = "";
    bool enforceIMRegulations_ = false;
    bool useSimmParameters_ = true;

    /***************
     * Zero to Par Conversion analytic
     ***************/
    bool parConversionXbsParConversion_ = false;
    bool parConversionOutputJacobi_ = false;
    bool parConversionAlignPillars_ = false;
    QuantLib::Real parConversionThreshold_ = 1e-6;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> parConversionSimMarketParams_;
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> parConversionScenarioData_;
    boost::shared_ptr<ore::data::EngineData> parConversionPricingEngine_;
    std::string parConversionInputFile_;
    std::string parConversionInputIdColumn_ = "TradeId";
    std::string parConversionInputRiskFactorColumn_ = "Factor_1";
    std::string parConversionInputDeltaColumn_ = "Delta";
    std::string parConversionInputCurrencyColumn_ = "Currency";
    std::string parConversionInputBaseNpvColumn_ = "Base NPV";
    std::string parConversionInputShiftSizeColumn_ = "ShiftSize_1";

    /***************
     * Scenario Statistics analytic
     ***************/
    Size scenarioDistributionSteps_ = 20;
    bool scenarioOutputZeroRate_ = false;
};

inline const std::string& InputParameters::marketConfig(const std::string& context) {
    auto it = marketConfigs_.find(context);
    return (it != marketConfigs_.end() ? it->second : Market::defaultConfiguration);
}
    
//! Traditional ORE input via ore.xml and various files, output into files
class OutputParameters {
public:
    OutputParameters(const boost::shared_ptr<Parameters>& params);
    //! map internal report name to the configured external file name
    const std::map<std::string, std::string>& fileNameMap() { return fileNameMap_; }
    std::string outputFileName(const std::string& internalName, const std::string& suffix);
    
private:
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
    std::string parConversionOutputFileName_;
    std::string parConversionJacobiFileName_;
    std::string parConversionJacobiInverseFileName_;
};
    
} // namespace analytics
} // namespace ore
