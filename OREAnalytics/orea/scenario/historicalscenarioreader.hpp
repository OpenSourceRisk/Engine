/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/historicalscenarioreader.hpp
    \brief historical scenario reader
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>

namespace ore {
namespace analytics {

//! Base Class for reading historical scenarios
class HistoricalScenarioReader {
public:
    //! Destructor
    virtual ~HistoricalScenarioReader() {}
    //! Return true if there is another Scenario to read and move to it
    virtual bool next() = 0;
    //! Return the current scenario's date if reader is still valid and `Null<Date>()` otherwise
    virtual QuantLib::Date date() const = 0;
    //! Return the current scenario if reader is still valid and `nullptr` otherwise
    virtual QuantLib::ext::shared_ptr<ore::analytics::Scenario> scenario() const = 0;
    // load the scenarios
    virtual void load(
        //! Simulation parameters - to provide list of curves to request
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simParams,
        //! Todays market params to provide the discount curves
        const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& marketParams){};
};

} // namespace analytics
} // namespace ore
