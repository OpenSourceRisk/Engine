/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file scenario/sensitivityscenariogenerator.hpp
    \brief Sensitivity scenario generation
    \ingroup scenario
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>

namespace ore {
using namespace data;
namespace analytics {

//! Stress Scenario Generator
/*!
  This class builds a vector of stress scenarios based on instructions in StressScenarioData
  and ScenarioSimMarketParameters objects passed.

  The ScenarioSimMarketParameters object determines the scope and structure of a "simulation"
  market (currencies, currency pairs, curve tenor points, vol matrix expiries and terms/strikes etc)
  to which sensitivity scenarios are applied in order to compute their NPV impact.

  The StressScenarioData object determines the structure of shift curves (shift tenor points
  can differ from the simulation market's tenor points), as well as type (relative/absolute) and size
  of shifts applied.

  The generator then produces comprehensive scenarios that can be applied to the simulation market,
  i.e. covering all quotes in the simulation market, possibly filled with "base" scenario values.

  The generator currently covers the IR/FX asset class, with shifts for the following term
  structure types:
  - FX spot rates
  - FX ATM volatilities
  - Discount curve zero rates
  - Index curve zero rates
  - Yield curve zero rates
  - Swaption ATM volatility matrices
  - Cap/Floor volatility matrices (by expiry and strike)

  Note:
  - For yield curves, the class applies shifts in the Zero rate domain only.
  - Likewise, Cap/Floor volatilitiy stress tests are applied in the optionlet domain.

  \ingroup scenario
 */
class StressScenarioGenerator : public ShiftScenarioGenerator {
public:
    //! Constructor
    StressScenarioGenerator(boost::shared_ptr<ScenarioFactory> scenarioFactory,
                            boost::shared_ptr<StressTestScenarioData> stressData,
                            boost::shared_ptr<ScenarioSimMarketParameters> simMarketData, QuantLib::Date today,
                            boost::shared_ptr<ore::data::Market> initMarket,
                            const std::string& configuration = Market::defaultConfiguration);
    //! Default destructor
    ~StressScenarioGenerator(){};

private:
    void addFxShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addDiscountCurveShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addIndexCurveShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addYieldCurveShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addFxVolShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addSwaptionVolShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);
    void addCapFloorVolShifts(StressTestScenarioData::StressTestData& data, boost::shared_ptr<Scenario>& scenario);

    boost::shared_ptr<StressTestScenarioData> stressData_;
};
}
}
