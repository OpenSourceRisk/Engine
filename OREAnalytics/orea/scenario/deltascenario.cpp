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

#include <ored/utilities/log.hpp>
#include <orea/scenario/deltascenario.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

// Delta Scenario class

void DeltaScenario::add(const ore::analytics::RiskFactorKey& key, Real value) {
    QL_REQUIRE(baseScenario_->has(key), "base scenario must also possess key");
    if (baseScenario_->get(key) != value)
        delta_->add(key, value);
}

Real DeltaScenario::get(const ore::analytics::RiskFactorKey& key) const {
    if (delta_->has(key)) {
        return delta_->get(key);
    } else {
        return baseScenario_->get(key);
    }
}

boost::shared_ptr<ore::analytics::Scenario> DeltaScenario::clone() const {
    // NOTE - we are not cloning the base here (is this appropriate?)
    boost::shared_ptr<Scenario> newDelta = delta_->clone();
    return boost::make_shared<DeltaScenario>(baseScenario_, newDelta);
}

Real DeltaScenario::getNumeraire() const {
    Real deltaNum = delta_->getNumeraire();
    if (deltaNum == 0.0)
        deltaNum = baseScenario_->getNumeraire();
    return deltaNum;
}
} // namespace analytic
} // namespace ore
