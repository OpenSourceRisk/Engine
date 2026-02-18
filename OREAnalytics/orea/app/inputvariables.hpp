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

/*! \file orea/app/inputvariables.hpp
    \brief Input Variables
*/
#pragma once

#include <vector>
#include <string>
#include <optional>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {

class InputParameters;

struct InputVariableInfo {
    std::vector<std::string> altNames;
    std::optional<std::string> defaultValue;

    InputVariableInfo(std::vector<std::string> an = {}, std::optional<std::string> dv = std::nullopt)
        : altNames(std::move(an)), defaultValue(std::move(dv)) {}
};

struct InputVariables {
    virtual ~InputVariables() = default;
    virtual void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) = 0;
    void loadVariables(const QuantLib::ext::weak_ptr<InputParameters>& inputs);
};

}; // namespace analytics
}; // namespace ore