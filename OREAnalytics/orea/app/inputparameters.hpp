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
#include <orea/app/inputvariables.hpp>

#include <orea/engine/sacvasensitivityrecord.hpp>
#include <orea/engine/cvasensitivityrecord.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <filesystem>

namespace ore { 
namespace data { 

class Portfolio; 
class BasicReferenceDataManager; 
class Conventions; 
class IborFallbackConfig; 
class EngineData; 
class TodaysMarketParameters; 
class CounterpartyManager;
class BaselTrafficLightData;
class CalendarAdjustmentConfig;
class CurrencyConfig;
class CrossAssetModelData;
class CollateralBalances;
class NettingSetManager;
class ScriptLibraryData;
}
}

namespace ore {
namespace analytics {
using namespace ore::data;

class InputParameters;
class SimmBasicNameMapper;
class SimmBucketMapper;
class SensitivityStream;
class Crif;
class CreditSimulationParameters;
class SensitivityScenarioData;
class StressTestScenarioData;
class NPVCube;
class SimmCalibrationData;
class SimmConfiguration;
class SensitivityFileStream;
class ReturnConfiguration;

struct SetupVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
    
    QuantLib::Date asof_;
    std::string baseCurrency_;
    std::string discountIndex_;
    std::filesystem::path resultsPath_;
    std::filesystem::path inputPath_;
    std::string resultCurrency_;
    bool continueOnError_ = true;
    bool allowModelBuilderFallbacks_ = true;
    bool lazyMarketBuilding_ = true;
    bool buildFailedTrades_ = true;
    std::string observationModel_ = "None";
    bool implyTodaysFixings_ = false;
    Date fixingCutOffDate_;
    bool useAtParCouponsCurves_ = true;
    bool useAtParCouponsTrades_ = true;
    bool enrichIndexFixings_ = false;
    Size ignoreFixingLead_ = 0;
    Size ignoreFixingLag_ = 0;
    std::string reportNaString_ = "#N/A";
    bool dryRun_ = false;
    QuantLib::Size nThreads_ = 1;
    std::string marketDataLoaderOutput_;
    std::string marketDataLoaderInput_;
    bool outputAdditionalResults_ = false;
    QuantLib::Natural additionalResultsReportPrecision_ = 6;
    bool includePastCashflows_ = false;
    bool entireMarket_ = false;
    bool allFixings_ = false;
    bool eomInflationFixings_ = true;
    bool useMarketDataFixings_ = true;

    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager> refDataManager_;
    QuantLib::ext::shared_ptr<ore::data::BaselTrafficLightData> baselTrafficLightConfig_;
    QuantLib::ext::shared_ptr<ore::data::Conventions> conventions_, mporConventions_;
    QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig> iborFallbackConfig_;
    CurveConfigurationsManager curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::CalendarAdjustmentConfig> calendarAdjustment_;
    QuantLib::ext::shared_ptr<ore::data::CurrencyConfig> currencyConfig_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> pricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> counterpartyManager_;
    QuantLib::ext::shared_ptr<ore::data::ScriptLibraryData> scriptLibraryData_;
    bool iborFallbackOverride_ = false;
    char csvCommentCharacter_ = '#';
    char csvSeparator_ = ',';
    Size reportBufferSize_ = 0;

    QuantLib::Period thetaPeriod_ = QuantLib::Period(1, QuantLib::Days);
    bool computeTheta_ = false;
    
};

//! Base class for input data, also exposed via SWIG
class InputParameters : public QuantLib::ext::enable_shared_from_this<InputParameters> {
public:
    InputParameters();
    virtual ~InputParameters() {} 
    
    // load an object directly from Parameters if it exists
    template<class T> bool loadFromParameters(T& obj, const std::string& analytic, const std::string& param) {
        if (parameters_.hasGroup(analytic) && parameters_.has(analytic, param)) {
            try {
                obj = parameters_.getParameter<T>(analytic, param, false);
                return true;
            } catch (...) {
            }
        }
        return false;
    }

    template <class T>
    bool loadFromParameters(T& obj, const std::vector<std::string>& analytics, std::vector<std::string>& params) {
        for (const auto& a : analytics) {
            for (const auto& p : params) {
                try {
                    if (loadFromParameters<T>(obj, a, p))
                        return true;
                } catch (...) {
                }
            }
        }
        return false;
    }

    template <class T>
    bool loadFromParameters(T& obj, const std::vector<std::string>& analytics, const std::string& param) {
        auto params = std::vector<std::string>({param});
        return loadFromParameters<T>(obj, analytics, params);
    }

    template <class T>
    bool loadFromParameters(T& obj, const std::string& analytic, const std::vector<std::string>& params) {
        auto analytics = std::vector<std::string>({analytic});
        return loadFromParameters<T>(obj, analytics, params);
    }

    //! load and convert an object from a string for the given (analytic, param) pair
    template <class T>
    bool loadParameter(
        T& obj,
        const std::string& analytic, 
        const std::string& param, 
        const bool mandatory = false,
        std::function<T(const std::string&)> parser = [](auto const& s) { return s; }) {

        // first check if it exists directly in Parameters
        bool loaded = loadFromParameters<T>(obj, analytic, param);
        if (loaded)
			return true;

        // else check for a string and load correctly
        string str = loadParameterString(analytic, param, mandatory);
        if (str.empty() && !mandatory)
            return false;
        bool b = tryParse(str, obj, parser);
        if (!b && mandatory)
			QL_FAIL("InputParameters::loadParameter(): mandatory parameter (" + analytic + "," + param + ") parsing failed");
        return b;
    }

    // allow multiple analytic names - ie "pfe" or "xva" or parameter names - ie "curveconfigFile" or "curveconfig"
    template <class T>
    bool loadParameter(
        T& obj, const std::vector<std::string>& analytics, const std::vector<std::string>& params, const bool mandatory = false,
        std::function<T(const std::string&)> parser = [](auto const& s) { return s; }) {
        for (const auto& a : analytics) {
            for (const auto& p : params) {
                try {
                    if (loadParameter<T>(obj, a, p, false, parser))
                        return true;
                } catch (...) {
                }
            }
        }
        if (mandatory)
            QL_FAIL("InputParameters::loadParameter(): mandatory parameter (" + to_string(analytics) + "," + to_string(params) +
                    ") parsing failed");
        return false;
    }

    // allow multiple analytic names - ie "pfe" or "xva"
    template <class T>
    bool loadParameter(
        T& obj, const std::vector<std::string>& analytics, const std::string& param, const bool mandatory = false,
        std::function<T(const std::string&)> parser = [](auto const& s) { return s; }) {
        auto params = std::vector<std::string>({param});
        return loadParameter<T>(obj, analytics, params, false, parser);
    }

    // allow multiple parameter names - ie "curveconfigFile" or "curveconfig"
    template <class T>
    bool loadParameter(
        T& obj, const std::string& analytic, const std::vector<std::string>& params, const bool mandatory = false,
        std::function<T(const std::string&)> parser = [](auto const& s) { return s; }) {
        auto analytics = std::vector<std::string>({analytic});
        return loadParameter<T>(obj, analytics, params, false, parser);
    }

    //! load the XML object from an XML string for the given (analytic, param) pair
    template <class T, typename... Args>
    bool loadParameterXML(
        QuantLib::ext::shared_ptr<T>& obj, const std::string& analytic, 
            const std::string& param, const bool mandatory = false, Args... args) {

        string str;
        // first check if we have a parameter of correct type stored in the Parameters object
        if (parameters_.hasGroup(analytic) && parameters_.has(analytic, param)) {
            try {            
                obj = parameters_.getParameter<QuantLib::ext::shared_ptr<T>>(analytic, param, false);
                return true;
            } catch (...) {
            }

            try {
                str = parameters_.getString(analytic, param, false);
            } catch (...) {
            }
        }

        // first get the string provided if needed
        if (str.empty()) {
            str = loadParameterString(analytic, param, mandatory);
            if (str.empty()) {
                if (mandatory)
                    QL_FAIL("InputParameters::loadParameterXML(): mandatory parameter (" + analytic + "," + param +
                            ") could not be found");
                else
                    return false;
            }
        }

        if (!obj)
            obj = QuantLib::ext::make_shared<T>(args...);

        // if the string sarts with a '<' we assume it's an XML string and try to load directly from it, otherwise we
        // treat it as a file name or reference and try to load the XML string from it
        auto first = str.find_first_not_of(" \t\r\n");
        if (first != std::string::npos && str[first] == '<') {
            try {
                obj->fromXMLString(str);
                return true;
            } catch (...) {
            }
        }

        // else take the string and retrieve the XML based on the string
        vector<string> xmlStr;
        try {
            xmlStr = loadParameterXMLString(str);
        }
        catch (const std::exception& e) {
            LOG("InputParameters::loadParameterXML(): Failed loading parameter ("
                << analytic << "," << param << ") from XML string: " << str << " , error: " << +e.what());
            if (mandatory)
			    QL_FAIL("InputParameters::loadParameterXML(): mandatory parameter (" + analytic + "," + param +
            						") XML parsing failed, with error: " + e.what());
        }

        if (xmlStr.size() == 0)
            return false;

        TLOG("InputParameters::loadParameterXML(): loading parameter (" << analytic << "," << param
                                                                       << ") from XML string: " << xmlStr[0]);
        for (const auto& s : xmlStr) {            
            try {
                obj->fromXMLString(s);
            } catch (const std::exception& e) {
                LOG("InputParameters::loadParameterXML(): Failed loading parameter (" << analytic << "," << param
                                                                               << ") from XML string: " << s << " , error: " << + e.what());
                if (mandatory)
                    QL_FAIL("InputParameters::loadParameterXML(): mandatory parameter (" + analytic + "," + param +
                                ") parsing failed, with error: " + e.what());
            }
        }
        return true;
    }

    template <class T, typename... Args>
    bool loadParameterXML(QuantLib::ext::shared_ptr<T>& obj, const std::vector<std::string>& analytics,
                          const std::vector<std::string>& params, const bool mandatory = false, Args... args) {
        for (const auto& a : analytics) {
            for (const auto& p : params) {
                try {
                    if (loadParameterXML<T>(obj, a, p, false, args...))
                        return true;
                } catch (...) {
                }
            }
        }
        if (mandatory)
            QL_FAIL("InputParameters::loadParameter(): mandatory parameter (" + to_string(analytics) + "," + to_string(params) +
                    ") parsing failed");
        return false;
    }

    template <class T, typename... Args>
    bool loadParameterXML(QuantLib::ext::shared_ptr<T>& obj, const std::string& analytic, const std::vector<std::string>& params, const bool mandatory = false, Args... args) {
        auto analytics = std::vector<std::string>({analytic});
        return loadParameterXML<T>(obj, analytics, params, false, args...);
    }

    template <class T, typename... Args>
    bool loadParameterXML(QuantLib::ext::shared_ptr<T>& obj, const std::vector<std::string>& analytics,
                          const std::string& param, const bool mandatory = false, Args... args) {
        auto params = std::vector<std::string>({param});
        return loadParameterXML<T>(obj, analytics, params, false, args...);
    }

    virtual QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::string& analytic,
                                                                         const std::string& param,
                                                                         const Date& startDate = Date(),
                                                                         const Date& endDate = Date());
    QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::string& analytic,
                                                                 const std::vector<std::string>& params,
                                                                 const Date& startDate = Date(),
                                                                 const Date& endDate = Date());
    QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::vector<std::string>& analytics,
                                                                 const std::string& param,
                                                                 const Date& startDate = Date(),
                                                                 const Date& endDate = Date());
    QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::vector<std::string>& analytics,
                                                                 const std::vector<std::string>& params,
                                                                 const Date& startDate = Date(),
                                                                 const Date& endDate = Date());

    virtual QuantLib::ext::shared_ptr<SensitivityStream> loadSensitivityStream(const std::string& analytic, const std::string& param);
    QuantLib::ext::shared_ptr<SensitivityStream> loadSensitivityStream(const std::string& analytic, const std::vector<std::string>& params);
    QuantLib::ext::shared_ptr<SensitivityStream> loadSensitivityStream(const std::vector<std::string>& analytics, const std::string& param);
    QuantLib::ext::shared_ptr<SensitivityStream> loadSensitivityStream(const std::vector<std::string>& analytics, const std::vector<std::string>& params);
       
    //! virtual function to load a parameter string for the given (analytic, param) pair
    virtual std::string loadParameterString(const std::string& analytic, const std::string& param, bool mandatory);
        
    //! virtual function to load an XML string for the given (analytic, param) pair
    virtual std::vector<std::string> loadParameterXMLString(const string& rawStr) {
        return std::vector<std::string>({rawStr});
    };

    void setParameter(std::string analytic, std::string parameter, std::string val) { 
        parameters_.set(analytic, parameter, val);
    }

    // setters for backward compatibility, use setParameter directly when possible

    void setOutputCurves(bool b) { setParameter("npv", "outputCurves", to_string(b)); }
    void setCurvesMarketConfig(const std::string& s) { setParameter("curves", "configuration", s); };
    void setCurvesGrid(const std::string& s) { setParameter("curves", "grid", s); };
        
     /*********
     * Setters
     *********/
    void setResultsPath(std::filesystem::path resultsPath) { setupVariables_.resultsPath_ = resultsPath; }
    void setRefDataManager(const QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager) {
        setupVariables_.refDataManager_ = refDataManager;
    }
    void setBaselTrafficLight(const QuantLib::ext::shared_ptr<ore::data::BaselTrafficLightData>& baselTrafficLight) {
        setupVariables_.baselTrafficLightConfig_ = baselTrafficLight;
    }
    void setTodaysMarketParams(const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams) {
        setupVariables_.todaysMarketParams_ = todaysMarketParams;
    }
    void setSensitivityScenarioData(
        const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData) {
        sensiScenarioData_ = sensiScenarioData;
    }

    void setAsOfDate(const std::string& s); // parse to Date
    void setResultsPath(const std::string& s) { setupVariables_.resultsPath_ = s; }
    void setInputPath(const std::string& s) { setupVariables_.inputPath_ = s; }
    void setBaseCurrency(const std::string& s) { setupVariables_.baseCurrency_ = s; }
    void setDiscountIndex(const std::string& discountIndex) { setupVariables_.discountIndex_ = discountIndex; }
    void setContinueOnError(bool b) { setupVariables_.continueOnError_ = b; }
    void setAllowModelBuilderFallbacks(bool b) { setupVariables_.allowModelBuilderFallbacks_ = b; }
    void setLazyMarketBuilding(bool b) { setupVariables_.lazyMarketBuilding_ = b; }
    void setBuildFailedTrades(bool b) { setupVariables_.buildFailedTrades_ = b; }
    void setObservationModel(const std::string& s) { setupVariables_.observationModel_ = s; }
    void setImplyTodaysFixings(bool b) { setupVariables_.implyTodaysFixings_ = b; }
    void setFixingCutOffDate(Date d) { setupVariables_.fixingCutOffDate_ = d; }
    void setUseAtParCouponsCurves(bool b) { setupVariables_.useAtParCouponsCurves_ = b; }
    void setUseAtParCouponsTrades(bool b) { setupVariables_.useAtParCouponsTrades_ = b; }
    void setEnrichIndexFixings(bool b) { setupVariables_.enrichIndexFixings_ = b; }
    void setIgnoreFixingLead(Size i) { setupVariables_.ignoreFixingLead_ = i; }
    void setIgnoreFixingLag(Size i) { setupVariables_.ignoreFixingLag_ = i; }
    void setIncludeTodaysCashFlows(bool b) {
        Settings::instance().includeTodaysCashFlows() = b;
    }
    void setIncludeReferenceDateEvents(bool b) {
        Settings::instance().includeReferenceDateEvents() = b;
    }
    void setMarketConfig(const std::string& config, const std::string& context);
    void setRefDataManager(const std::string& xml);
    void setRefDataManagerFromFile(const std::string& fileName);
    void setScriptLibrary(const std::string& xml);
    void setScriptLibraryFromFile(const std::string& fileName);
    void setConventions(const std::string& xml);
    void setConventions(const QuantLib::ext::shared_ptr<Conventions>& convs);
    void setConventionsFromFile(const std::string& fileName);
    void setMporConventions(const std::string& xml);
    void setMporConventionsFromFile(const std::string& fileName);
    void setIborFallbackConfig(const std::string& xml);
    void setIborFallbackConfigFromFile(const std::string& fileName);
    void setBaselTrafficLightConfig(const std::string& xml);
    void setBaselTrafficLightFromFile(const std::string& fileName);
    void setCurveConfigs(const std::string& xml, std::string id = std::string());
    void setCurveConfigs(const QuantLib::ext::shared_ptr<CurveConfigurations>& cc, std::string id = std::string());
    void setCurveConfigsFromFile(const std::string& fileName, std::string id = std::string());
    void setPricingEngine(const std::string& xml);
    void setPricingEngineFromFile(const std::string& fileName);
    void setPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& ed);
    void setTodaysMarketParams(const std::string& xml);
    void setTodaysMarketParamsFromFile(const std::string& fileName);
    void setPortfolio(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    void setPortfolio(const std::string& xml); 
    void setPortfolioFromFile(const std::string& fileNameString, const std::filesystem::path& inputPath);
    void setMporPortfolio(const std::string& xml);
    void setMporPortfolioFromFile(const std::string& fileNameString, const std::filesystem::path& inputPath); 
    void setMarketConfigs(const std::map<std::string, std::string>& m);
    void setThreads(int i) { setupVariables_.nThreads_ = i; }
    void setEntireMarket(bool b) { setupVariables_.entireMarket_ = b; }
    void setAllFixings(bool b) { setupVariables_.allFixings_ = b; }
    void setEomInflationFixings(bool b) { setupVariables_.eomInflationFixings_ = b; }
    void setUseMarketDataFixings(bool b) { setupVariables_.useMarketDataFixings_ = b; }
    void setIborFallbackOverride(bool b) { setupVariables_.iborFallbackOverride_ = b; }
    void setReportNaString(const std::string& s) { setupVariables_.reportNaString_ = s; }
    void setCsvQuoteChar(const char& c){ csvQuoteChar_ = c; }
    void setCsvSeparator(const char& c) { setupVariables_.csvSeparator_ = c; }
    void setCsvCommentCharacter(const char& c) { setupVariables_.csvCommentCharacter_ = c; }
    void setDryRun(bool b) { setupVariables_.dryRun_ = b; }
    void setMporDays(Size s) {
        mporDays_ = s;
        parameters_.set("pnl", "mporDays", s);
        parameters_.set("var", "mporDays", s);
    }
    void setMporOverlappingPeriods(bool b) {
        mporOverlappingPeriods_ = b;
        parameters_.set("pnl", "mporOverlappingPeriods", b);
        parameters_.set("var", "mporOverlappingPeriods", b);
    }
    void setMporDate(const QuantLib::Date& d) { 
        mporDate_ = d;
        parameters_.set("pnl", "mporDate", d);
        parameters_.set("var", "mporDate", d);
    }
    void setMporCalendar(const std::string& s); 
    void setMporForward(bool b) { mporForward_ = b; }
    void setMarketDataLoaderOutput(const std::string& s) { setupVariables_.marketDataLoaderOutput_ = s; }
    void setMarketDataLoaderInput(const std::string& s) { setupVariables_.marketDataLoaderInput_ = s; }
    void setOutputAdditionalResults(bool b) { setupVariables_.outputAdditionalResults_ = b; }
    void setAdditionalResultsReportPrecision(std::size_t p) { setupVariables_.additionalResultsReportPrecision_ = p; }
    void setIncludePastCashflows(bool b) { setupVariables_.includePastCashflows_ = b; }

    // Setters for curves/markets

    void setOutputTodaysMarketCalibration(bool b) { outputTodaysMarketCalibration_ = b; }
    void setTodaysMarketCalibrationPrecision(std::size_t p) { todaysMarketCalibrationPrecision_ = p; }
    void setCalendarAdjustment(const std::string& xml);
    void setCalendarAdjustmentPtr(const QuantLib::ext::shared_ptr<CalendarAdjustmentConfig>& adjusts);
    void setCalendarAdjustmentFromFile(const std::string& fileName);
    void setCurrencyConfig(const std::string& xml);
    void setCurrencyConfigPtr(const QuantLib::ext::shared_ptr<CurrencyConfig>& config);
    void setCurrencyConfigFromFile(const std::string& fileName);

    // Setters for sensi analytics
    void setXbsParConversion(bool b) { xbsParConversion_ = b; }
    void setParSensi(bool b) { parSensi_ = b; }
    void setComputeTheta(bool b) {
        parameters_.set("sensitivity", "computeTheta", b);
        parameters_.set("simm", "computeTheta", b);
        parameters_.set("crif", "computeTheta", b);
    }
    void setThetaPeriod(Period b) { 
        parameters_.set("sensitivity", "thetaPeriod", b);
        parameters_.set("simm", "thetaPeriod", b);
        parameters_.set("crif", "thetaPeriod", b); 
    }
    void setOptimiseRiskFactors(bool b) { optimiseRiskFactors_ = b; }
    void setAlignPillars(bool b) { alignPillars_ = b; }
    void setOutputJacobi(bool b) { outputJacobi_ = b; }
    void setUseSensiSpreadedTermStructures(bool b) { useSensiSpreadedTermStructures_ = b; }
    void setSensiThreshold(Real r) { sensiThreshold_ = r; }
    void setSensiRecalibrateModels(bool b) { sensiRecalibrateModels_ = b; }
    void setSensiLaxFxConversion(bool b) { sensiLaxFxConversion_ = b; }
    void setSensiDecomposition(bool b) { sensiDecomposition_ = b; }
    void setSensiSimMarketParams(const std::string& xml);
    void setSensiSimMarketParamsFromFile(const std::string& fileName);
    void setSensiScenarioData(const std::string& xml);
    void setSensiScenarioDataFromFile(const std::string& fileName);
    void setSensiPricingEngine(const std::string& xml);
    void setSensiPricingEngineFromFile(const std::string& fileName);
    void setSensiPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        sensiPricingEngine_ = engineData;
    }
    void setSensiOutputPrecision(Size p) { sensiOutputPrecision_ = p; }

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
    void setStressScenarioData(const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressScenarioData);
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
    void setStressPrecision(const Size value) { stressPrecision_ = value; };
    void setStressGenerateCashflows(const bool b) { stressGenerateCashflows_ = b; }
    // Setters for VaR
    void setVarQuantiles(const std::string& s) { parameters_.set("var", "quantiles", s); }
    void setVarBreakDown(bool b) { parameters_.set("var", "breakdown", b); }
    void setPortfolioFilter(const std::string& s) { parameters_.set("var", "portfolioFilter", s); }
    void setVarSalvagingAlgorithm(SalvagingAlgorithm::Type vsa) { parameters_.set("parametricVar", "SalvagingAlgorithm", vsa); }
    void setVarMethod(const std::string& s) { parameters_.set("parametricVar", "method", s); }
    void setMcVarSamples(Size s) { parameters_.set("parametricVar", "mcSamples", s); }
    void setMcVarSeed(long l) { parameters_.set("parametricVar", "mcSeed", l); }
    void setCovarianceData(ore::data::CSVReader& reader) { parameters_.set("parametricVar", "covarianceInputFile", reader); }
    void setCovarianceDataFromBuffer(const std::string& xml) { parameters_.set("parametricVar", "covarianceInputFile", xml); }
    void setSensitivityStream(const std::string& s) { parameters_.set("var", "sensitivityInputFile", s); }
    void setLookbackPeriod(const std::string& s) { parameters_.set("var", "lookbackPeriod", s); }
    void setBenchmarkVarPeriod(const std::string& period) { 
        parameters_.set("var", "lookbackPeriod", period);
        benchmarkVarPeriod_ = period;
    }
    void setScenarioReader(const std::string& fileName);
    void setHistVarSimMarketParams(const std::string& s) { parameters_.set("historicalSimulationVar", "simulationConfigFile", s); }
    void setHistVarSimMarketParamsFromFile(const std::string& s) { parameters_.set("historicalSimulationVar", "simulationConfigFile", s); }
    void setOutputHistoricalScenarios(const bool b) { parameters_.set("var", "outputHistoricalScenarios", b); }
    void setTradePnl(bool b) { parameters_.set("historicalSimulationVar", "tradePnl", b); }
    void setRiskFactorBreakdown(bool b) { parameters_.set("historicalSimulationVar", "riskFactorBreakdown", b); }
    void setIncludeExpectedShortfall(bool b) { parameters_.set("historicalSimulationVar", "includeExpectedShortfall", b); }
    void setHistVarReturnConfiguration(const QuantLib::ext::shared_ptr<ReturnConfiguration>& rc) { parameters_.set("historicalSimulationVar", "returnConfigFile", rc); }

    // Setters for Correlation
    void setCorrelationMethod(const std::string& s) { parameters_.set("correlation", "correlationMethod", s); }

    // Setters for exposure simulation
    void setExposureIncludeTodaysCashFlows(bool b) { parameters_.set("simulation", "includeTodaysCashFlows", b); }
    void setExposureIncludeReferenceDateEvents(bool b) { parameters_.set("simulation", "includeReferenceDateEvents", b); }
    void setAmc(bool b) { parameters_.set("simulation", "amc", b); }
    void setAmcCg(XvaEngineCG::Mode b) { parameters_.set("simulation", "amcCg", b); }
    void setAmcCg(const std::string& mode) {
        parameters_.set("simulation", "amcCg", parseXvaEngineCgMode(mode));
    }
    void setXvaCgBumpSensis(bool b) { parameters_.set("simulation", "xvaCgBumpSensis", b); }
    void setXvaCgDynamicIM(bool b) { parameters_.set("simulation", "xvaCgDynamicIM", b); }
    void setXvaCgDynamicIMStepSize(Size s) { parameters_.set("simulation", "xvaCgDynamicIMStepSize", s); }
    void setXvaCgRegressionOrder(Size r) { parameters_.set("simulation", "xvaCgRegressionOrder", r); }
    void setXvaCgRegressionVarianceCutoff(double c) { parameters_.set("simulation", "xvaCgRegressionVarianceCutoff", c); }
    void setXvaCgRegressionOrderDynamicIm(Size r) { parameters_.set("simulation", "xvaCgRegressionOrderDynamicIm", r); }
    void setXvaCgRegressionVarianceCutoffDynamicIm(double c) { parameters_.set("simulation", "xvaCgRegressionVarianceCutoffDynamicIm", c); }
    void setXvaCgTradeLevelBreakdown(bool b) { parameters_.set("simulation", "xvaCgTradeLevelBreakDown", b); }
    void setXvaCgRegressionReportTimeStepsDynamicIM(const std::vector<Size>& s) { parameters_.set("simulation", "xvaCgRegressionReportTimeStepsDynamicIM", s); }
    void setXvaCgUseRedBlocks(bool b) { parameters_.set("simulation", "xvaCgUseRedBlocks", b); }
    void setXvaCgUseExternalComputeDevice(bool b) { parameters_.set("simulation", "xvaCgUseExternalComputeDevice", b); }
    void setXvaCgExternalDeviceCompatibilityMode(bool b) { parameters_.set("simulation", "xvaCgExternalDeviceCompatibilityMode", b); }
    void setXvaCgUseDoublePrecisionForExternalCalculation(bool b) { parameters_.set("simulation", "xvaCgUseDoublePrecisionForExternalCalculation", b); }
    void setXvaCgExternalComputeDevice(string s) { parameters_.set("simulation", "xvaCgExternalComputeDevice", s); }
    void setXvaCgUsePythonIntegration(bool b) { parameters_.set("simulation", "xvaCgUsePythonIntegration", b); }
    void setXvaCgUsePythonIntegrationDynamicIm(bool b) { parameters_.set("simulation", "xvaCgUsePythonIntegrationDynamicIm", b); }
    void setXvaCgSensiScenarioData(const std::string& xml) { parameters_.set("simulation", "xvaCgSensitivityConfigFile", xml); };
    void setAmcPathDataInput(const std::string& s) { parameters_.set("simulation", "amcPathDataInput", s); }
    void setAmcPathDataOutput(const std::string& s) { parameters_.set("simulation", "amcPathDataOutput", s); }
    void setAmcIndividualTrainingInput(bool b) { parameters_.set("simulation", "amcIndividualTrainingInput", b); }
    void setAmcIndividualTrainingOutput(bool b) { parameters_.set("simulation", "amcIndividualTrainingOutput", b); }
    void setExposureBaseCurrency(const std::string& s) { parameters_.set("simulation", "baseCurrency", s); };
    void setExposureObservationModel(const std::string& s) { parameters_.set("simulation", "observationModel", s); };
    void setNettingSetId(const std::string& s) { parameters_.set("simulation", "nettingSetId", s); };
    void setStoreFlows(bool b) { parameters_.set("simulation", "storeFlows", b); };
    void setStoreExerciseValues(bool b) { parameters_.set("simulation", "storeExerciseValues", b); };
    void setStoreSensis(bool b) { parameters_.set("simulation", "storeSensis", b); };
    void setAllowPartialScenarios(bool b) { parameters_.set("simulation", "allowPartialScenarios", b); };
    void setStoreCreditStateNPVs(Size states) { parameters_.set("simulation", "storeCreditStateNPVs", states); };
    void setStoreSurvivalProbabilities(bool b) { parameters_.set("simulation", "storeSurvivalProbabilities", b); };
    void setWriteCube(bool b) { parameters_.set("simulation", "writeCube", b); };
    void setWriteScenarios(bool b) { parameters_.set("simulation", "writeScenarios", b); };
    void setExposureSimMarketParams(const std::string& xml) { parameters_.set("simulation", "simulationConfigFile", xml); };
    void setExposureSimMarketParams(const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& xml) { parameters_.set("simulation", "simulationConfigFile", xml); };
    void setScenarioGeneratorData(const std::string& xml) { parameters_.set("simulation", "scenarioGeneratorData", xml); };
    void setScenarioGeneratorData(const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& xml) { parameters_.set("simulation", "scenarioGeneratorData", xml); };
    void setCrossAssetModelData(const std::string& xml) { parameters_.set("simulation", "crossAssetModelData", xml); };
    void setCrossAssetModelData(const QuantLib::ext::shared_ptr<CrossAssetModelData>& xml) { parameters_.set("simulation", "crossAssetModelData", xml); };
    void setSimulationPricingEngine(const std::string& xml) { parameters_.set("simulation", "pricingEnginesFile", xml); };
    void setSimulationPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) { parameters_.set("simulation", "pricingEnginesFile", engineData); };
    void setAmcPricingEngine(const std::string& xml) { parameters_.set("simulation", "amcPricingEnginesFile", xml); };
    void setAmcPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) { parameters_.set("simulation", "amcPricingEnginesFile", engineData); };
    void setAmcCgPricingEngine(const std::string& xml) { parameters_.set("simulation", "amcCgPricingEnginesFile", xml); };
    void setAmcCgPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) { parameters_.set("simulation", "amcCgPricingEnginesFile", engineData); };
    void setNettingSetManager(const std::string& xml) { parameters_.set("xva", "csaFile", xml); };
    void setNettingSetManager(const QuantLib::ext::shared_ptr<NettingSetManager>& xml) { parameters_.set("xva", "csaFile", xml); };
    void setWriteCubeFile(bool b) { parameters_.set("simulation", "writeCube", b); };
    void setWriteRawCubeFile(bool b) { parameters_.set("xva", "rawCubeOutput", b); };
    void setWriteNetCubeFile(bool b) { parameters_.set("xva", "netCubeOutput", b); };
    void setCollateralBalances(const std::string& xml) { parameters_.set("xva", "collateralBalancesFile", xml); };
    void setCollateralBalances(const QuantLib::ext::shared_ptr<CollateralBalances>& xml) { parameters_.set("xva", "collateralBalancesFile", xml); };
    void setReportBufferSize(Size s) { setupVariables_.reportBufferSize_ = s; }
    void setCounterpartyManager(const std::string& xml);
    void setCalibrationModel(const std::string& s) { parameters_.set("calibration", "model", s); }
    void setHwCalibrationMode(const std::string& s) { parameters_.set("calibration", "mode", s); }
    void setPcaCalibration(bool b) { parameters_.set("calibration", "pcaCalibration", b); }
    void setMeanReversionCalibration(bool b) { parameters_.set("calibration", "meanReversionCalibration", b); }
    void setForeignCurrencies(const std::string& s) { parameters_.set("calibration", "foreignCurrencies", s); }
    void setCurveTenors(const std::string& s) { parameters_.set("calibration", "curveTenors", s); }
    void setScenarioInputFile(const std::string& fileName) { parameters_.set("calibration", "scenarioInputFile", fileName); }
    void setStartDate(const Date& d) { parameters_.set("calibration", "startDate", d); }
    void setEndDate(const Date& d) { parameters_.set("calibration", "endDate", d); }
    void setUseForwardOrZeroRate(const std::string& s) { parameters_.set("calibration", "useForwardOrZeroRate", s); }
    void setLambda(Real r) { parameters_.set("calibration", "lambda", r); }
    void setVarianceRetained(Real r) { parameters_.set("calibration", "varianceRetained", r); }
    void setPcaInputFiles(const std::string& fileName, const std::filesystem::path& inputPath) { parameters_.set("calibration", "pcaInputFileName", inputPath); }
    void setBasisFunctionNumber(Size s) { parameters_.set("calibration", "basisFunctionNumber", s); }
    void setKappaUpperBound(Real r) { parameters_.set("calibration", "kappaUpperBound", r); }
    void setHaltonMaxGuess(Size s) { parameters_.set("calibration", "haltonMaxGuess", s); }
    void setPcaOutputFileName(const std::string& fileName) { parameters_.set("calibration", "pcaOutputFileName", fileName); }
    void setMeanReversionOutputFileName(const std::string& fileName) { parameters_.set("calibration", "meanReversionOutputFileName", fileName); }

    // Setters for xva
    void setXvaUseDoublePrecisionCubes(const bool b) { parameters_.set("xva", "useDoublePrecisionCubes", b); };
    void setXvaBaseCurrency(const std::string& s) { parameters_.set("xva", "baseCurrency", s); };
    // TODO: API for setting NPV and market cubes
    /* This overwrites scenarioGeneratorData, storeFlows, storeCreditStateNpvs to the values stored together with the
       cube. Therefore this method should be called after setScenarioGeneratorData(), setStoreFlows(),
       setStoreCreditStateNPVs() to ensure that the overwrite takes place. */
    void setCube(const QuantLib::ext::shared_ptr<NPVCube>& cube) { parameters_.set("xva", "cubeFile", cube); };
    void setMarketCube(const QuantLib::ext::shared_ptr<AggregationScenarioData>& cube) { parameters_.set("xva", "scenarioFile", cube); };
    // QuantLib::ext::shared_ptr<AggregationScenarioData> mktCube();
    void setFlipViewXVA(bool b) { parameters_.set("xva", "flipViewXVA", b); }
    void setMporCashFlowMode(const MporCashFlowMode m) { parameters_.set("xva", "mporCashFlowMode", m); }
    void setFullInitialCollateralisation(bool b) { parameters_.set("xva", "fullInitialCollateralisation", b); }
    void setExposureProfiles(bool b) { parameters_.set("xva", "exposureProfiles", b); }
    void setExposureProfilesByTrade(bool b) { parameters_.set("xva", "exposureProfilesByTrade", b); }
    void setExposureProfilesUseCloseOutValues(bool b) { parameters_.set("xva", "exposureProfilesUseCloseOutValues", b); }
    void setWriteIndividualExposureReports(bool b) { parameters_.set("xva", "writeIndividualExposureReports", b); }
    void setPfeQuantile(Real r) { parameters_.set("xva", "quantile", r); }
    void setCollateralCalculationType(const std::string& s) { parameters_.set("xva", "calculationType", s); }
    void setExposureAllocationMethod(const std::string& s) { parameters_.set("xva", "allocationMethod", s); }
    void setMarginalAllocationLimit(Real r) { parameters_.set("xva", "marginalAllocationLimit", r); }
    void setExerciseNextBreak(bool b) { parameters_.set("xva", "exerciseNextBreak", b); }
    void setCvaAnalytic(bool b) { parameters_.set("xva", "cva", b); }
    void setDvaAnalytic(bool b) { parameters_.set("xva", "dva", b); }
    void setFvaAnalytic(bool b) { parameters_.set("xva", "fva", b); }
    void setColvaAnalytic(bool b) { parameters_.set("xva", "colva", b); }
    void setCollateralFloorAnalytic(bool b) { parameters_.set("xva", "collateralFloor", b); }
    void setDimAnalytic(bool b) { parameters_.set("xva", "dim", b); }
    void setDimModel(const std::string& s) { parameters_.set("xva", "dimModel", s); }
    void setMvaAnalytic(bool b) { parameters_.set("xva", "mva", b); }
    void setKvaAnalytic(bool b) { parameters_.set("xva", "kva", b); }
    void setDynamicCredit(bool b) { parameters_.set("xva", "dynamicCredit", b); }
    void setCvaSensi(bool b) { parameters_.set("xva", "cvaSensi", b); }
    void setCvaSensiShiftSize(Real r) { parameters_.set("xva", "cvaSensiShiftSize", r); }
    void setDvaName(const std::string& s) { parameters_.set("xva", "dvaName", s); }
    // FIXME: remove this from the base class?
    void setRawCubeOutputFile(const std::string& s) { parameters_.set("xva", "rawCubeOutputFile", s); }
    void setRawCubeOutput(bool b) { parameters_.set("xva", "rawCubeOutput", b); };
    void setNetCubeOutput(bool b) { parameters_.set("xva", "netCubeOutput", b); };
    void setNetCubeOutputFile(const std::string& s) { parameters_.set("xva", "netCubeOutputFile", s); }
    void setTimeAveragedNettedExposureOutputFile(const std::string& s) { parameters_.set("xva", "timeAveragedNettedExposureOutputFile", s); }
    // funding value adjustment details
    void setFvaBorrowingCurve(const std::string& s) { parameters_.set("xva", "fvaBorrowingCurve", s); }
    void setFvaLendingCurve(const std::string& s) { parameters_.set("xva", "fvaLendingCurve", s); }
    void setFlipViewBorrowingCurvePostfix(const std::string& s) { parameters_.set("xva", "flipViewBorrowingCurvePostfix", s); }
    void setFlipViewLendingCurvePostfix(const std::string& s) { parameters_.set("xva", "flipViewLendingCurvePostfix", s); }
    // dynamic initial margin details
    void setDimQuantile(Real r) { parameters_.set("xva", "dimQuantile", r); }
    void setDimHorizonCalendarDays(Size s) { parameters_.set("xva", "dimHorizonCalendarDays", s); }
    void setDimRegressionOrder(Size s) { parameters_.set("xva", "dimRegressionOrder", s); }
    void setDimRegressors(const std::string& s) { parameters_.set("xva", "dimRegressors", s); }
    void setDimOutputGridPoints(const std::string& s) { parameters_.set("xva", "dimOutputGridPoints", s); }
    void setDimDistributionCoveredStdDevs(Real r) { parameters_.set("xva", "dimOutputGridPoints", r); }
    void setDimDistributionGridSize(Size n) { parameters_.set("xva", "dimDistributionGridSize", n); }
    void setDimLocalRegressionEvaluations(Size s) { parameters_.set("xva", "dimLocalRegressionEvaluations", s); }
    void setDimLocalRegressionBandwidth(Real r) { parameters_.set("xva", "dimLocalRegressionBandwidth", r); }
    // capital value adjustment details
    void setKvaCapitalDiscountRate(Real r) { parameters_.set("xva", "kvaCapitalDiscountRate", r); }
    void setKvaAlpha(Real r) { parameters_.set("xva", "kvaAlpha", r); }
    void setKvaRegAdjustment(Real r) { parameters_.set("xva", "kvaRegAdjustment", r); }
    void setKvaCapitalHurdle(Real r) { parameters_.set("xva", "kvaCapitalHurdle", r); }
    void setKvaOurPdFloor(Real r) { parameters_.set("xva", "kvaOurPdFloor", r); }
    void setKvaTheirPdFloor(Real r) { parameters_.set("xva", "kvaTheirPdFloor", r); }
    void setKvaOurCvaRiskWeight(Real r) { parameters_.set("xva", "kvaOurCvaRiskWeight", r); }
    void setKvaTheirCvaRiskWeight(Real r) { parameters_.set("xva", "kvaTheirCvaRiskWeight", r); }
    void setfirstMporCollateralAdjustment(const bool constantInitialVm) { parameters_.set("xva", "firstMporCollateralAdjustment", constantInitialVm); }
    // credit simulation
    void setCreditMigrationAnalytic(bool b) { parameters_.set("xva", "kvaTheirCvaRiskWeight", b); }
    void setCreditMigrationDistributionGrid(const std::vector<Real>& grid) { parameters_.set("xva", "creditMigrationDistributionGrid", grid); }
    void setCreditSimulationParameters(const QuantLib::ext::shared_ptr<CreditSimulationParameters>& c) { parameters_.set("xva", "creditMigrationConfig", c); }
    void setCreditSimulationParametersFromBuffer(const std::string& xml ) { parameters_.set("xva", "creditMigrationConfig", xml); }
    void setCreditMigrationOutputFiles(const std::string& s) { parameters_.set("xva", "creditMigrationOutputFiles", s); }
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

    // Setters for sensitivityStress
    void setSensitivityStressSimMarketParams(const std::string& xml);
    void setSensitivityStressSimMarketParamsFromFile(const std::string& f);
    void setSensitivityStressScenarioData(const std::string& s);
    void setSensitivityStressScenarioDataFromFile(const std::string& s);
    void setSensitivityStressSensitivityScenarioData(const std::string& xml);
    void setSensitivityStressSensitivityScenarioDataFromFile(const std::string& fileName);
    void setSensitivityStressCalculateBaseScenario(const bool calcBaseScenario) { sensitivityStressCalcBaseScenario_ = calcBaseScenario; }

    // Setters for xvaSensi
    void setXvaSensiSimMarketParams(const std::string& xml);
    void setXvaSensiSimMarketParamsFromFile(const std::string& fileName);
    void setXvaSensiScenarioData(const std::string& xml);
    void setXvaSensiScenarioDataFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const std::string& xml);
    void setXvaSensiPricingEngineFromFile(const std::string& fileName);
    void setXvaSensiPricingEngine(const QuantLib::ext::shared_ptr<EngineData>& engineData) {
        sensiPricingEngine_ = engineData;
    }
    void setXvaSensiParSensi(const bool parSensi){ xvaSensiParSensi_ = parSensi;}
    void setXvaSensiOutputJacobi(const bool outputJacobi) { xvaSensiOutputJacobi_ = outputJacobi; };
    void setXvaSensiThreshold(const Real threshold) { xvaSensiThreshold_ = threshold; }
    void setXvaSensiOutputPrecision(Size p) { xvaSensiOutputPrecision_ = p; }

    // Setters for SA-CVA
    // input file matches the required format for SA-CVA calcs, aggregated per CvaRiskFactorKey
    void setSaCvaNetSensitivitiesFromFile(const std::string& fileName);
    // input file is the CVA par sensitivity from ORE's XVA Sensitivity Analytic
    void setCvaSensitivitiesFromFile(const std::string& fileName);
  
    // Setters for XVA Explain
    void setXvaExplainSimMarketParams(const std::string& xml);
    void setXvaExplainSimMarketParamsFromFile(const std::string& f);
    void setXvaExplainSensitivityScenarioData(const std::string& xml);
    void setXvaExplainSensitivityScenarioDataFromFile(const std::string& fileName);
    void setXvaExplainShiftThreshold(const double threshold) { xvaExplainShiftThreshold_ = threshold; }

    // Setters for SIMM
    void setSimmVersion(const std::string& s) { simmVersion_ = s; }
    void setCrif(const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif) { crif_ = crif; }
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
    void setEnforceIMRegulations(bool b) { enforceIMRegulations_ = b; }
    void setRemoveInvalidCrifRecords(bool b) { removeInvalidCrifRecords_ = b; }
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
    void removeAnalytic(const std::string& s);

    void setPnlDateAdjustedRiskFactors(const std::string& s); // parse to vector<RiskFactorKey::KeyType>

    void setRiskFactorLevel(bool b);

    /***************************
     * Getters for general setup
     ***************************/
    const QuantLib::Date& asof() const { return setupVariables_.asof_; }
    const std::filesystem::path& resultsPath() const { return setupVariables_.resultsPath_; }
    const std::string& baseCurrency() const { return setupVariables_.baseCurrency_; }
    const std::string& discountIndex() { return setupVariables_.discountIndex_; }
    const std::string& resultCurrency() const { return setupVariables_.resultCurrency_; }
    bool continueOnError() const { return setupVariables_.continueOnError_; }
    bool allowModelBuilderFallbacks() const { return setupVariables_.allowModelBuilderFallbacks_; }
    bool lazyMarketBuilding() const { return setupVariables_.lazyMarketBuilding_; }
    bool buildFailedTrades() const { return setupVariables_.buildFailedTrades_; }
    const std::string& observationModel() const { return setupVariables_.observationModel_; }
    bool implyTodaysFixings() const { return setupVariables_.implyTodaysFixings_; }
    Date fixingCutOffDate() const { return setupVariables_.fixingCutOffDate_; }
    bool useAtParCouponsCurves() const { return setupVariables_.useAtParCouponsCurves_; }
    bool useAtParCouponsTrades() const { return setupVariables_.useAtParCouponsTrades_; }
    bool enrichIndexFixings() const { return setupVariables_.enrichIndexFixings_; }
    Size ignoreFixingLead() const { return setupVariables_.ignoreFixingLead_; }
    Size ignoreFixingLag() const { return setupVariables_.ignoreFixingLag_; }
    const std::map<std::string, std::string>&  marketConfigs() const { return marketConfigs_; }
    const std::string& marketConfig(const std::string& context);
    const QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager() const {
        return setupVariables_.refDataManager_;
    }
    const QuantLib::ext::shared_ptr<ore::data::Conventions>& conventions() const {
        return setupVariables_.conventions_;
    }
    const QuantLib::ext::shared_ptr<ore::data::Conventions>& mporConventions() const { return mporConventions_; }
    const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig() const {
        return setupVariables_.iborFallbackConfig_;
    }
    const QuantLib::ext::shared_ptr<ore::data::BaselTrafficLightData>& baselTrafficLightConfig() const {
        return setupVariables_.baselTrafficLightConfig_;
    }
    
    CurveConfigurationsManager& curveConfigs() { return setupVariables_.curveConfigs_; }
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfig(const std::string& s = std::string()) const;
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& pricingEngine() const {
        return setupVariables_.pricingEngine_;
    }
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams() const {
        return setupVariables_.todaysMarketParams_;
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() const { return setupVariables_.portfolio_; }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& useCounterpartyOriginalPortfolio() const {
        return useCounterpartyOriginalPortfolio_;
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& mporPortfolio() const { return mporPortfolio_; }
    const QuantLib::ext::shared_ptr<ore::data::CurrencyConfig>& currencyConfigs() {
        return setupVariables_.currencyConfig_;
    }
    const QuantLib::ext::shared_ptr<ore::data::CalendarAdjustmentConfig>& calendarAdjustmentConfigs() {
        return setupVariables_.calendarAdjustment_;
    }
    const QuantLib::ext::shared_ptr<ore::data::ScriptLibraryData>& scriptLibraryData() {
        return setupVariables_.scriptLibraryData_;
    }
  
    QuantLib::Size maxRetries() const { return maxRetries_; }
    QuantLib::Size nThreads() const { return setupVariables_.nThreads_; }
    bool entireMarket() const { return setupVariables_.entireMarket_; }
    bool allFixings() const { return setupVariables_.allFixings_; }
    bool eomInflationFixings() const { return setupVariables_.eomInflationFixings_; }
    bool useMarketDataFixings() const { return setupVariables_.useMarketDataFixings_; }
    bool iborFallbackOverride() const { return setupVariables_.iborFallbackOverride_; }
    const std::string& reportNaString() const { return setupVariables_.reportNaString_; }
    char csvCommentCharacter() const { return setupVariables_.csvCommentCharacter_; }
    char csvEolChar() const { return csvEolChar_; }
    char csvQuoteChar() const { return csvQuoteChar_; }
    char csvSeparator() const { return setupVariables_.csvSeparator_; }
    char csvEscapeChar() const { return csvEscapeChar_; }
    bool dryRun() const { return setupVariables_.dryRun_; }
    bool computeTheta() const { return setupVariables_.computeTheta_; }
    Period thetaPeriod() const { return setupVariables_.thetaPeriod_; }
    QuantLib::Size mporDays() const { return mporDays_; }
    QuantLib::Date mporDate();
    const QuantLib::Calendar mporCalendar() {
        if (mporCalendar_.empty()) {
            QL_REQUIRE(!setupVariables_.baseCurrency_.empty(), "mpor calendar or baseCurrency must be provided";);
            return parseCalendar(setupVariables_.baseCurrency_);
        } else
            return mporCalendar_;
    }
    bool mporOverlappingPeriods() const { return mporOverlappingPeriods_; }
    bool mporForward() const { return mporForward_; }
    const std::string& marketDataLoaderOutput() { return setupVariables_.marketDataLoaderOutput_; }
    const std::string& marketDataLoaderInput() { return setupVariables_.marketDataLoaderInput_; }
    bool deriveCounterpartyDefaultCurves() const { return deriveCounterpartyDefaultCurves_; }
    const std::string& additionalMarketDataInput() const { return additionalMarketDataInput_; }

    /***************************
     * Getters for npv analytics
     ***************************/
    bool outputAdditionalResults() const { return setupVariables_.outputAdditionalResults_; };
    std::size_t additionalResultsReportPrecision() const { return setupVariables_.additionalResultsReportPrecision_; }

    /***********************
     * Getters for cashflows
     ***********************/
    bool includePastCashflows() const { return setupVariables_.includePastCashflows_; }

    /****************************
     * Getters for curves/markets
     ****************************/
    bool outputTodaysMarketCalibration() const { return outputTodaysMarketCalibration_; };
    std::size_t todaysMarketCalibrationPrecision() const { return todaysMarketCalibrationPrecision_; }

    /*****************************
     * Getters for sensi analytics
     *****************************/
    bool xbsParConversion() { return xbsParConversion_; }
    bool parSensi() const { return parSensi_; };
    bool optimiseRiskFactors() const { return optimiseRiskFactors_; }
    bool alignPillars() const { return alignPillars_; }
    bool outputJacobi() const { return outputJacobi_; }
    bool useSensiSpreadedTermStructures() const { return useSensiSpreadedTermStructures_; }
    QuantLib::Real sensiThreshold() const { return sensiThreshold_; }
    bool sensiRecalibrateModels() const { return sensiRecalibrateModels_; }
    bool sensiLaxFxConversion() const { return sensiLaxFxConversion_; }
    bool sensiDecomposition() const { return sensiDecomposition_; }
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& sensiSimMarketParams() const { return sensiSimMarketParams_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData() const { return sensiScenarioData_; }
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& sensiPricingEngine() const { return sensiPricingEngine_; }
    // const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& sensiTodaysMarketParams() { return sensiTodaysMarketParams_; }
    QuantLib::Size sensiOutputPrecision() const { return sensiOutputPrecision_; }
        
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
    Size stressPrecision() const { return stressPrecision_; };
    bool stressGenerateCashflows() const { return stressGenerateCashflows_; }

    QuantLib::ext::shared_ptr<ScenarioReader> scenarioReader() const { return scenarioReader_;};
    string benchmarkVarPeriod() const { return benchmarkVarPeriod_; };
    bool outputHistoricalScenarios() const { return outputHistoricalScenarios_; }
    QuantLib::Size reportBufferSize() const { return setupVariables_.reportBufferSize_; }
    
    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager() const {
        return setupVariables_.counterpartyManager_;
    }    

    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& xvaStressSimMarketParams() const {
        return xvaStressSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<StressTestScenarioData>& xvaStressScenarioData() const {
        return xvaStressScenarioData_;
    }
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& xvaStressSensitivityScenarioData() const {
        return xvaStressSensitivityScenarioData_;
    }
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>&
    sensitivityStressSimMarketParams() const {
        return sensitivityStressSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<StressTestScenarioData>& sensitivityStressScenarioData() const {
        return sensitivityStressScenarioData_;
    }
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>&
    sensitivityStressSensitivityScenarioData() const {
        return sensitivityStressSensitivityScenarioData_;
    }
    bool sensitivityStressCalcBaseScenario() const { return sensitivityStressCalcBaseScenario_; }
    bool xvaStressWriteCubes() const { return xvaStressWriteCubes_; }

    // Getters for XVA Explain
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& xvaExplainSimMarketParams() const {
        return xvaExplainSimMarketParams_;
    }
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& xvaExplainSensitivityScenarioData() const {
        return xvaExplainSensitivityScenarioData_;
    }

    double xvaExplainShiftThreshold() const { return xvaExplainShiftThreshold_; }

    /**************************************************
     * Getters for cashflow npv and dynamic backtesting
     **************************************************/

    const QuantLib::Date& cashflowHorizon() const { return cashflowHorizon_; };
    const QuantLib::Date& portfolioFilterDate() const { return portfolioFilterDate_; }    

    /******************
     * Getters for SIMM
     ******************/
    const std::string& simmVersion() const { return simmVersion_; }
    const QuantLib::ext::shared_ptr<Crif>& crif() const { return crif_; }
    const QuantLib::ext::shared_ptr<SimmBasicNameMapper>& simmNameMapper() const { return simmNameMapper_; }
    const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper() const { return simmBucketMapper_; }
    const QuantLib::ext::shared_ptr<SimmCalibrationData>& simmCalibrationData() const { return simmCalibrationData_; }
    const std::string& simmCalculationCurrencyCall() const { return simmCalculationCurrencyCall_; }
    const std::string& simmCalculationCurrencyPost() const { return simmCalculationCurrencyPost_; }
    const std::string& simmResultCurrency() const { return simmResultCurrency_; }
    const std::string& simmReportingCurrency() const { return simmReportingCurrency_; }
    bool enforceIMRegulations() const { return enforceIMRegulations_; }
    bool removeInvalidCrifRecords() const { return removeInvalidCrifRecords_; }
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

    bool xvaSensiParSensi() const { return xvaSensiParSensi_; }
    bool xvaSensiOutputJacobi() const { return xvaSensiOutputJacobi_; };
    Real xvaSensiThreshold() const { return xvaSensiThreshold_;}
    QuantLib::Size xvaSensiOutputPrecision() const { return xvaSensiOutputPrecision_; }

    /*************************************
     * SA-CVA 
     *************************************/
    const SaCvaNetSensitivities& saCvaNetSensitivities() const { return saCvaNetSensitivities_; }
    const vector<CvaSensitivityRecord>& cvaSensitivities() const { return cvaSensitivities_; }
    bool useUnhedgedCvaSensis() const { return useUnhedgedCvaSensis_; }
    const std::vector<std::string>& cvaPerfectHedges() const { return cvaPerfectHedges_; }

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
    const SetupVariables& setupVariables() const { return setupVariables_; }
    virtual void loadParameters();
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
        
    Parameters parameters_;
    SetupVariables setupVariables_;
  
    std::map<std::string, std::string> marketConfigs_;
    QuantLib::ext::shared_ptr<ore::data::Conventions> mporConventions_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> useCounterpartyOriginalPortfolio_, mporPortfolio_;
    QuantLib::Size maxRetries_ = 7;
;
    char csvEolChar_ = '\n';
    char csvQuoteChar_ = '\0';
    char csvEscapeChar_ = '\\';
    QuantLib::Date mporDate_;
    QuantLib::Size mporDays_ = 10;
    bool mporOverlappingPeriods_ = true;
    QuantLib::Calendar mporCalendar_;
    bool mporForward_ = true;
    bool deriveCounterpartyDefaultCurves_ = false;
    std::string additionalMarketDataInput_;

    /**************
     * NPV analytic
     *************/
    bool outputTodaysMarketCalibration_ = true;
    std::size_t todaysMarketCalibrationPrecision_ = 8;

    /***********************************
     * CASHFLOW and CASHFLOWNPV analytic
     ***********************************/
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
    bool sensiLaxFxConversion_ = false;
    bool sensiDecomposition_ = false;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> sensiSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> sensiPricingEngine_;
    // QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> sensiTodaysMarketParams_;
    QuantLib::Size sensiOutputPrecision_ = 2;

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
    Size stressPrecision_ = 2;
    bool xvaStressWriteCubes_ = false;
    bool stressGenerateCashflows_ = false;

    QuantLib::ext::shared_ptr<ScenarioReader> scenarioReader_;
    std::string benchmarkVarPeriod_;
    bool outputHistoricalScenarios_ = false;

    /***************
     * SIMM analytic
     ***************/
    std::string simmVersion_;
    QuantLib::ext::shared_ptr<ore::analytics::Crif> crif_;
  
    QuantLib::ext::shared_ptr<ore::analytics::SimmBasicNameMapper> simmNameMapper_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper> simmBucketMapper_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmCalibrationData> simmCalibrationData_;
    std::string simmCalculationCurrencyCall_ = "";
    std::string simmCalculationCurrencyPost_ = "";
    std::string simmResultCurrency_ = "";
    std::string simmReportingCurrency_ = "";
    bool enforceIMRegulations_ = false;
    bool removeInvalidCrifRecords_ = true;
    bool useSimmParameters_ = true;
    bool writeSimmIntermediateReports_ = true;
    bool loadCrifAdditionalFields_ = true;

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
     * XVA Stress analytic
     *****************/

    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> xvaStressSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> xvaStressScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaStressSensitivityScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> sensitivityStressSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> sensitivityStressScenarioData_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensitivityStressSensitivityScenarioData_;
    bool sensitivityStressCalcBaseScenario_ = false;

    /*****************
     * XVA Sensitivity analytic
     *****************/

    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> xvaSensiSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaSensiScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> xvaSensiPricingEngine_;
    bool xvaSensiParSensi_ = true;
    bool xvaSensiOutputJacobi_ = false;
    QuantLib::Real xvaSensiThreshold_ = 1e-6;
    QuantLib::Size xvaSensiOutputPrecision_ = 4;

    /*****************
     * SA-CVA 
     *****************/
    SaCvaNetSensitivities saCvaNetSensitivities_; 
    vector<CvaSensitivityRecord> cvaSensitivities_;
    bool useUnhedgedCvaSensis_ = true;
    std::vector<std::string> cvaPerfectHedges_ = {"ForeignExchange|Delta", "ForeignExchange|Vega"};

    /*****************
     * XVA Explain analytic
     *****************/
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> xvaExplainSimMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> xvaExplainSensitivityScenarioData_;
    double xvaExplainShiftThreshold_ = 0;
};


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
    std::string stressTestCashflowFileName_;
    std::string xvaStressTestFileName_;
    std::string sensitivityStressTestFileName_;
    std::string stressZeroScenarioDataFileName_;
    std::string varFileName_;
    std::string parConversionOutputFileName_;
    std::string parConversionJacobiFileName_;
    std::string parConversionJacobiInverseFileName_;
    std::string pnlOutputFileName_;
    std::string parStressTestConversionFile_;
    std::string pnlExplainOutputFileName_; 
    std::string correlationOutputFileName_;
    std::string riskFactorsOutputFileName_;
    std::string marketObjectsOutputFileName_;
    std::string zeroToParShiftFile_;
    std::string scenarioNpvOutputFileName_;
    std::string calibrationOutputFileName_;
    std::string pcaOutputFileName_;
    std::string meanReversionOutputFileName_;
    std::string xvaSensiJacobiFileName_;
    std::string xvaSensiJacobiInverseFileName_;
    std::string timeAveragedNettedExposureFileName_;
};

void scaleUpPortfolio(QuantLib::ext::shared_ptr<ore::data::Portfolio>& p);

} // namespace analytics
} // namespace ore

