/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file simulation/simmarket.hpp
    \brief A Market class that can be Simulated
    \ingroup simulation
*/

#pragma once

#include <ored/marketdata/marketimpl.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>
#include <ored/configuration/conventions.hpp>

namespace openriskengine {
namespace analytics {

//! Simulation Market
/*!
  A Simulation Market is a MarketImpl which is used for pricing under scenarios.
  It has an update method which is used to generate or retrieve a new market scenario,
  to apply the scenario to its term structures and to notify all termstructures and
  instruments of this change so that the instruments are recalculated with the NPV call.

  \ingroup simulation
 */
class SimMarket : public data::MarketImpl {
public:
    SimMarket(const Conventions& conventions) : MarketImpl(conventions), numeraire_(1.0) {}

    //! Generate or retrieve market scenario, update market, notify termstructures
    virtual void update(const Date&) = 0;

    //! Return current numeraire value
    Real numeraire() { return numeraire_; }

protected:
    Real numeraire_;
};
}
}
