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
#include <orea/app/xvarunner.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/ored.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/tradefactory.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;

class SensitivityScenarioData;
class SensitivityAnalysis;

//! Orchestrates the processes covered by ORE, data loading, analytics and reporting
/*! \ingroup app
 */
class OREApp {
public:
    //! Constructor
    OREApp(boost::shared_ptr<Parameters> params, std::ostream& out = std::cout);
    //! Destructor
    virtual ~OREApp();
    //! generates XVA reports for a given portfolio and market
    virtual int run();

    //! Load curve configurations from xml file, build t0 market using market data provided.
    //! If any of the passed vectors are empty fall back on OREApp::buildMarket() and use market/fixing data files
    void buildMarket(const std::string& todaysMarketXML = "", const std::string& curveConfigXML = "",
                     const std::string& conventionsXML = "",
                     const std::vector<std::string>& marketData = std::vector<std::string>(),
                     const std::vector<std::string>& fixingData = std::vector<std::string>());

    //! Get the t0 market
    // TODO: This should return a market, not market impl
    boost::shared_ptr<ore::data::MarketImpl> getMarket() const;

    //! build engine factory for a given market from an XML String
    boost::shared_ptr<ore::data::EngineFactory>
    buildEngineFactoryFromXMLString(const boost::shared_ptr<ore::data::Market>& market,
                                    const std::string& pricingEngineXML, const bool generateAdditionalResults = false);

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
    boost::shared_ptr<TodaysMarketParameters> getMarketParameters();
    //! load reference data
    void getReferenceData();
    //! build engine factory for a given market
    virtual boost::shared_ptr<EngineFactory> buildEngineFactory(const boost::shared_ptr<Market>& market,
                                                                const string& groupName = "setup",
                                                                const bool generateAdditionalResults = false) const;
    //! build trade factory
    boost::shared_ptr<TradeFactory> buildTradeFactory() const;
    //! build portfolio for a given market
    boost::shared_ptr<Portfolio> buildPortfolio(const boost::shared_ptr<EngineFactory>& factory, bool buildFailedTrades = false);
    //! load portfolio from file(s)
    boost::shared_ptr<Portfolio> loadPortfolio(bool loadFailedTrades = false);

    //! get the XVA runner
    virtual boost::shared_ptr<XvaRunner> getXvaRunner();
    //! generate NPV cube
    virtual void generateNPVCube();
    //! get an instance of an aggregationScenarioData class
    virtual void initAggregationScenarioData();
    //! get an instance of a cube class
    virtual void initCube(boost::shared_ptr<NPVCube>& cube, const std::vector<std::string>& ids, const Size cubeDepth);
    //! set depth of NPV cube in cubeDepth_
    virtual void setCubeDepth(const boost::shared_ptr<ScenarioGeneratorData>& sgd);
    //! build an NPV cube
    virtual void buildNPVCube();
    //! initialise NPV cube generation
    virtual void initialiseNPVCubeGeneration(boost::shared_ptr<Portfolio> portfolio);
    //! load simMarketData
    boost::shared_ptr<ScenarioSimMarketParameters> getSimMarketData();
    //! load scenarioGeneratorData
    boost::shared_ptr<ScenarioGeneratorData> getScenarioGeneratorData();
    //! build CAM
    boost::shared_ptr<QuantExt::CrossAssetModel> buildCam(boost::shared_ptr<Market> market,
                                                          const bool continueOnCalibrationError);
    //! load pricing engine data
    boost::shared_ptr<EngineData> getEngineData(string groupName);
    //! load cross asset model data
    boost::shared_ptr<CrossAssetModelData> getCrossAssetModelData();

    //! build scenarioGenerator
    virtual boost::shared_ptr<ScenarioGenerator>
    buildScenarioGenerator(boost::shared_ptr<Market> market,
                           boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                           boost::shared_ptr<ScenarioGeneratorData> sgd, const bool continueOnCalibrationError);

    //! load in scenarioData
    virtual void loadScenarioData();
    //! load in cube
    virtual void loadCube();
    //! run postProcessor to generate reports from cube
    virtual void runPostProcessor();

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
    void writeCube(boost::shared_ptr<NPVCube> cube, const std::string& cubeFileParam);
    //! write out scenarioData
    void writeScenarioData();
    //! write out base scenario
    void writeBaseScenario();
    //! load in nettingSet data
    boost::shared_ptr<NettingSetManager> initNettingSetManager();

    //! Get report writer
    /*! This calls the private method getReportWriterImpl() which returns the
        actual ReportWriter implementation. The private method is virtual and
        can be overridden in derived classes to provide a derived ReportWriter
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
    virtual std::map<std::string, boost::shared_ptr<AbstractTradeBuilder>>
    getExtraTradeBuilders(const boost::shared_ptr<TradeFactory>& = {}) const {
        return {};
    };
    //! Get parametric var calculator
    virtual boost::shared_ptr<ParametricVarCalculator>
    buildParametricVarCalculator(const std::map<std::string, std::set<std::string>>& tradePortfolio,
                                 const std::string& portfolioFilter,
                                 const boost::shared_ptr<SensitivityStream>& sensitivities,
                                 const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                                 const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                                 const Size mcSeed, const bool breakdown, const bool salvageCovarianceMatrix);
    /*! Generate market data (based on the market data available in the loader passed as an argument) and return the
     * generated data as a new loader, a nullptr can be returned if no data is generated */
    virtual boost::shared_ptr<Loader> generateMarketData(const boost::shared_ptr<Loader>& loader) { return nullptr; }

    void writePricingStats(const std::string& filename, const boost::shared_ptr<Portfolio>& portfolio);

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
    bool continueOnError_;
    bool lazyMarketBuilding_;
    std::string inputPath_;
    std::string outputPath_;
    bool buildFailedTrades_;

    boost::shared_ptr<Market> market_;               // T0 market
    boost::shared_ptr<EngineFactory> engineFactory_; // engine factory linked to T0 market
    boost::shared_ptr<Portfolio> portfolio_;         // portfolio linked to T0 market
    boost::shared_ptr<Conventions> conventions_;
    boost::shared_ptr<TodaysMarketParameters> marketParameters_;
    boost::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;

    boost::shared_ptr<ScenarioSimMarket> simMarket_; // sim market
    boost::shared_ptr<Portfolio> simPortfolio_;      // portfolio linked to sim market

    boost::shared_ptr<DateGrid> grid_;
    Size samples_;

    Size cubeDepth_; // depth of cube_ defined below
    bool storeFlows_, useCloseOutLag_, useMporStickyDate_, storeSp_;

    boost::shared_ptr<NPVCube> cube_;           // cube to store results on trade level (e.g. NPVs, flows)
    boost::shared_ptr<NPVCube> nettingSetCube_; // cube to store results on netting set level
    boost::shared_ptr<NPVCube> cptyCube_; // cube to store results at counterparty level (e.g. survival probability)
    boost::shared_ptr<AggregationScenarioData> scenarioData_;
    boost::shared_ptr<PostProcess> postProcess_;
    boost::shared_ptr<CubeInterpretation> cubeInterpreter_;
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;

    //! Populated if a sensitivity analysis is performed.
    boost::shared_ptr<SensitivityRunner> sensitivityRunner_;

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;

private:
    virtual ReportWriter* getReportWriterImpl() const { return new ReportWriter(); }
};
} // namespace analytics
} // namespace ore
