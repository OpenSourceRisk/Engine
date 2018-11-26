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

/*! \file oret/fileutilities.hpp
    \brief File utilities for use in unit tests
*/

#pragma once

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

using boost::filesystem::path;
using boost::filesystem::exists;
using boost::filesystem::remove_all;
using boost::filesystem::filesystem_error;

#ifdef BOOST_MSVC
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#endif

namespace {

// Use to remove the output directory when the suite exits
// Returns true if the operation completed without errors, otherwise false
bool clearOutput(const path& outputPath) {
    
    // If output path does not exist, nothing to do
    if (!exists(outputPath)) return true;

    // If the output path exists, attempt to remove it
    try {
        remove_all(outputPath);
        return true;
    } catch (filesystem_error& err) {
        BOOST_TEST_MESSAGE("The attempt to remove the output path, " << 
            outputPath << ", failed with error " << err.what());
        return false;
    }
}

}
