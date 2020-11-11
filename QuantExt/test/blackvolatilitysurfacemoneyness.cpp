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
#include <qle/termstructures/blackvolatilitysurfacemoneyness.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackVolatilitySurfaceMoneyness)

BOOST_AUTO_TEST_CASE(testVolatilitySurfaceMoneynessForward) {
    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVolatilitySurfaceMoneynessForward");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    // We setup a simple surface, illustrated below by implied (yearly) volatility inputs:
    // Moneyness\Times: 1.0 2.0
    // 0.9:   0.35   0.30
    // 1.1:   0.40   0.35
    // Then we ask it for vol at different tenors and strikes (moneyness levels)

    Calendar cal = NullCalendar();
    Handle<Quote> spot = Handle<Quote>(boost::make_shared<SimpleQuote>(100));
    vector<Time> expiryTimes = { 1.0, 2.0 };
    vector<Real> moneynessLevels = { 0.9, 1.1 };
    vector<vector<Handle<Quote> > > blackVolMatrix(moneynessLevels.size(), vector<Handle<Quote> >(expiryTimes.size()));
    
    blackVolMatrix[0][0] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.35));
    blackVolMatrix[0][1] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.30));
    blackVolMatrix[1][0] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.40));
    blackVolMatrix[1][1] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.35));

    DayCounter dc = ActualActual();
    Handle<YieldTermStructure> forTS(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.02)), ActualActual()));
    Handle<YieldTermStructure> domTS(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.01)), ActualActual()));

    // Create the volatility surface with a spot moneyness dimension
    QuantExt::BlackVolatilitySurfaceMoneynessForward surface(cal, spot, expiryTimes, moneynessLevels, blackVolMatrix,
                                                             dc, forTS, domTS, false, true);

    // Check original pillars for correctness
    for (Size i = 0; i < moneynessLevels.size(); ++i) {
        for (Size j = 0; j < expiryTimes.size(); ++j) {
            Time T = expiryTimes[j];
            Real strike = moneynessLevels[i] * spot->value() * forTS->discount(T) / domTS->discount(T);
            Volatility vol = surface.blackVol(T, strike);

            BOOST_CHECK_CLOSE(vol, blackVolMatrix[i][j]->value(), 1e-12);
        }
    }

    // Finally check the middle point of the surface, i.e. T = 1.5 and Mny = 1.0
    Time T = 1.5;
    Real strike = 1.0 * spot->value() * forTS->discount(T) / domTS->discount(T);
    Volatility vol = surface.blackVol(T, strike);
    BOOST_CHECK_CLOSE(vol, 0.35, 1e-12);

    // ... and check the same middle point although by its variance value
    vol = surface.blackVariance(T, strike);
    BOOST_CHECK_CLOSE(vol, 0.35 * 0.35 * T, 1e-12);
}

BOOST_AUTO_TEST_CASE(testVolatilitySurfaceMoneynessSpot) {
    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVolatilitySurfaceMoneynessSpot");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    // We setup a simple surface, illustrated below by implied (yearly) volatility inputs:
    // Moneyness\Times: 1.0 2.0
    // 0.9:   0.35   0.30
    // 1.1:   0.40   0.35
    // Then we ask it for vol at different tenors and strikes (moneyness levels)

    Calendar cal = NullCalendar();
    Handle<Quote> spot = Handle<Quote>(boost::make_shared<SimpleQuote>(100));
    vector<Time> expiryTimes = { 1.0, 2.0 };
    vector<Real> moneynessLevels = { 0.9, 1.1 };
    vector<vector<Handle<Quote> > > blackVolMatrix(moneynessLevels.size(), vector<Handle<Quote> >(expiryTimes.size()));

    blackVolMatrix[0][0] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.35));
    blackVolMatrix[0][1] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.30));
    blackVolMatrix[1][0] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.40));
    blackVolMatrix[1][1] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.35));

    DayCounter dc = ActualActual();

    // Create the volatility surface with a spot moneyness dimension
    QuantExt::BlackVolatilitySurfaceMoneynessSpot surface(cal, spot, expiryTimes, moneynessLevels, blackVolMatrix, dc,
                                                          false, true);

    // Check original pillars for correctness
    for (Size i = 0; i < moneynessLevels.size(); ++i) {
        for (Size j = 0; j < expiryTimes.size(); ++j) {
            Time T = expiryTimes[j];
            Real strike = moneynessLevels[i] * spot->value();
            Volatility vol = surface.blackVol(T, strike);

            BOOST_CHECK_CLOSE(vol, blackVolMatrix[i][j]->value(), 1e-12);
        }
    }

    // Finally check the middle point of the surface, i.e. T = 1.5 and Mny = 1.0
    Time T = 1.5;
    Real strike = 1.0 * spot->value();
    Volatility vol = surface.blackVol(T, strike);
    BOOST_CHECK_CLOSE(vol, 0.35, 1e-12);

    // ... and check the same middle point although by its variance value
    vol = surface.blackVariance(T, strike);
    BOOST_CHECK_CLOSE(vol, 0.35 * 0.35 * T, 1e-12);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
