/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/model/calibrationpointcache.hpp
    \brief cache for relevant points on curve / vol surfaces
    \ingroup utilities
*/

#pragma once

#include <ql/time/date.hpp>

#include <vector>

namespace ore {
namespace data {

using namespace QuantLib;

class CalibrationPointCache {
public:
    bool hasChanged(const std::vector<std::vector<Real>>& curveTimes, const std::vector<std::vector<Real>>& curveData,
                    const std::vector<std::vector<std::pair<Real, Real>>>& volTimesStrikes,
                    const std::vector<std::vector<Real>>& volData, const bool updateCache);

private:
    Date referenceDate_;
    std::vector<std::vector<Real>> curveTimes_;
    std::vector<std::vector<std::pair<Real, Real>>> volTimesStrikes_;
    std::vector<std::vector<Real>> curveData_, volData_;
};

} // namespace data
} // namespace oreplus
