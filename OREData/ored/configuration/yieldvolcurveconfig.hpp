/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/yieldvolcurveconfig.hpp
\brief yield volatility curve configuration classes
\ingroup configuration
*/

#pragma once

#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {

//! Yield volatility curve configuration
/*!
\ingroup configuration
*/
class YieldVolatilityCurveConfig : public GenericYieldVolatilityCurveConfig {
public:
    YieldVolatilityCurveConfig()
        : GenericYieldVolatilityCurveConfig("Bond", "YieldVolatility", "BOND_OPTION", "Qualifier", false, false) {}
    //! Detailed constructor
    YieldVolatilityCurveConfig(const string& curveID, const string& curveDescription, const string& qualifier,
                               const Dimension& dimension, const VolatilityType volatilityType,
                               const VolatilityType outputVolatilityType, const Interpolation interpolation,
                               const Extrapolation extrapolation, const vector<string>& optionTenors,
                               const vector<string>& bondTenors, const DayCounter& dayCounter, const Calendar& calendar,
                               const BusinessDayConvention& businessDayConvention)
        : GenericYieldVolatilityCurveConfig("Bond", "YieldVolatility", "BOND_OPTION", "Qualifier", curveID,
                                            curveDescription, qualifier, dimension, volatilityType,
                                            outputVolatilityType, interpolation, extrapolation, optionTenors,
                                            bondTenors, dayCounter, calendar, businessDayConvention) {}
    //@}
};

} // namespace data
} // namespace ore
