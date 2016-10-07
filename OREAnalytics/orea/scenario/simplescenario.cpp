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

#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

// Simple Scenario class

bool SimpleScenario::has(const RiskFactorKey& key) const { return data_.find(key) != data_.end(); }

void SimpleScenario::add(const RiskFactorKey& key, Real value) {
    // TODO: why can we not overwrite??
    QL_REQUIRE(!has(key), "key " << key << " already used, we do not overwrite");
    data_.emplace(key, value);
    keys_.emplace_back(key);
}

Real SimpleScenario::get(const RiskFactorKey& key) const {
    auto it = data_.find(key);
    QL_REQUIRE(it != data_.end(), "Scenario does not provide data for key " << key);
    return it->second;
}
}
}
