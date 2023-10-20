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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/time/calendars/target.hpp>
#include <qle/calendars/belgium.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace check {

    void checkDates(const Schedule& s,
                     const std::vector<Date>& expected) {
        BOOST_CHECK(s.size() == expected.size());
        for (Size i=0; i<expected.size(); ++i) {
            if (s[i] != expected[i]) {
                BOOST_ERROR("expected " << expected[i]
                            << " at index " << i << ", "
                            "found " << s[i]);
            }
        }
    }
    void checkDay(const Schedule& s,
                     Weekday w) {
        //we check all but the first and last dates
        for (Size i=1; i<s.size() - 1; ++i) {
            if (s[i].weekday() != w) {
                BOOST_ERROR("expected " << w
                            << " at index " << i << ", "
                            "found " << s[i].weekday());
            }
        }
    }
} // namespace check

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DateGenerationTest)

BOOST_AUTO_TEST_CASE(testThirdThursday) {

    BOOST_TEST_MESSAGE("Testing ThirdThursday DateGeneration rule");

    std::vector<Date> expectedDates;

    expectedDates.push_back(Date(12, December, 2016));
    expectedDates.push_back(Date(16, March, 2017));
    expectedDates.push_back(Date(15, June, 2017));
    expectedDates.push_back(Date(21, September, 2017));
    expectedDates.push_back(Date(21, December, 2017));
    expectedDates.push_back(Date(15, March, 2018));
    expectedDates.push_back(Date(21, June, 2018));
    expectedDates.push_back(Date(20, September, 2018));
    expectedDates.push_back(Date(12, December, 2018));

    Schedule s =
        MakeSchedule().from(Date(12, December, 2016))
                      .to(Date(12, December, 2016) + Period(2, Years))
                      .withCalendar(TARGET())
                      .withTenor(3*Months)
                      .withConvention(ModifiedFollowing)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::ThirdThursday);

    check::checkDates(s, expectedDates);
    check::checkDay(s, Thursday);
}

BOOST_AUTO_TEST_CASE(testThirdFriday) {

    BOOST_TEST_MESSAGE("Testing ThirdFriday DateGeneration rule");

    std::vector<Date> expectedDates;

    expectedDates.push_back(Date(12, December, 2016));
    expectedDates.push_back(Date(17, March, 2017));
    expectedDates.push_back(Date(16, June, 2017));
    expectedDates.push_back(Date(15, September, 2017));
    expectedDates.push_back(Date(15, December, 2017));
    expectedDates.push_back(Date(16, March, 2018));
    expectedDates.push_back(Date(15, June, 2018));
    expectedDates.push_back(Date(21, September, 2018));
    expectedDates.push_back(Date(12, December, 2018));

    Schedule s =
        MakeSchedule().from(Date(12, December, 2016))
                      .to(Date(12, December, 2016) + Period(2, Years))
                      .withCalendar(TARGET())
                      .withTenor(3*Months)
                      .withConvention(ModifiedFollowing)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::ThirdFriday);

    check::checkDates(s, expectedDates);
    check::checkDay(s, Friday);
}

BOOST_AUTO_TEST_CASE(testMondayAfterThirdFriday) {

    BOOST_TEST_MESSAGE("Testing MondayAfterThirdFriday DateGeneration rule");

    std::vector<Date> expectedDates;

    expectedDates.push_back(Date(12, December, 2016));
    expectedDates.push_back(Date(20, March, 2017));
    expectedDates.push_back(Date(19, June, 2017));
    expectedDates.push_back(Date(18, September, 2017));
    expectedDates.push_back(Date(18, December, 2017));
    expectedDates.push_back(Date(19, March, 2018));
    expectedDates.push_back(Date(18, June, 2018));
    expectedDates.push_back(Date(24, September, 2018));
    expectedDates.push_back(Date(12, December, 2018));

    Schedule s =
        MakeSchedule().from(Date(12, December, 2016))
                      .to(Date(12, December, 2016) + Period(2, Years))
                      .withCalendar(TARGET())
                      .withTenor(3*Months)
                      .withConvention(ModifiedFollowing)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::MondayAfterThirdFriday);

    check::checkDates(s, expectedDates);
    check::checkDay(s, Monday);
}

BOOST_AUTO_TEST_CASE(testTuesdayAfterThirdFriday) {

    BOOST_TEST_MESSAGE("Testing TuesdayAfterThirdFriday DateGeneration rule");

    std::vector<Date> expectedDates;

    expectedDates.push_back(Date(12, December, 2016));
    expectedDates.push_back(Date(21, March, 2017));
    expectedDates.push_back(Date(20, June, 2017));
    expectedDates.push_back(Date(19, September, 2017));
    expectedDates.push_back(Date(19, December, 2017));
    expectedDates.push_back(Date(20, March, 2018));
    expectedDates.push_back(Date(19, June, 2018));
    expectedDates.push_back(Date(25, September, 2018));
    expectedDates.push_back(Date(12, December, 2018));

    Schedule s =
        MakeSchedule().from(Date(12, December, 2016))
                      .to(Date(12, December, 2016) + Period(2, Years))
                      .withCalendar(TARGET())
                      .withTenor(3*Months)
                      .withConvention(ModifiedFollowing)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::TuesdayAfterThirdFriday);

    check::checkDates(s, expectedDates);
    check::checkDay(s, Tuesday);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
