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

/*! \file orea/scenario/deltascenariofactory.hpp
    \brief factory class for cloning a cached scenario
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace analytics {

//! Factory class for cloning scenario objects
/*! \ingroup scenario
 */
class DeltaScenarioFactory : public ore::analytics::ScenarioFactory {
public:
    //! Constructor
    DeltaScenarioFactory(const boost::shared_ptr<ore::analytics::Scenario>& baseScenario,
                         const boost::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory =
                             boost::make_shared<ore::analytics::SimpleScenarioFactory>());
    //! returns a new scenario, using the base scenario as a starting point
    const boost::shared_ptr<ore::analytics::Scenario> buildScenario(QuantLib::Date asof, const std::string& label = "",
                                                                    QuantLib::Real numeraire = 0.0) const override;

private:
    boost::shared_ptr<ore::analytics::Scenario> baseScenario_;
    boost::shared_ptr<ore::analytics::ScenarioFactory> scenarioFactory_;
};

} // namespace analytics
} // namespace ore
