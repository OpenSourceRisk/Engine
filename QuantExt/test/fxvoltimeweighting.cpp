/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/calendars/amendedcalendar.hpp>
#include <qle/termstructures/fxvoltimeweighting.hpp>

#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FxVolTimeWeighting)

BOOST_AUTO_TEST_CASE(testSimpleWeights) {

    BOOST_TEST_MESSAGE("Testing simple case of fx vol time weighting...");

    SavedSettings backup;

    Date refDate = Date(10, Jul, 2024);

    AmendedCalendar c1(WeekendsOnly(), "cal1");
    AmendedCalendar c2(WeekendsOnly(), "cal1");

    c1.addHoliday(Date(25, Jul, 2024));
    c1.addHoliday(Date(31, Jul, 2024));

    c2.addHoliday(Date(29, Jul, 2024));
    c2.addHoliday(Date(31, Jul, 2024));

    FxVolatilityTimeWeighting w(refDate, Actual365Fixed(), {0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3}, {{c1, 0.5}, {c2, 0.4}},
                                {{Date(23, Jul, 2024), 8.0}});

    double tol = 1E-12;

    BOOST_CHECK_CLOSE(w(Date(10, Jul, 2024)), 1.0 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(11, Jul, 2024)), 2.0 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(12, Jul, 2024)), 3.0 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(13, Jul, 2024)), 3.3 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(14, Jul, 2024)), 3.6 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(15, Jul, 2024)), 4.6 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(16, Jul, 2024)), 5.6 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(17, Jul, 2024)), 6.6 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(18, Jul, 2024)), 7.6 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(19, Jul, 2024)), 8.6 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(20, Jul, 2024)), 8.9 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(21, Jul, 2024)), 9.2 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(22, Jul, 2024)), 10.2 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(23, Jul, 2024)), 18.2 / 365.0, tol); // event
    BOOST_CHECK_CLOSE(w(Date(24, Jul, 2024)), 19.2 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(25, Jul, 2024)), 19.7 / 365.0, tol); // c1
    BOOST_CHECK_CLOSE(w(Date(26, Jul, 2024)), 20.7 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(27, Jul, 2024)), 21.0 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(28, Jul, 2024)), 21.3 / 365.0, tol); // weekend
    BOOST_CHECK_CLOSE(w(Date(29, Jul, 2024)), 21.7 / 365.0, tol); // c2
    BOOST_CHECK_CLOSE(w(Date(30, Jul, 2024)), 22.7 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(31, Jul, 2024)), 22.9 / 365.0, tol); // c1, c2
    BOOST_CHECK_CLOSE(w(Date(1, Aug, 2024)), 23.9 / 365.0, tol);
    BOOST_CHECK_CLOSE(w(Date(2, Aug, 2024)), 24.9 / 365.0, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
