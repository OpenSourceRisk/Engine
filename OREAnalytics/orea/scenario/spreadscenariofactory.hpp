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

#pragma once

#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/spreadscenario.hpp>

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace analytics {

//! Factory class for building spread scenario objects
class SpreadScenarioFactory : public ScenarioFactory {
public:
    explicit SpreadScenarioFactory(
        const boost::shared_ptr<ScenarioFactory>& baseFactory = boost::make_shared<SimpleScenarioFactory>());

    const boost::shared_ptr<Scenario> buildScenario(Date asof, const std::string& label = "",
                                                    Real numeraire = 0.0) const;

private:
    boost::shared_ptr<ScenarioFactory> baseFactory_;
};

} // namespace analytics
} // namespace ore
