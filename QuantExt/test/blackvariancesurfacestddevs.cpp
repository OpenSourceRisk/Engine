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
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/currencies/europe.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/blackvariancesurfacestddevs.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackVarianceSurfaceStdDevs)

BOOST_AUTO_TEST_CASE(testFlatSurface) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceStdDevs");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    // we setup a flat surface, all at 12%
    // Then we ask it for vol at different tenors and strikes
    Calendar cal = NullCalendar();
    Handle<Quote> spot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100));
    vector<Time> times = { 1.0, 2.0, 3.0, 4.0 };
    vector<Real> stdDevs = { -1.0, -0.5, 0, 0.5, 1.0 };
    Volatility flatVol = 0.12;
    Handle<Quote> flatVolQ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(flatVol));
    vector<vector<Handle<Quote> > > blackVolMatrix(stdDevs.size(), vector<Handle<Quote> >(times.size(), flatVolQ));
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Handle<YieldTermStructure> forTS(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> domTS(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.01)), ActualActual(ActualActual::ISDA)));

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex = 
        QuantLib::ext::make_shared<QuantExt::FxIndex>("dummy", 2, EURCurrency(),
        GBPCurrency(), cal, spot, forTS, domTS);
    QuantExt::BlackVarianceSurfaceStdDevs surface(cal, spot, times, stdDevs, blackVolMatrix, dc, fxIndex);

    // Now get a vol for different times and strikes
    for (Time t = 0.05; t < 5.0; t += 0.1) {
        // spot is 100 so strikes should range from (say) 70 to 150
        for (Real k = 70; k < 150; k += 0.5) {
            Volatility vol = surface.blackVol(t, k);
            // BOOST_TEST_MESSAGE("BlackVarianceSurfaceStdDevs vol for t=" << t << " and k=" << k << " is " << vol);
            BOOST_CHECK_CLOSE(vol, flatVol, 1e-12);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
