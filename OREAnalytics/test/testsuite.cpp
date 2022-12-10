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

#include <iomanip>
#include <iostream>

#include <oret/config.hpp>

// Boost.Test
#define BOOST_TEST_MODULE "OREAnalyticsTestSuite"
#ifdef ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER
#include <test-suite/paralleltestrunner.hpp>
#else
#include <boost/test/included/unit_test.hpp>
#endif
#include <boost/test/parameterized_test.hpp>
#include <boost/test/test_tools.hpp>

// Boost
using namespace boost;
using boost::unit_test::test_suite;
using boost::unit_test::framework::master_test_suite;

#include <oret/oret.hpp>
using ore::test::setupTestLogging;

#ifdef BOOST_MSVC
#include <orea/auto_link.hpp>
#include <ored/auto_link.hpp>
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
#define BOOST_LIB_NAME boost_serialization
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_regex
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_timer
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_chrono
#include <boost/config/auto_link.hpp>
#endif

class OreaGlobalFixture {
public:
    OreaGlobalFixture() {
        int argc = master_test_suite().argc;
        char** argv = master_test_suite().argv;

        // Set up test logging
        setupTestLogging(argc, argv);
    }
};

// Breaking change in 1.65.0
// https://www.boost.org/doc/libs/1_65_0/libs/test/doc/html/boost_test/change_log.html
// Deprecating BOOST_GLOBAL_FIXTURE in favor of BOOST_TEST_GLOBAL_FIXTURE
#if BOOST_VERSION < 106500
BOOST_GLOBAL_FIXTURE(OreaGlobalFixture);
#else
BOOST_TEST_GLOBAL_FIXTURE(OreaGlobalFixture);
#endif
