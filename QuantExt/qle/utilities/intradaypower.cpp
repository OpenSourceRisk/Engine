/*
 Copyright (C) 2026 AcadiaSoft Inc
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
#pragma once

#include <boost/algorithm/string.hpp>
#include <ql/errors.hpp>
#include <qle/utilities/intradaypower.hpp>

std::ostream& operator<<(std::ostream& out, const IntradayPowerTimeUnit& unit) {
    switch (unit) {
    case IntradayPowerTimeUnit::HOUR:
        return out << "HOUR";
    case IntradayPowerTimeUnit::SECOND:
        return out << "SECOND";
    default:
        QL_FAIL("Unknown IntradayPowerTimeUnit: " << static_cast<int>(unit));
    }
}

IntradayPowerTimeUnit parseIntradayPowerTimeUnit(const std::string& s) {
    std::string str = boost::algorithm::to_upper_copy(s);
    if (str == "HOUR" || str == "HOURS" || str == "H" || str == "HR" || str == "HRS")
        return IntradayPowerTimeUnit::HOUR;
    else if (str == "SECOND" || str == "SECONDS" || str == "S" || str == "SEC" || str == "SECS")
        return IntradayPowerTimeUnit::SECOND;
    else
        QL_FAIL("Unknown IntradayPowerTimeUnit: " << s);
}