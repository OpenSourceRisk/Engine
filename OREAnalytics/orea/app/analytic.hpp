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

#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/marketdata/inmemoryloader.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/report/inmemoryreport.hpp>

#include <orea/aggregation/collateralaccount.hpp>
#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/aggregation/postprocess.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>

#include <orea/app/inputparameters.hpp>
#include <orea/app/marketcalibrationreport.hpp>

#include <boost/any.hpp>
#include <iostream>

namespace ore {
namespace analytics {

class Analytic {
public:
    class Impl;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>
        analytic_reports;

    typedef std::map<std::string, std::map<std::string, QuantLib::ext::shared_ptr<NPVCube>>>
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
        QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams;
        QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData;
        QuantLib::ext::shared_ptr<ore::analytics::ScenarioGeneratorData> scenarioGeneratorData;
        QuantLib::ext::shared_ptr<ore::analytics::CrossAssetModelData> crossAssetModelData;
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
    virtual void setUpConfigurations();
    
    virtual void buildMarket(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const bool marketRequired = true);
    virtual void buildPortfolio();
    virtual void marketCalibration(const QuantLib::ext::shared_ptr<MarketCalibrationReportBase>& mcr = nullptr);
    virtual void modifyPortfolio() {}
    virtual void replaceTrades() {}

    //! Inspectors
    const std::string label() const;
    const std::set<std::string>& analyticTypes() const { return types_; }
    const QuantLib::ext::shared_ptr<InputParameters>& inputs() const { return inputs_; }
    const QuantLib::ext::shared_ptr<ore::data::Market>& market() const { return market_; };
    // To allow SWIG wrapping
    QuantLib::ext::shared_ptr<MarketImpl> getMarket() const {        
        return QuantLib::ext::dynamic_pointer_cast<MarketImpl>(market_);
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; };
    void setInputs(const QuantLib::ext::shared_ptr<InputParameters>& inputs) { inputs_ = inputs; }
    void setMarket(const QuantLib::ext::shared_ptr<ore::data::Market>& market) { market_ = market; };
    void setPortfolio(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) { portfolio_ = portfolio; };
    std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    const QuantLib::ext::shared_ptr<ore::data::Loader>& loader() const { return loader_; };
    Configurations& configurations() { return configurations_; }

    //! Result reports
    analytic_reports& reports() { return reports_; };
    analytic_npvcubes& npvCubes() { return npvCubes_; };
    analytic_mktcubes& mktCubes() { return mktCubes_; };
    analytic_stresstests& stressTests() { return stressTests_;}
    
    const bool getWriteIntermediateReports() const { return writeIntermediateReports_; }
    void setWriteIntermediateReports(const bool flag) { writeIntermediateReports_ = flag; }

    //! Check whether any of the requested run types is covered by this analytic
    bool match(const std::set<std::string>& runTypes);

    const std::unique_ptr<Impl>& impl() { 
        return impl_;
    }

    std::set<QuantLib::Date> marketDates() const;

    std::vector<QuantLib::ext::shared_ptr<Analytic>> allDependentAnalytics() const;

protected:
    std::unique_ptr<Impl> impl_;

    //! list of analytic types run by this analytic
    std::set<std::string> types_;
    //! contains all the input parameters for the run
    QuantLib::ext::shared_ptr<InputParameters> inputs_;

    Configurations configurations_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;

    analytic_reports reports_;
    analytic_npvcubes npvCubes_;
    analytic_mktcubes mktCubes_;
    analytic_stresstests stressTests_;

    //! Whether to write intermediate reports or not.
    //! This would typically be used when the analytic is being called by another analytic
    //! and that parent/calling analytic will be writing its own set of intermediate reports
    bool writeIntermediateReports_ = true;
};

class Analytic::Impl {
public:    
    Impl() {}
    Impl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : inputs_(inputs) {}
    virtual ~Impl(){}
    
    virtual void runAnalytic(
        const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes = {}) = 0;
    
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
    const std::map<std::string, QuantLib::ext::shared_ptr<Analytic>>& dependentAnalytics() const {
        return dependentAnalytics_;
    }
    void addDependentAnalytic(const std::string& key, const QuantLib::ext::shared_ptr<Analytic>& analytic) {
        dependentAnalytics_[key] = analytic;
    }
    std::vector<QuantLib::ext::shared_ptr<Analytic>> allDependentAnalytics() const;
    virtual std::vector<QuantLib::Date> additionalMarketDates() const { return {}; }

protected:
    QuantLib::ext::shared_ptr<InputParameters> inputs_;

    //! label for logging purposes primarily
    std::string label_;

    std::map<std::string, QuantLib::ext::shared_ptr<Analytic>> dependentAnalytics_;

private:
    Analytic* analytic_;
    bool generateAdditionalResults_ = false;
};

/*! Market analytics
  Does not need a portfolio
  Builds the market
  Reports market calibration and curves
*/
class MarketDataAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "MARKETDATA";

    MarketDataAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class MarketDataAnalytic : public Analytic {
public:
    MarketDataAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<MarketDataAnalyticImpl>(inputs), {"MARKETDATA"}, inputs) {}
};

template <class T> inline QuantLib::ext::shared_ptr<T> Analytic::Impl::dependentAnalytic(const std::string& key) const {
    auto it = dependentAnalytics_.find(key);
    QL_REQUIRE(it != dependentAnalytics_.end(), "Could not find dependent Analytic " << key);
    QuantLib::ext::shared_ptr<T> analytic = QuantLib::ext::dynamic_pointer_cast<T>(it->second);
    QL_REQUIRE(analytic, "Could not cast analytic for key " << key);
    return analytic;
}

QuantLib::ext::shared_ptr<ore::data::Loader> implyBondSpreads(const Date& asof,
                 const QuantLib::ext::shared_ptr<InputParameters>& params,
                 const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                 const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
                 const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                 const std::string& excludeRegex);

} // namespace analytics
} // namespace oreplus
