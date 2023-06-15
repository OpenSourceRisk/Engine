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

/*! \file engine/sensitivityanalysis.hpp
    \brief Perform sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvsensicube.hpp>
#include <orea/cube/sensitivitycube.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/progressbar.hpp>

#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {

class ScenarioFactory;
class ValuationCalculator;

//! Sensitivity Analysis
/*!
  This class wraps functionality to perform a sensitivity analysis for a given portfolio.
  It comprises
  - building the "simulation" market to which sensitivity scenarios are applied
  - building the portfolio linked to this simulation market
  - generating sensitivity scenarios
  - running the scenario "engine" to apply these and compute the NPV impacts of all required shifts
  - compile first and second order sensitivities for all factors and all trades
  - fill result structures that can be queried

  \ingroup simulation
*/

class SensitivityAnalysis : public ore::data::ProgressReporter {
public:
    //! Constructor
    SensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                        const boost::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                        const boost::shared_ptr<ore::data::EngineData>& engineData,
                        const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                        const boost::shared_ptr<SensitivityScenarioData>& sensitivityData, const bool recalibrateModels,
                        const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
                        const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams = nullptr,
                        const bool nonShiftedBaseCurrencyConversion = false,
                        const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                        const bool continueOnError = false, bool dryRun = false);

    virtual ~SensitivityAnalysis() {}

    //! Generate the Sensitivities
    virtual void generateSensitivities(boost::shared_ptr<NPVSensiCube> cube = boost::shared_ptr<NPVSensiCube>());

    //! The ASOF date for the sensitivity analysis
    virtual const QuantLib::Date asof() const { return asof_; }

    //! The market configuration string
    virtual const std::string marketConfiguration() const { return marketConfiguration_; }

    //! A getter for the sim market
    virtual const boost::shared_ptr<ScenarioSimMarket> simMarket() const { return simMarket_; }

    //! A getter for SensitivityScenarioGenerator
    virtual const boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator() const {
        return scenarioGenerator_;
    }

    //! A getter for ScenarioSimMarketParameters
    virtual const boost::shared_ptr<ScenarioSimMarketParameters> simMarketData() const { return simMarketData_; }

    //! A getter for SensitivityScenarioData
    virtual const boost::shared_ptr<SensitivityScenarioData> sensitivityData() const { return sensitivityData_; }

    //! override shift tenors with sim market tenors
    void overrideTenors(const bool b) { overrideTenors_ = b; }

    //! the portfolio of trades
    boost::shared_ptr<Portfolio> portfolio() const { return portfolio_; }

    //! a wrapper for the sensitivity results cube
    boost::shared_ptr<SensitivityCube> sensiCube() const { return sensiCube_; }

protected:
    //! initialize the various components that will be passed to the sensitivities valuation engine
    virtual void initialize(boost::shared_ptr<NPVSensiCube>& cube);
    //! initialize the cube with the appropriate dimensions
    virtual void initializeCube(boost::shared_ptr<NPVSensiCube>& cube) const;
    //! build engine factory
    virtual boost::shared_ptr<EngineFactory> buildFactory() const;
    //! reset and rebuild the portfolio to make use of the appropriate engine factory
    virtual void resetPortfolio(const boost::shared_ptr<EngineFactory>& factory);
    /*! build the ScenarioSimMarket that will be used by ValuationEngine and
     * initialize the SensitivityScenarioGenerator that determines which sensitivities to compute */
    virtual void initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact = {});

    //! build valuation calculators for valuation engine
    virtual std::vector<boost::shared_ptr<ValuationCalculator>> buildValuationCalculators() const;

    boost::shared_ptr<ore::data::Market> market_;
    std::string marketConfiguration_;
    Date asof_;
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    boost::shared_ptr<SensitivityScenarioData> sensitivityData_;
    bool recalibrateModels_;
    //! Optional curve configurations. Used in building the scenario sim market.
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    //! Optional todays market parameters. Used in building the scenario sim market.
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    bool overrideTenors_;

    // if true, convert sensis to base currency using the original (non-shifted) FX rate
    bool nonShiftedBaseCurrencyConversion_;
    boost::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    // if true, the processing is continued even on build errors
    bool continueOnError_;
    //! the engine data (provided as input, needed to construct the engine factory)
    boost::shared_ptr<EngineData> engineData_;
    //! the portfolio (provided as input)
    boost::shared_ptr<Portfolio> portfolio_;
    //! do dry run
    bool dryRun_;

    //! initializationFlag
    bool initialized_, computed_;
    //! model builders
    std::set<std::pair<string, boost::shared_ptr<QuantExt::ModelBuilder>>> modelBuilders_;
    //! sensitivityCube
    boost::shared_ptr<SensitivityCube> sensiCube_;
};

/*! Returns the absolute shift size corresponding to a particular risk factor \p key
    given sensitivity parameters \p sensiParams and a simulation market \p simMarket
*/
Real getShiftSize(const RiskFactorKey& key, const SensitivityScenarioData& sensiParams,
                  const boost::shared_ptr<ScenarioSimMarket>& simMarket, const std::string& marketConfiguration = "");

} // namespace analytics
} // namespace ore
