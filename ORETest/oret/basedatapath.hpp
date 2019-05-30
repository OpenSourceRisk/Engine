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
#include <ql/patterns/singleton.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <string>

#ifdef BOOST_MSVC
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#endif

namespace ore {
namespace test {

//! Repository holding the base data path for the current test suite
class BasePath : public QuantLib::Singleton<BasePath> {
    friend class QuantLib::Singleton<BasePath>;

private:
    BasePath() {
        
        // Localised usings for readability
        using boost::unit_test::framework::master_test_suite;
        using boost::filesystem::path;
        using boost::filesystem::exists;
        using boost::filesystem::is_directory;
        using boost::filesystem::system_complete;
        using std::string;
        using std::vector;

        // Test suite command line parameters
        int argc = master_test_suite().argc;
        char** argv = master_test_suite().argv;

        // Default initial value for the base data path
        // Allows a standard run on Unix or Windows from the executable directory without having 
        // to specify a base_data_path on the command line
#ifdef BOOST_MSVC
        value_ = "..";
#else
        value_ = ".";
#endif

        // Check if a base data path has been provided in the command line arguments
        for (int i = 1; i < argc; ++i) {
            if (boost::starts_with(argv[i], "--base_data_path")) {
                vector<string> strs;
                boost::split(strs, argv[i], boost::is_any_of("="));
                if (strs.size() > 1) {
                    value_ = strs[1];
                }
            }
        }

        // Test that we have a valid path and it contains an 'input' folder
        // Would like to do this but can't because of the bug at:
        // https://svn.boost.org/trac10/ticket/12987
        // path p(value_);
        // QL_REQUIRE(is_directory(p), "Test set up failed: the path '" <<
        //     value_ << "' is not a directory");
        // QL_REQUIRE(is_directory(p / path("input")), "Test set up failed: the path '" <<
        //     value_ << "' does not contain an 'input' directory");
    }

public:
    const std::string& value() const {
        return value_;
    }

private:
    std::string value_;
};

}
}
