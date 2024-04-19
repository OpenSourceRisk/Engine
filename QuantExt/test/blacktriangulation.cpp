/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/blacktriangulationatmvol.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackTriangulationATMVolTest)

BOOST_AUTO_TEST_CASE(testBlackVol) {

    BOOST_TEST_MESSAGE("Testing BlackTriangulationATMVol");

    SavedSettings backup;

    Date today(5, Feb, 2019);
    Settings::instance().evaluationDate() = today;
    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    // Set up one vol and a correlation of 1.
    // BlackTriangulationATM vol should return zero vol for every time and string
    Handle<BlackVolTermStructure> constantVol1(QuantLib::ext::make_shared<BlackConstantVol>(today, cal, 0.1, dc));
    Handle<CorrelationTermStructure> rhoOne(QuantLib::ext::make_shared<FlatCorrelation>(today, 1.0, dc));

    BlackTriangulationATMVolTermStructure btavs(constantVol1, constantVol1, rhoOne);
    // check it
    for (Time t = 0.1; t < 5.0; t += 0.1) {
        for (Real k = 100; k < 200; k += 10) {
            Volatility v1 = btavs.blackVol(t, k);
            BOOST_CHECK_EQUAL(v1, 0.0);
        }
    }

    // Set up a second vol of 0.0 and a non-zero correlation, we should just get
    // the first vol each time
    Handle<BlackVolTermStructure> constantVol0(QuantLib::ext::make_shared<BlackConstantVol>(today, cal, 0.0, dc));
    Handle<CorrelationTermStructure> rhoFifty(QuantLib::ext::make_shared<FlatCorrelation>(today, 0.5, dc));
    BlackTriangulationATMVolTermStructure btavs2(constantVol1, constantVol0, rhoFifty);
    for (Time t = 0.1; t < 5.0; t += 0.1) {
        Real k = Null<Real>();
        Volatility v1 = constantVol1->blackVol(t, k);
        Volatility v2 = btavs2.blackVol(t, k);
        BOOST_CHECK_EQUAL(v1, v2);
    }

    // Set up a second vol and a correlation of 0
    // Triangulation vol squared should equal sum of squares
    Handle<BlackVolTermStructure> constantVol2(QuantLib::ext::make_shared<BlackConstantVol>(today, cal, 0.2, dc));
    Handle<CorrelationTermStructure> rhoZero(QuantLib::ext::make_shared<FlatCorrelation>(today, 0.0, dc));

    BlackTriangulationATMVolTermStructure btavs3(constantVol1, constantVol2, rhoZero);
    for (Time t = 0.1; t < 5.0; t += 0.1) {
        Real k = Null<Real>();
        Volatility v1 = constantVol1->blackVol(t, k);
        Volatility v2 = constantVol2->blackVol(t, k);
        Volatility v3 = btavs3.blackVol(t, k);
        BOOST_CHECK_EQUAL(v1 * v1 + v2 * v2, v3 * v3);
    }

    // Now test a non-trivial case, assume correlation of 0.8 then
    // vol between 10% and 20% should be 13.4%
    Handle<CorrelationTermStructure> rhoEighty(QuantLib::ext::make_shared<FlatCorrelation>(today, 0.8, dc));
    BlackTriangulationATMVolTermStructure btavs4(constantVol1, constantVol2, rhoEighty);
    for (Time t = 0.1; t < 5.0; t += 0.1) {
        for (Real k = 100; k < 200; k += 10) {
            Volatility vol = btavs4.blackVol(t, k);
            Volatility expectedVol = 0.13416407865; // calc by hand!
            BOOST_CHECK_CLOSE(vol, expectedVol, 1E-8);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
