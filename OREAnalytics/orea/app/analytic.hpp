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

#include <iostream>

// #include <orepsensi/orea/scenario/sensitivityscenariodataplus.hpp>
// #include <orepsensi/orea/engine/parsensitivityanalysis.hpp>
// #include <orepsensi/orea/engine/sensitivityanalysispm.hpp>
// #include <orepsensi/orea/engine/sensitivityanalysis.hpp>

namespace ore {
namespace analytics {

class Analytic {
public:

    typedef std::map<std::string, std::map<std::string, boost::shared_ptr<ore::data::InMemoryReport>>>
        analytic_reports;

    struct Configurations {
        boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams;
        boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams;
        boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData;
    };

    //! Constructor
    Analytic(//! Label for logging purposes
             const std::string& label,
             //! The types of all (sub) analytics covered by this Analytic object
             //! e.g. NPV, CASHFLOW, CASHFLOWNPV, etc., covered by the PricingAnalytic
             const std::vector<std::string>& analyticTypes,
             //! Any inputs required by this Analytic
             const boost::shared_ptr<InputParameters>& inputs,
             //! Stream for optional output
             std::ostream& out = std::cout,
             //! Flag to indicate whether a simularion config file is required for this analytic
             bool simulationConfig = false,
             //! Flag to indicate whether a sensitivity config file is required for this analytic
             bool sensitivityConfig = false)
        : label_(label), types_(analyticTypes), inputs_(inputs), out_(out),
          simulationConfig_(simulationConfig), sensitivityConfig_(sensitivityConfig),
          tab_(50), progressBarWidth_(72 - std::min<Size>(tab_, 67)) {
        // This call does not work with pure virtual functions, compiler error.
        // setUpConfigurations();
        // With a non-pure virtual function it calls the base class version here, not the overridden ones ?
        // So I am currently calling the overriden setUpConfigurations() in each derived class.
    }

    virtual ~Analytic() {}

    //! Run only those analytic types that are inclcuded in the runTypes vector, run all if the runType vector is empty 
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::vector<std::string>& runTypes = {}) = 0;

    // we usually build configurations here (today's market params, scenario sim market params, sensitivity scenasrio data)
    virtual void buildConfigurations() {};
    virtual void setUpConfigurations() = 0;
    virtual void buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const boost::shared_ptr<CurveConfigurations>& curveConfig, 
                             const bool marketRequired = true);
    virtual void buildPortfolio();
    virtual void marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr = nullptr);

    //! Inspectors
    const std::string& label() const { return label_; }
    const std::vector<std::string>& analyticTypes() const { return types_; }
    const boost::shared_ptr<ore::data::Market>& market() const { return market_; };
    const boost::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; };
    virtual std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    const boost::shared_ptr<ore::data::Loader>& loader() const { return loader_; };
    Configurations configurations() const { return configurations_; }

    //! Result reports
    const analytic_reports& reports() const { return reports_; };
    const bool getWriteIntermediateReports() const { return writeIntermediateReports_; }
    void setWriteIntermediateReports(const bool flag) { writeIntermediateReports_ = flag; }

protected:
    //! label for logging purposes primarily
    const std::string label_;
    //! list of analytic types run by this analytic
    std::vector<std::string> types_;
    //! contains all the input parameters for the run
    boost::shared_ptr<InputParameters> inputs_;
    std::ostream& out_;

    Configurations configurations_;
    boost::shared_ptr<ore::data::Market> market_;
    boost::shared_ptr<ore::data::Loader> loader_;
    boost::shared_ptr<ore::data::Portfolio> portfolio_;

    analytic_reports reports_;

    //! Booleans to determine if these configs are needed
    bool simulationConfig_ = false;
    bool sensitivityConfig_ = false;

    //! flag to replace trades with schedule trades
    bool replaceScheduleTrades_ = false;

    //! Whether to write intermediate reports or not.
    //! This would typically be used when the analytic is being called by another analytic
    //! and that parent/calling analytic will be writing its own set of intermediate reports
    bool writeIntermediateReports_ = true;

    //! build an engine factory
    virtual boost::shared_ptr<ore::data::EngineFactory> engineFactory();

    //! optional output formatting
    Size tab_;
    Size progressBarWidth_;
};
    
/*! Pricing-type analytics
  \todo integrate multe-threaded sensi analysis
  \todo align pillars for par sensitivity analysis
  \todo add par sensi analysis
*/
class PricingAnalytic : public Analytic {
public:
    PricingAnalytic(const boost::shared_ptr<InputParameters>& inputs, 
                    std::ostream& out = std::cout,
                    bool applySimmExemptions = false)
        : Analytic("PRICING", {"NPV", "ADDITIONAL_RESULTS", "CASHFLOW", "CASHFLOWNPV", "SENSITIVITY", "MARKETDATA"}, inputs, out) {
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::vector<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

protected:
    virtual boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
};

class VarAnalytic : public Analytic {
public:
    VarAnalytic(const boost::shared_ptr<InputParameters>& inputs, std::ostream& out = std::cout)
        : Analytic("VAR", {"DELTA-VAR", "DELTA-GAMMA-VAR", "DELTA-GAMMA-NORMAL-VAR", "MONTE-CARLO-VAR"}, inputs, out) {
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::vector<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class XvaAnalytic : public Analytic {
public:
    XvaAnalytic(const boost::shared_ptr<InputParameters>& inputs, std::ostream& out = std::cout)
        : Analytic("XVA", {"EXPOSURE", "CVA", "DVA", "FVA", "COLVA", "COLLATERALFLOOR", "KVA", "MVA"}, inputs, out) {
        setUpConfigurations();
    }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::vector<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    
private:
    virtual boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError);
    void buildScenarioGenerator(bool continueOnError);
    void initCube(boost::shared_ptr<NPVCube>& cube, const std::vector<std::string>& ids, Size cubeDepth);
    // override this for AMC ?
    virtual void buildCube();
    void runPostProcessor();
    
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<CrossAssetModel> model_;
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_;
    boost::shared_ptr<InMemoryAggregationScenarioData> scenarioData_;
    boost::shared_ptr<CubeInterpretation> cubeInterpreter_;
    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    boost::shared_ptr<PostProcess> postProcess_;
    
    Size cubeDepth_;
    boost::shared_ptr<DateGrid> grid_;
    Size samples_;
};

} // namespace analytics
} // namespace oreplus
