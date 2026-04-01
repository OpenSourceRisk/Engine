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

#include <ored/utilities/commodity.hpp>
#include <ored/utilities/parsers.hpp>

using std::string;

namespace ore {
namespace data {

bool parseCommodityCalendarSpreadVolSurfaceName(const string& name, string& underlyingName, int& offset) {
    static const string tag = "_CALENDAR_SPREAD_";

    underlyingName.clear();
    offset = 0;

    const auto pos = name.rfind(tag);
    if (pos == string::npos || pos == 0)
        return false;

    if (name.find(tag) != pos)
        return false;

    const string offsetString = name.substr(pos + tag.size());
    if (offsetString.empty())
        return false;

    int parsedOffset;
    try {
        parsedOffset = parseInteger(offsetString);
    } catch (...) {
        return false;
    }

    if (parsedOffset == 0)
        return false;

    underlyingName = name.substr(0, pos);
    offset = parsedOffset;
    return true;
}

} // namespace data
} // namespace ore