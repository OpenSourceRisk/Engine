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

#pragma once

#include <string>

namespace ore {
namespace data {

//! Parse a commodity calendar spread volatility surface name.
/*! Expected format is [underlying]_CALENDAR_SPREAD_[offset].
    On success, returns true and populates \p underlyingName and \p offset.
*/
bool parseCommodityCalendarSpreadVolSurfaceName(const std::string& name, std::string& underlyingName, int& offset);

} // namespace data
} // namespace ore