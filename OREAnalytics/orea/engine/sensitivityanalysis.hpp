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

#include <ored/portfolio/portfolio.hpp>
#include <ored/marketdata/market.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>

#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {

//! Sensitivity Analysis
/*!
  This class wraps functionality to perform a sensitivity analysis for a given portfolio.
  It comprises
  - building the "simulation" market to which sensitivity scenarios are applied
  - building the portfolio linked to this simulation market
  - generating sensitivity scenarios
  - running the scenario "engine" to apply these and compute the NPV impacts of all required shifts
  - compile first and second order sensitivities for all factors and all trades
  - convert zero rate / caplet vol sensitivities into "par" rate / cap vol sensitivities
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
                        const Conventions& conventions);

    //! Return set of trades analysed
    const std::set<std::string>& trades() { return trades_; }

    //! Return unique set of factors shifted
    const std::set<std::string>& factors() { return factors_; }

    //! Return base NPV by trade, before shift
    const std::map<std::string, Real>& baseNPV() { return baseNPV_; }

    //! Return delta/vega (first order sensitivity times shift) by trade/factor pair
    const std::map<std::pair<std::string, std::string>, Real>& delta() { return delta_; }

    //! Return par delta (first order sensitivity times shift) by trade/factor pair
    virtual const std::map<std::pair<std::string, std::string>, Real>& parDelta();

    //! Return gamma (second order sensitivity times shift^2) by trade/factor pair
    const std::map<std::pair<std::string, std::string>, Real>& gamma() { return gamma_; }

    //! Return cross gamma (mixed second order sensitivity times shift^2) by trade/factor1/factor2
    const std::map<std::tuple<std::string, std::string, std::string>, Real>& crossGamma() { return crossGamma_; }

    //! Write "raw" NPV by trade/scenario to a file (contains base, up and down shift scenarios)
    void writeScenarioReport(string fileName, Real outputThreshold = 0.0);

    //! Write deltas and gammas by trade/factor pair to a file
    void writeSensitivityReport(string fileName, Real outputThreshold = 0.0);

    //! Write cross gammas by trade/factor1/factor2 to a file
    void writeCrossGammaReport(string fileName, Real outputThreshold = 0.0);

    //! Write sensitivity of par rates w.r.t. zero rates to a file
    virtual void writeParRateSensitivityReport(string fileName) { QL_FAIL("par sensitivities not implemented"); };

    //! Run par delta conversion, zero to par rate sensi, caplet/floorlet vega to cap/floor vega
    virtual void parDeltaConversion() { QL_FAIL("par delta conversion not implemented"); }

protected:
    boost::shared_ptr<ore::data::Market> market_;
    std::string marketConfiguration_;
    Date asof_;
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    boost::shared_ptr<SensitivityScenarioData> sensitivityData_;
    Conventions conventions_;

    // base NPV by trade
    std::map<std::string, Real> baseNPV_;
    // NPV respectively sensitivity by trade and factor
    std::map<std::pair<string, string>, Real> upNPV_, downNPV_, delta_, gamma_;
    // cross gamma by trade, factor1, factor2
    std::map<std::tuple<string, string, string>, Real> crossGamma_;
    // unique set of factors
    std::set<std::string> factors_;
    // unique set of trades
    std::set<std::string> trades_;
};
}
}
