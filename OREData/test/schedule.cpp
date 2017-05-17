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

#include <ored/portfolio/schedule.hpp>
#include <test/schedule.hpp>

using namespace boost::unit_test_framework;
using namespace ore::data;
using namespace QuantLib;

namespace testsuite {

void ScheduleDataTest::testScheduleData() {
    BOOST_TEST_MESSAGE("Testing ScheduleData...");

    ScheduleDates dates1("TARGET", {"2015-01-09", "2015-02-09", "2015-03-09", "2015-04-09"});

    ScheduleDates dates2("TARGET", {"2015-04-09",
                                    "2016-04-11", // 9th = Saturday
                                    "2017-04-10", // 9th = Sunday
                                    "2018-04-09"});

    ScheduleRules rules1("2015-01-09", "2015-04-09", "1M", "TARGET", "MF", "MF", "Forward");

    ScheduleRules rules2("2015-04-09", "2018-04-09", "1Y", "TARGET", "MF", "MF", "Forward");

    // Test <Dates> and <Dates>
    ScheduleData data(dates1);
    data.addDates(dates2);
    QuantLib::Schedule s = makeSchedule(data);
    BOOST_CHECK_EQUAL(s.size(), 7UL); // 7 dates, 6 coupons
    BOOST_CHECK_EQUAL(s.startDate(), Date(9, Jan, 2015));
    BOOST_CHECK_EQUAL(s.endDate(), Date(9, Apr, 2018));

    // Test <Rules> and <Rules>
    data = ScheduleData(rules1);
    data.addRules(rules2);
    QuantLib::Schedule s2 = makeSchedule(data);
    BOOST_CHECK_EQUAL(s2.size(), s.size());
    for (Size i = 0; i < s.size(); i++)
        BOOST_CHECK_EQUAL(s[i], s2[i]);

    // Test <Dates> and <Rules>
    data = ScheduleData(dates1);
    data.addRules(rules2);
    QuantLib::Schedule s3 = makeSchedule(data);
    BOOST_CHECK_EQUAL(s3.size(), s.size());
    for (Size i = 0; i < s.size(); i++)
        BOOST_CHECK_EQUAL(s[i], s3[i]);
}

test_suite* ScheduleDataTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("ScheduleDataTest");
    suite->add(BOOST_TEST_CASE(&ScheduleDataTest::testScheduleData));
    return suite;
}
} // namespace testsuite
