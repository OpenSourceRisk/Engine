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
#include <ored/utilities/parsers.hpp>
#include <ql/time/calendars/all.hpp>
#include <qle/calendars/all.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;

using std::ostream;

namespace {

struct TestDatum {
    const char* calendarName;
    Calendar calendar;
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the TestDatum
ostream& operator<<(ostream& os, const TestDatum& testDatum) {
    return os << "[" << testDatum.calendarName << "," << testDatum.calendar.name() << "]";
}

TestDatum calendarData[] = {
    {"TGT", TARGET()},
    {"EUR", TARGET()},
    {"CHF", Switzerland()},
    {"USD", UnitedStates()},
    {"GBP", UnitedKingdom()},
    {"CAD", Canada()},
    {"AUD", Australia()},
    {"JPY", Japan()},
    {"ZAR", SouthAfrica()},
    {"SEK", Sweden()},
    {"ARS", Argentina()},
    {"BRL", Brazil()},
    {"CNH", China()},
    {"CNY", China()},
    {"CZK", CzechRepublic()},
    {"DKK", Denmark()},
    {"FIN", Finland()},
    {"HKD", HongKong()},
    {"ISK", Iceland()},
    {"INR", India()},
    {"IDR", Indonesia()},
    {"MXN", Mexico()},
    {"NZD", NewZealand()},
    {"NOK", Norway()},
    {"PLN", Poland()},
    {"RUB", Russia()},
    {"SAR", SaudiArabia()},
    {"SGD", Singapore()},
    {"KRW", SouthKorea()},
    {"TWD", Taiwan()},
    {"TRY", Turkey()},
    {"UAH", Ukraine()},
    {"HUF", Hungary()},
    // Emerging currencies that default to TARGET
    {"AED", TARGET()},
    {"BHD", TARGET()},
    {"CLF", TARGET()},
    {"CLP", Chile()},
    {"COP", Colombia()},
    {"EGP", TARGET()},
    {"ILS", TARGET()},
    {"KWD", TARGET()},
    {"KZT", TARGET()},
    {"MAD", TARGET()},
    {"MXV", TARGET()},
    {"MYR", Malaysia()},
    {"NGN", TARGET()},
    {"OMR", TARGET()},
    {"PEN", Peru()},
    {"PHP", Philippines()},
    {"QAR", TARGET()},
    {"RON", TARGET()},
    {"THB", QuantExt::Thailand()},
    {"TND", TARGET()},
    {"VND", TARGET()},
    // joint calendars
    {"US,TARGET", JointCalendar(UnitedStates(), TARGET())},
    {"NYB,TGT", JointCalendar(UnitedStates(), TARGET())},
    {"NYB,LNB", JointCalendar(UnitedStates(), UnitedKingdom())},
    {"LNB,ZUB", JointCalendar(UnitedKingdom(), Switzerland())},
    {"LNB,NYB,TGT", JointCalendar(UnitedKingdom(), UnitedStates(), TARGET())},
    {"NYB,ZUB,LNB", JointCalendar(UnitedStates(), Switzerland(), UnitedKingdom())},
    {"NYB,TRB,LNB", JointCalendar(UnitedStates(), Canada(), UnitedKingdom())},
    {"TKB,USD,LNB", JointCalendar(Japan(), UnitedStates(), UnitedKingdom())},
    {"NYB,SYB", JointCalendar(UnitedStates(), Australia())}
};

}

BOOST_AUTO_TEST_SUITE(OREDataTestSuite)

BOOST_AUTO_TEST_SUITE(CalendarTests)

BOOST_DATA_TEST_CASE(testCalendarNameParsing, bdata::make(calendarData), calendarDatum) {
    
    Calendar calendar;
    BOOST_REQUIRE_NO_THROW(calendar = ore::data::parseCalendar(calendarDatum.calendarName));
    BOOST_REQUIRE(!calendar.empty());
    BOOST_CHECK_EQUAL(calendar, calendarDatum.calendar);

    BOOST_TEST_MESSAGE("Parsed " << calendarDatum.calendarName << " and got " << calendar.name());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
