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
#include <orea/aggregation/all.hpp>
#include <orea/app/parameters.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/ored.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

class SensitivityScenarioData;
class SensitivityAnalysis;

class OREApp {
public:
    OREApp(boost::shared_ptr<Parameters> params, std::ostream& out = std::cout)
        : params_(params), out_(out), cubeDepth_(0) {
        tab_ = 40;
        progressBarWidth_ = 72 - std::min<Size>(tab_, 67);

        asof_ = parseDate(params->get("setup", "asofDate"));
        Settings::instance().evaluationDate() = asof_;
    }
    virtual ~OREApp() {}
    //! generates XVA reports for a given portfolio and market
    int run();
    //! read setup from params_
    virtual void readSetup();
    //! set up logging
    void setupLog();
    //! load market conventions
    void getConventions();
    //! load market parameters
    void getMarketParameters();
    //! build today's market
    void buildMarket();
    //! build engine factory for a given market
    virtual boost::shared_ptr<EngineFactory> buildEngineFactory(const boost::shared_ptr<Market>& market,
                                                                const string& groupName = "setup");
    //! build trade factory
    virtual boost::shared_ptr<TradeFactory> buildTradeFactory();
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

    //! run sensitivity analysis and write out reports
    virtual void runSensitivityAnalysis();
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

    //! write out additional reports
    virtual void writeAdditionalReports() {}

protected:
    //! Get report writer
    virtual boost::shared_ptr<ReportWriter> getReportWriter();

    //! Initialize input parameters to the sensitivities analysis
    void sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                              boost::shared_ptr<SensitivityScenarioData>& sensiData,
                              boost::shared_ptr<EngineData>& engineData, boost::shared_ptr<Portfolio>& sensiPortfolio,
                              string& marketConfiguration);

    //! Write out some standard sensitivities reports
    void sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis);

    //! Get parametric var calculator
    virtual boost::shared_ptr<ParametricVarCalculator>
    buildParametricVarCalculator(const std::map<std::string, std::set<std::string>>& tradePortfolio,
                                 const std::string& portfolioFilter,
                                 const boost::shared_ptr<SensitivityData>& sensitivities,
                                 const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                                 const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                                 const Size mcSeed, const bool breakdown, const bool salvageCovarianceMatrix);

    Size tab_, progressBarWidth_;
    Date asof_;
    //! ORE Input parameters
    boost::shared_ptr<Parameters> params_;
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
};
} // namespace analytics
} // namespace ore
