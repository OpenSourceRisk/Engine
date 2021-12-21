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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
