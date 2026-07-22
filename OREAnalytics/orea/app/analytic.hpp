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

#include <ored/utilities/timer.hpp>

#include <ql/any.hpp>
#include <ql/time/date.hpp>
#include <ql/shared_ptr.hpp>

#include <set>
#include <vector>
#include <map>

namespace ore::data {
class CrossAssetModelData;
class InMemoryReport;
class InMemoryLoader;
class Portfolio;
class CurveConfigurations;
class EngineData;
class TodaysMarketParameters;
}; // namespace ore::data

namespace ore::analytics {

class InputParameters;
class MarketCalibrationReportBase;
class AnalyticsManager;
class StressTestScenarioData;
class ScenarioSimMarketParameters;
class SensitivityScenarioData;
class ScenarioGeneratorData;
class ParSensitivityCubeStream;
class AggregationScenarioData;
class MarketCalibrationReportBase;
class NPVCubeWithMetaData;

class Analytic {
public:
    class Impl;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>
        analytic_reports;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<NPVCubeWithMetaData>>>
        analytic_npvcubes;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<AggregationScenarioData>>>
        analytic_mktcubes;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<StressTestScenarioData>>>
        analytic_stresstests;

    struct Configurations { 
        //! Booleans to determine if these configs are needed
        bool simulationConfigRequired = false;
        bool sensitivityConfigRequired = false;
        bool scenarioGeneratorConfigRequired = false;
        bool crossAssetModelConfigRequired = false;
        QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams;
        QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketParams;
        QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiScenarioData;
        QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData;
        QuantLib::ext::shared_ptr<ore::data::CrossAssetModelData> crossAssetModelData;
        QuantLib::ext::shared_ptr<CurveConfigurations> curveConfig;
        QuantLib::ext::shared_ptr<ore::data::EngineData> engineData;
        QuantLib::Date asofDate;
    };

    //! Constructors
    Analytic(){};
    Analytic(//! Concrete implementation of the analytic
             std::unique_ptr<Impl> impl,
             //! The types of all (sub) analytics covered by this Analytic object
             //! e.g. NPV, CASHFLOW, CASHFLOWNPV, etc., covered by the PricingAnalytic
             const std::set<std::string>& analyticTypes,
             //! Any inputs required by this Analytic
             const QuantLib::ext::shared_ptr<InputParameters>& inputs,
             //! Pointer to the analytics manager
             const QuantLib::ext::weak_ptr<AnalyticsManager>& analyticsManager,
             //! Flag to indicate whether a simulation config file is required for this analytic
             bool simulationConfig = false,
             //! Flag to indicate whether a sensitivity config file is required for this analytic
             bool sensitivityConfig = false,
             //! Flag to indicate whether a scenario generator config file is required for this analytic
             bool scenarioGeneratorConfig = false,
             //! Flag to indicate whether a cross asset model config file is required for this analytic
             bool crossAssetModelConfig = false);

    virtual ~Analytic() {}

    //! Run only those analytic types that are inclcuded in the runTypes vector, run all if the runType vector is empty 
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {});

    // we can build configurations here (today's market params, scenario sim market params, sensitivity scenasrio data)
    virtual void buildConfigurations(const bool = false){};
    void setUp();
    void initialise();
    
    virtual void buildMarket(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const bool marketRequired = true);
    virtual void buildPortfolio(const bool emitStructuredError = true);
    virtual void marketCalibration(const std::vector<QuantLib::ext::shared_ptr<MarketCalibrationReportBase>>& mcr = {});
    virtual void modifyPortfolio() {}
    virtual void replaceTrades() {}
    virtual void enrichIndexFixings(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);
    virtual bool requiresMarketData() const { return true; }

    //! Inspectors
    const std::string label() const;
    const std::set<std::string>& analyticTypes() const { return types_; }
    const QuantLib::ext::shared_ptr<InputParameters>& inputs() const { return inputs_; }
    const QuantLib::ext::weak_ptr<AnalyticsManager>& analyticsManager() const { return analyticsManager_; }
    const QuantLib::ext::shared_ptr<ore::data::Market>& market() const { return market_; };
    // To allow SWIG wrapping
    QuantLib::ext::shared_ptr<MarketImpl> getMarket() const {        
        return QuantLib::ext::dynamic_pointer_cast<MarketImpl>(market_);
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; };
    void setInputs(const QuantLib::ext::shared_ptr<InputParameters>& inputs) { inputs_ = inputs; }
    void setMarket(const QuantLib::ext::shared_ptr<ore::data::Market>& market) { market_ = market; }
    void setPortfolio(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) { portfolio_ = portfolio; }
    std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    const QuantLib::ext::shared_ptr<ore::data::Loader>& loader() const { return loader_; };
    Configurations& configurations() { return configurations_; }

    //! Analytic results
    analytic_reports reports();
    void addReport(const std::string& key, const std::string& subKey,
                   const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report);
    const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& getReport(const std::string& key,
                                                                          const std::string& subKey);
    virtual void reset();
    //! Release heavy internal state (e.g. scenarios, sim market) while keeping reports intact
    void releaseMemory();

    analytic_npvcubes& npvCubes() { return npvCubes_; };
    analytic_mktcubes& mktCubes() { return mktCubes_; };
    analytic_stresstests& stressTests() { return stressTests_;}
    QuantLib::ext::shared_ptr<ParSensitivityCubeStream>& parCvaSensiCubeStream() { return parCvaSensiCubeStream_; }

    const bool getWriteIntermediateReports() const { return writeIntermediateReports_; }
    void setWriteIntermediateReports(const bool flag) { writeIntermediateReports_ = flag; }

    //! Check whether any of the requested run types is covered by this analytic
    bool match(const std::set<std::string>& runTypes);

    const std::unique_ptr<Impl>& impl() { 
        return impl_;
    }

    std::set<QuantLib::Date> marketDates() const;

    std::vector<QuantLib::ext::shared_ptr<Analytic>> allDependentAnalytics() const;
    
    const Timer& getTimer();
    void startTimer(const std::string& key) { timer_.start(key); }
    QuantLib::ext::optional<boost::timer::cpu_timer> stopTimer(const std::string& key, const bool returnTimer = false) {
        return timer_.stop(key, returnTimer);
    }
    void addTimer(const std::string& key, const Timer& timer) { timer_.addTimer(key, timer); }

    void setApplySimmExemptions(bool flag) { applySimmExemptions_ = flag; }
    bool applySimmExemptions() const { return applySimmExemptions_; }

    void setOffsetScenario(const QuantLib::ext::shared_ptr<Scenario>& offsetScenario,
                           const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketParams);

    const QuantLib::ext::shared_ptr<Scenario>& offsetScenario() const {
        return offsetScenario_;
    }

    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams() const {
        return offsetSimMarketParams_ == nullptr ? configurations_.simMarketParams : offsetSimMarketParams_;
    }

    void applyOffsetScenario(bool continueOnError = true, bool useSpreadedTermStructures = true,
                             bool overrideTenors = true);

protected:
    std::unique_ptr<Impl> impl_;

    //! list of analytic types run by this analytic
    std::set<std::string> types_;
    //! contains all the input parameters for the run
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    //! the analytics manger, used for sharing analytics
    QuantLib::ext::weak_ptr<AnalyticsManager> analyticsManager_;

    Configurations configurations_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;

    analytic_reports reports_;
    analytic_npvcubes npvCubes_;
    analytic_mktcubes mktCubes_;
    analytic_stresstests stressTests_;
    QuantLib::ext::shared_ptr<ParSensitivityCubeStream> parCvaSensiCubeStream_;

    bool applySimmExemptions_ = false;
  
    //! Whether to write intermediate reports or not.
    //! This would typically be used when the analytic is being called by another analytic
    //! and that parent/calling analytic will be writing its own set of intermediate reports
    bool writeIntermediateReports_ = true;

    Timer timer_;

    QuantLib::ext::shared_ptr<Scenario> offsetScenario_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> offsetSimMarketParams_;

private:
    bool analyticComplete_ = false;
};

class Analytic::Impl {
public:    
    Impl() {}
    Impl(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
         QuantLib::ext::shared_ptr<InputVariables> inputVars = nullptr)
        : inputs_(inputs), inputVariables_(inputVars) {}
    virtual ~Impl(){}
    
    virtual void runAnalytic(
        const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes = {}) = 0;
    
    void initialise();
    
    virtual void reset() {
        for (auto& a : dependentAnalytics_) {
            a.second.first->reset();
        }
    }
    //! Release heavy internal computation state while keeping reports intact
    virtual void releaseMemory(){};
    const bool initialised() { return initialised_; };
    virtual void buildDependencies(){};
    virtual void buildConfigurations(){};
    virtual void setUpConfigurations(){};

    //! build an engine factory
    virtual QuantLib::ext::shared_ptr<ore::data::EngineFactory> engineFactory();

    void setLabel(const string& label) { label_ = label; }
    const std::string& label() const { return label_; };

    void setAnalytic(Analytic* analytic) { analytic_ = analytic; }
    Analytic* analytic() const { return analytic_; }
    void setInputs(const QuantLib::ext::shared_ptr<InputParameters>& inputs) { inputs_ = inputs; }
    
    bool generateAdditionalResults() const { return generateAdditionalResults_; }
    void setGenerateAdditionalResults(const bool generateAdditionalResults) {
        generateAdditionalResults_ = generateAdditionalResults;
    }

    bool hasDependentAnalytic(const std::string& key) {
        return dependentAnalytics_.find(key) != dependentAnalytics_.end();
    }
    template <class T> QuantLib::ext::shared_ptr<T> dependentAnalytic(const std::string& key) const;
    QuantLib::ext::shared_ptr<Analytic> dependentAnalytic(const std::string& key) const;
    const std::map<std::string, std::pair<QuantLib::ext::shared_ptr<Analytic>, bool>>& dependentAnalytics() const {
        return dependentAnalytics_;
    }
    void addDependentAnalytic(const std::string& key, const QuantLib::ext::shared_ptr<Analytic>& analytic, const bool incDependentReports = false) {
        dependentAnalytics_[key] = std::make_pair(analytic, incDependentReports);
    }
    std::vector<QuantLib::ext::shared_ptr<Analytic>> allDependentAnalytics() const;
    virtual std::vector<QuantLib::Date> additionalMarketDates() const { return {}; }

    QuantLib::ext::shared_ptr<InputVariables> inputVariables() { return inputVariables_; }
    template <class T> QuantLib::ext::shared_ptr<T> inputVariablesAs() {
		return QuantLib::ext::dynamic_pointer_cast<T>(inputVariables_);
	}

protected:
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    QuantLib::ext::shared_ptr<InputVariables> inputVariables_;

    //! label for logging purposes primarily
    std::string label_;

    //! map to dependent analytics, holds a bool if we want to report intermeditate reports
    std::map<std::string, std::pair<QuantLib::ext::shared_ptr<Analytic>, bool>> dependentAnalytics_;

private:
    Analytic* analytic_;
    bool generateAdditionalResults_ = false;
    bool initialised_ = false;
};

struct MarketDataVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
};

/*! Market analytics
  Does not need a portfolio
  Builds the market
  Reports market calibration and curves
*/
class MarketDataAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "MARKETDATA";

    MarketDataAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs, QuantLib::ext::make_shared<MarketDataVariables>()) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class MarketDataAnalytic : public Analytic {
public:
    MarketDataAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                       const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<MarketDataAnalyticImpl>(inputs), {"MARKETDATA"}, inputs, analyticsManager) {}
};

template <class T> inline QuantLib::ext::shared_ptr<T> Analytic::Impl::dependentAnalytic(const std::string& key) const {
    auto it = dependentAnalytics_.find(key);
    QL_REQUIRE(it != dependentAnalytics_.end(), "Could not find dependent Analytic " << key);
    QuantLib::ext::shared_ptr<T> analytic = QuantLib::ext::dynamic_pointer_cast<T>(it->second.first);
    QL_REQUIRE(analytic, "Could not cast analytic for key " << key);
    return analytic;
}

QuantLib::ext::shared_ptr<ore::data::Loader> implyBondSpreads(const Date& asof,
                 const QuantLib::ext::shared_ptr<InputParameters>& params,
                 const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                 const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
                 const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                 const std::string& excludeRegex);

} // namespace ore::analytics
