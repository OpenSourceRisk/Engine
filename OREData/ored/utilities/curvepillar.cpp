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

CurvePillar parseCurvePillar(const std::string& str) {
    QL_REQUIRE(str.size() > 1, "parseCurvePillar: string must have at least 2 characters");

    if (str.at(0) == 'c') {
        FutureContinuationExpiry expiry;
        expiry.fromString(str);
        return expiry;
    } else {
        QuantLib::Date date;
        QuantLib::Period period;
        bool isDate;
        parseDateOrPeriod(str, date, period, isDate);
        if (isDate) {
            return date;
        } else {
            return period;
        }
    }
}

std::ostream& operator<<(std::ostream& os, const CurvePillar& v) {
    std::visit([&](const auto& x) { os << to_string(x); }, v);
    return os;
}

} // namespace data
} // namespace ore