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

#include <boost/test/unit_test.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionlet.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>
#include <test/toplevelfixture.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

using qle::test::TopLevelFixture;
using std::vector;

namespace {

// Fixture to create an optionlet surface that is used in the tests
class F : public TopLevelFixture {
public:
    Date asof;
    vector<Date> expiries;
    vector<Rate> strikes;
    DayCounter dc;
    vector<vector<Handle<Quote> > > vols;
    QuantLib::ext::shared_ptr<StrippedOptionlet> optionletSurface;

    F() : expiries(2), strikes(2), dc(Actual365Fixed()), vols(2) {

        // Set the evaluation date
        asof = Date(17, Apr, 2019);
        Settings::instance().evaluationDate() = asof;

        // Inputs for the optionlet surface
        Natural settlementDays = 2;
        UnitedStates calendar(UnitedStates::Settlement);
        BusinessDayConvention bdc = Following;
        QuantLib::ext::shared_ptr<IborIndex> dummyIborIndex;
        VolatilityType type = Normal;

        expiries[0] = Date(17, Apr, 2020);
        expiries[1] = Date(19, Apr, 2021);

        strikes[0] = 0.02;
        strikes[1] = 0.04;

        // clang-format off
        vols[0].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0091)));
        vols[0].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0092)));
        vols[1].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0070)));
        vols[1].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0088)));
        // clang-format on

        // Create the optionlet surface
        optionletSurface = QuantLib::ext::make_shared<StrippedOptionlet>(settlementDays, calendar, bdc, dummyIborIndex,
                                                                 expiries, strikes, vols, dc, type);
    }

    ~F() {}
};

} // namespace
BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(StrippedOptionletAdapterTwoTests)

BOOST_FIXTURE_TEST_CASE(testFlatExtrapAfterLastExpiry, F) {

    // Set up optionlet adapter with flat extrapolation
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter2> adapter =
        QuantLib::ext::make_shared<StrippedOptionletAdapter2>(optionletSurface, true);

    // Pick a date 1Y after the max date
    Date testDate = expiries.back() + 1 * Years;

    // Check flat extrapolation on the pillar strikes
    for (Size i = 0; i < strikes.size(); i++) {
        Volatility testVol = adapter->volatility(testDate, strikes[i], true);
        BOOST_CHECK_CLOSE(testVol, vols.back()[i]->value(), 1e-12);
    }

    // Check flat extrapolation below first strike
    Volatility testVol = adapter->volatility(testDate, strikes.front() / 2.0, true);
    BOOST_CHECK_CLOSE(testVol, vols.back().front()->value(), 1e-12);

    // Check flat extrapolation above last strike
    testVol = adapter->volatility(testDate, 2.0 * strikes.back(), true);
    BOOST_CHECK_CLOSE(testVol, vols.back().back()->value(), 1e-12);

    // Check flat extrapolation between two strikes
    Rate avgStrike = (strikes[0] + strikes[1]) / 2.0;
    Volatility expectedVol = (vols.back()[0]->value() + vols.back()[1]->value()) / 2.0;
    testVol = adapter->volatility(testDate, avgStrike, true);
    BOOST_CHECK_CLOSE(testVol, expectedVol, 1e-12);
}

BOOST_FIXTURE_TEST_CASE(testFlatExtrapBetweenFirstLastExpiry, F) {

    // Set up optionlet adapter with flat extrapolation
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter2> adapter =
        QuantLib::ext::make_shared<StrippedOptionletAdapter2>(optionletSurface, true);

    // Check flat extrapolation on expiry pillars
    for (Size i = 0; i < expiries.size(); i++) {
        // Below first strike
        Volatility testVol = adapter->volatility(expiries[i], strikes.front() / 2.0, true);
        BOOST_CHECK_CLOSE(testVol, vols[i].front()->value(), 1e-12);
        // Above last strike
        testVol = adapter->volatility(expiries[i], 2.0 * strikes.back(), true);
        BOOST_CHECK_CLOSE(testVol, vols[i].back()->value(), 1e-12);
    }

    // Pick a date between first expiry and last expiry
    Size numDays = dc.dayCount(expiries.front(), expiries.back()) / 2;
    Date testDate = expiries.front() + numDays * Days;

    // Check flat extrapolation below first strike
    Volatility testVol = adapter->volatility(testDate, strikes.front() / 2.0, true);
    Volatility expectedVol = adapter->volatility(testDate, strikes.front(), true);
    BOOST_CHECK_CLOSE(testVol, expectedVol, 1e-12);

    // Check flat extrapolation above last strike
    testVol = adapter->volatility(testDate, 2.0 * strikes.back(), true);
    expectedVol = adapter->volatility(testDate, strikes.back(), true);
    BOOST_CHECK_CLOSE(testVol, expectedVol, 1e-12);
}

BOOST_FIXTURE_TEST_CASE(testFlatExtrapBeforeFirstExpiry, F) {

    // Set up optionlet adapter with flat extrapolation
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter2> adapter =
        QuantLib::ext::make_shared<StrippedOptionletAdapter2>(optionletSurface, true);

    // Pick a date between evaluation date and first expiry
    Size numDays = dc.dayCount(asof, expiries.front()) / 2;
    Date testDate = asof + numDays * Days;

    // Check flat extrapolation on the pillar strikes
    for (Size i = 0; i < strikes.size(); i++) {
        Volatility testVol = adapter->volatility(testDate, strikes[i], true);
        BOOST_CHECK_CLOSE(testVol, vols.front()[i]->value(), 1e-12);
    }

    // Check flat extrapolation below first strike
    Volatility testVol = adapter->volatility(testDate, strikes.front() / 2.0, true);
    BOOST_CHECK_CLOSE(testVol, vols.front().front()->value(), 1e-12);

    // Check flat extrapolation above last strike
    testVol = adapter->volatility(testDate, 2.0 * strikes.back(), true);
    BOOST_CHECK_CLOSE(testVol, vols.front().back()->value(), 1e-12);

    // Check flat extrapolation between two strikes
    Rate avgStrike = (strikes[0] + strikes[1]) / 2.0;
    Volatility expectedVol = (vols.front()[0]->value() + vols.front()[1]->value()) / 2.0;
    testVol = adapter->volatility(testDate, avgStrike, true);
    BOOST_CHECK_CLOSE(testVol, expectedVol, 1e-12);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
