/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef orea_app_i
#define orea_app_i

%include orea_scenario.i
%include ored_market.i
%include ored_reports.i
%include ored_curveconfigurations.i
%include ored_referencedatamanager.i
%include ored_iborfallbackconfig.i
%include ored_log.i
%include ored_portfolio.i
%include std_map.i
%include std_set.i
%include std_filesystem.i
%include tuple.i

%{
using ore::analytics::Parameters;
using ore::analytics::OREApp;
using ore::analytics::Analytic;
using ore::analytics::AnalyticsManager;
using ore::analytics::InputParameters;
using ore::analytics::NPVCube;
using ore::analytics::AggregationScenarioData;
using ore::analytics::MarketDataLoaderImpl;
using ore::analytics::MarketDataLoader;
using ore::analytics::MarketDataInMemoryLoader;
using ore::analytics::MarketCalibrationReport;
using ore::analytics::DummyMarketDataLoader;
using ore::analytics::PortfolioAnalyser;
using ore::analytics::ReturnConfiguration;
using ore::analytics::ScenarioSimMarketParameters;
using ore::analytics::SensitivityScenarioData;
using ore::analytics::StressTestScenarioData;;
using ore::analytics::ScenarioGeneratorData;
using ore::data::Portfolio;
using ore::data::MarketImpl;
using ore::data::PlainInMemoryReport;
using ore::data::TodaysMarketParameters;
using ore::data::ReferenceDatum;
using ore::data::ReferenceDataManager;
using ore::data::BasicReferenceDataManager;
using ore::data::CurveConfigurations;
using ore::data::EngineData;
using ore::data::IborFallbackConfig;
using ore::data::Report;
using ore::data::AssetClass;
using ore::data::MarketObject;
using ore::data::NettingSetManager;
using ore::data::CollateralBalances;
using ore::data::MporCashFlowMode;
%}

namespace std {
    %template(StringSet) set<string>;
    %template(IntSet) set<int>;
    %template(UnsignedIntSet) set<unsigned int>;
    %template() pair<Date, string>;
    %template(DateStringPairVector) vector<pair<Date, string>>;
    %template(SizeVector) vector<QuantLib::Size>;
    %template(MarketObjectMap) map<enum MarketObject, set<string>>;
    %template(AllMarketObjectMap) map<string, map<enum MarketObject, set<string>>>;
}

// ORE Analytics

%shared_ptr(Parameters)
class Parameters {
  public:
    Parameters();
    void clear();
    void fromFile(const std::string& name);
    bool hasGroup(const std::string& groupName) const;
    bool has(const std::string& groupName, const std::string& paramName) const;
    ext::any get(const std::string& groupName, const std::string& paramName) const;
    std::string getString(const std::string& groupName, const std::string& paramName) const;
    template <class T> T getParameter(const string& groupName, const string& paramName, bool fail = true);
    //TODO: add this function to ORE, then add here
    //void add(const std::string& groupName, const std::string& paramName, const std::string& paramValue);
};

%shared_ptr(IborFallbackConfig)
%shared_ptr(InputParameters)
class InputParameters {
public:
    
    void setParameter(std::string analytic, std::string parameter, std::string val);

    // Getters, to be continued
    const QuantLib::Date& asof();
    const ext::shared_ptr<Portfolio>& portfolio();
    Size nThreads();
    CurveConfigurationsManager& curveConfigs();
    const ext::shared_ptr<TodaysMarketParameters>& todaysMarketParams() const;
    const std::string& marketDataLoaderOutput();
    const ext::shared_ptr<BasicReferenceDataManager>& refDataManager() const;
    const ext::shared_ptr<EngineData>& pricingEngine() const;
    const ext::shared_ptr<Conventions>& conventions() const;
    const ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig() const;
    const ext::shared_ptr<ore::data::BaselTrafficLightData>& baselTrafficLightConfig() const;
    const ext::shared_ptr<ore::data::CurrencyConfig>& currencyConfigs();
    const ext::shared_ptr<ore::data::CalendarAdjustmentConfig>& calendarAdjustmentConfigs();
    const ext::shared_ptr<CurveConfigurations>& curveConfig(const std::string& s = std::string()) const;
    QuantLib::Date mporDate();
        
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& stressSimMarketParams() const;
    const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressScenarioData() const;
    const QuantLib::ext::shared_ptr<EngineData>& stressPricingEngine() const;
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& stressSensitivityScenarioData() const;

    // and Setters
    void setAsOfDate(const std::string& s); 
    void setResultsPath(const std::string& s);
    void setBaseCurrency(const std::string& s);
    void setDiscountIndex(const std::string& discountIndex);
    void setContinueOnError(bool b);
    void setAllowModelBuilderFallbacks(bool b);
    void setLazyMarketBuilding(bool b);
    void setBuildFailedTrades(bool b);
    void setObservationModel(const std::string& s);
    void setImplyTodaysFixings(bool b);
    void setFixingCutOffDate(Date d);
    void setUseAtParCouponsCurves(bool b);
    void setUseAtParCouponsTrades(bool b);
    void setEnrichIndexFixings(bool b);
    void setIgnoreFixingLead(Size i);
    void setIgnoreFixingLag(Size i);
    void setIncludeTodaysCashFlows(bool b);
    void setIncludeReferenceDateEvents(bool b);
    void setMarketConfig(const std::string& config, const std::string& context); 
    void setResultsPath(std::filesystem::path resultsPath);
    void setRefDataManager(const ext::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager);
    void setBaselTrafficLight(const ext::shared_ptr<ore::data::BaselTrafficLightData>& baselTrafficLight);
    void setTodaysMarketParams(const ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams);    
    void setTodaysMarketParamsFromFile(const std::string& fileName);
    void setSensitivityScenarioData(
        const ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData);
    void setRefDataManager(const std::string& xml);
    void setRefDataManagerFromFile(const std::string& fileName);
    void setScriptLibrary(const std::string& xml);
    void setScriptLibraryFromFile(const std::string& fileName);
    void setConventions(const std::string& xml);
    void setConventions(const ext::shared_ptr<Conventions>& convs);    
    void setConventionsFromFile(const std::string& fileName);
    void setMporConventions(const std::string& xml);
    void setIborFallbackConfig(const std::string& xml);
    void setIborFallbackConfigFromFile(const std::string& fileName);
    void setCurveConfigs(const std::string& xml, std::string id = std::string());
    void setCurveConfigs(const ext::shared_ptr<CurveConfigurations>& cc, std::string id = std::string());
    void setCurveConfigsFromFile(const std::string& fileName, std::string id = std::string());
    void setCalendarAdjustment(const std::string& xml);
    void setCalendarAdjustmentFromFile(const std::string& fileName);
    void setCurrencyConfig(const std::string& xml);    
    void setCurrencyConfigFromFile(const std::string& fileName);
    void setPricingEngine(const std::string& xml);
    void setPricingEngine(const ext::shared_ptr<EngineData>& ed);
    void setPricingEngineFromFile(const std::string& fileName);
    void setTodaysMarketParams(const std::string& xml);
    void setTodaysMarketParamsFromFile(const std::string& fileName);
    void setPortfolio(const std::string& xml);
    void setPortfolio(const ext::shared_ptr<Portfolio>& portfolio);
    void setPortfolioFromFile(const std::string& fileNameString, const std::filesystem::path& inputPath);
    void setMporPortfolio(const std::string& xml);
    void setMporPortfolioFromFile(const std::string& fileNameString, const std::filesystem::path& inputPath); 
    void setMarketConfigs(const std::map<std::string, std::string>& m);
    void setThreads(int i);
    void setEntireMarket(bool b);
    void setAllFixings(bool b);
    void setEomInflationFixings(bool b);
    void setUseMarketDataFixings(bool b);
    void setIborFallbackOverride(bool b);
    void setReportNaString(const std::string& s);
    void setCsvQuoteChar(const char& c);
    void setCsvSeparator(const char& c);
    void setCsvCommentCharacter(const char& c);
    void setDryRun(bool b);
    void setMporDate(const QuantLib::Date& d);
    void setMporDays(Size s);
    void setMporCalendar(const std::string& s); 
    void setMporForward(bool b);
    void setMporOverlappingPeriods(bool b);
    void setMarketDataLoaderOutput(const std::string& s);
    // Setters for npv analytics
    void setOutputAdditionalResults(bool b);
    void setAdditionalResultsReportPrecision(std::size_t p);
    // Setters for cashflows
    void setIncludePastCashflows(bool b);
    // Setters for curves/markets
    void setOutputCurves(bool b);
    void setOutputTodaysMarketCalibration(bool b);
    void setTodaysMarketCalibrationPrecision(std::size_t p);
    void setCurvesMarketConfig(const std::string& s);
    void setCurvesGrid(const std::string& s);
    // Setters for sensi analytics
    void setXbsParConversion(bool b);
    void setParSensi(bool b);
    void setComputeTheta(bool b);
    void setThetaPeriod(Period b);
    void setOptimiseRiskFactors(bool b);
    void setAlignPillars(bool b);
    void setOutputJacobi(bool b);
    void setUseSensiSpreadedTermStructures(bool b);
    void setSensiThreshold(Real r);
    void setSensiRecalibrateModels(bool b);
    void setSensiLaxFxConversion(bool b);
    void setSensiDecomposition(bool b);
    void setSensiSimMarketParams(const std::string& xml);
    void setSensiSimMarketParamsFromFile(const std::string& fileName);
    void setSensiScenarioData(const std::string& xml);
    void setSensiScenarioDataFromFile(const std::string& fileName);
    void setSensiPricingEngine(const std::string& xml);
    void setSensiPricingEngine(const ext::shared_ptr<EngineData>& engineData);
    void setSensiPricingEngineFromFile(const std::string& fileName);
    void setSensiOutputPrecision(Size p);
    // Setters for scenario
    void setScenarioSimMarketParams(const std::string& xml);
    void setScenarioSimMarketParamsFromFile(const std::string& fileName);
    void setScenarioOutputFile(const std::string& filename);
    // Setters for stress testing
    void setStressThreshold(Real r);
    void setStressOptimiseRiskFactors(bool optimise);
    void setStressSimMarketParams(const std::string& xml); 
    void setStressSimMarketParamsFromFile(const std::string& fileName); 
    void setStressScenarioData(const std::string& xml);
    void setStressScenarioData(const ext::shared_ptr<StressTestScenarioData>& stressScenarioData);
    void setStressScenarioDataFromFile(const std::string& fileName);
    void setStressPricingEngine(const std::string& xml); 
    void setStressPricingEngine(const ext::shared_ptr<EngineData>& engineData);
    void setStressPricingEngineFromFile(const std::string& fileName);
    void setStressSensitivityScenarioData(const std::string& xml);
    void setStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setStressLowerBoundCapFloorVolatility(const double value);
    void setStressUpperBoundCapFloorVolatility(const double value);
    void setStressLowerBoundSurvivalProb(const double value);
    void setStressUpperBoundSurvivalProb(const double value);
    void setStressLowerBoundRatesDiscountFactor(const double value);
    void setStressUpperBoundRatesDiscountFactor(const double value);
    void setStressAccurary(const double value);
    void setStressPrecision(const Size value);
    void setStressGenerateCashflows(const bool b);
    // Setters for VaR
    void setVarQuantiles(const std::string& s); // parse to vector<Real>
    void setVarBreakDown(bool b);
    void setPortfolioFilter(const std::string& s);
    void setVarSalvagingAlgorithm(SalvagingAlgorithm::Type vsa);
    void setVarMethod(const std::string& s);
    void setMcVarSamples(Size s);
    void setMcVarSeed(long l);
    void setCovarianceData(ore::data::CSVReader& reader);
    void setCovarianceDataFromBuffer(const std::string& xml); 
    void setSensitivityStream(const std::string& s);
    void setLookbackPeriod(const std::string& s);
    void setBenchmarkVarPeriod(const std::string& period);
    void setScenarioReader(const std::string& fileName);
    void setHistVarSimMarketParams(const std::string& s);
    void setHistVarSimMarketParamsFromFile(const std::string& fileName);
    void setOutputHistoricalScenarios(const bool b);
    void setTradePnl(bool b);
    void setRiskFactorBreakdown(bool b);
    void setIncludeExpectedShortfall(bool b);
    void setHistVarReturnConfiguration(const QuantLib::ext::shared_ptr<ReturnConfiguration>& rc);
    // Setters for Correlation
    void setCorrelationMethod(const std::string& s);

    void setSimmVersion(const std::string& s);
    void setCrifFromBuffer(const std::string& csvBuffer,
                           char eol = '\n', char delim = ',', char quoteChar = '\0', char escapeChar = '\\');
    void setSimmCalculationCurrencyCall(const std::string& s);
    void setSimmCalculationCurrencyPost(const std::string& s);
    void setSimmResultCurrency(const std::string& s);
    void setSimmReportingCurrency(const std::string& s);

    // Setters for exposure simulation
    void setExposureIncludeTodaysCashFlows(bool b);
    void setExposureIncludeReferenceDateEvents(bool b);
    void setAmc(bool b);
    void setAmcCg(const std::string& mode);
    void setXvaCgBumpSensis(bool b);
    void setXvaCgDynamicIM(bool b);
    void setXvaCgDynamicIMStepSize(Size s);
    void setXvaCgRegressionOrder(Size r);
    void setXvaCgRegressionVarianceCutoff(double c);
    void setXvaCgRegressionOrderDynamicIm(Size r);
    void setXvaCgRegressionVarianceCutoffDynamicIm(double c);
    void setXvaCgTradeLevelBreakdown(bool b);
    void setXvaCgRegressionReportTimeStepsDynamicIM(const std::vector<Size>& s);
    void setXvaCgUseRedBlocks(bool b);
    void setXvaCgUseExternalComputeDevice(bool b);
    void setXvaCgExternalDeviceCompatibilityMode(bool b);
    void setXvaCgUseDoublePrecisionForExternalCalculation(bool b);
    void setXvaCgExternalComputeDevice(string s);
    void setXvaCgUsePythonIntegration(bool b);
    void setXvaCgUsePythonIntegrationDynamicIm(bool b);
    void setXvaCgSensiScenarioData(const std::string& xml);    
    void setAmcPathDataInput(const std::string& s);
    void setAmcPathDataOutput(const std::string& s);
    void setAmcIndividualTrainingInput(bool b);
    void setAmcIndividualTrainingOutput(bool b);
    void setExposureBaseCurrency(const std::string& s);
    void setExposureObservationModel(const std::string& s);
    void setNettingSetId(const std::string& s);
    void setStoreFlows(bool b);
    void setStoreExerciseValues(bool b);
    void setStoreSensis(bool b);
    void setAllowPartialScenarios(bool b);
    void setStoreCreditStateNPVs(Size states);
    void setStoreSurvivalProbabilities(bool b);
    void setWriteCube(bool b);
    void setWriteScenarios(bool b);
    void setExposureSimMarketParams(const std::string& xml);
    void setExposureSimMarketParams(const ext::shared_ptr<ScenarioSimMarketParameters>& xml);
    void setScenarioGeneratorData(const std::string& xml);
    void setScenarioGeneratorData(const ext::shared_ptr<ScenarioGeneratorData>& xml);
    void setCrossAssetModelData(const std::string& xml);
    void setCrossAssetModelData(const ext::shared_ptr<CrossAssetModelData>& xml);
    void setSimulationPricingEngine(const std::string& xml); 
    void setSimulationPricingEngine(const ext::shared_ptr<EngineData>& engineData);
    void setAmcPricingEngine(const std::string& xml);
    void setAmcPricingEngine(const ext::shared_ptr<EngineData>& engineData);
    void setAmcCgPricingEngine(const std::string& xml);
    void setAmcCgPricingEngine(const ext::shared_ptr<EngineData>& engineData);
    void setNettingSetManager(const std::string& xml);
    void setNettingSetManager(const ext::shared_ptr<NettingSetManager>& xml);
    void setWriteCubeFile(bool b);
    void setWriteRawCubeFile(bool b);
    void setWriteNetCubeFile(bool b);
    // TODO: load from XML
    // void setCounterpartyManager(const std::string& xml);
    void setCollateralBalances(const std::string& xml);
    void setCollateralBalances(const ext::shared_ptr<CollateralBalances>& xml);
    void setReportBufferSize(Size s);

    // Setters for calibration
    void setCalibrationModel(const std::string& s);
    void setHwCalibrationMode(const std::string& s);
    void setPcaCalibration(bool b);
    void setMeanReversionCalibration(bool b);
    void setForeignCurrencies(const std::string& s);
    void setCurveTenors(const std::string& s);
    void setScenarioInputFile(const std::string& fileName);
    void setStartDate(const Date& d);
    void setEndDate(const Date& d);
    void setUseForwardOrZeroRate(const std::string& s);
    void setLambda(Real r);
    void setVarianceRetained(Real r);
    void setPcaInputFiles(const std::string& fileName, const std::filesystem::path& inputPath);
    void setBasisFunctionNumber(Size s);
    void setKappaUpperBound(Real r);
    void setHaltonMaxGuess(Size s);
    void setPcaOutputFileName(const std::string& fileName);
    void setMeanReversionOutputFileName(const std::string& fileName);

    // Setters for xva
    void setXvaUseDoublePrecisionCubes(const bool b);
    void setXvaBaseCurrency(const std::string& s);
    void setCube(const QuantLib::ext::shared_ptr<NPVCube>& file);
    void setMarketCube(const QuantLib::ext::shared_ptr<AggregationScenarioData>& file);
    void setFlipViewXVA(bool b);
    void setMporCashFlowMode(const MporCashFlowMode m);
    void setFullInitialCollateralisation(bool b);
    void setExposureProfiles(bool b);
    void setExposureProfilesByTrade(bool b);
    void setExposureProfilesUseCloseOutValues(bool b);
    void setWriteIndividualExposureReports(bool b);
    void setPfeQuantile(Real r);
    void setCollateralCalculationType(const std::string& s);
    void setExposureAllocationMethod(const std::string& s);
    void setMarginalAllocationLimit(Real r);
    void setExerciseNextBreak(bool b);
    void setCvaAnalytic(bool b);
    void setDvaAnalytic(bool b);
    void setFvaAnalytic(bool b);
    void setColvaAnalytic(bool b);
    void setCollateralFloorAnalytic(bool b);
    void setDimAnalytic(bool b);
    void setDimModel(const std::string& s);
    void setMvaAnalytic(bool b);
    void setKvaAnalytic(bool b);
    void setDynamicCredit(bool b);
    void setCvaSensi(bool b);
    void setCvaSensiShiftSize(Real r);
    void setDvaName(const std::string& s);
    // FIXME: remove this from the base class?
    void setRawCubeOutputFile(const std::string& s);
    void setRawCubeOutput(bool b);
    void setNetCubeOutput(bool b);
    void setNetCubeOutputFile(const std::string& s);
    // funding value adjustment details
    void setFvaBorrowingCurve(const std::string& s);
    void setFvaLendingCurve(const std::string& s);
    void setFlipViewBorrowingCurvePostfix(const std::string& s);
    void setFlipViewLendingCurvePostfix(const std::string& s);
    void setDimQuantile(Real r);
    void setDimHorizonCalendarDays(Size s);
    void setDimRegressionOrder(Size s);
    void setDimRegressors(const std::string& s); 
    void setDimOutputGridPoints(const std::string& s); 
    void setDimDistributionCoveredStdDevs(Real r);
    void setDimDistributionGridSize(Size n);
    void setDimLocalRegressionEvaluations(Size s);
    void setDimLocalRegressionBandwidth(Real r);
    // capital value adjustment details
    void setKvaCapitalDiscountRate(Real r);
    void setKvaAlpha(Real r);
    void setKvaRegAdjustment(Real r);
    void setKvaCapitalHurdle(Real r);
    void setKvaOurPdFloor(Real r);
    void setKvaTheirPdFloor(Real r);
    void setKvaOurCvaRiskWeight(Real r);
    void setKvaTheirCvaRiskWeight(Real r);
    void setfirstMporCollateralAdjustment(const bool constantInitialVm); 

    // Setters for Credit Simulation
    void setCreditMigrationAnalytic(bool b);
    void setCreditMigrationDistributionGrid(const std::vector<Real>& grid);
    void setCreditSimulationParametersFromBuffer(const std::string& xml);
    void setCreditMigrationOutputFiles(const std::string& s);

    // Setters for cashflow npv and dynamic backtesting
    void setCashflowHorizon(const std::string& s); 
    void setPortfolioFilterDate(const std::string& s);

    // Setters for xvaStress
    void setXvaStressSimMarketParams(const std::string& xml);
    void setXvaStressSimMarketParamsFromFile(const std::string& f);
    void setXvaStressScenarioData(const std::string& s);
    void setXvaStressScenarioDataFromFile(const std::string& s);
    void setXvaStressSensitivityScenarioData(const std::string& xml);
    void setXvaStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setXvaStressWriteCubes(const bool writeCubes);

    // Setters for sensitivityStress
    void setSensitivityStressSimMarketParams(const std::string& xml);
    void setSensitivityStressSimMarketParamsFromFile(const std::string& f);
    void setSensitivityStressScenarioData(const std::string& s);
    void setSensitivityStressScenarioDataFromFile(const std::string& s);
    void setSensitivityStressSensitivityScenarioData(const std::string& xml);
    void setSensitivityStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setSensitivityStressCalculateBaseScenario(const bool calcBaseScenario);

    // Setters for xvaSensi
    void setXvaSensiSimMarketParams(const std::string& xml);
    void setXvaSensiSimMarketParamsFromFile(const std::string& fileName);
    void setXvaSensiScenarioData(const std::string& xml);
    void setXvaSensiScenarioDataFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const std::string& xml);
    void setXvaSensiPricingEngineFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData);
    void setXvaSensiParSensi(const bool parSensi);
    void setXvaSensiOutputJacobi(const bool outputJacobi);
    void setXvaSensiThreshold(const Real threshold);
    void setXvaSensiOutputPrecision(Size p);

    // Setters for SA-CVA
    void setSaCvaNetSensitivitiesFromFile(const std::string& fileName);
    void setCvaSensitivitiesFromFile(const std::string& fileName);
  
    // Setters for XVA Explain
    void setXvaExplainSimMarketParams(const std::string& xml);
    void setXvaExplainSimMarketParamsFromFile(const std::string& f);
    void setXvaExplainSensitivityScenarioData(const std::string& xml);
    void setXvaExplainSensitivityScenarioDataFromFile(const std::string& fileName);
    void setXvaExplainShiftThreshold(const double threshold);

    // Setters for ZeroToParSensiConversion
    void setParConversionXbsParConversion(bool b);
    void setParConversionAlignPillars(bool b);
    void setParConversionOutputJacobi(bool b);
    void setParConversionThreshold(Real r);
    void setParConversionSimMarketParams(const std::string& xml);
    void setParConversionSimMarketParamsFromFile(const std::string& fileName);
    void setParConversionScenarioData(const std::string& xml);
    void setParConversionScenarioDataFromFile(const std::string& fileName);
    void setParConversionPricingEngine(const std::string& xml);
    void setParConversionPricingEngineFromFile(const std::string& fileName);
    void setParConversionPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData);
    void setParConversionInputFile(const std::string& s);
    void setParConversionInputIdColumn(const std::string& s);
    void setParConversionInputRiskFactorColumn(const std::string& s);
    void setParConversionInputDeltaColumn(const std::string& s);
    void setParConversionInputCurrencyColumn(const std::string& s);
    void setParConversionInputBaseNpvColumn(const std::string& s);
    void setParConversionInputShiftSizeColumn(const std::string& s);

    // Setters for par stress conversion
    void setParStressSimMarketParams(const std::string& xml);
    void setParStressSimMarketParamsFromFile(const std::string& fileName);
    void setParStressScenarioData(const std::string& xml);
    void setParStressScenarioDataFromFile(const std::string& fileName);
    void setParStressPricingEngine(const std::string& xml);
    void setParStressPricingEngineFromFile(const std::string& fileName);
    void setParStressPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData);
    void setParStressSensitivityScenarioData(const std::string& xml);
    void setParStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setParStressLowerBoundCapFloorVolatility(const double value);
    void setParStressUpperBoundCapFloorVolatility(const double value);
    void setParStressLowerBoundSurvivalProb(const double value);
    void setParStressUpperBoundSurvivalProb(const double value);
    void setParStressLowerBoundRatesDiscountFactor(const double value);
    void setParStressUpperBoundRatesDiscountFactor(const double value);
    void setParStressAccurary(const double value);

    // Setters for zeroToParShift conversion
    void setZeroToParShiftSimMarketParams(const std::string& xml);
    void setZeroToParShiftSimMarketParamsFromFile(const std::string& fileName);
    void setZeroToParShiftScenarioData(const std::string& xml);
    void setZeroToParShiftScenarioDataFromFile(const std::string& fileName);
    void setZeroToParShiftPricingEngine(const std::string& xml);
    void setZeroToParShiftPricingEngineFromFile(const std::string& fileName);
    void setZeroToParShiftPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData);
    void setZeroToParShiftSensitivityScenarioData(const std::string& xml);
    void setZeroToParShiftSensitivityScenarioDataFromFile(const std::string& fileName);
    
    // Set list of analytics that shall be run
    void setAnalytics(const std::string& s);
    void insertAnalytic(const std::string& s); 
    void removeAnalytic(const std::string& s);

    void setPnlDateAdjustedRiskFactors(const std::string& s); 
    void setRiskFactorLevel(bool b);

    const std::map<std::string, std::string>&  marketConfigs() const; 
};

%shared_ptr(OREApp)
class OREApp {
  public:

    OREApp(const ext::shared_ptr<Parameters>& params, bool console = false);

    OREApp(const ext::shared_ptr<InputParameters>& inputs, const std::string& logFile = "", Size logLevel = 31,
           bool console = false, bool clearLog = true);
    
    void run();

    void run(const std::vector<std::string>& marketData,
             const std::vector<std::string>& fixingData);
                          
    void run(const ext::shared_ptr<MarketDataLoader> loader);

    void setupLog(QuantLib::Size mask = 15, const std::string& path = "", const std::string& file = "", 
                  const std::filesystem::path& logRootPath = std::filesystem::path(),
                  const std::string& progressLogFile = "", QuantLib::Size progressLogRotationSize = 100 * 1024 * 1024,
                  bool progressLogToConsole = false, const std::string& structuredLogFile = "",
                  QuantLib::Size structuredLogRotationSize = 100 * 1024 * 1024);

    ext::shared_ptr<InputParameters> getInputs();

    std::set<std::string> getAnalyticTypes();
    std::set<std::string> getSupportedAnalyticTypes();
    const ext::shared_ptr<Analytic>& getAnalytic(std::string type); 

    std::set<std::string> getReportNames();
    ext::shared_ptr<PlainInMemoryReport> getReport(std::string reportName);

    std::set<std::string> getCubeNames();
    ext::shared_ptr<NPVCube> getCube(std::string cubeName);

    std::set<std::string> getMarketCubeNames();
    ext::shared_ptr<AggregationScenarioData> getMarketCube(std::string cubeName);

    std::vector<std::string> getErrors();

    Real getRunTime();

    ext::shared_ptr<BufferLogger> getLogger(const std::string& name);
    std::vector<std::string>& getProgressLog();

    std::string version();
    std::string gitHash();

    void closeLog();
};

%shared_ptr(Analytic)
class Analytic {
 public:
    ext::shared_ptr<MarketImpl> getMarket() const;
    const ext::shared_ptr<Portfolio>& portfolio() const;
};

%shared_ptr(MarketDataLoader)
class MarketDataLoader {
public:
    MarketDataLoader(const ext::shared_ptr<InputParameters>& inputs, ext::shared_ptr<MarketDataLoaderImpl> impl)
        : inputs_(inputs), impl_(impl);

     virtual void populateLoader(const std::vector<ext::shared_ptr<TodaysMarketParameters>>& todaysMarketParameters,
        const std::set<Date>& loaderDates);

     virtual void populateLoader(const ext::shared_ptr<TodaysMarketParameters>& todaysMarketParameters,
                   const std::set<Date>& loaderDates);
};

%shared_ptr(AnalyticsManager)
class AnalyticsManager {
public:
    AnalyticsManager(const ext::shared_ptr<InputParameters>& inputs,
                     const ext::shared_ptr<MarketDataLoader>& marketDataLoader);
    void initialise();
    void runAnalytics(const ext::shared_ptr<MarketCalibrationReport>& marketCalibrationReport = nullptr);
};

%shared_ptr(MarketDataInMemoryLoader)
class MarketDataInMemoryLoader : public MarketDataLoader {
public: 
    MarketDataInMemoryLoader(const ext::shared_ptr<InputParameters>& inputs,
                             const std::vector<std::string>& marketData,
                             const std::vector<std::string>& fixingData);
};

%template(StringStringPair) std::pair<std::string, std::string>;
%template(PairOfStringsPairBool) std::pair<std::pair<std::string, std::string>, bool>;
%template(MarketFixingsVector) std::vector<std::pair<std::pair<std::string, std::string>, bool>>;

%shared_ptr(DummyMarketDataLoader)
class DummyMarketDataLoader : public MarketDataLoader {
public:
    DummyMarketDataLoader(const ext::shared_ptr<InputParameters>& inputs);

    std::vector<std::pair<std::string, std::string>> marketDataQuotes();
    std::vector<std::pair<std::pair<std::string, std::string>, bool>> marketFixings();
};


%shared_ptr(PortfolioAnalyser)
class PortfolioAnalyser {
public:
    PortfolioAnalyser(const ext::shared_ptr<Portfolio>& p,
                      const ext::shared_ptr<EngineData>& ed, const std::string& baseCcy,
                      const ext::shared_ptr<CurveConfigurations>& curveConfigs = nullptr,
                      const ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                      const ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig =
                          ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()),
                      bool recordSecuritySpecificCreditCurves = false, const std::string& baseCcyDiscountCurve = std::string());

    bool hasRiskFactorType(const RiskFactorKey::KeyType& riskFactorType) const;
    bool hasMarketObjectType(const MarketObject& marketObject) const;
    std::map<ore::analytics::RiskFactorKey::KeyType, std::set<std::string>> riskFactors() const;
    std::set<std::string> riskFactorNames(const RiskFactorKey::KeyType& riskFactorType) const;
    std::set<RiskFactorKey::KeyType> riskFactorTypes() const;
    std::map<MarketObject, std::set<std::string>>
    marketObjects(const QuantLib::ext::optional<std::string> config = QuantLib::ext::nullopt) const;
    std::map<std::string, std::map<MarketObject, std::set<std::string>>> allMarketObjects() const;
    std::set<std::string> swapindices() const;
    void riskFactorReport(Report& reportOut) const;
    void marketObjectReport(Report& reportOut) const;
    std::set<std::string> counterparties();
    Date maturity();
    const ext::shared_ptr<Portfolio>& portfolio() const;
    std::map<AssetClass, std::set<std::string>> underlyingIndices() const;
    void addDependencies();
};

#endif
