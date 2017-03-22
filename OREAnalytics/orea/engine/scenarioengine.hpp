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

/*! \file engine/scenarioengine.hpp
    \brief Cube valuation specialised to 2-d (trades, scenarios) for sensitivity scenarios at t_0
    \ingroup simulation
*/

#pragma once

#include <orea/simulation/dategrid.hpp>
#include <orea/simulation/simmarket.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>

#include <map>

namespace ore {
namespace analytics {

//! Scenario Engine
/*!
  The scenario engine's purpose is to generate a 2-d NPV "cube".
  The time dimension is colapsed to a single date (today), and scenarios
  are sensitivity, stress or historical scenaries applied to today's market. 
  Its buildCube loops over samples->trades and prices the portfolio
  under all samples.

  The number of trades is defined by the size of the portfolio passed to buildCube().
  The number of samples is defined by the NPVCube that is passed to buildCube(), this
  can be dynamic.

  \ingroup simulation
*/
class ScenarioEngine : public ore::data::ProgressReporter {
public:
    //! Constructor
    ScenarioEngine(
        //! Valuation date
        const QuantLib::Date& today,
        //! Simulated market object
        const boost::shared_ptr<analytics::SimMarket>& simMarket,
	string baseCurrency);

    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const boost::shared_ptr<data::Portfolio>& portfolio,
        //! Object for storing the resulting NPV cube
        boost::shared_ptr<analytics::NPVCube>& outputCube);

private:
    QuantLib::Date today_;
    boost::shared_ptr<analytics::SimMarket> simMarket_;
    string baseCurrency_;
};
}
}
