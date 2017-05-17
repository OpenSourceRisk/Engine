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
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

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

class SensitivityAnalysis {
public:
    //! Constructor
    SensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                        const boost::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                        const boost::shared_ptr<ore::data::EngineData>& engineData,
                        const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                        const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                        const Conventions& conventions, const bool recalibrateModels,
                        const bool nonShiftedBaseCurrencyConversion = false);

    //! Generate the Sensitivities
    void generateSensitivities();

    //! Return set of trades analysed
    const std::set<std::string>& trades() const;

    //! Return unique set of factors shifted
    const std::map<std::string, QuantLib::Real>& factors() const;

    //! Return base NPV by trade, before shift
    const std::map<std::string, Real>& baseNPV() const;

    //! Return delta/vega (first order sensitivity times shift) by trade/factor pair
    const std::map<std::pair<std::string, std::string>, Real>& delta() const;

    //! Return gamma (second order sensitivity times shift^2) by trade/factor pair
    const std::map<std::pair<std::string, std::string>, Real>& gamma() const;

    //! Return cross gamma (mixed second order sensitivity times shift^2) by trade/factor1/factor2
    const std::map<std::tuple<std::string, std::string, std::string>, Real>& crossGamma() const;

    //! Write "raw" NPV by trade/scenario (contains base, up and down shift scenarios)
    void writeScenarioReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

    //! Write deltas and gammas by trade/factor pair
    void writeSensitivityReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

    //! Write cross gammas by trade/factor1/factor2
    void writeCrossGammaReport(const boost::shared_ptr<ore::data::Report>& report, Real outputThreshold = 0.0);

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

protected:
    //! initialize the various components that will be passed to the sensitivities valuation engine
    void initialize(boost::shared_ptr<NPVCube>& cube);
    //! initialize the cube with the appropriate dimensions
    virtual void initializeCube(boost::shared_ptr<NPVCube>& cube) const;
    //! build engine factory
    virtual boost::shared_ptr<EngineFactory>
    buildFactory(const std::vector<boost::shared_ptr<EngineBuilder>> extraBuilders = {}) const;
    //! reset and rebuild the portfolio to make use of the appropriate engine factory
    virtual void resetPortfolio(const boost::shared_ptr<EngineFactory>& factory);
    //! build the ScenarioSimMarket that will be used by ValuationEngine
    virtual void initializeSimMarket();
    //! initialize the SensitivityScenarioGenerator that determines which sensitivities to compute
    virtual void initializeSensitivityScenarioGenerator(boost::shared_ptr<ScenarioFactory> scenFact = {});

    //! build valuation calculators for valuation engine
    std::vector<boost::shared_ptr<ValuationCalculator>> buildValuationCalculators() const;
    //! collect the sensitivity results from the cube and populate appropriate containers
    void collectResultsFromCube(const boost::shared_ptr<NPVCube>& cube);

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

    // base NPV by trade
    std::map<std::string, Real> baseNPV_;
    // NPV respectively sensitivity by trade and factor
    std::map<std::pair<string, string>, Real> upNPV_, downNPV_, delta_, gamma_;
    // cross gamma by trade, factor1, factor2
    std::map<std::tuple<string, string, string>, Real> crossNPV_, crossGamma_;
    // unique set of factors
    std::map<std::string, QuantLib::Real> factors_;
    // unique set of trades
    std::set<std::string> trades_;
    // if true, convert sensis to base currency using the original (non-shifted) FX rate
    bool nonShiftedBaseCurrencyConversion_;
    //! the engine data (provided as input, needed to construct the engine factory)
    boost::shared_ptr<EngineData> engineData_;
    //! the portfolio (provided as input)
    boost::shared_ptr<Portfolio> portfolio_;
    //! initializationFlag
    bool initialized_, computed_;
    //! model builders
    std::set<boost::shared_ptr<ModelBuilder>> modelBuilders_;
};
} // namespace analytics
} // namespace ore
