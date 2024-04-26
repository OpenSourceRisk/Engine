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

#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/deltascenario.hpp>

#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace analytics {

DeltaScenarioFactory::DeltaScenarioFactory(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& baseScenario,
                                           const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory)
    : baseScenario_(baseScenario), scenarioFactory_(scenarioFactory) {
    QL_REQUIRE(baseScenario_ != nullptr, "DeltaScenarioFactory: base scenario pointer must not be NULL");
    QL_REQUIRE(scenarioFactory_ != nullptr, "DeltaScenarioFactory: scenario factory must not be NULL");
}

const QuantLib::ext::shared_ptr<ore::analytics::Scenario>
DeltaScenarioFactory::buildScenario(QuantLib::Date asof, bool isAbsolute, const std::string& label,
                                    QuantLib::Real numeraire) const {
    QL_REQUIRE(asof == baseScenario_->asof(),
               "unexpected asof date (" << asof << "), does not match base - " << baseScenario_->asof());
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> incremental =
        scenarioFactory_->buildScenario(asof, isAbsolute, label, numeraire);
    QL_REQUIRE((label == incremental->label()) || (label == ""), "DeltaScenarioFactory has not updated scenario label");
    return QuantLib::ext::make_shared<DeltaScenario>(baseScenario_, incremental);
}
} // namespace sensitivity
} // namespace ore
