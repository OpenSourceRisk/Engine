/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace std;

using ore::test::TopLevelFixture;

namespace {

// Test data from ECB as of 8 Jan 2016
vector<pair<string, Real>> fxtData() {

    // clang-format off
    vector<pair<string, Real>> testData{
        { "EURUSD", 1.0861 },
        { "EURJPY", 128.51 },
        { "EURCZK", 27.022 },
        { "EURDKK", 7.4598 },
        { "EURGBP", 0.74519 },
        { "EURHUF", 315.53 },
        { "EURPLN", 4.3523 },
        { "EURSEK", 9.2640 },
        { "EURCHF", 1.0860 },
        { "EURNOK", 9.6810 },
        { "EURAUD", 1.5495 },
        { "ZZZEUR", 3.141 }, // just to test reverse quotes
        { "AUDNZD", 1.0616327848 } // Should be enough for USDNZD (value = 1.645 / 1.5495)
    };
    // clang-format on

    return testData;
}

// Provide the FXTriangulation object for the tests
class FxTriFixture : public TopLevelFixture {
public:
    FXTriangulation fx;

    FxTriFixture() {
        // Initialise FX data
	std::map<std::string, Handle<Quote>> quotes;
        for (const auto& p : fxtData()) {
            Handle<Quote> q(QuantLib::ext::make_shared<SimpleQuote>(p.second));
            quotes[p.first] = q;
        }
	fx = FXTriangulation(quotes);
    }

    ~FxTriFixture() {}
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(FXTriangulationTests, FxTriFixture)

BOOST_AUTO_TEST_CASE(testDataLoaded) {
    for (const auto& p : fxtData()) {
        BOOST_CHECK_EQUAL(fx.getQuote(p.first)->value(), p.second);
    }
}

BOOST_AUTO_TEST_CASE(testUnity) {
    BOOST_CHECK_EQUAL(fx.getQuote("EUREUR")->value(), 1.0);
    BOOST_CHECK_EQUAL(fx.getQuote("USDUSD")->value(), 1.0);
}

BOOST_AUTO_TEST_CASE(testValues) {

    // Tolerance for comparisons
    Real tol = 1e-12;

    // Check inverse
    BOOST_CHECK_CLOSE(fx.getQuote("USDEUR")->value(), 1.0 / 1.0861, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("JPYEUR")->value(), 1.0 / 128.51, tol);

    // Check Triangulation
    BOOST_CHECK_CLOSE(fx.getQuote("USDJPY")->value(), 128.51 / 1.0861, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("JPYUSD")->value(), 1.0861 / 128.51, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("USDGBP")->value(), 0.74519 / 1.0861, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("GBPUSD")->value(), 1.0861 / 0.74519, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("NOKSEK")->value(), 9.2640 / 9.6810, tol);

    // Check Triangulation where the EUR quote is reversed
    BOOST_CHECK_CLOSE(fx.getQuote("ZZZUSD")->value(), 3.141 * 1.0861, tol);
    BOOST_CHECK_CLOSE(fx.getQuote("USDZZZ")->value(), 1 / (3.141 * 1.0861), tol);
}

BOOST_AUTO_TEST_CASE(testMoreThanOneStep) {

    // Larger tolerance for multiple steps
    Real tol = 1e-8;

    BOOST_CHECK_CLOSE(fx.getQuote("USDNZD")->value(), 1.6450 / 1.0861, tol);
}

BOOST_AUTO_TEST_CASE(testBadInputsThrow) {
    BOOST_CHECK_THROW(fx.getQuote("BadInput"), QuantLib::Error);
    BOOST_CHECK_THROW(fx.getQuote(""), QuantLib::Error);
    BOOST_CHECK_THROW(fx.getQuote("MXNZAR"), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
