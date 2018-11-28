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

#include <ql/errors.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <string>

using boost::filesystem::path;
using boost::filesystem::exists;
using boost::filesystem::is_directory;
using std::string;

namespace ore {
namespace test {

/*! Gets passed the command line arguments from a unit test suite
    and checks that the base data path has been provided

    Specify the base data path as --base_data_path. The base data path
    should have a child 'input' directory containing any input files for
    the tests. Any output from the tests will be added to child 'output'
    directory under this base data path.

    \warning this function raises an exception if base data path is not
             provided.
*/
string getBaseDataPath(int argc, char** argv) {

    for (int i = 1; i < argc; ++i) {
        if (boost::starts_with(argv[i], "--base_data_path")) {
            vector<string> strs;
            boost::split(strs, argv[i], boost::is_any_of("="));
            if (strs.size() > 1) {
                // Test that we have a valid path and it contains an 'input' folder
                string strPath = strs[1];
                path p(strPath);
                QL_REQUIRE(is_directory(p), "Test set up failed: the path '" <<
                    strPath << "' is not a directory");
                QL_REQUIRE(is_directory(p / path("input")), "Test set up failed: the path '" <<
                    strPath << "' does not contain an 'input' directory");

                BOOST_TEST_MESSAGE("Got a valid base data path: " << strPath);

                return strPath;
            }
        }
    }

    // If we get to here, it is an error
    QL_FAIL("Test set up failed: did not get a base_data_path parameter");
}
}

}
