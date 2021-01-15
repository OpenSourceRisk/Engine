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

/*! \file marketdata/todaysmarketcalibrationinfo.hpp
    \brief a container holding information on calibration results during the t0 market build
    \ingroup marketdata
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/utilities/null.hpp>

#include <map>
#include <string>

namespace ore {
namespace data {

struct YieldCurveCalibrationInfo {
    virtual ~YieldCurveCalibrationInfo() {}
    bool valid = false;
};

struct FittedBondCurveCalibrationInfo : public YieldCurveCalibrationInfo {
    std::string fittingMethod;
    std::vector<double> solution;
    int iterations;
    double costValue = QuantLib::Null<QuantLib::Real>();
    double tolerance = QuantLib::Null<QuantLib::Real>();
    std::vector<std::string> securities;
    std::vector<double> marketPrices;
    std::vector<double> modelPrices;
    std::vector<double> marketYields;
    std::vector<double> modelYields;
};

struct TodaysMarketCalibrationInfo {
    // type for identifiers (market object name, configuration)
    using MarketObjectIdentifier = std::pair<std::string, std::string>;
    // fitted bond curve calibration info
    std::map<MarketObjectIdentifier, boost::shared_ptr<YieldCurveCalibrationInfo>> yieldCurveCalibrationInfo;
};

} // namespace data
} // namespace ore
