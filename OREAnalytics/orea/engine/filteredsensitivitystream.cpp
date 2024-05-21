/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <math.h>
#include <orea/engine/filteredsensitivitystream.hpp>

using QuantLib::Real;
using std::fabs;

namespace ore {
namespace analytics {

FilteredSensitivityStream::FilteredSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                                                     Real deltaThreshold, Real gammaThreshold)
    : ss_(ss), deltaThreshold_(deltaThreshold), gammaThreshold_(gammaThreshold) {
    // Reset the underlying stream in case
    ss_->reset();
    while (SensitivityRecord sr = ss_->next()) {
        if (sr.isCrossGamma() && fabs(sr.gamma) > gammaThreshold_) {
            deltaKeys_.insert(sr.key_1);
            deltaKeys_.insert(sr.key_2);
        }
    }
    ss_->reset();
}

FilteredSensitivityStream::FilteredSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                                                     QuantLib::Real threshold)
    : FilteredSensitivityStream(ss, threshold, threshold) {}

SensitivityRecord FilteredSensitivityStream::next() {
    // Return the next sensitivity record in the underlying stream that satisfies
    // the threshold conditions
    while (SensitivityRecord sr = ss_->next()) {
        if (fabs(sr.delta) > deltaThreshold_ || fabs(sr.gamma) > gammaThreshold_ ||
            (!sr.isCrossGamma() && deltaKeys_.find(sr.key_1) != deltaKeys_.end())) {
            return sr;
        }
    }

    // If we get to here, no more records in underlying stream
    return SensitivityRecord();
}

void FilteredSensitivityStream::reset() {
    // Reset the underlying stream
    ss_->reset();
}

} // namespace analytics
} // namespace ore
