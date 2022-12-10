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

/*! \file oret/basedatapath.hpp
    \brief Parse base data path from the Boost test command line arguments
*/

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <ql/errors.hpp>
#include <string>

using boost::filesystem::exists;
using boost::filesystem::is_directory;
using boost::filesystem::path;

#ifdef BOOST_MSVC
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#endif

namespace ore {
namespace test {

/*! Gets passed the command line arguments from a unit test suite
    and checks if a base data path has been provided

    Specify the base data path as --base_data_path. The base data path
    should have a child 'input' directory containing any input files for
    the tests. Any output from the tests will be added to child 'output'
    directory under this base data path.

    A default base data path of ".." and "." on Windows and Unix respectively
    is returned.
*/
std::string getBaseDataPath(int argc, char** argv) {

    // Default initial value for the base data path
    // Allows a standard run on Unix or Windows from the executable directory without having
    // to specify a base_data_path on the command line
#ifdef BOOST_MSVC
    std::string strPath = "..";
#else
    std::string strPath = ".";
#endif

    // Check if a base data path has been provided in the command line arguments
    for (int i = 1; i < argc; ++i) {
        if (boost::starts_with(argv[i], "--base_data_path")) {
            std::vector<std::string> strs;
            boost::split(strs, argv[i], boost::is_any_of("="));
            if (strs.size() > 1) {
                strPath = strs[1];
            }
        }
    }

    // Test that we have a valid path
    path p(strPath);
    QL_REQUIRE(is_directory(p), "Test set up failed: the path '" << strPath << "' is not a directory");

    return strPath;
}

} // namespace test
} // namespace ore
