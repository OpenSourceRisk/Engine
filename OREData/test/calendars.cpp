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

#include <ored/utilities/parsers.hpp>
#include <ql/time/calendars/all.hpp>
#include <qle/calendars/all.hpp>
#include <test/calendars.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

namespace testsuite {

struct test_data {
    const char* name;
    Calendar cal;
};

static struct test_data cal_data[] = {
    // Name, QL Cal
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
    {"NYB,SYB", JointCalendar(UnitedStates(), Australia())}};

void CalendarNameTest::testCalendarNameParsing() {
    BOOST_TEST_MESSAGE("Testing Calendar name parsing...");

    Size len = sizeof(cal_data) / sizeof(cal_data[0]);
    for (Size i = 0; i < len; ++i) {
        string name(cal_data[i].name);
        Calendar ql_cal = cal_data[i].cal;

        Calendar cal;
        try {
            cal = ore::data::parseCalendar(name);
        } catch (...) {
            BOOST_FAIL("ore::data::parseCalendar() failed to parse " << name);
        }

        if (cal.empty() || cal != ql_cal)
            BOOST_FAIL("ore::data::parseCalendar(" << name << ") returned cal " << cal.name() << " expected "
                                                   << ql_cal.name());

        BOOST_TEST_MESSAGE("Parsed " << name << " and got " << cal.name());
    }
}

test_suite* CalendarNameTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CalendarNameTest");
    suite->add(BOOST_TEST_CASE(&CalendarNameTest::testCalendarNameParsing));
    return suite;
}
} // namespace testsuite
