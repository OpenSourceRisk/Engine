/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/csvutils.hpp
    \brief CSV utility functions
    \ingroup utilities
*/

#pragma once

#include <istream>
#include <string>

namespace ore {
namespace data {

//! Base class for all classes that can be populated from CSV
/*! \ingroup utilities
 */
class CSVSerializable {
public:
    virtual ~CSVSerializable() {}

    //! Populate the object from a CSV input stream
    virtual void fromCSV(std::istream& stream) = 0;

    //! Populate the object from a CSV file
    void fromCSVFile(const std::string& filename);

    //! Populate the object from a CSV string
    void fromCSVString(const std::string& csv);
};

} // namespace data
} // namespace ore
