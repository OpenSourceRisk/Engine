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

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>
#include <string>

using boost::filesystem::exists;
using boost::filesystem::filesystem_error;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using std::equal;
using std::ifstream;
using std::istreambuf_iterator;
using std::string;

#ifdef BOOST_MSVC
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#endif

namespace {

// Use to remove the output directory when the suite exits
// Returns true if the operation completed without errors, otherwise false
inline bool clearOutput(const path& outputPath) {

    // If output path does not exist, nothing to do
    if (!exists(outputPath))
        return true;

    // If the output path exists, attempt to remove it
    try {
        remove_all(outputPath);
        return true;
    } catch (filesystem_error& err) {
        BOOST_TEST_MESSAGE("The attempt to remove the output path, " << outputPath << ", failed with error "
                                                                     << err.what());
        return false;
    }
}

// Basic comparison of two files based on the post:
// https://stackoverflow.com/a/37575457/1771882
inline bool compareFiles(const string& p1, const string& p2) {

    ifstream f1(p1, ifstream::binary | ifstream::ate);
    ifstream f2(p2, ifstream::binary | ifstream::ate);

    if (f1.fail() || f2.fail()) {
        BOOST_TEST_MESSAGE("Attempt to compare file, " << p1 << ", with file, " << p2 << " failed.");
        return false;
    }

    if (f1.tellg() != f2.tellg()) {
        BOOST_TEST_MESSAGE("File size of " << p1 << " is not equal to file size of " << p2 << ".");
        return false;
    }

    // Seek back to beginning and use std::equal to compare contents
    f1.seekg(0, ifstream::beg);
    f2.seekg(0, ifstream::beg);
    return std::equal(istreambuf_iterator<char>(f1.rdbuf()), istreambuf_iterator<char>(),
                      istreambuf_iterator<char>(f2.rdbuf()));
}

} // namespace
