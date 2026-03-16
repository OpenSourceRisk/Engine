/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file orea/cube/cubewriter.hpp
    \brief A Class to write a cube out to file
    \ingroup cube
*/

#pragma once

#include <ql/shared_ptr.hpp>
#include <map>
#include <orea/cube/npvcube.hpp>
#include <string>

namespace ore {
namespace analytics {

//! Write an NPV cube to a human readable text file
/*! \ingroup cube
 */
class CubeWriter {
public:
    //! ctor
    CubeWriter(const std::string& filename);

    //! Return the filename this writer is writing too
    const std::string& filename() { return filename_; }

    //! Write a cube out to file
    void write(const QuantLib::ext::shared_ptr<NPVCube>& cube, const std::map<std::string, std::string>& nettingSetMap,
               bool append = false);

private:
    std::string filename_;
};
} // namespace analytics
} // namespace ore
