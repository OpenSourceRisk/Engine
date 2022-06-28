/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

const std::vector<Period> YieldCurveCalibrationInfo::defaultPeriods = {
    1 * Weeks, 2 * Weeks, 3 * Months, 6 * Months, 9 * Months, 1 * Years,  2 * Years,
    3 * Years, 5 * Years, 7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};

} // namespace data
} // namespace ore
