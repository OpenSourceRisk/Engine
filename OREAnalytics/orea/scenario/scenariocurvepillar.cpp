/*
 Copyright (C) 2026 AcadiaSoft Inc.
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
#include <orea/scenario/scenariocurvepillar.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

using ore::data::parsePeriod;
using ore::data::to_string;

std::ostream& operator<<(std::ostream& os, const IrFutureExpiryYearMonth& v) { return os << v.toString(); }

ScenarioCurvePillar parseScenarioCurvePillar(const std::string& str) {
    QL_REQUIRE(str.size() > 1, "parseScenarioCurvePillar: string must have at least 2 characters");
    QuantLib::Period p;
    if (ore::data::tryParse<QuantLib::Period>(str, p, [](const std::string& s) { return parsePeriod(s); })) {
        return p;
    } else if (str.size() == 7 && str[4] == '-') {
        return IrFutureExpiryYearMonth(str);
    } else {
        QL_FAIL("parseScenarioCurvePillar: string '" << str << "' is neither a valid period nor of the form YYYY-MM");
    }
}

std::ostream& operator<<(std::ostream& os, const ScenarioCurvePillar& v) {
    std::visit([&](const auto& x) { os << x; }, v);
    return os;
}

} // namespace analytics
} // namespace ore