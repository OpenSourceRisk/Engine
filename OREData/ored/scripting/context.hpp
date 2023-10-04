/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/context.hpp
    \brief script engine context holding variable names and values
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/ast.hpp>
#include <ored/scripting/value.hpp>

#include <boost/variant.hpp>

#include <map>
#include <ostream>
#include <set>
#include <vector>

namespace ore {
namespace data {

struct Context {
    // not assignable, will throw an error if tried
    std::set<std::string> constants;
    // not assignable, but won't throw an error if tried (has a special use case for AMC runs)
    std::set<std::string> ignoreAssignments;
    //
    std::map<std::string, ValueType> scalars;
    std::map<std::string, std::vector<ValueType>> arrays;
    // variable size
    Size varSize() const;
    // is context empty?
    bool empty() const { return scalars.empty() && arrays.empty(); }
    // reset size to given value
    void resetSize(const std::size_t n);
};

std::ostream& operator<<(std::ostream& out, const Context& context);

} // namespace data
} // namespace ore
