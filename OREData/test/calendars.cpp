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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ored/utilities/parsers.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/time/calendars/all.hpp>
// #include <qle/calendars/austria.hpp>
#include <qle/calendars/amendedcalendar.hpp>
#include <qle/calendars/belgium.hpp>
//#include <qle/calendars/chile.hpp>
#include <qle/calendars/cme.hpp>
#include <qle/calendars/colombia.hpp>
#include <qle/calendars/france.hpp>
#include <qle/calendars/ice.hpp>
#include <qle/calendars/luxembourg.hpp>
#include <qle/calendars/malaysia.hpp>
#include <qle/calendars/peru.hpp>
#include <qle/calendars/philippines.hpp>
#include <qle/calendars/russia.hpp>
#include <qle/calendars/spain.hpp>
#include <qle/calendars/switzerland.hpp>
#include <qle/calendars/wmr.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

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

TestDatum calendarData[] = {{"TGT", TARGET()},
                            {"EUR", TARGET()},
                            {"ATS", QuantExt::Austria()},
                            {"FRF", QuantExt::France()},
                            {"CHF", QuantExt::Switzerland()},
                            {"USD", UnitedStates(UnitedStates::Settlement)},
                            {"GBP", UnitedKingdom()},
                            {"CAD", Canada()},
                            {"AUD", Australia()},
                            {"JPY", Japan()},
                            {"ZAR", SouthAfrica()},
                            {"SEK", Sweden()},
                            {"ARS", Argentina()},
                            {"BWP", Botswana()},
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
                            {"KRW", SouthKorea(SouthKorea::Settlement)},
                            {"TWD", Taiwan()},
                            {"TRY", Turkey()},
                            {"UAH", Ukraine()},
                            {"HUF", Hungary()},
                            // Emerging currencies that default to WeekendsOnly
                            {"CLP", Chile()},
                            {"COP", Colombia()},
                            {"ILS", Israel()},
                            {"MYR", Malaysia()},
                            {"PEN", Peru()},
                            {"PHP", Philippines()},
                            {"RON", Romania()},
                            {"THB", Thailand()},
                            {"CHF", QuantExt::Switzerland()},
                            {"ZA", SouthAfrica()},
                            {"MISX", RussiaModified(Russia::MOEX)},
                            {"XSWX", QuantExt::Switzerland(QuantExt::Switzerland::SIX)},
                            {"XLON", UnitedKingdom(UnitedKingdom::Exchange)},
                            {"WMR", Wmr()},
                            {"LUX", Luxembourg()},
                            {"BEL", Belgium()},
                            {"ESP", Spain()},
                            {"AUT", Austria()},
                            // ICE exchange calendars
                            {"ICE_FuturesUS", ICE(ICE::FuturesUS)},
                            {"ICE_FuturesUS_1", ICE(ICE::FuturesUS_1)},
                            {"ICE_FuturesUS_2", ICE(ICE::FuturesUS_2)},
                            {"ICE_FuturesEU", ICE(ICE::FuturesEU)},
                            {"ICE_FuturesEU_1", ICE(ICE::FuturesEU_1)},
                            {"ICE_EndexEnergy", ICE(ICE::EndexEnergy)},
                            {"ICE_EndexEquities", ICE(ICE::EndexEquities)},
                            {"ICE_SwapTradeUS", ICE(ICE::SwapTradeUS)},
                            {"ICE_SwapTradeUK", ICE(ICE::SwapTradeUK)},
                            {"ICE_FuturesSingapore", ICE(ICE::FuturesSingapore)},
                            // CME exchange calendar
                            {"CME", CME()},
                            // joint calendars
                            {"US,TARGET", JointCalendar(UnitedStates(UnitedStates::Settlement), TARGET())},
                            {"NYB,TGT", JointCalendar(UnitedStates(UnitedStates::Settlement), TARGET())},
                            {"NYB,LNB", JointCalendar(UnitedStates(UnitedStates::Settlement), UnitedKingdom())},
                            {"LNB,ZUB", JointCalendar(UnitedKingdom(), QuantExt::Switzerland())},
                            {"LNB,NYB,TGT", JointCalendar(UnitedKingdom(), UnitedStates(UnitedStates::Settlement), TARGET())},
                            {"NYB,ZUB,LNB", JointCalendar(UnitedStates(UnitedStates::Settlement), QuantExt::Switzerland(), UnitedKingdom())},
                            {"NYB,TRB,LNB", JointCalendar(UnitedStates(UnitedStates::Settlement), Canada(), UnitedKingdom())},
                            {"TKB,USD,LNB", JointCalendar(Japan(), UnitedStates(UnitedStates::Settlement), UnitedKingdom())},
                            {"NYB,SYB", JointCalendar(UnitedStates(UnitedStates::Settlement), Australia())}};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

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
