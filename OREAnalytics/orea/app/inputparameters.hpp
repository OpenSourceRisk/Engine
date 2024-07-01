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

#include <boost/filesystem/path.hpp>
#include <orea/aggregation/creditsimulationparameters.hpp>
#include <orea/app/parameters.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/historicalscenarioreader.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmcalibration.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <boost/filesystem/path.hpp>
#include <filesystem>

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
    void setPortfolioFromFile(const std::string& fileNameString, const std::filesystem::path& inputPath); 
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
    void setMporOverlappingPeriods(bool b) { mporOverlappingPeriods_ = b; }
    void setMporDate(const QuantLib::Date& d) { mporDate_ = d; }
    void setMporCalendar(const std::string& s); 
    void setMporForward(bool b) { mporForward_ = b; }

    // Setters for npv analytics
    void setOutputAdditionalResults(bool b) { outputAdditionalResults_ = b; }
    void setAdditionalResultsReportPrecision(std::size_t p) { additionalResultsReportPrecision_ = p; }
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
    void setOptimiseRiskFactors(bool b) { optimiseRiskFactors_ = b; }
    void setAlignPillars(bool b) { alignPillars_ = b; }
    void setOutputJacobi(bool b) { outputJacobi_ = b; }
    void setUseSensiSpreadedTermStructures(bool b) { useSensiSpreadedTermStructures_ = b; }
    void setSensiThreshold(Real r) { sensiThreshold_ = r; }
    void setSensiRecalibrateModels(bool b) { sensiRecalibrateModels_ = b; }
    void setSensiSimMarketParams(const std::string& xml);
    void setSensiSimMarketParamsFromFile(const std::string& fileName);
    void setSensiScenarioData(const std::string& xml);
    void setSensiScenarioDataFromFile(const std::string& fileName);
    void setSensiPricingEngine(const std::string& xml);
    void setSensiPricingEngineFromFile(const std::string& fileName);
    void setSensiPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        sensiPricingEngine_ = engineData;
    }

    // Setters for scenario
    void setScenarioSimMarketParams(const std::string& xml);
    void setScenarioSimMarketParamsFromFile(const std::string& fileName);
    void setScenarioOutputFile(const std::string& filename) { scenarioOutputFile_ = filename; }

    // Setters for stress testing
    void setStressThreshold(Real r) { stressThreshold_ = r; }
    void setStressOptimiseRiskFactors(bool optimise) { stressOptimiseRiskFactors_ = optimise; }
    void setStressSimMarketParams(const std::string& xml); 
    void setStressSimMarketParamsFromFile(const std::string& fileName); 
    void setStressScenarioData(const std::string& xml); 
    void setStressScenarioDataFromFile(const std::string& fileName); 
    void setStressPricingEngine(const std::string& xml); 
    void setStressPricingEngineFromFile(const std::string& fileName); 
    void setStressPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        stressPricingEngine_ = engineData;
    }
    void setStressSensitivityScenarioData(const std::string& xml);
    void setStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setStressLowerBoundCapFloorVolatility(const double value) { stressLowerBoundCapFloorVolatility_ = value; }
    void setStressUpperBoundCapFloorVolatility(const double value) { stressUpperBoundCapFloorVolatility_ = value; }
    void setStressLowerBoundSurvivalProb(const double value) { stressLowerBoundSurvivalProb_ = value; }
    void setStressUpperBoundSurvivalProb(const double value) { stressUpperBoundSurvivalProb_ = value; }
    void setStressLowerBoundRatesDiscountFactor(const double value) { stressLowerBoundRatesDiscountFactor_ = value; }
    void setStressUpperBoundRatesDiscountFactor(const double value) { stressUpperBoundRatesDiscountFactor_ = value; }
    void setStressAccurary(const double value) { stressAccurary_ = value; };
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
    void setBenchmarkVarPeriod(const std::string& period);
    void setHistoricalScenarioReader(const std::string& fileName);
    void setSensitivityStreamFromBuffer(const std::string& buffer);
    void setHistVarSimMarketParams(const std::string& xml);
    void setHistVarSimMarketParamsFromFile(const std::string& fileName);
    void setOutputHistoricalScenarios(const bool b) { outputHistoricalScenarios_ = b; }

    // Setters for exposure simulation
    void setSalvageCorrelationMatrix(bool b) { salvageCorrelationMatrix_ = b; }
    void setAmc(bool b) { amc_ = b; }
    void setAmcCg(bool b) { amcCg_ = b; }
    void setXvaCgBumpSensis(bool b) { xvaCgBumpSensis_ = b; }
    void setXvaCgUseExternalComputeDevice(bool b) { xvaCgUseExternalComputeDevice_ = b; }
    void setXvaCgExternalDeviceCompatibilityMode(bool b) { xvaCgExternalDeviceCompatibilityMode_ = b; }
    void setXvaCgUseDoublePrecisionForExternalCalculation(bool b) { xvaCgUseDoublePrecisionForExternalCalculation_ = b; }
    void setXvaCgExternalComputeDevice(string s) { xvaCgExternalComputeDevice_ = std::move(s); }
    void setXvaCgSensiScenarioData(const std::string& xml);
    void setXvaCgSensiScenarioDataFromFile(const std::string& fileName);
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
    void setSimulationPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        simulationPricingEngine_ = engineData;
    }
    void setAmcPricingEngine(const std::string& xml);
    void setAmcPricingEngineFromFile(const std::string& fileName);
    void setAmcPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        amcPricingEngine_ = engineData;
    }
    void setNettingSetManager(const std::string& xml);
    void setNettingSetManagerFromFile(const std::string& fileName);
    void setCollateralBalances(const std::string& xml); 
    void setCollateralBalancesFromFile(const std::string& fileName);
    // TODO: load from XML
    // void setCounterpartyManager(const std::string& xml);

    // Setters for xva
    void setXvaBaseCurrency(const std::string& s) { xvaBaseCurrency_ = s; }
    void setLoadCube(bool b) { loadCube_ = b; }
    // TODO: API for setting NPV and market cubes
    /* This overwrites scenarioGeneratorData, storeFlows, storeCreditStateNpvs to the values stored together with the
       cube. Therefore this method should be called after setScenarioGeneratorData(), setStoreFlows(),
       setStoreCreditStateNPVs() to ensure that the overwrite takes place. */
    void setCubeFromFile(const std::string& file);
    void setCube(const QuantLib::ext::shared_ptr<NPVCube>& cube);
    void setNettingSetCubeFromFile(const std::string& file);
    void setCptyCubeFromFile(const std::string& file);
    void setMarketCubeFromFile(const std::string& file);
    void setMarketCube(const QuantLib::ext::shared_ptr<AggregationScenarioData>& cube);
    // QuantLib::ext::shared_ptr<AggregationScenarioData> mktCube();
    void setFlipViewXVA(bool b) { flipViewXVA_ = b; }
    void setMporCashFlowMode(const MporCashFlowMode m) { mporCashFlowMode_ = m; }
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
    void setDimModel(const std::string& s) { dimModel_ = s; }
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
    void setCreditSimulationParameters(const QuantLib::ext::shared_ptr<CreditSimulationParameters>& c) {
        creditSimulationParameters_ = c;
    }
    void setCreditSimulationParametersFromBuffer(const std::string& xml);
    void setCreditSimulationParametersFromFile(const std::string& fileName);
    void setCreditMigrationOutputFiles(const std::string& s) { creditMigrationOutputFiles_ = s; }
    // Setters for cashflow npv and dynamic backtesting
    void setCashflowHorizon(const std::string& s); // parse to Date 
    void setPortfolioFilterDate(const std::string& s); // parse to Date

    // Setters for xvaStress
    void setXvaStressSimMarketParams(const std::string& xml);
    void setXvaStressSimMarketParamsFromFile(const std::string& f);
    void setXvaStressScenarioData(const std::string& s);
    void setXvaStressScenarioDataFromFile(const std::string& s);
    void setXvaStressSensitivityScenarioData(const std::string& xml);
    void setXvaStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setXvaStressWriteCubes(const bool writeCubes) { xvaStressWriteCubes_ = writeCubes; }

    // Setters for xvaStress
    void setXvaSensiSimMarketParams(const std::string& xml);
    void setXvaSensiSimMarketParamsFromFile(const std::string& fileName);
    void setXvaSensiScenarioData(const std::string& xml);
    void setXvaSensiScenarioDataFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const std::string& xml);
    void setXvaSensiPricingEngineFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        sensiPricingEngine_ = engineData;
    }

    // Setters for SIMM
    void setSimmVersion(const std::string& s) { simmVersion_ = s; }
    
    void setCrifFromFile(const std::string& fileName,
                         char eol = '\n', char delim = ',', char quoteChar = '\0', char escapeChar = '\\');
    void setCrifFromBuffer(const std::string& csvBuffer,
                           char eol = '\n', char delim = ',', char quoteChar = '\0', char escapeChar = '\\');
    void setSimmNameMapper(const QuantLib::ext::shared_ptr<ore::analytics::SimmBasicNameMapper>& p) { simmNameMapper_ = p; }
    void setSimmNameMapper(const std::string& xml);
    void setSimmNameMapperFromFile(const std::string& fileName);
    void setSimmBucketMapper(const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& p) { simmBucketMapper_ = p; }
    void setSimmBucketMapper(const std::string& xml);
    void setSimmBucketMapperFromFile(const std::string& fileName);
    void setSimmCalibrationData(const QuantLib::ext::shared_ptr<ore::analytics::SimmCalibrationData>& s) {
        simmCalibrationData_ = s;
    }
    void setSimmCalibrationDataFromFile(const std::string& fileName);
    void setSimmCalculationCurrencyCall(const std::string& s) { simmCalculationCurrencyCall_ = s; }
    void setSimmCalculationCurrencyPost(const std::string& s) { simmCalculationCurrencyPost_ = s; }
    void setSimmResultCurrency(const std::string& s) { simmResultCurrency_ = s; }
    void setSimmReportingCurrency(const std::string& s) { simmReportingCurrency_ = s; }
    void setEnforceIMRegulations(bool b) { enforceIMRegulations_= b; }
    void setWriteSimmIntermediateReports(bool b) { writeSimmIntermediateReports_ = b; }

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
    void setParConversionPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
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
    // Setters for par stress conversion
    void setParStressSimMarketParams(const std::string& xml);
    void setParStressSimMarketParamsFromFile(const std::string& fileName);
    void setParStressScenarioData(const std::string& xml);
    void setParStressScenarioDataFromFile(const std::string& fileName);
    void setParStressPricingEngine(const std::string& xml);
    void setParStressPricingEngineFromFile(const std::string& fileName);
    void setParStressPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        parStressPricingEngine_ = engineData;
    }
    void setParStressSensitivityScenarioData(const std::string& xml);
    void setParStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setParStressLowerBoundCapFloorVolatility(const double value)  { parStressLowerBoundCapFloorVolatility_ = value; }
    void setParStressUpperBoundCapFloorVolatility(const double value)  { parStressUpperBoundCapFloorVolatility_ = value; }
    void setParStressLowerBoundSurvivalProb(const double value)  { parStressLowerBoundSurvivalProb_ = value; }
    void setParStressUpperBoundSurvivalProb(const double value)  { parStressUpperBoundSurvivalProb_ = value; }
    void setParStressLowerBoundRatesDiscountFactor(const double value)  { parStressLowerBoundRatesDiscountFactor_ = value; }
    void setParStressUpperBoundRatesDiscountFactor(const double value)  { parStressUpperBoundRatesDiscountFactor_ = value; }
    void setParStressAccurary(const double value)  { parStressAccurary_ = value; };

    // Setters for zeroToParShift conversion
    void setZeroToParShiftSimMarketParams(const std::string& xml);
    void setZeroToParShiftSimMarketParamsFromFile(const std::string& fileName);
    void setZeroToParShiftScenarioData(const std::string& xml);
    void setZeroToParShiftScenarioDataFromFile(const std::string& fileName);
    void setZeroToParShiftPricingEngine(const std::string& xml);
    void setZeroToParShiftPricingEngineFromFile(const std::string& fileName);
    void setZeroToParShiftPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        zeroToParShiftPricingEngine_ = engineData;
    }
    void setZeroToParShiftSensitivityScenarioData(const std::string& xml);
    void setZeroToParShiftSensitivityScenarioDataFromFile(const std::string& fileName);

    // Set list of analytics that shall be run
    void setAnalytics(const std::string& s); // parse to set<string>
    void insertAnalytic(const std::string& s); 



    /***************************
     * Getters for general setup
     ***************************/
    const QuantLib::Date& asof() const { return asof_; }
    const boost::filesystem::path& resultsPath() const { return resultsPath_; }
    const std::string& baseCurrency() const { return baseCurrency_; }
    const std::string& resultCurrency() const { return resultCurrency_; }
    bool continueOnError() const { return continueOnError_; }
    bool lazyMarketBuilding() const { return lazyMarketBuilding_; }
    bool buildFailedTrades() const { return buildFailedTrades_; }
    const std::string& observationModel() const { return observationModel_; }
    bool implyTodaysFixings() const { return implyTodaysFixings_; }
    const std::map<std::string, std::string>&  marketConfigs() const { return marketConfigs_; }
    const std::string& marketConfig(const std::string& context);
    const QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager() const { return refDataManager_; }
    const QuantLib::ext::shared_ptr<ore::data::Conventions>& conventions() const { return conventions_; }
    const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig() const { return iborFallbackConfig_; }
    CurveConfigurationsManager& curveConfigs() { return curveConfigs_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& pricingEngine() const { return pricingEngine_; }
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams() const { return todaysMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& useCounterpartyOriginalPortfolio() const {
        return useCounterpartyOriginalPortfolio_;
    }

    QuantLib::Size maxRetries() const { return maxRetries_; }
    QuantLib::Size nThreads() const { return nThreads_; }
    bool entireMarket() const { return entireMarket_; }
    bool allFixings() const { return allFixings_; }
    bool eomInflationFixings() const { return eomInflationFixings_; }
    bool useMarketDataFixings() const { return useMarketDataFixings_; }
    bool iborFallbackOverride() const { return iborFallbackOverride_; }
    const std::string& reportNaString() const { return reportNaString_; }
    char csvCommentCharacter() const { return csvCommentCharacter_; }
    char csvEolChar() const { return csvEolChar_; }
    char csvQuoteChar() const { return csvQuoteChar_; }
    char csvSeparator() const { return csvSeparator_; }
    char csvEscapeChar() const { return csvEscapeChar_; }
    bool dryRun() const { return dryRun_; }
    QuantLib::Size mporDays() const { return mporDays_; }
    QuantLib::Date mporDate();
    const QuantLib::Calendar mporCalendar() {
        if (mporCalendar_.empty()) {
            QL_REQUIRE(!baseCurrency_.empty(), "mpor calendar or baseCurrency must be provided";);
            return parseCalendar(baseCurrency_);
        } else
            return mporCalendar_;
    }
    bool mporOverlappingPeriods() const { return mporOverlappingPeriods_; }
    bool mporForward() const { return mporForward_; }

    /***************************
     * Getters for npv analytics
     ***************************/
    bool outputAdditionalResults() const { return outputAdditionalResults_; };
    std::size_t additionalResultsReportPrecision() const { return additionalResultsReportPrecision_; }

    /***********************
     * Getters for cashflows
     ***********************/
    bool includePastCashflows() const { return includePastCashflows_; }

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
    bool optimiseRiskFactors() const { return optimiseRiskFactors_; }
    bool alignPillars() const { return alignPillars_; };
    bool outputJacobi() const { return outputJacobi_; };
    bool useSensiSpreadedTermStructures() const { return useSensiSpreadedTermStructures_; }
    QuantLib::Real sensiThreshold() const { return sensiThreshold_; }
    bool sensiRecalibrateModels() const { return sensiRecalibrateModels_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& sensiSimMarketParams() const { return sensiSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData() const { return sensiScenarioData_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& sensiPricingEngine() const { return sensiPricingEngine_; }
    // const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& sensiTodaysMarketParams() { return sensiTodaysMarketParams_; }
        
    /****************************
     * Getters for scenario build
     ****************************/
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& scenarioSimMarketParams() const { return scenarioSimMarketParams_; }
    const std::string& scenarioOutputFile() const { return scenarioOutputFile_; }

    /****************************
     * Getters for stress testing
     ****************************/
    QuantLib::Real stressThreshold() const { return stressThreshold_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& stressSimMarketParams() const { return stressSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressScenarioData() const { return stressScenarioData_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& stressPricingEngine() const { return stressPricingEngine_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& stressSensitivityScenarioData() const {
        return stressSensitivityScenarioData_;
    }
    bool stressOptimiseRiskFactors() const { return stressOptimiseRiskFactors_; }
    double stressLowerBoundCapFloorVolatility() const {
        return stressLowerBoundCapFloorVolatility_;
    }
    double stressUpperBoundCapFloorVolatility() const {
        return stressUpperBoundCapFloorVolatility_;
    }
    double stressLowerBoundSurvivalProb() const { return stressLowerBoundSurvivalProb_; }
    double stressUpperBoundSurvivalProb() const { return stressUpperBoundSurvivalProb_; }
    double stressLowerBoundRatesDiscountFactor() const {
        return stressLowerBoundRatesDiscountFactor_;
    }
    double stressUpperBoundRatesDiscountFactor() const {
        return stressUpperBoundRatesDiscountFactor_;
    }
    double stressAccurary() const { return stressAccurary_; };
    /*****************
     * Getters for VaR
     *****************/
    bool salvageCovariance() const { return salvageCovariance_; }
    const std::vector<Real>& varQuantiles() const { return varQuantiles_; }
    bool varBreakDown() const { return varBreakDown_; }
    const std::string& portfolioFilter() const { return portfolioFilter_; }
    const std::string& varMethod() const { return varMethod_; }
    Size mcVarSamples() const { return mcVarSamples_; }
    long mcVarSeed() const { return mcVarSeed_; }
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& covarianceData() const { return covarianceData_; }
    const QuantLib::ext::shared_ptr<SensitivityStream>& sensitivityStream() const { return sensitivityStream_; }
    std::string benchmarkVarPeriod() const { return benchmarkVarPeriod_; }
    QuantLib::ext::shared_ptr<HistoricalScenarioReader> historicalScenarioReader() const { return historicalScenarioReader_;};
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& histVarSimMarketParams() const { return histVarSimMarketParams_; }
    bool outputHistoricalScenarios() const { return outputHistoricalScenarios_; }
    
    /*********************************
     * Getters for exposure simulation 
     *********************************/
    bool salvageCorrelationMatrix() const { return salvageCorrelationMatrix_; }
    bool amc() const { return amc_; }
    bool amcCg() const { return amcCg_; }
    bool xvaCgBumpSensis() const { return xvaCgBumpSensis_; }
    bool xvaCgUseExternalComputeDevice() const { return xvaCgUseExternalComputeDevice_; }
    bool xvaCgExternalDeviceCompatibilityMode() const { return xvaCgExternalDeviceCompatibilityMode_; }
    bool xvaCgUseDoublePrecisionForExternalCalculation() const {
        return xvaCgUseDoublePrecisionForExternalCalculation_;
    }
    const std::string& xvaCgExternalComputeDevice() const { return xvaCgExternalComputeDevice_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& xvaCgSensiScenarioData() const { return xvaCgSensiScenarioData_; }
    const std::set<std::string>& amcTradeTypes() const { return amcTradeTypes_; }
    const std::string& exposureBaseCurrency() const { return exposureBaseCurrency_; }
    const std::string& exposureObservationModel() const { return exposureObservationModel_; }
    const std::string& nettingSetId() const { return nettingSetId_; }
    const std::string& scenarioGenType() const { return scenarioGenType_; }
    bool storeFlows() const { return storeFlows_; }
    Size storeCreditStateNPVs() const { return storeCreditStateNPVs_; }
    bool storeSurvivalProbabilities() const { return storeSurvivalProbabilities_; }
    bool writeCube() const { return writeCube_; }
    bool writeScenarios() const { return writeScenarios_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& exposureSimMarketParams() const { return exposureSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData() const { return scenarioGeneratorData_; }
    const QuantLib::ext::shared_ptr<CrossAssetModelData>& crossAssetModelData() const { return crossAssetModelData_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& simulationPricingEngine() const { return simulationPricingEngine_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& amcPricingEngine() const { return amcPricingEngine_; }
    const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& nettingSetManager() const { return nettingSetManager_; }
    // const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager() const { return counterpartyManager_; }
    const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances() const { return collateralBalances_; }
    const Real& simulationBootstrapTolerance() const { return simulationBootstrapTolerance_; }

    /*****************
     * Getters for xva
     *****************/
    const std::string& xvaBaseCurrency() const { return xvaBaseCurrency_; }
    bool loadCube() { return loadCube_; }
    const QuantLib::ext::shared_ptr<NPVCube>& cube() const { return cube_; }
    const QuantLib::ext::shared_ptr<NPVCube>& nettingSetCube() const { return nettingSetCube_; }
    const QuantLib::ext::shared_ptr<NPVCube>& cptyCube() const { return cptyCube_; }
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& mktCube() const { return mktCube_; }
    bool flipViewXVA() const { return flipViewXVA_; }
    MporCashFlowMode mporCashFlowMode() const { return mporCashFlowMode_; }
    bool fullInitialCollateralisation() const { return fullInitialCollateralisation_; }
    bool exposureProfiles() const { return exposureProfiles_; }
    bool exposureProfilesByTrade() const { return exposureProfilesByTrade_; }
    Real pfeQuantile() const { return pfeQuantile_; }
    const std::string&  collateralCalculationType() const { return collateralCalculationType_; }
    const std::string& exposureAllocationMethod() const { return exposureAllocationMethod_; }
    Real marginalAllocationLimit() const { return marginalAllocationLimit_; }
    bool exerciseNextBreak() const { return exerciseNextBreak_; }
    bool cvaAnalytic() const { return cvaAnalytic_; }
    bool dvaAnalytic() const { return dvaAnalytic_; }
    bool fvaAnalytic() const { return fvaAnalytic_; }
    bool colvaAnalytic() const { return colvaAnalytic_; }
    bool collateralFloorAnalytic()  const { return collateralFloorAnalytic_; }
    bool dimAnalytic()  const { return dimAnalytic_; }
    const std::string& dimModel() const { return dimModel_; }
    bool mvaAnalytic() const { return mvaAnalytic_; }
    bool kvaAnalytic() const { return kvaAnalytic_; }
    bool dynamicCredit() const { return dynamicCredit_; }
    bool cvaSensi() const { return cvaSensi_; }
    const std::vector<Period>& cvaSensiGrid() const { return cvaSensiGrid_; }
    Real cvaSensiShiftSize() const { return cvaSensiShiftSize_; }
    const std::string& dvaName() const { return dvaName_; }
    bool rawCubeOutput() const { return rawCubeOutput_; }
    bool netCubeOutput() const { return netCubeOutput_; }
    const std::string& rawCubeOutputFile() const { return rawCubeOutputFile_; }
    const std::string& netCubeOutputFile() const { return netCubeOutputFile_; }
    // funding value adjustment details
    const std::string& fvaBorrowingCurve() const { return fvaBorrowingCurve_; }
    const std::string& fvaLendingCurve() const { return fvaLendingCurve_; }
    const std::string& flipViewBorrowingCurvePostfix() const { return flipViewBorrowingCurvePostfix_; }
    const std::string& flipViewLendingCurvePostfix() const { return flipViewLendingCurvePostfix_; }
    // deterministic initial margin input by nettingset
    TimeSeries<Real> deterministicInitialMargin(const std::string& n) {
        if (deterministicInitialMargin_.find(n) != deterministicInitialMargin_.end())
            return deterministicInitialMargin_.at(n);
        else
            return TimeSeries<Real>();
    }
    // dynamic initial margin details
    Real dimQuantile() const { return dimQuantile_; }
    Size dimHorizonCalendarDays() const { return dimHorizonCalendarDays_; }
    Size dimRegressionOrder() const { return dimRegressionOrder_; }
    const std::vector<std::string>& dimRegressors() const { return dimRegressors_; }
    const std::vector<Size>& dimOutputGridPoints() const { return dimOutputGridPoints_; }
    const std::string& dimOutputNettingSet() const { return dimOutputNettingSet_; }
    Size dimLocalRegressionEvaluations() const { return dimLocalRegressionEvaluations_; }
    Real dimLocalRegressionBandwidth() const { return dimLocalRegressionBandwidth_; }
    // capital value adjustment details
    Real kvaCapitalDiscountRate() const { return kvaCapitalDiscountRate_; } 
    Real kvaAlpha() const { return kvaAlpha_; }
    Real kvaRegAdjustment() const { return kvaRegAdjustment_; }
    Real kvaCapitalHurdle() const { return kvaCapitalHurdle_; }
    Real kvaOurPdFloor() const { return kvaOurPdFloor_; }
    Real kvaTheirPdFloor() const { return kvaTheirPdFloor_; }
    Real kvaOurCvaRiskWeight() const { return kvaOurCvaRiskWeight_; }
    Real kvaTheirCvaRiskWeight() const { return kvaTheirCvaRiskWeight_; }
    // credit simulation details
    bool creditMigrationAnalytic() const { return creditMigrationAnalytic_; }
    const std::vector<Real>& creditMigrationDistributionGrid() const { return creditMigrationDistributionGrid_; }
    std::vector<Size> creditMigrationTimeSteps() const { return creditMigrationTimeSteps_; }
    const QuantLib::ext::shared_ptr<CreditSimulationParameters>& creditSimulationParameters() const { return creditSimulationParameters_; }
    const std::string& creditMigrationOutputFiles() const { return creditMigrationOutputFiles_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& xvaStressSimMarketParams() const { return xvaStressSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& xvaStressScenarioData() const { return xvaStressScenarioData_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& xvaStressSensitivityScenarioData() const {
        return xvaStressSensitivityScenarioData_;
    }
    bool xvaStressWriteCubes() const { return xvaStressWriteCubes_; }
    /**************************************************
     * Getters for cashflow npv and dynamic backtesting
     **************************************************/
    
    const QuantLib::Date& cashflowHorizon() const { return cashflowHorizon_; };
    const QuantLib::Date& portfolioFilterDate() const { return portfolioFilterDate_; }    

    /******************
     * Getters for SIMM
     ******************/
    const std::string& simmVersion() const { return simmVersion_; }
    const ore::analytics::Crif& crif() const { return crif_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SimmBasicNameMapper>& simmNameMapper() const { return simmNameMapper_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& simmBucketMapper() const { return simmBucketMapper_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SimmCalibrationData>& simmCalibrationData() const { return simmCalibrationData_; }
    const std::string& simmCalculationCurrencyCall() const { return simmCalculationCurrencyCall_; }
    const std::string& simmCalculationCurrencyPost() const { return simmCalculationCurrencyPost_; }
    const std::string& simmResultCurrency() const { return simmResultCurrency_; }
    const std::string& simmReportingCurrency() const { return simmReportingCurrency_; }
    bool enforceIMRegulations() const { return enforceIMRegulations_; }
    QuantLib::ext::shared_ptr<SimmConfiguration> getSimmConfiguration();
    bool writeSimmIntermediateReports() const { return writeSimmIntermediateReports_; }

    /**************************************************
     * Getters for Zero to Par Sensi conversion
     **************************************************/
    bool parConversionXbsParConversion() const { return parConversionXbsParConversion_; }
    bool parConversionAlignPillars() const { return parConversionAlignPillars_; };
    bool parConversionOutputJacobi() const { return parConversionOutputJacobi_; };
    QuantLib::Real parConversionThreshold() const { return parConversionThreshold_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& parConversionSimMarketParams() const {
        return parConversionSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& parConversionScenarioData() const {
        return parConversionScenarioData_;
    }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& parConversionPricingEngine() const { return parConversionPricingEngine_; }
    const std::string& parConversionInputFile() const { return parConversionInputFile_; }
    // Column Names in the input
    const std::string& parConversionInputIdColumn() const { return parConversionInputIdColumn_; }
    const std::string& parConversionInputRiskFactorColumn() const { return parConversionInputRiskFactorColumn_; }
    const std::string& parConversionInputDeltaColumn() const { return parConversionInputDeltaColumn_; }
    const std::string& parConversionInputCurrencyColumn() const { return parConversionInputCurrencyColumn_; }
    const std::string& parConversionInputBaseNpvColumn() const { return parConversionInputBaseNpvColumn_; }
    const std::string& parConversionInputShiftSizeColumn() const { return parConversionInputShiftSizeColumn_; }

    // Getters for ScenarioStatistics
    const Size& scenarioDistributionSteps() const { return scenarioDistributionSteps_; }
    const bool& scenarioOutputZeroRate() const { return scenarioOutputZeroRate_; }

    // Getters for ParStressConversion
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& parStressSimMarketParams() const {
        return parStressSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& parStressScenarioData() const {
        return parStressScenarioData_;
    }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& parStressPricingEngine() const {
        return parStressPricingEngine_;
    }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& parStressSensitivityScenarioData() const {
        return parStressSensitivityScenarioData_;
    }

    double parStressLowerBoundCapFloorVolatility() const { return parStressLowerBoundCapFloorVolatility_; }
    double parStressUpperBoundCapFloorVolatility() const { return parStressUpperBoundCapFloorVolatility_; }
    double parStressLowerBoundSurvivalProb() const { return parStressLowerBoundSurvivalProb_; }
    double parStressUpperBoundSurvivalProb() const { return parStressUpperBoundSurvivalProb_; }
    double parStressLowerBoundRatesDiscountFactor() const { return parStressLowerBoundRatesDiscountFactor_; }
    double parStressUpperBoundRatesDiscountFactor() const { return parStressUpperBoundRatesDiscountFactor_; }
    double parStressAccurary() const { return parStressAccurary_; };

    /*************************************
     * XVA Sensitivity
     *************************************/

    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& xvaSensiSimMarketParams() const {
        return xvaSensiSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& xvaSensiScenarioData() const {
        return xvaSensiScenarioData_;
    }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& xvaSensiPricingEngine() const { return xvaSensiPricingEngine_; }

    /****************************
     * Getters for zero to par shift
     ****************************/
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& zeroToParShiftSimMarketParams() const { return zeroToParShiftSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& zeroToParShiftScenarioData() const { return zeroToParShiftScenarioData_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& zeroToParShiftPricingEngine() const { return zeroToParShiftPricingEngine_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& zeroToParShiftSensitivityScenarioData() const {
        return zeroToParShiftSensitivityScenarioData_;
    }

    /*************************************
     * List of analytics that shall be run
     *************************************/
    const std::set<std::string>& analytics() const { return analytics_; }

    virtual void loadParameters(){}
    virtual void writeOutParameters(){}

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
    QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager> refDataManager_;
    QuantLib::ext::shared_ptr<ore::data::Conventions> conventions_;
    QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig> iborFallbackConfig_;
    CurveConfigurationsManager curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> pricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_, useCounterpartyOriginalPortfolio_;
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
    bool mporOverlappingPeriods_ = true;
    QuantLib::Calendar mporCalendar_;
    bool mporForward_ = true;

    /**************
     * NPV analytic
     *************/
    bool outputAdditionalResults_ = false;
    std::size_t additionalResultsReportPrecision_ = 6;
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
    bool optimiseRiskFactors_ = false;
    bool outputJacobi_ = false;
    bool alignPillars_ = false;
    bool useSensiSpreadedTermStructures_ = true;
    QuantLib::Real sensiThreshold_ = 1e-6;
    bool sensiRecalibrateModels_ = true;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> sensiSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> sensiPricingEngine_;
    // QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> sensiTodaysMarketParams_;

    /**********************
     * SCENARIO analytic
     **********************/
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> scenarioSimMarketParams_;
    std::string scenarioOutputFile_;

    /*****************
     * STRESS analytic
     *****************/
    QuantLib::Real stressThreshold_ = 0.0;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> stressSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> stressScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> stressSensitivityScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> stressPricingEngine_;
    bool stressOptimiseRiskFactors_ = false;
    double stressLowerBoundCapFloorVolatility_;
    double stressUpperBoundCapFloorVolatility_;
    double stressLowerBoundSurvivalProb_;
    double stressUpperBoundSurvivalProb_;
    double stressLowerBoundRatesDiscountFactor_;
    double stressUpperBoundRatesDiscountFactor_;
    double stressAccurary_;

    /*****************
     * VAR analytics
     *****************/
    bool salvageCovariance_ = false;
    std::vector<Real> varQuantiles_;
    bool varBreakDown_ = false;
    std::string portfolioFilter_;
    // Delta, DeltaGammaNormal, MonteCarlo, Cornish-Fisher, Saddlepoint 
    std::string varMethod_ = "DeltaGammaNormal";
    Size mcVarSamples_ = 1000000;
    long mcVarSeed_ = 42;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarianceData_;
    QuantLib::ext::shared_ptr<SensitivityStream> sensitivityStream_;
    std::string benchmarkVarPeriod_;
    QuantLib::ext::shared_ptr<HistoricalScenarioReader> historicalScenarioReader_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> histVarSimMarketParams_;
    std::string baseScenarioLoc_;
    bool outputHistoricalScenarios_ = false;

    /*******************
     * EXPOSURE analytic
     *******************/
    bool salvageCorrelationMatrix_ = false;
    bool amc_ = false;
    bool amcCg_ = false;
    bool xvaCgBumpSensis_ = false;
    bool xvaCgUseExternalComputeDevice_ = false;
    bool xvaCgExternalDeviceCompatibilityMode_ = false;
    bool xvaCgUseDoublePrecisionForExternalCalculation_ = false;
    string xvaCgExternalComputeDevice_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaCgSensiScenarioData_;
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
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> exposureSimMarketParams_;
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::shared_ptr<CrossAssetModelData> crossAssetModelData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> simulationPricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> amcPricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::NettingSetManager> nettingSetManager_;
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances_;
    bool exposureProfiles_ = true;
    bool exposureProfilesByTrade_ = true;
    Real pfeQuantile_ = 0.95;
    bool fullInitialCollateralisation_ = false;
    std::string collateralCalculationType_ = "NoLag";
    std::string exposureAllocationMethod_ = "None";
    Real marginalAllocationLimit_ = 1.0;
    // intermediate results of the exposure simulation, before aggregation
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> mktCube_;
    Real simulationBootstrapTolerance_ = 0.0001;

    /**************
     * XVA analytic
     **************/
    std::string xvaBaseCurrency_ = "";
    bool loadCube_ = false;
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
    QuantLib::ext::shared_ptr<CreditSimulationParameters> creditSimulationParameters_;
    std::string creditMigrationOutputFiles_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> xvaStressSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> xvaStressScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaStressSensitivityScenarioData_;
    bool xvaStressWriteCubes_ = false;
    /***************
     * SIMM analytic
     ***************/
    std::string simmVersion_;
    ore::analytics::Crif crif_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmBasicNameMapper> simmNameMapper_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper> simmBucketMapper_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmCalibrationData> simmCalibrationData_;
    std::string simmCalculationCurrencyCall_ = "";
    std::string simmCalculationCurrencyPost_ = "";
    std::string simmResultCurrency_ = "";
    std::string simmReportingCurrency_ = "";
    bool enforceIMRegulations_ = false;
    bool useSimmParameters_ = true;
    bool writeSimmIntermediateReports_ = true;

    /***************
     * Zero to Par Conversion analytic
     ***************/
    bool parConversionXbsParConversion_ = false;
    bool parConversionOutputJacobi_ = false;
    bool parConversionAlignPillars_ = false;
    QuantLib::Real parConversionThreshold_ = 1e-6;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> parConversionSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> parConversionScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> parConversionPricingEngine_;
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

    /*****************
     * PAR STRESS CONVERSION analytic
     *****************/
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> parStressSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> parStressScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> parStressSensitivityScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> parStressPricingEngine_;
    double parStressLowerBoundCapFloorVolatility_;
    double parStressUpperBoundCapFloorVolatility_;
    double parStressLowerBoundSurvivalProb_;
    double parStressUpperBoundSurvivalProb_;
    double parStressLowerBoundRatesDiscountFactor_;
    double parStressUpperBoundRatesDiscountFactor_;
    double parStressAccurary_;

    /*****************
     * ZERO TO PAR SHIFT CONVERSION analytic
     *****************/
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> zeroToParShiftSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> zeroToParShiftScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> zeroToParShiftSensitivityScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> zeroToParShiftPricingEngine_;

    /*****************
     * XVA Sensitivity analytic
     *****************/
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> xvaSensiSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaSensiScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> xvaSensiPricingEngine_;
};

inline const std::string& InputParameters::marketConfig(const std::string& context) {
    auto it = marketConfigs_.find(context);
    return (it != marketConfigs_.end() ? it->second : Market::defaultConfiguration);
}
std::vector<std::string> getFileNames(const std::string& fileString, const std::filesystem::path& path);
    
//! Traditional ORE input via ore.xml and various files, output into files
class OutputParameters {
public:
    OutputParameters(const QuantLib::ext::shared_ptr<Parameters>& params);
    //! map internal report name to the configured external file name
    const std::map<std::string, std::string>& fileNameMap() { return fileNameMap_; }
    std::string outputFileName(const std::string& internalName, const std::string& suffix);
    
private:
    std::map<std::string, std::string> fileNameMap_;    
    std::string npvOutputFileName_;
    std::string cashflowOutputFileName_;
    std::string curvesOutputFileName_;
    std::string scenarioDumpFileName_;
    std::string scenarioOutputName_;
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
    std::string xvaStressTestFileName_;
    std::string stressZeroScenarioDataFileName_;
    std::string varFileName_;
    std::string parConversionOutputFileName_;
    std::string parConversionJacobiFileName_;
    std::string parConversionJacobiInverseFileName_;
    std::string pnlOutputFileName_;
    std::string parStressTestConversionFile_;
    std::string pnlExplainOutputFileName_;
    std::string zeroToParShiftFile_;
};

} // namespace analytics
} // namespace ore
