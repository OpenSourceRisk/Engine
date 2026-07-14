/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <orea/app/inputvariables.hpp>
#include <orea/app/inputparameters.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

void InputVariables::loadVariables(const QuantLib::ext::weak_ptr<InputParameters>& inputs) {
    if (auto s = inputs.lock())
        loadVariablesImpl(s);
    else
        QL_FAIL("Internal error: could not lock inputParameters_ in InputVariables::loadVariables. Contact dev.");
}

void InputVariables::applyEngineDataOverride(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                             QuantLib::ext::shared_ptr<ore::data::EngineData>& engine,
                                             const std::string& analytic, const std::string& param) {
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineOverride;
    inputs->loadParameterXML<ore::data::EngineData>(engineOverride, analytic, param);
    if (engine && engineOverride) {
        engine = QuantLib::ext::make_shared<ore::data::EngineData>(*engine);
        engine->setEngineDataOverride(engineOverride);
    }
}

} // namespace analytics
} // namespace ore