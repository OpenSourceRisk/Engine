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

#include <ored/model/calibrationpointcache.hpp>

#include <ql/math/comparison.hpp>

namespace ore {
namespace data {

bool CalibrationPointCache::hasChanged(const std::vector<std::vector<Real>>& curveTimes,
                                       const std::vector<std::vector<Real>>& curveData,
                                       const std::vector<std::vector<std::pair<Real, Real>>>& volTimesStrikes,
                                       const std::vector<std::vector<Real>>& volData, const bool updateCache) {

    bool dirty = false;

    // check for changes in the times/strikes or data

    dirty = dirty || curveTimes.size() != curveTimes_.size();
    dirty = dirty || curveData.size() != curveData_.size();
    dirty = dirty || volTimesStrikes.size() != volTimesStrikes_.size();
    dirty = dirty || volData.size() != volData_.size();

    for (Size i = 0; i < curveTimes.size() && !dirty; ++i) {
        dirty = dirty || (curveTimes[i].size() != curveTimes_[i].size());
        for (Size j = 0; j < curveTimes[i].size() && !dirty; ++j) {
            dirty = dirty || curveTimes[i][j] != curveTimes_[i][j];
        }
    }

    for (Size i = 0; i < curveData.size() && !dirty; ++i) {
        dirty = dirty || (curveData[i].size() != curveData_[i].size());
        for (Size j = 0; j < curveData[i].size() && !dirty; ++j) {
            dirty = dirty || curveData[i][j] != curveData_[i][j];
        }
    }

    for (Size i = 0; i < volTimesStrikes.size() && !dirty; ++i) {
        dirty = dirty || (volTimesStrikes[i].size() != volTimesStrikes_[i].size());
        for (Size j = 0; j < volTimesStrikes[i].size() && !dirty; ++j) {
            dirty = dirty || volTimesStrikes[i][j].first != volTimesStrikes_[i][j].first;
            dirty = dirty || volTimesStrikes[i][j].second != volTimesStrikes_[i][j].second;
        }
    }

    for (Size i = 0; i < volData.size() && !dirty; ++i) {
        dirty = dirty || (volData[i].size() != volData_[i].size());
        for (Size j = 0; j < volData[i].size() && !dirty; ++j) {
            dirty = dirty || volData[i][j] != volData_[i][j];
        }
    }

    // if dirty update reference results if requested

    if (dirty && updateCache) {
        curveTimes_ = curveTimes;
        curveData_ = curveData;
        volTimesStrikes_ = volTimesStrikes;
        volData_ = volData;
    }

    return dirty;
}

} // namespace data
} // namespace oreplus
