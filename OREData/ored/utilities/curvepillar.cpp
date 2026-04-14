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
#include <ored/utilities/curvepillar.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& os, const ExpiryMonthYear& v) { return os << v.toString(); }

CurvePillar parseCurvePillar(const std::string& str) {
    QL_REQUIRE(str.size() > 1, "parseCurvePillar: string must have at least 2 characters");
    QuantLib::Period p;
    if (tryParse<Period>(str, p, [](const std::string& s) { return parsePeriod(s); })) {
        return p;
    } else if (str.size() == 7 && str[4] == '-') {
        return ExpiryMonthYear(str);
    } else {
        QL_FAIL("parseCurvePillar: string '" << str << "' is neither a valid period nor of the form YYYY-MM");
    }
}

std::ostream& operator<<(std::ostream& os, const CurvePillar& v) {
    std::visit([&](const auto& x) { os << to_string(x); }, v);
    return os;
}

} // namespace data
} // namespace ore