/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file orea/app/oreapp.hpp
  \brief Open Risk Engine App
  \ingroup app
 */

#pragma once

#include <boost/make_shared.hpp>
#include <iostream>
#include <orea/aggregation/collateralaccount.hpp>
#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/aggregation/postprocess.hpp>
#include <orea/app/parameters.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/sensitivityrunner.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/ored.hpp>
#include <ored/portfolio/tradefactory.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

class SensitivityScenarioData;
class SensitivityAnalysis;

class OREApp {
public:
    //! Constructor
    OREApp(boost::shared_ptr<Parameters> params, std::ostream& out = std::cout);
    //! Destructor
    virtual ~OREApp();
    //! generates XVA reports for a given portfolio and market
    virtual int run();

protected:
    //! read setup from params_
    virtual void readSetup();
    //! set up logging
    void setupLog();
    //! remove logs
    void closeLog();
    //! load market conventions
    void getConventions();
    //! load market parameters
    void getMarketParameters();
    //! build today's market
    void buildMarket();
    //! build engine factory for a given market
    virtual boost::shared_ptr<EngineFactory> buildEngineFactory(const boost::shared_ptr<Market>& market,
                                                                const string& groupName = "setup") const;
    //! build trade factory
    boost::shared_ptr<TradeFactory> buildTradeFactory() const;
    //! build portfolio for a given market
    boost::shared_ptr<Portfolio> buildPortfolio(const boost::shared_ptr<EngineFactory>& factory);

    //! generate NPV cube
    void generateNPVCube();
    //! get an instance of an aggregationScenarioData class
    virtual void initAggregationScenarioData();
    //! get an instance of a cube class
    virtual void initCube();
    //! build an NPV cube
    virtual void buildNPVCube();
    //! load simMarketData
    boost::shared_ptr<ScenarioSimMarketParameters> getSimMarketData();
    //! load scenarioGeneratorData
    boost::shared_ptr<ScenarioGeneratorData> getScenarioGeneratorData();
    //! build scenarioGenerator
    virtual boost::shared_ptr<ScenarioGenerator>
    buildScenarioGenerator(boost::shared_ptr<Market> market,
                           boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                           boost::shared_ptr<ScenarioGeneratorData> sgd);

    //! load in scenarioData
    virtual void loadScenarioData();
    //! load in cube
    virtual void loadCube();
    //! run postProcessor to generate reports from cube
    void runPostProcessor();

    //! run stress tests and write out report
    virtual void runStressTest();
    //! run parametric var and write out report
    void runParametricVar();

    //! write out initial (pre-cube) reports
    void writeInitialReports();
    //! write out XVA reports
    void writeXVAReports();
    //! write out DIM reports
    void writeDIMReport();
    //! write out cube
    void writeCube();
    //! write out scenarioData
    void writeScenarioData();
    //! write out base scenario
    void writeBaseScenario();
    //! load in nettingSet data
    boost::shared_ptr<NettingSetManager> initNettingSetManager();

    //! Get report writer
    /*! This calls the private method getReportWriterImpl() which returns the 
        actual ReportWriter implementation. The private method is virtual and 
        can be overridden in derived classes to provide a dervied ReportWriter 
        instance. This method is not virtual and can be hidden in derived 
        classes by a method with the same name. This method can then return a 
        shared pointer to the derived ReportWriter class.

        For example, we can have the following in a derived class:
        \code{.cpp}
        class MyApp : public OREApp {
            // stuff
        protected:
            boost::shared_ptr<MyReportWriter> getReportWriter() {
                return boost::shared_ptr<MyReportWriter>(getReportWriterImpl());
            }
        private:
            virtual MyReportWriter* getReportWriterImpl() const {
                return new MyReportWriter();
            }
        };
        \endcode
        where we have our own special report writer class MyReportWriter that 
        derives from ReportWriter or any class in its hierarchy.
    */
    boost::shared_ptr<ReportWriter> getReportWriter() const;
    //! Get sensitivity runner
    virtual boost::shared_ptr<SensitivityRunner> getSensitivityRunner();
    //! Add extra engine builders
    virtual std::vector<boost::shared_ptr<EngineBuilder>> getExtraEngineBuilders() const { return {}; };
    //! Add extra leg builders
    virtual std::vector<boost::shared_ptr<LegBuilder>> getExtraLegBuilders() const { return {}; };
    //! Add extra trade builders
    virtual std::map<std::string, boost::shared_ptr<AbstractTradeBuilder>> getExtraTradeBuilders() const { return {}; };

    //! Get parametric var calculator
    virtual boost::shared_ptr<ParametricVarCalculator>
    buildParametricVarCalculator(const std::map<std::string, std::set<std::string>>& tradePortfolio,
                                 const std::string& portfolioFilter,
                                 const boost::shared_ptr<SensitivityStream>& sensitivities,
                                 const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                                 const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                                 const Size mcSeed, const bool breakdown, const bool salvageCovarianceMatrix);

    Size tab_, progressBarWidth_;
    //! ORE Input parameters
    boost::shared_ptr<Parameters> params_;
    Date asof_;
    std::ostream& out_;
    bool writeInitialReports_;
    bool simulate_;
    bool buildSimMarket_;
    bool xva_;
    bool writeDIMReport_;
    bool sensitivity_;
    bool stress_;
    bool parametricVar_;
    bool writeBaseScenario_;
    std::string inputPath_;
    std::string outputPath_;

    boost::shared_ptr<Market> market_;               // T0 market
    boost::shared_ptr<EngineFactory> engineFactory_; // engine factory linked to T0 market
    boost::shared_ptr<Portfolio> portfolio_;         // portfolio linked to T0 market
    Conventions conventions_;
    TodaysMarketParameters marketParameters_;

    boost::shared_ptr<ScenarioSimMarket> simMarket_; // sim market
    boost::shared_ptr<Portfolio> simPortfolio_;      // portfolio linked to sim market

    boost::shared_ptr<DateGrid> grid_;
    Size samples_;

    Size cubeDepth_;
    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<AggregationScenarioData> scenarioData_;
    boost::shared_ptr<PostProcess> postProcess_;

private:
    virtual ReportWriter* getReportWriterImpl() const {
        return new ReportWriter();
    }
};
} // namespace analytics
} // namespace ore
