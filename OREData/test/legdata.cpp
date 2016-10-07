/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "legdata.hpp"
#include <ored/portfolio/legdata.hpp>

using namespace QuantLib;
using namespace openriskengine::data;
using namespace boost::unit_test_framework;

namespace testsuite {

void LegDataTest::testLegDataNotionals() {
    BOOST_TEST_MESSAGE("Testing LegData Notionals...");

    vector<double> notionals = {100, 200, 300};
    vector<string> dates = {"", "2015-01-01", "2016-01-01"};

    ScheduleRules sr("2014-06-01", "2016-12-01", "6M", "TARGET", "F", "F", "Forward");
    Schedule s = makeSchedule(sr);
    BOOST_CHECK_EQUAL(s.size(), 6UL);

    vector<double> notionalsOut = buildScheduledVector(notionals, dates, s);

    // Expect 100, 100, 200, 200, 300
    BOOST_CHECK_EQUAL(notionalsOut.size(), 5UL);
    BOOST_CHECK_EQUAL(notionalsOut[0], 100);
    BOOST_CHECK_EQUAL(notionalsOut[1], 100);
    BOOST_CHECK_EQUAL(notionalsOut[2], 200);
    BOOST_CHECK_EQUAL(notionalsOut[3], 200);
    BOOST_CHECK_EQUAL(notionalsOut[4], 300);

    // Now check single value
    notionals = vector<double>(1, 123);
    notionalsOut = buildScheduledVector(notionals, {}, s);
    BOOST_CHECK_EQUAL(notionalsOut.size(), 1UL);
    BOOST_CHECK_EQUAL(notionalsOut[0], 123);

    // Now check long value with no "dates" is uneffected
    notionals = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    notionalsOut = buildScheduledVector(notionals, {}, s);
    BOOST_CHECK_EQUAL(notionalsOut.size(), 10UL);
    for (Size i = 0; i < notionalsOut.size(); i++) {
        BOOST_CHECK_EQUAL(notionalsOut[i], (double)i);
    }
}

void LegDataTest::testLegDataCashflows() {
    BOOST_TEST_MESSAGE("Testing LegData Cashflows...");

    vector<double> amounts = {1000000, 2000000, 3000000};
    vector<string> dates = {"2015-01-01", "2016-01-01", "2017-01-01"};

    CashflowData data(amounts, dates);

    LegData legData(true, "EUR", data);
    Leg leg = makeSimpleLeg(legData);

    // Expect 100000, 200000, 300000
    BOOST_CHECK_EQUAL(leg.size(), 3);

    BOOST_CHECK_EQUAL(leg[0]->amount(), 1000000);
    BOOST_CHECK_EQUAL(leg[1]->amount(), 2000000);
    BOOST_CHECK_EQUAL(leg[2]->amount(), 3000000);

    BOOST_CHECK_EQUAL(leg[0]->date(), parseDate("2015-01-01"));
    BOOST_CHECK_EQUAL(leg[1]->date(), parseDate("2016-01-01"));
    BOOST_CHECK_EQUAL(leg[2]->date(), parseDate("2017-01-01"));
}

test_suite* LegDataTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("LegDataTests");
    suite->add(BOOST_TEST_CASE(&LegDataTest::testLegDataNotionals));
    suite->add(BOOST_TEST_CASE(&LegDataTest::testLegDataCashflows));
    return suite;
}
}
