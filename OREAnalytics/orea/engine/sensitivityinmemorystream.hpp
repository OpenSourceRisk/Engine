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

/*! \file orea/engine/sensitivityinmemorystream.hpp
    \brief Class for streaming SensitivityRecords from in-memory container
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>

#include <set>

namespace ore {
namespace analytics {

//! Class for streaming SensitivityRecords from csv file
class SensitivityInMemoryStream : public SensitivityStream {
public:
    //! Default constructor
    SensitivityInMemoryStream();
    //! Constructor from set of sensitivity records
    template<class Iter>
    SensitivityInMemoryStream(Iter begin, Iter end);
    //! Returns the next SensitivityRecord in the stream
    SensitivityRecord next() override;
    //! Resets the stream so that SensitivityRecords can be streamed again
    void reset() override;
    /*! Add a record to the in-memory collection.

        \warning this causes reset() to be called. In other words, after any call
                 to add, a call to next() will start at the beginning again.
    */
    void add(const SensitivityRecord& sr);

private:
    //! Container of records
    std::vector<SensitivityRecord> records_;
    //! Iterator to current element
    std::vector<SensitivityRecord>::iterator itCurrent_;
};

template <class Iter> 
SensitivityInMemoryStream::SensitivityInMemoryStream(Iter begin, Iter end) : records_(begin, end) {}

} // namespace analytics
} // namespace ore
