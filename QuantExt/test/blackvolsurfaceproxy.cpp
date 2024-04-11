/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
#include <ql/currencies/america.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/termstructures/blackvolsurfaceproxy.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackVolSurfaceProxy)

BOOST_AUTO_TEST_CASE(testBlackVolSurfaceProxy) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVolSurfaceProxy...");

    // take an index and one of it's underlyings, proxy the underlyings vol surface off the
    // index vol surface, check that the forward ATM vols are the same

    Date today = Date(1, Jan, 2020);
    DayCounter dc = ActualActual(ActualActual::ISDA);

    Settings::instance().evaluationDate() = today;

    vector<Date> dates;
    dates.push_back(Date(3, Feb, 2020));
    dates.push_back(Date(2, Mar, 2020));
    dates.push_back(Date(1, Apr, 2020));
    dates.push_back(Date(4, Jan, 2021));

    vector<Real> strikes;
    strikes.push_back(500);
    strikes.push_back(1000);
    strikes.push_back(1500);

    Matrix vols = Matrix(3, 4);
    vols[0][0] = 0.12;
    vols[1][0] = 0.10;
    vols[2][0] = 0.13;
    vols[0][1] = 0.22;
    vols[1][1] = 0.20;
    vols[2][1] = 0.23;
    vols[0][2] = 0.32;
    vols[1][2] = 0.30;
    vols[2][2] = 0.33;
    vols[0][3] = 0.42;
    vols[1][3] = 0.40;
    vols[2][3] = 0.43;

    // spots for the index and underlying
    Handle<Quote> indexSpot = Handle<Quote>(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(1000)));
    Handle<Quote> underlyingSpot = Handle<Quote>(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(150)));

    // forecast and dividend yields for the index
    Handle<YieldTermStructure> indexForecast = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), dc));
    Handle<YieldTermStructure> indexDividend = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), dc));

    // forecast and dividend yields for the underlying
    Handle<YieldTermStructure> underlyingForecast = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), dc));
    Handle<YieldTermStructure> underlyingDividend = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.01)), dc));

    // set up EquityIndexes for the index and underlying
    QuantLib::ext::shared_ptr<EquityIndex2> index = QuantLib::ext::make_shared<EquityIndex2>("Index", UnitedStates(UnitedStates::Settlement), USDCurrency(),
                                                                           indexSpot, indexForecast, indexDividend);
    QuantLib::ext::shared_ptr<EquityIndex2> underlying = QuantLib::ext::make_shared<EquityIndex2>(
        "Underlying", UnitedStates(UnitedStates::Settlement), USDCurrency(), underlyingSpot, underlyingForecast, underlyingDividend);

    // set up a vol surface for the index
    QuantLib::ext::shared_ptr<BlackVolTermStructure> indexVolSurface =
        QuantLib::ext::make_shared<BlackVarianceSurface>(today, UnitedStates(UnitedStates::Settlement), dates, strikes, vols, dc);

    // set up a vol surface for the underlying, to be proxied from the index surface
    QuantLib::ext::shared_ptr<BlackVolatilitySurfaceProxy> underlyingVolSurface =
        QuantLib::ext::make_shared<BlackVolatilitySurfaceProxy>(indexVolSurface, underlying, index);

    // Check the ATM forward vols
    for (auto d : dates) {
        // underlying forward
        Real underlyingF = underlying->fixing(d);
        // vol from proxy surface
        Real underlyingVol = underlyingVolSurface->blackVol(d, underlyingF);

        // index forward
        Real indexF = index->fixing(d);
        // vol from index surface
        Real indexVol = indexVolSurface->blackVol(d, indexF);

        BOOST_CHECK_CLOSE(underlyingVol, indexVol, 0.001);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
