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

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<ore::data::InMemoryReport>>>
        analytic_reports;

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<NPVCube>>>
        analytic_npvcubes;

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<AggregationScenarioData>>>
        analytic_mktcubes;

    struct Configurations {
        boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams;
        boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams;
        boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData;
        boost::shared_ptr<ore::analytics::ScenarioGeneratorData> scenarioGeneratorData;
        boost::shared_ptr<ore::analytics::CrossAssetModelData> crossAssetModelData;
    };

    //! Constructors
    Analytic() {}
    Analytic(//! Label for logging purposes
             const std::string& label,
             //! The types of all (sub) analytics covered by this Analytic object
             //! e.g. NPV, CASHFLOW, CASHFLOWNPV, etc., covered by the PricingAnalytic
             const std::set<std::string>& analyticTypes,
             //! Any inputs required by this Analytic
             const boost::shared_ptr<InputParameters>& inputs,
             //! Flag to indicate whether a simularion config file is required for this analytic
             bool simulationConfig = false,
             //! Flag to indicate whether a sensitivity config file is required for this analytic
             bool sensitivityConfig = false,
             //! Flag to indicate whether a scenario generator config file is required for this analytic
             bool scenarioGeneratorConfig = false,
             //! Flag to indicate whether a cross asset model config file is required for this analytic
             bool crossAssetModelConfig = false)
        : label_(label), types_(analyticTypes), inputs_(inputs), 
          simulationConfig_(simulationConfig), sensitivityConfig_(sensitivityConfig),
          scenarioGeneratorConfig_(scenarioGeneratorConfig), crossAssetModelConfig_(crossAssetModelConfig) {
        // This call does not work with pure virtual functions, compiler error.
        // setUpConfigurations();
        // With a non-pure virtual function it calls the base class version here, not the overridden ones ?
        // So I am currently calling the overriden setUpConfigurations() in each derived class.
    }

    virtual ~Analytic() {}

    //! Run only those analytic types that are inclcuded in the runTypes vector, run all if the runType vector is empty 
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) {}

    // we can build configurations here (today's market params, scenario sim market params, sensitivity scenasrio data)
    virtual void buildConfigurations() {}
    virtual void setUpConfigurations() {}
    virtual void buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const boost::shared_ptr<CurveConfigurations>& curveConfig, 
                             const bool marketRequired = true);
    virtual void buildPortfolio();
    virtual void marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr = nullptr);

    //! Inspectors
    const std::string& label() const { return label_; }
    const std::set<std::string>& analyticTypes() const { return types_; }
    const boost::shared_ptr<ore::data::Market>& market() const { return market_; };
    // To allow SWIG wrapping
    boost::shared_ptr<MarketImpl> getMarket() const {        
        return boost::dynamic_pointer_cast<MarketImpl>(market_);
    }
    const boost::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; };
    virtual std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    const boost::shared_ptr<ore::data::Loader>& loader() const { return loader_; };
    Configurations configurations() const { return configurations_; }

    //! Result reports
    const analytic_reports& reports() const { return reports_; };
    const analytic_npvcubes& npvCubes() const { return npvCubes_; };
    const analytic_mktcubes& mktCubes() const { return mktCubes_; };
    const bool getWriteIntermediateReports() const { return writeIntermediateReports_; }
    void setWriteIntermediateReports(const bool flag) { writeIntermediateReports_ = flag; }

    //! Check whether any of the requested run types is covered by this analytic
    bool match(const std::set<std::string>& runTypes);

protected:
    //! label for logging purposes primarily
    const std::string label_;
    //! list of analytic types run by this analytic
    std::set<std::string> types_;
    //! contains all the input parameters for the run
    boost::shared_ptr<InputParameters> inputs_;
    //! Booleans to determine if these configs are needed
    bool simulationConfig_ = false;
    bool sensitivityConfig_ = false;
    bool scenarioGeneratorConfig_ = false;
    bool crossAssetModelConfig_ = false;

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

    //! build an engine factory
    virtual boost::shared_ptr<ore::data::EngineFactory> engineFactory();
};
    
/*! Pricing-type analytics
  \todo integrate multe-threaded sensi analysis
  \todo align pillars for par sensitivity analysis
  \todo add par sensi analysis
*/
class PricingAnalytic : public virtual Analytic {
public:
    PricingAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic("PRICING", {"NPV", "NPV_LAGGED", "CASHFLOW", "CASHFLOWNPV", "SENSITIVITY", "STRESS"},
                   inputs, false, false, false, false) {
        if (find(begin(types_), end(types_), "SENSITIVITY") != end(types_)) {
            simulationConfig_ = true;
            sensitivityConfig_ = true;
        }        
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    virtual void modifyPortfolio() {}

protected:
    boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
};

class VarAnalytic : public virtual Analytic {
public:
    // FIXME: Add DELTA-GAMMA-VAR (Saddlepoint method)
    VarAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic("VAR", {"VAR"}, inputs, false, false, false, false) {
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

protected:
    // boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
};

class XvaAnalytic : public virtual Analytic {
public:
    XvaAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic("XVA", {"XVA", "EXPOSURE"}, inputs, true, false, false, false) {
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    
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

} // namespace analytics
} // namespace oreplus
