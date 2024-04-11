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

/*! \file scenario/stressscenariogenerator.hpp
    \brief Stress scenario generation
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <ored/marketdata/market.hpp>

namespace ore {
namespace analytics {
using namespace data;

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
  - Likewise, Cap/Floor volatility stress tests are applied in the optionlet domain.

  \ingroup scenario
 */
class StressScenarioGenerator : public ShiftScenarioGenerator {
public:
    //! Constructor
    StressScenarioGenerator(const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                            const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
                            const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                            const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                            const QuantLib::ext::shared_ptr<ScenarioFactory>& stressScenarioFactory,
                            const QuantLib::ext::shared_ptr<Scenario>& baseScenarioAbsolute = nullptr);
    //! Default destructor
    ~StressScenarioGenerator() {}

private:
    void generateScenarios();
    void addFxShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addEquityShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addDiscountCurveShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addIndexCurveShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addYieldCurveShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addFxVolShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addEquityVolShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addSwaptionVolShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addCapFloorVolShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addSecuritySpreadShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addDefaultCurveShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addRecoveryRateShifts(StressTestScenarioData::StressTestData& data, QuantLib::ext::shared_ptr<Scenario>& scenario);
    void addSurvivalProbabilityShifts(StressTestScenarioData::StressTestData& data,
                                      QuantLib::ext::shared_ptr<Scenario>& scenario);

    QuantLib::ext::shared_ptr<StressTestScenarioData> stressData_;
    QuantLib::ext::shared_ptr<ScenarioFactory> stressScenarioFactory_;
    QuantLib::ext::shared_ptr<Scenario> baseScenarioAbsolute_;
};
} // namespace analytics
} // namespace ore
