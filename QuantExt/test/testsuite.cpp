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

/*! \file testsuite.cpp
    \brief wrapper calling all individual test cases
    \ingroup
*/

#include <iomanip>
#include <iostream>
using namespace std;

// Boost
#include <boost/timer/timer.hpp>
using namespace boost;

// Boost.Test
#define BOOST_TEST_MODULE QuantExtTestSuite
#include <boost/test/unit_test.hpp>
using boost::unit_test::test_suite;
using boost::unit_test::framework::master_test_suite;

#include "toplevelfixture.hpp"

#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_timer
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_chrono
#include <boost/config/auto_link.hpp>
#endif

class QleGlobalFixture {
public:
    QleGlobalFixture() {}

    ~QleGlobalFixture() { stopTimer(); }

    // Method called in destructor to log time taken
    void stopTimer() {
        t.stop();
        double seconds = t.elapsed().wall * 1e-9;
        int hours = int(seconds / 3600);
        seconds -= hours * 3600;
        int minutes = int(seconds / 60);
        seconds -= minutes * 60;
        cout << endl << "QuantExt tests completed in ";
        if (hours > 0)
            cout << hours << " h ";
        if (hours > 0 || minutes > 0)
            cout << minutes << " m ";
        cout << fixed << setprecision(0) << seconds << " s" << endl;
    }

private:
    // Timing the test run
    boost::timer::cpu_timer t;
};

// Breaking change in 1.65.0
// https://www.boost.org/doc/libs/1_65_0/libs/test/doc/html/boost_test/change_log.html
// Deprecating BOOST_GLOBAL_FIXTURE in favor of BOOST_TEST_GLOBAL_FIXTURE
#if BOOST_VERSION < 106500
BOOST_GLOBAL_FIXTURE(QleGlobalFixture);
#else
BOOST_TEST_GLOBAL_FIXTURE(QleGlobalFixture);
#endif
