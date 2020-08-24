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

#include <algorithm>
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ored/marketdata/csvloader.hpp>
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/time/date.hpp>
#include <string>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

using ore::test::TopLevelFixture;

namespace bdata = boost::unit_test::data;

namespace {

// Fixture used in test case below:
// - Provide calendar adjustment file
// - Provide expected calendars from another system
class F : public TopLevelFixture {
public:
    CalendarAdjustmentConfig calendarAdjustments;
    Date startDate;
    Date endDate;

    F() {
        calendarAdjustments.fromFile(TEST_INPUT_FILE("calendaradjustments.xml"));
        startDate = Date(1, Jan, 2019);
        endDate = Date(31, Dec, 2020);
    }

    ~F() {}
};

struct TestDatum {
    string calendarName;
    std::vector<Date> holidays;
};

std::vector<TestDatum> loadExpectedHolidays() {
    // load from file
    string fileName = TEST_INPUT_FILE("holidays.csv");
    std::vector<TestDatum> data;
    ifstream file;
    file.open(fileName);
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    std::string line;
    // skip empty lines
    while (!file.eof()) {
        getline(file, line);
        boost::trim(line);
        if (line != "") {
            vector<string> elements;
            boost::split(elements, line, boost::is_any_of(","), boost::token_compress_on);
            QL_REQUIRE(elements.size() > 1, "Not enough elements in the calendar");
            TestDatum td;
            td.calendarName = elements.front();
            for (Size i = 1; i < elements.size(); i++) {
                Date d = parseDate(elements[i]);
                if (d.weekday() != Saturday && d.weekday() != Sunday) {
                    td.holidays.push_back(parseDate(elements[i]));
                }
            }
            data.push_back(td);
        }
    }
    file.close();
    return data;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, F)

BOOST_AUTO_TEST_SUITE(CalendarAdjustmentsTests)

// BOOST_DATA_TEST_CASE(testCalendarAdjustmentRealCalendars, bdata::make(loadExpectedHolidays()), expectedHolidays)
BOOST_AUTO_TEST_CASE(testCalendarAdjustmentRealCalendars) {
    // loop over expected holidays, for each calendar call parseCalendar()
    // and check that the holidays match the expected ones
    for (auto expectedHolidays : loadExpectedHolidays()) {
        vector<Date> qcalHols;
        qcalHols = parseCalendar(expectedHolidays.calendarName).holidayList(startDate, endDate, false);
        BOOST_CHECK_EQUAL_COLLECTIONS(qcalHols.begin(), qcalHols.end(), expectedHolidays.holidays.begin(),
                                      expectedHolidays.holidays.end());
    }
}

BOOST_AUTO_TEST_CASE(testCalendarAdjustment) {
    BOOST_TEST_MESSAGE("Testing calendar adjustments...");

    CalendarAdjustmentConfig cac;
    CalendarAdjustmentConfig cacad;
    BOOST_REQUIRE(cac.getCalendars().empty());

    // adding UK holiday not in Quantlub calendar
    cac.addHolidays("UK", Date(29, April, 2011));
    // Checking that we get it back
    // Note getCalenders get the quantlib name
    BOOST_REQUIRE(cac.getCalendars() == set<string>({"UK settlement"}));

    BOOST_REQUIRE(cac.getHolidays("UK") == set<Date>({Date(29, April, 2011)}));
    BOOST_REQUIRE(cac.getBusinessDays("UK").empty());

    cac.addBusinessDays("UK", Date(25, December, 2011));
    BOOST_REQUIRE(cac.getBusinessDays("UK") == set<Date>({Date(25, December, 2011)}));
    cac.addHolidays("UK", Date(25, December, 2011));

    cacad.addHolidays("JPY", Date(1, May, 2019));
    cac.append(cacad);
    set<string> expcal = {"Japan", "UK settlement"};
    set<string> res = {cac.getCalendars()};
    BOOST_CHECK_EQUAL_COLLECTIONS(res.begin(), res.end(), expcal.begin(), expcal.end());
    BOOST_REQUIRE(cac.getHolidays("JPY") == set<Date>({Date(1, May, 2019)}));
}

BOOST_AUTO_TEST_CASE(testInvalidCalendarAdjustment) {
    BOOST_TEST_MESSAGE("Testing that incorrect CalendarAdjustments are not accepted...");

    // we check that new calendars can't be declared using another new calendar as a base
    CalendarAdjustmentConfig calendarAdjustments_1;
    BOOST_CHECK_THROW(calendarAdjustments_1.fromFile(TEST_INPUT_FILE("invalid_calendaradjustments_1.xml")), QuantLib::Error);
    
    // we check that new calendars can't be declared using a joint calendar as a base 
    CalendarAdjustmentConfig calendarAdjustments_2;
    BOOST_CHECK_THROW(calendarAdjustments_2.fromFile(TEST_INPUT_FILE("invalid_calendaradjustments_2.xml")), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
