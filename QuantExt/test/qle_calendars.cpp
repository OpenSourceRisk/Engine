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
#include <ql/time/calendar.hpp>
#include <ql/time/calendars/austria.hpp>
#include <ql/time/calendars/thailand.hpp>
#include <ql/time/calendars/chile.hpp>
#include <qle/calendars/belgium.hpp>
#include <qle/calendars/cyprus.hpp>
#include <qle/calendars/colombia.hpp>
#include <qle/calendars/france.hpp>
#include <qle/calendars/greece.hpp>
#include <qle/calendars/ireland.hpp>
#include <qle/calendars/israel.hpp>
#include <qle/calendars/luxembourg.hpp>
#include <qle/calendars/malaysia.hpp>
#include <qle/calendars/netherlands.hpp>
#include <qle/calendars/peru.hpp>
#include <qle/calendars/philippines.hpp>
#include <qle/calendars/spain.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace check {

void checkCalendars(const std::vector<Date>& expectedHolidays, const std::vector<Date>& testHolidays) {

    for (Size i = 0; i < expectedHolidays.size(); i++) {
        if (testHolidays[i] != expectedHolidays[i])
            BOOST_FAIL("expected holiday was " << expectedHolidays[i] << " while calculated holiday is "
                                               << testHolidays[i]);
    }
}
} // namespace check

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CalendarsTest)

BOOST_AUTO_TEST_CASE(testPeruvianCalendar) {

    BOOST_TEST_MESSAGE("Testing Peruvian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(29, March, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(29, June, 2018));
    expectedHolidays.push_back(Date(27, July, 2018));
    expectedHolidays.push_back(Date(30, August, 2018));
    expectedHolidays.push_back(Date(31, August, 2018));
    expectedHolidays.push_back(Date(8, October, 2018));
    expectedHolidays.push_back(Date(1, November, 2018));
    expectedHolidays.push_back(Date(2, November, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));

    Calendar c = Peru();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testColombianCalendar) {

    BOOST_TEST_MESSAGE("Testing Colombian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(8, January, 2018));
    expectedHolidays.push_back(Date(19, March, 2018));
    expectedHolidays.push_back(Date(29, March, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(14, May, 2018));
    expectedHolidays.push_back(Date(4, June, 2018));
    expectedHolidays.push_back(Date(11, June, 2018));
    expectedHolidays.push_back(Date(2, July, 2018));
    expectedHolidays.push_back(Date(20, July, 2018));
    expectedHolidays.push_back(Date(7, August, 2018));
    expectedHolidays.push_back(Date(20, August, 2018));
    expectedHolidays.push_back(Date(15, October, 2018));
    expectedHolidays.push_back(Date(5, November, 2018));
    expectedHolidays.push_back(Date(12, November, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));

    Calendar c = Colombia();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testPhilippineCalendar) {

    BOOST_TEST_MESSAGE("Testing Philippine holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(2, January, 2018));
    expectedHolidays.push_back(Date(29, March, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(9, April, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(12, June, 2018));
    expectedHolidays.push_back(Date(21, August, 2018));
    expectedHolidays.push_back(Date(27, August, 2018));
    expectedHolidays.push_back(Date(1, November, 2018));
    expectedHolidays.push_back(Date(30, November, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));
    expectedHolidays.push_back(Date(31, December, 2018));

    Calendar c = Philippines();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testThaiCalendar) {

    BOOST_TEST_MESSAGE("Testing Thai holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    //expectedHolidays.push_back(Date(2, January, 2018));
    expectedHolidays.push_back(Date(1, March, 2018)); // Makha Bucha Day
    expectedHolidays.push_back(Date(6, April, 2018));
    expectedHolidays.push_back(Date(13, April, 2018));
    expectedHolidays.push_back(Date(16, April, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(29, May, 2018));  // Wisakha Bucha Day
    expectedHolidays.push_back(Date(27, July, 2018)); // Asarnha Bucha Day
    expectedHolidays.push_back(Date(30, July, 2018));
    expectedHolidays.push_back(Date(13, August, 2018));
    expectedHolidays.push_back(Date(15, October, 2018));
    expectedHolidays.push_back(Date(23, October, 2018));
    expectedHolidays.push_back(Date(5, December, 2018));
    expectedHolidays.push_back(Date(10, December, 2018));
    expectedHolidays.push_back(Date(31, December, 2018));

    Calendar c = Thailand();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testMalaysianCalendar) {

    BOOST_TEST_MESSAGE("Testing Malaysian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(1, February, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(31, August, 2018));
    expectedHolidays.push_back(Date(17, September, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));

    Calendar c = Malaysia();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testChileanCalendar) {

    BOOST_TEST_MESSAGE("Testing Chilean holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(16, January, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(21, May, 2018));
    expectedHolidays.push_back(Date(2, July, 2018));
    expectedHolidays.push_back(Date(16, July, 2018));
    expectedHolidays.push_back(Date(15, August, 2018));
    expectedHolidays.push_back(Date(17, September, 2018));
    expectedHolidays.push_back(Date(18, September, 2018));
    expectedHolidays.push_back(Date(19, September, 2018));
    expectedHolidays.push_back(Date(15, October, 2018));
    expectedHolidays.push_back(Date(1, November, 2018));
    expectedHolidays.push_back(Date(2, November, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));
    expectedHolidays.push_back(Date(31, December, 2018));

    Calendar c = Chile();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testNetherlandianCalendar) {

    BOOST_TEST_MESSAGE("Testing Netherlandian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(2, April, 2018));
    expectedHolidays.push_back(Date(27, April, 2018));
    expectedHolidays.push_back(Date(10, May, 2018));
    expectedHolidays.push_back(Date(21, May, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));
    expectedHolidays.push_back(Date(26, December, 2018));

    Calendar c = Netherlands();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testFrenchCalendar) {

    BOOST_TEST_MESSAGE("Testing French holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2018));
    expectedHolidays.push_back(Date(30, March, 2018));
    expectedHolidays.push_back(Date(2, April, 2018));
    expectedHolidays.push_back(Date(1, May, 2018));
    expectedHolidays.push_back(Date(8, May, 2018));
    expectedHolidays.push_back(Date(10, May, 2018));
    expectedHolidays.push_back(Date(21, May, 2018));
    expectedHolidays.push_back(Date(15, August, 2018));
    expectedHolidays.push_back(Date(1, November, 2018));
    expectedHolidays.push_back(Date(25, December, 2018));
    expectedHolidays.push_back(Date(26, December, 2018));

    Calendar c = France();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2018), Date(31, December, 2018));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testIsraelTelbor) {

    BOOST_TEST_MESSAGE("Testing Israel Telbor standard weekends");

    Calendar c = QuantExt::Israel(QuantExt::Israel::Telbor);

    BOOST_CHECK(!c.isWeekend(Friday));
    BOOST_CHECK(c.isWeekend(Saturday));
    BOOST_CHECK(c.isWeekend(Sunday));
}

BOOST_AUTO_TEST_CASE(testIsraelTelAviv) {

    BOOST_TEST_MESSAGE("Testing Israel TelAviv weekends");

    Calendar c = QuantExt::Israel(QuantExt::Israel::Settlement);

    BOOST_CHECK(c.isWeekend(Friday));
    BOOST_CHECK(c.isWeekend(Saturday));
    BOOST_CHECK(!c.isWeekend(Sunday));
}

BOOST_AUTO_TEST_CASE(testAustrianCalendar) {

    BOOST_TEST_MESSAGE("Testing Austrian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2020));
    expectedHolidays.push_back(Date(6, January, 2020));
    expectedHolidays.push_back(Date(13, April, 2020));
    expectedHolidays.push_back(Date(1, May, 2020));
    expectedHolidays.push_back(Date(21, May, 2020));
    expectedHolidays.push_back(Date(1, June, 2020));
    expectedHolidays.push_back(Date(11, June, 2020));
    // expectedHolidays.push_back(Date(15, August, 2020)); //Saturday
    expectedHolidays.push_back(Date(26, October, 2020));
    // expectedHolidays.push_back(Date(1, November, 2020)); //Sunday
    expectedHolidays.push_back(Date(8, December, 2020));
    expectedHolidays.push_back(Date(25, December, 2020));
    // expectedHolidays.push_back(Date(26, December, 2020)); //Saturday

    Calendar c = Austria();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2020), Date(31, December, 2020));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testSpanishCalendar) {

    BOOST_TEST_MESSAGE("Testing Spanish holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2020));
    expectedHolidays.push_back(Date(6, January, 2020));
    expectedHolidays.push_back(Date(10, April, 2020));
    expectedHolidays.push_back(Date(1, May, 2020));
    // expectedHolidays.push_back(Date(15, August, 2020)); //Saturday
    expectedHolidays.push_back(Date(12, October, 2020));
    // expectedHolidays.push_back(Date(1, November, 2020)); //Sunday
    // expectedHolidays.push_back(Date(6, December, 2020));  //Sunday
    expectedHolidays.push_back(Date(8, December, 2020));
    expectedHolidays.push_back(Date(25, December, 2020));

    Calendar c = Spain();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2020), Date(31, December, 2020));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testLuxembourgianCalendar) {

    BOOST_TEST_MESSAGE("Testing Luxembourgian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2020));
    expectedHolidays.push_back(Date(13, April, 2020));
    expectedHolidays.push_back(Date(1, May, 2020));
    // expectedHolidays.push_back(Date(9, May, 2020)); //Saturday
    expectedHolidays.push_back(Date(21, May, 2020));
    expectedHolidays.push_back(Date(1, June, 2020));
    expectedHolidays.push_back(Date(23, June, 2020));
    // expectedHolidays.push_back(Date(15, August, 2020)); //Saturday
    // expectedHolidays.push_back(Date(1, November, 2020)); //Sunday
    expectedHolidays.push_back(Date(25, December, 2020));
    // expectedHolidays.push_back(Date(26, December, 2020)); //Saturday

    Calendar c = Luxembourg();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2020), Date(31, December, 2020));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testBelgianCalendar) {

    BOOST_TEST_MESSAGE("Testing Belgian holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2020));
    // expectedHolidays.push_back(Date(12, April, 2020)); //Sunday
    expectedHolidays.push_back(Date(13, April, 2020));
    expectedHolidays.push_back(Date(1, May, 2020));
    expectedHolidays.push_back(Date(21, May, 2020));
    // expectedHolidays.push_back(Date(31, May, 2020)); //Sunday
    expectedHolidays.push_back(Date(1, June, 2020));
    expectedHolidays.push_back(Date(21, July, 2020));
    // expectedHolidays.push_back(Date(15, August, 2020)); //Saturday
    // expectedHolidays.push_back(Date(1, November, 2020)); //Sunday
    expectedHolidays.push_back(Date(11, November, 2020));
    expectedHolidays.push_back(Date(25, December, 2020));

    Calendar c = Belgium();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2020), Date(31, December, 2020));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testCyprusCalendar) {

    BOOST_TEST_MESSAGE("Testing Cyprus holiday list");

    std::vector<Date> expectedHolidays;

    //expectedHolidays.push_back(Date(1, January, 2022)); //weekend
    expectedHolidays.push_back(Date(6, January, 2022));
    expectedHolidays.push_back(Date(7, March, 2022));
    expectedHolidays.push_back(Date(25, March, 2022));
    expectedHolidays.push_back(Date(1, April, 2022));
    expectedHolidays.push_back(Date(22, April, 2022));
    expectedHolidays.push_back(Date(25, April, 2022));
    expectedHolidays.push_back(Date(26, April, 2022));
    //expectedHolidays.push_back(Date(1, May, 2022)); //weekend
    expectedHolidays.push_back(Date(13, June, 2022));
    expectedHolidays.push_back(Date(15, August, 2022));
    //expectedHolidays.push_back(Date(1, October, 2022)); weekend
    expectedHolidays.push_back(Date(28, October, 2022));
    //expectedHolidays.push_back(Date(25, December, 2022));weekend
    expectedHolidays.push_back(Date(26, December, 2022));
    
    Calendar c = Cyprus();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2022), Date(31, December, 2022));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testGreeceCalendar) {

    BOOST_TEST_MESSAGE("Testing Greece holiday list");

    std::vector<Date> expectedHolidays;
    
    expectedHolidays.push_back(Date(1, January, 2021));
    expectedHolidays.push_back(Date(6, January, 2021));
    expectedHolidays.push_back(Date(15, March, 2021));
    expectedHolidays.push_back(Date(25, March, 2021));
    expectedHolidays.push_back(Date(30, April, 2021));
    expectedHolidays.push_back(Date(3, May, 2021));
    expectedHolidays.push_back(Date(4, May, 2021));
    expectedHolidays.push_back(Date(21, June, 2021)); 
    expectedHolidays.push_back(Date(28, October, 2021));
 
    expectedHolidays.push_back(Date(6, January, 2022));
    expectedHolidays.push_back(Date(7, March, 2022));
    expectedHolidays.push_back(Date(25, March, 2022));
    expectedHolidays.push_back(Date(22, April, 2022));
    expectedHolidays.push_back(Date(25, April, 2022));
    expectedHolidays.push_back(Date(13, June, 2022));
    expectedHolidays.push_back(Date(15, August, 2022));
    expectedHolidays.push_back(Date(28, October, 2022));
    expectedHolidays.push_back(Date(26, December, 2022));

    

    Calendar c = Greece();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2021), Date(31, December, 2022));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testIrishStockExchangeCalendar) {

    BOOST_TEST_MESSAGE("Testing Irish Stock Exchange holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2021));
    expectedHolidays.push_back(Date(17, March, 2021));
    expectedHolidays.push_back(Date(2, April, 2021));
    expectedHolidays.push_back(Date(5, April, 2021));
    expectedHolidays.push_back(Date(3, May, 2021));
    expectedHolidays.push_back(Date(7, June, 2021));
    expectedHolidays.push_back(Date(2, August, 2021));
    expectedHolidays.push_back(Date(25, October, 2021));
    //expectedHolidays.push_back(Date(25, December, 2021)); Falls on weekend
    //expectedHolidays.push_back(Date(26, December, 2021)); Falls on weekend
    expectedHolidays.push_back(Date(27, December, 2021));
    expectedHolidays.push_back(Date(28, December, 2021));

    Calendar c = Ireland();

    std::vector<Date> hol = c.holidayList(Date(1, January, 2021), Date(31, December, 2021));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testIrishBankHolidaysCalendar) {

    BOOST_TEST_MESSAGE("Testing Irish bank holiday list");

    std::vector<Date> expectedHolidays;

    expectedHolidays.push_back(Date(1, January, 2021));
    expectedHolidays.push_back(Date(17, March, 2021));
    expectedHolidays.push_back(Date(2, April, 2021));
    expectedHolidays.push_back(Date(5, April, 2021));
    expectedHolidays.push_back(Date(3, May, 2021));
    expectedHolidays.push_back(Date(7, June, 2021));
    expectedHolidays.push_back(Date(2, August, 2021));
    expectedHolidays.push_back(Date(25, October, 2021));
    // expectedHolidays.push_back(Date(25, December, 2021)); Falls on weekend
    // expectedHolidays.push_back(Date(26, December, 2021)); Falls on weekend
    expectedHolidays.push_back(Date(27, December, 2021));
    expectedHolidays.push_back(Date(28, December, 2021));
    expectedHolidays.push_back(Date(29, December, 2021));

    Calendar c = Ireland(Ireland::BankHolidays);

    std::vector<Date> hol = c.holidayList(Date(1, January, 2021), Date(31, December, 2021));

    BOOST_CHECK(hol.size() == expectedHolidays.size());

    check::checkCalendars(expectedHolidays, hol);
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
