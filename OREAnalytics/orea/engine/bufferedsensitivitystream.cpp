/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/engine/bufferedsensitivitystream.hpp>

namespace ore {
namespace analytics {

BufferedSensitivityStream::BufferedSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& stream)
    : stream_(stream) {}

SensitivityRecord BufferedSensitivityStream::next() {
    if (index_ == QuantLib::Null<Size>()) {
        buffer_.push_back(stream_->next());
        return buffer_.back();
    }
    if (index_ < buffer_.size()) {
        return buffer_[index_++];
    }
    return {};
}

void BufferedSensitivityStream::reset() {
    // if next() was never called, we do not switch to the buffered mode
    if (!buffer_.empty())
        index_ = 0;
}

} // namespace analytics
} // namespace ore
