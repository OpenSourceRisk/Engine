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

/*! \file ored/utilities/to_string.cpp
    \brief
    \ingroup utilities
*/

#include <ored/utilities/to_string.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

#include <iostream>
#include <stdio.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

using std::string;

namespace ore {
namespace data {

using namespace QuantLib;

std::string to_string(const Date& date) {
    if (date == Date())
        return "1901-01-01";

    char buf[11];
    int y = date.year();
    int m = static_cast<int>(date.month());
    int d = date.dayOfMonth();

    int n = snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    QL_REQUIRE(n == 10, "Failed to convert date " << date << " to_string() n:" << n);
    return std::string(buf);
}

string to_string(bool aBool) { return aBool ? "true" : "false"; }

std::string to_string(const Period& period) {
    Integer n = period.length();
    Integer m = 0;
    std::ostringstream o;
    switch (period.units()) {
    case Days:
        if (n>=7) {
	    m = n/7;
	    o << m << "W";
	    n = n%7;
	}
	if (n != 0 || m == 0) {
	    o << n << "D";
	    return o.str();
	}
	else {
	    return o.str();
	}
    case Weeks: {
        o << n << "W";
	return o.str();
    }
    case Months:
        if (n>=12) {
	    m = n/12;
	    o << n/12 << "Y";
	    n = n%12;
	}
	if (n != 0 || m == 0) {
	    o << n << "M";
	    return o.str();
	}
	else {
	    return o.str();
	}
    case Years: {
        o << n << "Y";
	return o.str();
    }
    default:
        ALOG("unknown time unit (" << Integer(period.units()) << ")");
	o << period;
	return o.str();
    }
}

} // namespace data
} // namespace ore
