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

#include <ored/scripting/context.hpp>

#include <ored/utilities/to_string.hpp>

#include <iomanip>
#include <sstream>

namespace ore {
namespace data {

Size Context::varSize() const {
    QL_REQUIRE(!scalars.empty() || !arrays.empty(), "Context::varSize(): context is empty()");
    Size res = Null<Size>();
    for (auto const& v : scalars) {
        if (res != Null<Size>())
            QL_REQUIRE(size(v.second) == res, "Context::varSize(): inconsistent var sizes");
        else
            res = size(v.second);
    }
    for (auto const& v : arrays) {
        for (auto const& a : v.second) {
            if (res != Null<Size>())
                QL_REQUIRE(size(a) == res, "Context::varSize(): inconsistent var sizes");
            else
                res = size(a);
        }
    }
    return res;
}

std::ostream& operator<<(std::ostream& out, const Context& context) {
    constexpr Size wd1 = 30, wd2 = 10;
    for (auto const& v : context.scalars) {
        out << std::setw(wd1) << std::left << v.first;
        out << "(" << std::setw(wd2) << valueTypeLabels.at(v.second.which()) << ")";
        if (std::find(context.constants.begin(), context.constants.end(), v.first) != context.constants.end())
            out << "    const    ";
        else
            out << "             ";
        out << std::left << v.second << '\n';
    }
    for (auto const& v : context.arrays) {
        Size counter = 0;
        for (auto const& d : v.second) {
            out << std::setw(wd1) << std::left << (v.first + "[" + std::to_string(counter + 1) + "]");
            out << "(" << std::setw(wd2) << valueTypeLabels.at(d.which()) << ")";
            if (std::find(context.constants.begin(), context.constants.end(), v.first) != context.constants.end())
                out << "    const    ";
            else
                out << "             ";
            out << std::left << d << '\n';
            ++counter;
        }
    }
    return out;
}

} // namespace data
} // namespace ore
