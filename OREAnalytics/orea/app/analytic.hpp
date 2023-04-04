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

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<ore::data::InMemoryReport>>>
        analytic_reports;

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<NPVCube>>>
        analytic_npvcubes;

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<AggregationScenarioData>>>
        analytic_mktcubes;

    struct Configurations { 
        //! Booleans to determine if these configs are needed
        bool simulationConfigRequired = false;
        bool sensitivityConfigRequired = false;
        bool scenarioGeneratorConfigRequired = false;
        bool crossAssetModelConfigRequired = false;
        boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams;
        boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams;
        boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData;
        boost::shared_ptr<ore::analytics::ScenarioGeneratorData> scenarioGeneratorData;
        boost::shared_ptr<ore::analytics::CrossAssetModelData> crossAssetModelData;
        boost::shared_ptr<CurveConfigurations> curveConfig;
        boost::shared_ptr<ore::data::EngineData> engineData;
    };

    //! Constructors
    Analytic(){};
    Analytic(//! Concrete implementation of the analytic
             std::unique_ptr<Impl> impl,
             //! The types of all (sub) analytics covered by this Analytic object
             //! e.g. NPV, CASHFLOW, CASHFLOWNPV, etc., covered by the PricingAnalytic
             const std::set<std::string>& analyticTypes,
             //! Any inputs required by this Analytic
             const boost::shared_ptr<InputParameters>& inputs,
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
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {});

    // we can build configurations here (today's market params, scenario sim market params, sensitivity scenasrio data)
    virtual void buildConfigurations() {};
    virtual void setUpConfigurations();
    
    virtual void buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const bool marketRequired = true);
    virtual void buildPortfolio();
    virtual void marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr = nullptr);
    virtual void modifyPortfolio() {}

    //! Inspectors
    const std::string label() const;
    const std::set<std::string>& analyticTypes() const { return types_; }
    const boost::shared_ptr<InputParameters>& inputs() const { return inputs_; }
    const boost::shared_ptr<ore::data::Market>& market() const { return market_; };
    // To allow SWIG wrapping
    boost::shared_ptr<MarketImpl> getMarket() const {        
        return boost::dynamic_pointer_cast<MarketImpl>(market_);
    }
    const boost::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; };
    void setPortfolio(const boost::shared_ptr<ore::data::Portfolio>& portfolio) { portfolio_ = portfolio; };
    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    const boost::shared_ptr<ore::data::Loader>& loader() const { return loader_; };
    Configurations& configurations() { return configurations_; }

    //! Result reports
    analytic_reports& reports() { return reports_; };
    analytic_npvcubes& npvCubes() { return npvCubes_; };
    analytic_mktcubes& mktCubes() { return mktCubes_; };
    const bool getWriteIntermediateReports() const { return writeIntermediateReports_; }
    void setWriteIntermediateReports(const bool flag) { writeIntermediateReports_ = flag; }

    //! Check whether any of the requested run types is covered by this analytic
    bool match(const std::set<std::string>& runTypes);

    const std::unique_ptr<Impl>& impl() { 
        return impl_;
    }

    bool hasDependentAnalytic(const std::string& key) {  return dependentAnalytics_.find(key) != dependentAnalytics_.end(); }
    template <class T> boost::shared_ptr<T> dependentAnalytic(const std::string& key) const;

private:
    std::unique_ptr<Impl> impl_;

protected:
    //! list of analytic types run by this analytic
    std::set<std::string> types_;
    //! contains all the input parameters for the run
    boost::shared_ptr<InputParameters> inputs_;

    Configurations configurations_;
    boost::shared_ptr<ore::data::Market> market_;
    boost::shared_ptr<ore::data::Loader> loader_;
    boost::shared_ptr<ore::data::Portfolio> portfolio_;

    analytic_reports reports_;
    analytic_npvcubes npvCubes_;
    analytic_mktcubes mktCubes_;

    //! flag to replace trades with schedule trades
    bool replaceScheduleTrades_ = false;

    //! Whether to write intermediate reports or not.
    //! This would typically be used when the analytic is being called by another analytic
    //! and that parent/calling analytic will be writing its own set of intermediate reports
    bool writeIntermediateReports_ = true;

    std::map<std::string, boost::shared_ptr<Analytic>> dependentAnalytics_;
};

class Analytic::Impl {
public:    
    Impl() {}
    Impl(const boost::shared_ptr<InputParameters>& inputs) : inputs_(inputs) {}
    virtual ~Impl(){}
    
    virtual void runAnalytic(
        const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
        const std::set<std::string>& runTypes = {}) = 0;
    
    virtual void setUpConfigurations() = 0;

    //! build an engine factory
    virtual boost::shared_ptr<ore::data::EngineFactory> engineFactory();

    void setLabel(const string& label) { label_ = label; }
    const std::string& label() const { return label_; };

    void setAnalytic(Analytic* analytic) { analytic_ = analytic; }
    Analytic* analytic() const { return analytic_; }
    
    bool generateAdditionalResults() const { return generateAdditionalResults_; }
    void setGenerateAdditionalResults(const bool generateAdditionalResults) {
        generateAdditionalResults_ = generateAdditionalResults;
    }

protected:
    boost::shared_ptr<InputParameters> inputs_;

private:
    Analytic* analytic_;
    //! label for logging purposes primarily
    std::string label_;
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

    MarketDataAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) { setLabel(LABEL); }
    void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const std::set<std::string>& runTypes = {}) override;

    void setUpConfigurations() override;
};

class MarketDataAnalytic : public Analytic {
public:
    MarketDataAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<MarketDataAnalyticImpl>(inputs), {"MARKETDATA"}, inputs) {}
};

/*! Pricing-type analytics
  \todo align pillars for par sensitivity analysis
*/
class PricingAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "PRICING";

    PricingAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) { setLabel(LABEL); }
    void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const std::set<std::string>& runTypes = {}) override;

    void setUpConfigurations() override;
};

class PricingAnalytic : public Analytic {
public:
    PricingAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<PricingAnalyticImpl>(inputs),
            {"NPV", "NPV_LAGGED", "CASHFLOW", "CASHFLOWNPV", "SENSITIVITY", "STRESS"},
                   inputs) {}
};
    
class VarAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "VAR";

    VarAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) { setLabel(LABEL); }
    void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class VarAnalytic : public Analytic {
public:
    VarAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<VarAnalyticImpl>(inputs), {"VAR"}, inputs, false, false, false, false) {}
};

class XvaAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA";

    XvaAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) { setLabel(LABEL); }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    void checkConfigurations(const boost::shared_ptr<Portfolio>& portfolio);
    
protected:
    boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError);
    void buildScenarioGenerator(bool continueOnError);

    void initCubeDepth();
    void initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth);    

    void initClassicRun(const boost::shared_ptr<Portfolio>& portfolio);
    void buildClassicCube(const boost::shared_ptr<Portfolio>& portfolio);
    boost::shared_ptr<Portfolio> classicRun(const boost::shared_ptr<Portfolio>& portfolio);

    boost::shared_ptr<EngineFactory> amcEngineFactory(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                      const std::vector<Date>& grid);
    void buildAmcPortfolio();
    void amcRun(bool doClassicRun);

    void runPostProcessor();
    
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<EngineFactory> engineFactory_;
    boost::shared_ptr<CrossAssetModel> model_;
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<Portfolio> amcPortfolio_, classicPortfolio_;
    boost::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_, amcCube_;
    boost::shared_ptr<AggregationScenarioData> scenarioData_;
    boost::shared_ptr<CubeInterpretation> cubeInterpreter_;
    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    boost::shared_ptr<PostProcess> postProcess_;
    
    Size cubeDepth_ = 0;
    boost::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;

    bool runSimulation_ = false;
    bool runXva_ = false;
};

class XvaAnalytic : public Analytic {
public:
    XvaAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<XvaAnalyticImpl>(inputs), {"XVA", "EXPOSURE"}, inputs, false, false, false, false) {}
};

template <class T> inline boost::shared_ptr<T> Analytic::dependentAnalytic(const std::string& key) const {
    auto it = dependentAnalytics_.find(key);
    QL_REQUIRE(it != dependentAnalytics_.end(), "Could not find dependent Analytic " << key);
    boost::shared_ptr<T> analytic = boost::dynamic_pointer_cast<T>(it->second);
    QL_REQUIRE(analytic, "Could not cast analytic for key " << key);
    return analytic;
}


} // namespace analytics
} // namespace oreplus
