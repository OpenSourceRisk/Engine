/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadscenariofactory.hpp
    \brief factory class for spread scenarios
    \ingroup scenario
*/

#include <orea/scenario/spreadscenariofactory.hpp>

namespace ore {
namespace analytics {

SpreadScenarioFactory::SpreadScenarioFactory(const boost::shared_ptr<ScenarioFactory>& baseFactory)
    : baseFactory_(baseFactory) {
    QL_REQUIRE(baseFactory_, "SpreadScenarioFactory: no base factory given");
}

const boost::shared_ptr<Scenario> SpreadScenarioFactory::buildScenario(Date asof, const std::string& label,
                                                                       Real numeraire) const {
    return boost::make_shared<SpreadScenario>(baseFactory_->buildScenario(asof, label, numeraire),
                                              baseFactory_->buildScenario(asof, label, numeraire));
}

} // namespace analytics
} // namespace ore
