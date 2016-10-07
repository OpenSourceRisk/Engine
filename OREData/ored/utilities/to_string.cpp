/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/tos_tring.cpp
    \brief
    \ingroup utilities
*/

#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <stdio.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

namespace ore {
namespace data {

std::string to_string(const QuantLib::Date& date) {
    char buf[11];
    int y = date.year();
    int m = static_cast<int>(date.month());
    int d = date.dayOfMonth();

    int n = snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    QL_REQUIRE(n == 10, "Failed to convert date " << date << " to_string() n:" << n);
    return std::string(buf);
}
}
}
