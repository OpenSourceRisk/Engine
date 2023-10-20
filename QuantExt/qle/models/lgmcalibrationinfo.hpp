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

/*! \file lgmcalibrationinfo.hpp
    \brief info data on how a lgm model was calibrated
    \ingroup models
*/

#pragma once

#include <ql/types.hpp>
#include <ql/utilities/null.hpp>

#include <boost/any.hpp>

#include <map>
#include <vector>

namespace QuantExt {
using namespace QuantLib;

struct SwaptionData {
    Real timeToExpiry;
    Real swapLength;
    Real strike;
    Real atmForward;
    Real annuity;
    Real vega;
    Real stdDev;
};

struct LgmCalibrationData {
    Real modelTime;
    Real modelVol;
    Real marketVol;
    Real modelValue;
    Real marketValue;
    Real modelAlpha;
    Real modelKappa;
    Real modelHwSigma;
};

struct LgmCalibrationInfo {
    bool valid = false;
    Real rmse = Null<Real>();
    std::vector<SwaptionData> swaptionData;
    std::vector<LgmCalibrationData> lgmCalibrationData;
};

std::map<std::string, boost::any> getAdditionalResultsMap(const LgmCalibrationInfo& info);

} // namespace QuantExt
