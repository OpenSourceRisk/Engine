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

/*! \file oret/inputfilepath.hpp
    \brief Utility to retrieve the path for a unit test's input files
*/

#pragma once

#include <boost/filesystem.hpp>

using namespace boost::filesystem;

#ifdef BOOST_MSVC
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#endif

// Expands to give the Boost path for the input directory for the test xpp file in which it is called 
#define TEST_INPUT_PATH path(__FILE__).parent_path() / "input" / path(__FILE__).stem()

// Expands to give the Boost path for an input file, with name 'filename', for the test xpp file in which it is called 
#define TEST_INPUT_FILE_PATH(filename) TEST_INPUT_PATH / filename

// Expands to give the Boost path for the output directory for the test xpp file in which it is called 
#define TEST_OUTPUT_PATH path(__FILE__).parent_path() / "output" / path(__FILE__).stem()

// Expands to give the Boost path for an output file, with name 'filename', for the test xpp file in which it is called 
#define TEST_OUTPUT_FILE_PATH(filename) TEST_OUTPUT_PATH / filename

// Gives the string representation of the input path
#define TEST_INPUT (TEST_INPUT_PATH).string()

// Gives the string representation of the input file
#define TEST_INPUT_FILE(filename) (TEST_INPUT_FILE_PATH(filename)).string()

// Gives the string representation of the output path
#define TEST_OUTPUT (TEST_OUTPUT_PATH).string()

// Gives the string representation of the output file
#define TEST_OUTPUT_FILE(filename) (TEST_OUTPUT_FILE_PATH(filename)).string()
