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

#include <boost/test/unit_test.hpp>
#include <ored/portfolio/schedule.hpp>
#include <oret/toplevelfixture.hpp>

using namespace boost::unit_test_framework;
using namespace ore::data;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ScheduleDataTests)

BOOST_AUTO_TEST_CASE(testScheduleData) {

    BOOST_TEST_MESSAGE("Testing ScheduleData...");

    ScheduleDates dates1("TARGET", "", "", {"2015-01-09", "2015-02-09", "2015-03-09", "2015-04-09"});

    ScheduleDates dates2("TARGET", "", "",
                         {"2015-04-09",
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

// The original reason for adding the LastWednesday date generation rule was to generate the AU CPI publication dates 
// using a rules based schedule. Here, we compare the dates for a number of years against the release dates from 
// https://www.abs.gov.au/statistics/economy/price-indexes-and-inflation/consumer-price-index-australia
BOOST_AUTO_TEST_CASE(testLastWednesdayDateGenerationRule) {

    vector<Date> expected{
        Date(30, Oct, 2018),
        Date(30, Jan, 2019), Date(24, Apr, 2019), Date(31, Jul, 2019), Date(30, Oct, 2019), // 2019
        Date(29, Jan, 2020), Date(29, Apr, 2020), Date(29, Jul, 2020), Date(28, Oct, 2020), // 2020
        Date(27, Jan, 2021), Date(28, Apr, 2021), Date(28, Jul, 2021), Date(27, Oct, 2021), // 2021
        Date(25, Jan, 2022)
    };

    // AU CPI publication dates are the last Wednesday of Jan, Apr, Jul and Oct. If that is not a good AU business 
    // day, it is the preceding good AU business day.
    Schedule s = makeSchedule(ScheduleData(ScheduleRules("2018-10-30", "2022-01-25", "3M", "AUD",
        "Preceding", "Unadjusted", "LastWednesday")));

    // Check
    BOOST_CHECK_EQUAL_COLLECTIONS(s.dates().begin(), s.dates().end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
