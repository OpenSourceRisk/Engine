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
#include <boost/test/data/test_case.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/time/date.hpp>


using namespace ore::data;
using namespace boost::unit_test_framework;
using namespace std;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CalendarAdjustmentsTests)

BOOST_AUTO_TEST_CASE(testCalendarAdjustment) {
    BOOST_TEST_MESSAGE("Testing calendar adjustments...");

    CalendarAdjustmentConfig cac;
    CalendarAdjustmentConfig cacad;
    BOOST_REQUIRE(cac.getCalendars().empty());

    //adding UK holiday not in Quantlub calendar
    cac.addHolidays("UK", Date(29, April, 2011));
    //Checking that we get it back
    BOOST_REQUIRE(cac.getCalendars() == set<string>({ "UK settlement" }));

    BOOST_REQUIRE(cac.getHolidays("UK") == set<Date>({ Date(29, April, 2011)}));
    BOOST_REQUIRE(cac.getBusinessDays("UK").empty());
    
    cac.addBusinessDays("UK", Date(25, December, 2011));
    BOOST_REQUIRE(cac.getBusinessDays("UK") == set<Date>({ Date(25, December, 2011) }));
    cac.addHolidays("UK", Date(25, December, 2011));

    cacad.addHolidays("JPY", Date(1, May, 2019));
    cac.append(cacad);
    set<string> expcal = { "Japan", "UK settlement" };
    set<string> res = { cac.getCalendars() };
    BOOST_CHECK_EQUAL_COLLECTIONS(res.begin(),res.end(), expcal.begin(), expcal.end());
    BOOST_REQUIRE(cac.getHolidays("JPY") == set<Date>({ Date(1, May, 2019) }));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
