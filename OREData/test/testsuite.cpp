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
using namespace std;

// Boost
#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>
using namespace boost;
using boost::timer::cpu_timer;

// Boost.Test
#define BOOST_TEST_MODULE OREDataTestSuite
#include <boost/test/unit_test.hpp>
using boost::unit_test::test_suite;
using boost::unit_test::framework::master_test_suite;

#include <oret/basedatapath.hpp>
#include <oret/datapaths.hpp>
#include <oret/oret.hpp>
using ore::test::getBaseDataPath;
using ore::test::setupTestLogging;

#ifdef BOOST_MSVC
#include <ored/auto_link.hpp>
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
#define BOOST_LIB_NAME boost_date_time
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_serialization
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_regex
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_timer
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_chrono
#include <boost/config/auto_link.hpp>
#endif

// Global base path variable
string basePath = "";

class OredGlobalFixture {
public:
    OredGlobalFixture() {
        int argc = master_test_suite().argc;
        char** argv = master_test_suite().argv;

        // Set up test logging
        setupTestLogging(argc, argv);

        // Set the base data path for the unit tests
        basePath = getBaseDataPath(argc, argv);
    }

    ~OredGlobalFixture() { stopTimer(); }

    // Method called in destructor to log time taken
    void stopTimer() {
        t.stop();
        double seconds = t.elapsed().wall * 1e-9;
        int hours = int(seconds / 3600);
        seconds -= hours * 3600;
        int minutes = int(seconds / 60);
        seconds -= minutes * 60;
        cout << endl << "OREData tests completed in ";
        if (hours > 0)
            cout << hours << " h ";
        if (hours > 0 || minutes > 0)
            cout << minutes << " m ";
        cout << fixed << setprecision(0) << seconds << " s" << endl;
    }

private:
    // Timing the test run
    cpu_timer t;
};

// Breaking change in 1.65.0
// https://www.boost.org/doc/libs/1_65_0/libs/test/doc/html/boost_test/change_log.html
// Deprecating BOOST_GLOBAL_FIXTURE in favor of BOOST_TEST_GLOBAL_FIXTURE
#if BOOST_VERSION < 106500
BOOST_GLOBAL_FIXTURE(OredGlobalFixture);
#else
BOOST_TEST_GLOBAL_FIXTURE(OredGlobalFixture);
#endif
