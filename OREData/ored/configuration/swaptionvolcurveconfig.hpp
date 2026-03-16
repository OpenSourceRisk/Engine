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

/*! \file ored/configuration/swaptionvolcurveconfig.hpp
    \brief Swaption volatility curve configuration classes
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

//! Swaption volatility curve configuration class
/*!
  \ingroup configuration
*/
class SwaptionVolatilityCurveConfig : public GenericYieldVolatilityCurveConfig {
public:
    //! ctor, currency is derived from swap index base
    SwaptionVolatilityCurveConfig()
        : GenericYieldVolatilityCurveConfig("Swap", "SwaptionVolatility", "SWAPTION", "", true, true) {}
    //! Detailed constructor
    SwaptionVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension dimension,
                                  const VolatilityType volatilityType, const VolatilityType outputVolatilityType,
                                  const Interpolation interpolation, const Extrapolation extrapolation,
                                  const vector<string>& optionTenors, const vector<string>& swapTenors,
                                  const DayCounter& dayCounter, const Calendar& calendar,
                                  const BusinessDayConvention& businessDayConvention, const string& shortSwapIndexBase,
                                  const string& swapIndexBase,
                                  // Only required for smile
                                  const vector<string>& smileOptionTenors = vector<string>(),
                                  const vector<string>& smileSwapTenors = vector<string>(),
                                  const vector<string>& smileSpreads = vector<string>())
        : GenericYieldVolatilityCurveConfig("Swap", "SwaptionVolatility", "SWAPTION", "", curveID, curveDescription, "",
                                            dimension, volatilityType, outputVolatilityType, interpolation,
                                            extrapolation, optionTenors, swapTenors, dayCounter, calendar,
                                            businessDayConvention, shortSwapIndexBase, swapIndexBase, smileOptionTenors,
                                            smileSwapTenors, smileSpreads) {}
    //! Detailled constructor for proxy config
    SwaptionVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                  const string& proxySourceCurveId, const string& proxySourceShortSwapIndexBase,
                                  const string& proxySourceSwapIndexBase, const string& proxyTargetShortSwapIndexBase,
                                  const string& proxyTargetSwapIndexBase)
        : GenericYieldVolatilityCurveConfig("Swap", "SwaptionVolatility", "", curveID, curveDescription, "",
                                            proxySourceCurveId, proxySourceShortSwapIndexBase, proxySourceSwapIndexBase,
                                            proxyTargetShortSwapIndexBase, proxyTargetSwapIndexBase) {}
};

} // namespace data
} // namespace ore
