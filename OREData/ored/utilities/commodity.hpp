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

#pragma once

#include <string>

namespace ore {
namespace data {

//! Parse a commodity calendar spread volatility surface name.
//! Expect the name to be of the form underlyingName_CALENDAR_SPREAD_offset, where offset is the number of contract
//! expiries between the two contracts in the calendar spread. If the name can be parsed successfully, the function
//! returns true and underlyingName and offset are set accordingly. Otherwise, it returns false and underlyingName is
//! set to an empty string and offset to 0.
bool parseCommodityCalendarSpreadVolSurfaceName(const std::string& name, std::string& underlyingName, int& offset);

} // namespace data
} // namespace ore