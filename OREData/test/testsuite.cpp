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
#include <boost/timer.hpp>
#include <boost/make_shared.hpp>
using namespace boost;

// Boost.Test
#include <boost/test/parameterized_test.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
using boost::unit_test::test_suite;

#include <ored/utilities/log.hpp>

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
#endif

// Lib test suites
#include "bond.hpp"
#include "calendars.hpp"
#include "ccyswapwithresets.hpp"
#include "cds.hpp"
#include "cms.hpp"
#include "cpiswap.hpp"
#include "crossassetmodeldata.hpp"
#include "equitymarketdata.hpp"
#include "equitytrades.hpp"
#include "fxoption.hpp"
#include "fxswap.hpp"
#include "fxtriangulation.hpp"
#include "indices.hpp"
#include "legdata.hpp"
#include "parser.hpp"
#include "schedule.hpp"
#include "todaysmarket.hpp"
#include "xmlmanipulation.hpp"
#include "yieldcurve.hpp"
#include "swaption.hpp"
#include "portfolio.hpp"

namespace {

boost::timer t;

void startTimer() { t.restart(); }
void stopTimer() {
    double seconds = t.elapsed();
    int hours = int(seconds / 3600);
    seconds -= hours * 3600;
    int minutes = int(seconds / 60);
    seconds -= minutes * 60;
    std::cout << endl << " OREData tests completed in ";
    if (hours > 0)
        cout << hours << " h ";
    if (hours > 0 || minutes > 0)
        cout << minutes << " m ";
    cout << fixed << setprecision(0) << seconds << " s" << endl << endl;
}
} // namespace

test_suite* init_unit_test_suite(int, char* []) {
    
    boost::shared_ptr<ore::data::BoostTestLogger> logger =
    boost::make_shared<ore::data::BoostTestLogger>();
    ore::data::Log::instance().removeAllLoggers();
    ore::data::Log::instance().registerLogger(logger);
    ore::data::Log::instance().switchOn();
    ore::data::Log::instance().setMask(255);

    test_suite* test = BOOST_TEST_SUITE("OREDataTestSuite");

    test->add(BOOST_TEST_CASE(startTimer));

    test->add(testsuite::FXSwapTest::suite());
    test->add(testsuite::FXOptionTest::suite());
    test->add(testsuite::CcySwapWithResetsTest::suite());
    test->add(testsuite::CPISwapTest::suite());
    test->add(testsuite::FXTriangulationTest::suite());
    test->add(testsuite::CalendarNameTest::suite());
    test->add(testsuite::IndexTest::suite());
    test->add(testsuite::ParseTest::suite());
    test->add(testsuite::XMLManipulationTest::suite());
    test->add(testsuite::LegDataTest::suite());
    test->add(testsuite::ScheduleDataTest::suite());
    test->add(testsuite::TodaysMarketTest::suite());
    test->add(testsuite::CrossAssetModelDataTest::suite());
    test->add(testsuite::YieldCurveTest::suite());
    test->add(testsuite::EquityMarketDataTest::suite());
    test->add(testsuite::EquityTradesTest::suite());
    test->add(testsuite::BondTest::suite());
    test->add(testsuite::CmsTest::suite());
    test->add(testsuite::SwaptionTest::suite());
    test->add(testsuite::PortfolioTest::suite());
    test->add(testsuite::CreditDefaultSwapTest::suite());

    test->add(BOOST_TEST_CASE(stopTimer));

    return test;
}
