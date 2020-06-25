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

/*! \file ored/utilities/to_string.hpp
    \brief string conversion utilities
    \ingroup utilities
*/

#pragma once

#include <ored/configuration/equitycurveconfig.hpp>
#include <ql/time/date.hpp>
#include <sstream>

namespace ore {
namespace data {

//! Convert QuantLib::Date to std::string
/*!
  Returns date as a string in YYYY-MM-DD format, which matches QuantLib::io::iso_date()
  However that function can have issues with locale so we have a local snprintf() based version.

  If date == Date() returns 1900-01-01 so the above format is preserved.

  \ingroup utilities
*/
std::string to_string(const QuantLib::Date& date);

//! Convert bool to std::string
/*! Returns "true" for true and "false" for false
    \ingroup utilities
*/
std::string to_string(bool aBool);

//! Convert type to std::string
/*!
  Utility to give to_string() interface to classes and enums that have ostream<< operators defined.
  \ingroup utilities
*/
template <class T> std::string to_string(const T& t) {
    std::ostringstream oss;
    oss << t;
    return oss.str();
}
} // namespace data
} // namespace ore
