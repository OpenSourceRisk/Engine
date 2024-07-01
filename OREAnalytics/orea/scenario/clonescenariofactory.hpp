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

/*! \file orea/scenario/clonescenariofactory.hpp
    \brief factory class for cloning a cached scenario
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>

namespace ore {
namespace analytics {

//! Factory class for cloning scenario objects
/*! \ingroup scenario
 */
class CloneScenarioFactory : public ScenarioFactory {
public:
    //! Constructor
    CloneScenarioFactory(const QuantLib::ext::shared_ptr<Scenario>& baseScenario);
    //! returns a new scenario, using the base scenario as a starting point
    const QuantLib::ext::shared_ptr<Scenario> buildScenario(Date asof, bool isAbsolute, const std::string& label = "",
                                                            Real numeraire = 0.0) const override;

private:
    QuantLib::ext::shared_ptr<Scenario> baseScenario_;
};

} // namespace analytics
} // namespace ore
