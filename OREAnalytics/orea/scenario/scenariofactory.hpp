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

/*! \file orea/scenario/scenariofactory.hpp
    \brief factory classes for scenarios
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

namespace openriskengine {
namespace analytics {

//! Scenario factory base class
/*! \ingroup scenario
*/
class ScenarioFactory {
public:
    //! Default destructor
    virtual ~ScenarioFactory(){};
    //! Build a scenario instance without filling it
    virtual const boost::shared_ptr<Scenario> buildScenario(Date asof, const std::string& label = "",
                                                            Real numeraire = 0.0) const = 0;
};

} // namesapce scenario
} // namespace openriskengine
