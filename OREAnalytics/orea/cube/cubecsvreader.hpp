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

/*! \file orea/cube/cubecsvreader.hpp
    \brief A Class to read a cube file from csv input
    \ingroup cube
*/

#pragma once

#include <ql/shared_ptr.hpp>
#include <map>
#include <orea/cube/npvcube.hpp>
#include <string>

namespace ore {
namespace analytics {

//! Read an NPV cube from a human readable text file
/*! \ingroup cube
 */
class CubeCsvReader {
public:
    //! ctor
    CubeCsvReader(const std::string& filename);

    //! Return the filename this reader is reader from
    const std::string& filename() const { return filename_; }

    //! Read a cube from a csv file
    void read(QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube, std::map<std::string, std::string>& nettingSetMap);

private:
    std::string filename_;
};
} // namespace analytics
} // namespace ore
