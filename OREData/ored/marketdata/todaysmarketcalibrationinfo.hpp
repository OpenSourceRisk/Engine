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
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <ql/utilities/null.hpp>

#include <map>
#include <string>
#include <vector>

namespace ore {
namespace data {

// yield curves

struct YieldCurveCalibrationInfo {
    virtual ~YieldCurveCalibrationInfo() {}

    // default periods to determine pillarDates relative to asof
    const static std::vector<QuantLib::Period> defaultPeriods;

    std::string dayCounter;
    std::string currency;

    std::vector<QuantLib::Date> pillarDates;
    std::vector<double> zeroRates;
    std::vector<double> discountFactors;
    std::vector<double> times;
};

struct PiecewiseYieldCurveCalibrationInfo : public YieldCurveCalibrationInfo {
    // ... add instrument types?
};

struct FittedBondCurveCalibrationInfo : public YieldCurveCalibrationInfo {
    std::string fittingMethod;
    std::vector<double> solution;
    int iterations = 0;
    double costValue = QuantLib::Null<QuantLib::Real>();
    double tolerance = QuantLib::Null<QuantLib::Real>();
    std::vector<std::string> securities;
    std::vector<QuantLib::Date> securityMaturityDates;
    std::vector<double> marketPrices;
    std::vector<double> modelPrices;
    std::vector<double> marketYields;
    std::vector<double> modelYields;
};

// inflation curves

struct InflationCurveCalibrationInfo {
    virtual ~InflationCurveCalibrationInfo() {}
    std::string dayCounter;
    std::string calendar;
    QuantLib::Date baseDate;
    std::vector<QuantLib::Date> pillarDates;
    std::vector<double> times;
};

struct ZeroInflationCurveCalibrationInfo : public InflationCurveCalibrationInfo {
    double baseCpi = 0.0;
    std::vector<double> zeroRates;
    std::vector<double> forwardCpis;
};

struct YoYInflationCurveCalibrationInfo : public InflationCurveCalibrationInfo {
    std::vector<double> yoyRates;
};

// ... add more curve types here

// main container

struct TodaysMarketCalibrationInfo {
    QuantLib::Date asof;
    // discount, index and yield curves
    std::map<std::string, boost::shared_ptr<YieldCurveCalibrationInfo>> yieldCurveCalibrationInfo;
    // equity dividend yield curves
    std::map<std::string, boost::shared_ptr<YieldCurveCalibrationInfo>> dividendCurveCalibrationInfo;
    // inflation curves
    std::map<std::string, boost::shared_ptr<InflationCurveCalibrationInfo>> inflationCurveCalibrationInfo;
    // ...
};

} // namespace data
} // namespace ore
