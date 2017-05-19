/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include "volsmile.hpp"
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/blackvariancesurface2.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace testsuite {

void VolSmileTest::testVolSmileEquityFlat() {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurface2 with a flat surface...");

    // Set up a curve with lots of strikes mixed around, always set the vol to 10% and make sure we
    // get 10% back when we ask for random time/strike pairs
    SavedSettings backup;
    Date refDate(19, May, 2017);
    Settings::instance().evaluationDate() = refDate;

    vector<Date> dates;
    dates.push_back(Date(19, Sep, 2017));
    dates.push_back(Date(19, May, 2018));
    dates.push_back(Date(19, May, 2019));

    vector<vector<Real> > strikes(dates.size());
    strikes[0].push_back(100.0); // { 100.0, 200.0, 300.0, 400.0, 500.0 },
    strikes[0].push_back(200.0);
    strikes[0].push_back(300.0);
    strikes[0].push_back(400.0);
    strikes[1].push_back(10.0); // { 10.0, 1000.0 }
    strikes[1].push_back(1000.0);
    strikes[2].push_back(50.0); //{ 50.0, 250.0, 300.0, 550.0}
    strikes[2].push_back(250.0);
    strikes[2].push_back(300.0);
    strikes[2].push_back(550.0);

    // build a set of vols the same "shape" as strikes, all 10%
    vector<vector<Volatility> > vols(strikes.size());
    for (Size i = 0; i < vols.size(); i++)
        vols[i] = vector<Volatility>(strikes[i].size(), 0.10);

    Calendar cal = TARGET();
    DayCounter dc = ActualActual();
    BlackVarianceSurface2 surface(refDate, cal, dates, strikes, vols, dc);

    // Check we get back the inputs
    for (Size i = 0; i < dates.size(); i++) {
        for (Size j = 0; j < strikes[i].size(); j++) {
            Real expectedVol = 0.10;
            BOOST_TEST_MESSAGE("Checking " << dates[i] << " and strike " << strikes[i][j]);
            Real actualVol = surface.blackVol(dates[i], strikes[i][j]);
            BOOST_CHECK_EQUAL(expectedVol, actualVol);
        }
    }

    // Now check a few (hard-coded) interpolation points.
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(dates[0], 150.0));  // on an input date, different strike
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(dates[0], 1000.0)); // on an input date, extrapolated strike
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(dates[1], 1.0));
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(dates[1], 1001.0));
    BOOST_TEST_MESSAGE("Now test with time");
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(1.5, 100.0)); // 1.5Y - between dates[1] and [2]
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(1.5, 10000.0));
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(5, 100.0)); // extrapolated time
    BOOST_CHECK_EQUAL(0.10, surface.blackVol(0.001, 100.0));
}

void VolSmileTest::testVolSmileEquitySmile() {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurface2 with a smile surface...");

    // Simple 3x3 grid
    SavedSettings backup;
    Date refDate(19, May, 2017);
    Settings::instance().evaluationDate() = refDate;

    vector<Date> dates;
    dates.push_back(Date(19, May, 2018));
    dates.push_back(Date(19, May, 2019));
    dates.push_back(Date(19, May, 2020));

    vector<vector<Real> > strikes(dates.size());
    strikes[0].push_back(100.0);
    strikes[0].push_back(200.0);
    strikes[0].push_back(300.0);
    strikes[1].push_back(110.0); // +10%
    strikes[1].push_back(220.0);
    strikes[1].push_back(330.0);
    strikes[2].push_back(120.0); // +20%
    strikes[2].push_back(240.0);
    strikes[2].push_back(360.0);

    // build a set of vols the same "shape" as strikes, all 10%
    vector<vector<Volatility> > vols(strikes.size());
    for (Size i = 0; i < vols.size(); i++) {
        vols[i] = vector<Volatility>(strikes[i].size());
        for (Size j = 0; j < vols[i].size(); j++)
            vols[i][j] = (1.0 + (i / 10.0)) * strikes[i][j] / 1000.0; // date[0]: K=100 => vol=10%, date[1] => 11%
    }

    Calendar cal = TARGET();
    DayCounter dc = ActualActual();
    BlackVarianceSurface2 surface(refDate, cal, dates, strikes, vols, dc);

    // allow a tolerance
    Real tolerance = 1e-8;

    // Check we get back the inputs
    BOOST_TEST_MESSAGE("Checking we get back the input vols");
    for (Size i = 0; i < dates.size(); i++) {
        for (Size j = 0; j < strikes[i].size(); j++) {
            Real expectedVol = vols[i][j];
            Real actualVol = surface.blackVol(dates[i], strikes[i][j]);
            BOOST_CHECK_CLOSE(expectedVol, actualVol, tolerance);
        }
    }

    // Now some basic interpolation checks
    BOOST_TEST_MESSAGE("Checking some interpolation values");
    BOOST_CHECK_CLOSE(surface.blackVol(dates[0], 150.0), 0.15, tolerance);
    BOOST_CHECK_CLOSE(surface.blackVol(dates[0], 250.0), 0.25, tolerance);

    BOOST_CHECK_CLOSE(surface.blackVol(dates[1], 150.0), 1.1 * 0.15, tolerance);
    BOOST_CHECK_CLOSE(surface.blackVol(dates[1], 160.0), 1.1 * 0.16, tolerance);

    BOOST_CHECK_CLOSE(surface.blackVol(dates[2], 200.0), 1.2 * 0.2, tolerance);
    BOOST_CHECK_CLOSE(surface.blackVol(dates[2], 300.0), 1.2 * 0.3, tolerance);

    // Now check with some times instead of times.
    // Linear in variance - just check it's between the 1y and 2y expected values (18% and 1.1x18%)
    Real vol = surface.blackVol(1.5, 180.0);
    BOOST_CHECK(0.18 < vol && vol < (0.18 * 1.1));

    vol = surface.blackVol(2.5, 180.0);
    BOOST_CHECK((0.18 * 1.1) < vol && vol < (0.18 * 1.2));
}

test_suite* VolSmileTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("VolSmileTest");
    suite->add(BOOST_TEST_CASE(&VolSmileTest::testVolSmileEquityFlat));
    suite->add(BOOST_TEST_CASE(&VolSmileTest::testVolSmileEquitySmile));
    return suite;
}

} // namespace testsuite
