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

/*! \file orea/engine/sensitivitystream.hpp
    \brief Base class for sensitivity record streamer
 */

#pragma once

#include <orea/engine/sensitivityrecord.hpp>

namespace ore {
namespace analytics {

//! Base Class for streaming SensitivityRecords
class SensitivityStream {
public:
    //! Destructor
    virtual ~SensitivityStream() {}
    //! Returns the next SensitivityRecord in the stream
    virtual SensitivityRecord next() = 0;
    //! Resets the stream so that SensitivityRecord objects can be streamed again
    virtual void reset() = 0;
};

} // namespace analytics
} // namespace ore
