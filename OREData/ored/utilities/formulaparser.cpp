/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/utilities/formulaparser.hpp>

namespace ore {
namespace data {

QuantExt::CompiledFormula parseFormula(const std::string& text, std::vector<std::string>& variables) {
    variables.clear();
    auto mapping = [&variables](const std::string& s) {
        auto it = std::find(variables.begin(), variables.end(), s);
        if (it != variables.end()) {
            return QuantExt::CompiledFormula(QuantLib::Size(it - variables.begin()));
        } else {
            variables.push_back(s);
            return QuantExt::CompiledFormula(QuantLib::Size(variables.size() - 1));
        }
    };
    return parseFormula<QuantExt::CompiledFormula>(text, mapping);
} // parseFormula

} // namespace data
} // namespace ore
