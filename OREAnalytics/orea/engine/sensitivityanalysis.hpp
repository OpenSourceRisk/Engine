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
    \brief Perfrom sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/sensitivitycube.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
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
  - write sensitivity report to a file

  \ingroup simulation
*/

class SensitivityAnalysis : public ore::data::ProgressReporter {
public:
    //! Constructor
    SensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                        const boost::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                        const boost::shared_ptr<ore::data::EngineData>& engineData,
                        const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                        const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                        const Conventions& conventions, const bool recalibrateModels,
                        const bool nonShiftedBaseCurrencyConversion = false);

    virtual ~SensitivityAnalysis() {}

    //! Generate the Sensitivities
    void generateSensitivities(boost::shared_ptr<NPVCube>& cube = boost::shared_ptr<NPVCube>());

    //! Return base NPV by trade, before shift
    Real baseNPV(std::string& id) const;

    //! Return delta/vega (first order sensitivity times shift) for a trade/factor pair
    Real delta(const std::string& trade, const std::string& factor) const;

    //! Return gamma (second order sensitivity times shift^2) for a trade/factor pair
    Real gamma(const std::string& trade, const std::string& factor) const;

    //! Return cross gamma (mixed second order sensitivity times shift^2) for a given trade/factor1/factor2
    Real crossGamma(const std::string& trade, const std::string& factor1, const std::string& factor2) const;

    //! Write "raw" NPV by trade/scenario (contains base, up and down shift scenarios)
    void writeScenarioReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

    //! Write deltas and gammas by trade/factor pair
    void writeSensitivityReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

    //! Write cross gammas by trade/factor1/factor2
    virtual void writeCrossGammaReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

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

    //! A getter for Conventions
    virtual const Conventions& conventions() const { return conventions_; }

    //! override shift tenors with sim market tenors
    void overrideTenors(const bool b) { overrideTenors_ = b; }

    //! the portfolio of trades
    boost::shared_ptr<Portfolio> portfolio() const { return portfolio_; }

    //! a wrapper for the sensivity results cube
    boost::shared_ptr<SensitivityCube> sensiCube() const { return sensiCube_; }

protected:
    //! initialize the various components that will be passed to the sensitivities valuation engine
    virtual void initialize(boost::shared_ptr<NPVCube>& cube);
    //! initialize the cube with the appropriate dimensions
    virtual void initializeCube(boost::shared_ptr<NPVCube>& cube) const;
    //! build engine factory
    virtual boost::shared_ptr<EngineFactory>
    buildFactory(const std::vector<boost::shared_ptr<EngineBuilder>> extraBuilders = {},
                 const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders = {}) const;
    //! reset and rebuild the portfolio to make use of the appropriate engine factory
    virtual void resetPortfolio(const boost::shared_ptr<EngineFactory>& factory);
    /*! build the ScenarioSimMarket that will be used by ValuationEngine and
     * initialize the SensitivityScenarioGenerator that determines which sensitivities to compute */
    virtual void initializeSimMarket(boost::shared_ptr<ScenarioFactory> scenFact = {});

    //! build valuation calculators for valuation engine
    std::vector<boost::shared_ptr<ValuationCalculator>> buildValuationCalculators() const;

    //! archive the actual shift size for each risk factor (so as to interpret results)
    void storeFactorShifts(const ShiftScenarioGenerator::ScenarioDescription& desc);
    //! returns the shift size corresponding to a particular risk factor
    Real getShiftSize(const RiskFactorKey& desc) const;

    boost::shared_ptr<ore::data::Market> market_;
    std::string marketConfiguration_;
    Date asof_;
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    boost::shared_ptr<SensitivityScenarioData> sensitivityData_;
    Conventions conventions_;
    bool recalibrateModels_, overrideTenors_;

    // if true, convert sensis to base currency using the original (non-shifted) FX rate
    bool nonShiftedBaseCurrencyConversion_;
    //! the engine data (provided as input, needed to construct the engine factory)
    boost::shared_ptr<EngineData> engineData_;
    //! the portfolio (provided as input)
    boost::shared_ptr<Portfolio> portfolio_;
    //! initializationFlag
    bool initialized_, computed_;
    //! model builders
    std::set<std::pair<string, boost::shared_ptr<ModelBuilder>>> modelBuilders_;
    //! sensitivityCube
    boost::shared_ptr<SensitivityCube> sensiCube_;
    // unique set of factors
    std::map<RiskFactorKey, QuantLib::Real> factors_;
};
} // namespace analytics
} // namespace ore
