/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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
#include <qle/calendars/russia.hpp>
#include <qle/calendars/unitedarabemirates.hpp>
#include <ql/settings.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CalendarTests)

BOOST_AUTO_TEST_CASE(testRussiaModified) {

    BOOST_TEST_MESSAGE("Testing RussiaModified calendar");

    Calendar russiaSettlement = Russia(Russia::Settlement);
    Calendar russiaExchange = Russia(Russia::MOEX);
    Calendar russiaModifiedSettlement = RussiaModified(Russia::Settlement);
    Calendar russiaModifiedExchange = RussiaModified(Russia::MOEX);

    // check that the new modified Russia exchange calendar does not throw before 2012
    Date pre(31, December, 2011);
    BOOST_CHECK_THROW(russiaExchange.isBusinessDay(pre), QuantLib::Error);
    BOOST_CHECK_NO_THROW(russiaModifiedExchange.isBusinessDay(pre));

    // before 2012: The new modifed Russia exchange calendar uses QuantLib's Russia settlement
    Date start2011(1, Jan, 2011);
    Date end2011(31, Dec, 2011);
    for(Date d = start2011; d <= end2011; d++) {
        BOOST_CHECK_EQUAL(russiaSettlement.isBusinessDay(d), russiaModifiedSettlement.isBusinessDay(d));
        BOOST_CHECK_EQUAL(russiaSettlement.isBusinessDay(d), russiaModifiedExchange.isBusinessDay(d));
    }

    // 2012 onwards: The new modifed Russia exchange calendar matches uses QuantLib's Russia exchange
    Date start2012(1, Jan, 2012);
    Date end2012(31, Dec, 2012);
    for(Date d = start2012; d <= end2012; d++) {
        BOOST_CHECK_EQUAL(russiaSettlement.isBusinessDay(d), russiaModifiedSettlement.isBusinessDay(d));
        BOOST_CHECK_EQUAL(russiaExchange.isBusinessDay(d), russiaModifiedExchange.isBusinessDay(d));
    }
}

BOOST_AUTO_TEST_CASE(updatedArabEmirates){
    BOOST_TEST_MESSAGE("Testing updated UAE calendar");
    Calendar UAE = UnitedArabEmirates();
    std::vector<QuantLib::Date> testDates{
        Date(4,Feb,2021),
        Date(5,Feb,2021),
        Date(6,Feb,2021),
        Date(7,Feb,2021),
        Date(8,Feb,2021),
        Date(9,Feb,2021),
        Date(10,Feb,2021),
        Date(30,Dec,2021),
        Date(31,Dec,2021),
        Date(1,Jan,2022), // this is always holiday, but being Sat it serves the goal of the test
        Date(2,Jan,2022),
        Date(3,Jan,2022),
        Date(4,Jan,2022),
        Date(5,Jan,2022),
        Date(4,Feb,2022),
        Date(5,Feb,2022),
        Date(6,Feb,2022),
        Date(7,Feb,2022),
        Date(8,Feb,2022),
        Date(9,Feb,2022),
        Date(10,Feb,2022),
    };

    for (auto d:testDates){
        if(d.year()<2022)
            switch (d.weekday()) {
            case QuantLib::Friday:
            case QuantLib::Saturday:
                BOOST_TEST(!UAE.isBusinessDay(d));
                break ;
            default:
                BOOST_TEST(UAE.isBusinessDay(d));
                break ;
            }
        else{
            switch (d.weekday()) {
            case QuantLib::Saturday:
            case QuantLib::Sunday:
                BOOST_TEST(!UAE.isBusinessDay(d));
                break ;
            default:
                BOOST_TEST(UAE.isBusinessDay(d));
                break ;
            }
        }
    }

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
