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

/*! \file ored/utilities/currencycheck.hpp
    \brief utility class to check whether string is ISO 4217 compliant
    \ingroup utilities
*/

#pragma once

#include <string>
#include <ql/currency.hpp>

namespace ore {
namespace data {
using std::string;
//! Currency Check
/*! check whether string is ISO 4217 compliant
    \ingroup utilities
*/
bool checkCurrency(const string& s);

//! Minor Currency Check
/*! check whether string is a minor currency
    \ingroup utilities
*/
bool checkMinorCurrency(const string& s);

//! Convert a value from a minor ccy to major
/*! .i.e 100 GBp to 1 GBP
    \ingroup utilities
*/
QuantLib::Real convertMinorToMajorCurrency(const std::string& s, QuantLib::Real value);

} // namespace data
} // namespace ore
