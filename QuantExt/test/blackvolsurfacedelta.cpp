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
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackVolSurfaceDeltaTest)

BOOST_AUTO_TEST_CASE(testBlackVolSurfaceDeltaConstantVol) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVolatilitySurfaceDelta...");

    Volatility constVol = 0.10; // 10%

    Date refDate(1, Jan, 2010);
    Settings::instance().evaluationDate() = refDate;

    // Setup a 2x2
    vector<Date> dates = { Date(1, Jan, 2011), Date(1, Jan, 2012) };
    vector<Real> putDeltas = { -0.25 };
    vector<Real> callDeltas = { 0.25 };
    bool hasAtm = false;
    Matrix blackVolMatrix(2, 2, constVol);

    // dummy spot and zero yield curve
    Handle<Quote> spot(boost::make_shared<SimpleQuote>(1.0));
    Handle<YieldTermStructure> dts(boost::make_shared<FlatForward>(0, TARGET(), 0.011, ActualActual()));
    Handle<YieldTermStructure> fts(boost::make_shared<FlatForward>(0, TARGET(), 0.012, ActualActual()));

    // build a vol surface
    BOOST_TEST_MESSAGE("Build Surface");
    BlackVolatilitySurfaceDelta surface(refDate, dates, putDeltas, callDeltas, hasAtm, blackVolMatrix, ActualActual(),
                                        TARGET(), spot, dts, fts);

    // ask for volatility at lots of points, should be constVol at every point
    // make sure we ask for vols outside 25D and 2Y
    for (Time t : { 0.25, 0.5, 1.0, 1.5, 2.0, 2.5, 10.0 }) {
        for (Real k = 0.5; k < 2.0; k += 0.05) {
            Volatility vol = surface.blackVol(t, k);
            BOOST_CHECK_EQUAL(vol, constVol);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
