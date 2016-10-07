/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

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

#include <ql/time/date.hpp>
#include <sstream>

namespace openriskengine {
namespace data {

//! Convert QuantLib::Date to std::string
/*!
  Returns date as a string in YYYY-MM-DD format, which matches QuantLib::io::iso_date()
  However that function can have issues with locale so we have a local snprintf() based version.

  \ingroup utilities
*/
std::string to_string(const QuantLib::Date& date);

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
}
}
