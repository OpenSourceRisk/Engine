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
  \ingroup
 */

#pragma once

#include <orea/app/parameters.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/aggregation/all.hpp>
#include <ored/ored.hpp>
#include <boost/make_shared.hpp>
#include <iostream>

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
        asof_ = parseDate(params->get("setup", "asofDate"));
        Settings::instance().evaluationDate() = asof_;
    }
    //! generates XVA reports for a given portfolio and market
    void run();
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
    virtual boost::shared_ptr<EngineFactory> buildEngineFactory(const boost::shared_ptr<Market>& market);
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
    //! load in nettingSet data
    boost::shared_ptr<NettingSetManager> initNettingSetManager();

protected:
    //! Initialize input parameters to the sensitivities analysis
    void sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                              boost::shared_ptr<SensitivityScenarioData>& sensiData,
                              boost::shared_ptr<EngineData>& engineData, boost::shared_ptr<Portfolio>& sensiPortfolio,
                              string& marketConfiguration);

    //! Write out some standard sensitivities reports
    void sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis);

    Size tab_;
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

    boost::shared_ptr<Market> market_;
    boost::shared_ptr<Portfolio> portfolio_;
    Conventions conventions_;
    TodaysMarketParameters marketParameters_;

    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<Portfolio> simPortfolio_;
    set<boost::shared_ptr<ModelBuilder>> modelBuilders_;

    boost::shared_ptr<DateGrid> grid_;
    Size samples_;

    Size cubeDepth_;
    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<AggregationScenarioData> scenarioData_;
    boost::shared_ptr<PostProcess> postProcess_;
};
}
}
