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

/*! \file orea/engine/bufferedsensitivitystream.hpp
    \brief a wrapper to buffer sensi stream records
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>

namespace ore {
namespace analytics {

class BufferedSensitivityStream : public SensitivityStream {
public:
    explicit BufferedSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& stream);
    SensitivityRecord next() override;
    void reset() override;

private:
    QuantLib::ext::shared_ptr<SensitivityStream> stream_;
    std::vector<SensitivityRecord> buffer_;
    QuantLib::Size index_ = QuantLib::Null<QuantLib::Size>();
};

} // namespace analytics
} // namespace ore
