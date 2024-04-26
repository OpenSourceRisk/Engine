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

#include <orea/scenario/clonescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

CloneScenarioFactory::CloneScenarioFactory(const QuantLib::ext::shared_ptr<Scenario>& baseScenario)
    : baseScenario_(baseScenario) {
    QL_REQUIRE(baseScenario_ != NULL, "base scenario pointer must not be NULL");
}

const QuantLib::ext::shared_ptr<Scenario>
CloneScenarioFactory::buildScenario(Date asof, bool isAbsolute, const std::string& label, Real numeraire) const {
    QuantLib::ext::shared_ptr<Scenario> newScen = baseScenario_->clone();
    QL_REQUIRE(asof == newScen->asof(),
               "unexpected asof date (" << asof << "), does not match base - " << baseScenario_->asof());
    newScen->label(label);
    QL_REQUIRE((label == newScen->label()) || (label == ""), "CloneScenarioFactory has not updated scenario label");
    if (numeraire != 0.0)
        newScen->setNumeraire(numeraire);
    newScen->setAbsolute(isAbsolute);
    return newScen;
}
} // namespace analytics
} // namespace ore
