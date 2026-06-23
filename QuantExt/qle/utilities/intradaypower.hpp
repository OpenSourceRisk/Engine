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

#include <iostream>
#include <qle/utilities/time.hpp>
#include <string>

namespace QuantExt {

constexpr int SECONDS_PER_HOUR = 3600;
constexpr int SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

enum class IntradayPowerTimeUnit { HOUR = 3600, SECOND = 1 };

enum class IntradayPowerDSTAdjustment { Forward = -1, NoAdjustment = 0, Backward = 1 };

inline IntradayPowerDSTAdjustment dayTimeSavingsAdjustment(const QuantLib::Date& d,
                                                           const std::string& daylightSavingsLocation) {
    return static_cast<IntradayPowerDSTAdjustment>(daylightSavingCorrection(daylightSavingsLocation.empty() ? "Null" : daylightSavingsLocation, d, d + 1));
}

std::ostream& operator<<(std::ostream& out, const IntradayPowerTimeUnit& unit);

IntradayPowerTimeUnit parseIntradayPowerTimeUnit(const std::string& s);
} // namespace QuantExt