/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/engine/sensitivityinmemorystream.hpp>
#include <ored/utilities/log.hpp>

using std::set;

namespace ore {
namespace analytics {

SensitivityInMemoryStream::SensitivityInMemoryStream() : itCurrent_(records_.begin()) {}

SensitivityRecord SensitivityInMemoryStream::next() {
    // If there are no more records, return the empty record
    if (itCurrent_ == records_.end())
        return SensitivityRecord();

    // If there are more, return the current record and advance the iterator
    return *(itCurrent_++);
}

void SensitivityInMemoryStream::reset() {
    // Reset iterator to start of container
    itCurrent_ = records_.begin();
}

void SensitivityInMemoryStream::add(const SensitivityRecord& sr) {
    // Insert the record
    records_.push_back(sr);

    // Reset because itCurrent_ possibly invalidated by the insert
    reset();
}

} // namespace analytics
} // namespace ore
