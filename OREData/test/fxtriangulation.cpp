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

#include "fxtriangulation.hpp"
#include <boost/make_shared.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace ore::data;
using namespace boost::unit_test_framework;
using namespace std;
using namespace QuantLib;

namespace testsuite {

void FXTriangulationTest::testFXTriangulation() {

    // ECB asof 8 Jan 2016
    vector<pair<string, Real>> testData = {
        {"EURUSD", 1.0861},      {"EURJPY", 128.51}, {"EURCZK", 27.022}, {"EURDKK", 7.4598}, {"EURGBP", 0.74519},
        {"EURHUF", 315.53},      {"EURPLN", 4.3523}, {"EURSEK", 9.2640}, {"EURCHF", 1.0860}, {"EURNOK", 9.6810},
        {"EURAUD", 1.5495},      {"ZZZEUR", 3.141}, // just to test reverse quotes
        {"AUDNZD", 1.0616327848}                    // Should be enough for USDNZD (value = 1.645 / 1.5495)
    };

    // Initialise FX data
    FXTriangulation fx;
    for (const auto& it : testData) {
        Handle<Quote> q(boost::make_shared<SimpleQuote>(it.second));
        fx.addQuote(it.first, q);
    }

    // First check that we got everything back
    for (const auto& it : testData) {
        BOOST_CHECK_EQUAL(fx.getQuote(it.first)->value(), it.second);
    }

    // Check EUREUR
    BOOST_CHECK_EQUAL(fx.getQuote("EUREUR")->value(), 1.0);
    BOOST_CHECK_EQUAL(fx.getQuote("USDUSD")->value(), 1.0);

    // Check inverse
    //    BOOST_CHECK_CLOSE(fx.getQuote("USDEUR")->value(), 1.0 / 1.0861, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("JPYEUR")->value(), 1.0 / 128.51, 1e-12);

    // Check Triangulation
    BOOST_CHECK_CLOSE(fx.getQuote("USDJPY")->value(), 128.51 / 1.0861, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("JPYUSD")->value(), 1.0861 / 128.51, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("USDGBP")->value(), 0.74519 / 1.0861, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("GBPUSD")->value(), 1.0861 / 0.74519, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("NOKSEK")->value(), 9.2640 / 9.6810, 1e-12);

    // Check Triangulation where the EUR quote is reversed
    BOOST_CHECK_CLOSE(fx.getQuote("ZZZUSD")->value(), 3.141 * 1.0861, 1e-12);
    BOOST_CHECK_CLOSE(fx.getQuote("USDZZZ")->value(), 1 / (3.141 * 1.0861), 1e-12);

    // Check that we don't handle more than one step
    // EURUSD + EURAUD + AUDNZD => USDNZD
    BOOST_CHECK_THROW(fx.getQuote("USDNZD"), std::exception);
    // but if we cache EURNZD first....
    BOOST_CHECK_CLOSE(fx.getQuote("EURNZD")->value(), 1.6450, 1e-8); // larger tolerance
    // then we should be able to get it with one step
    BOOST_CHECK_CLOSE(fx.getQuote("USDNZD")->value(), 1.6450 / 1.0861, 1e-8); // larger tolerance

    // Check that we don't return false data on bad inputs
    BOOST_CHECK_THROW(fx.getQuote("BadInput"), std::exception);
    BOOST_CHECK_THROW(fx.getQuote(""), std::exception);
    BOOST_CHECK_THROW(fx.getQuote("MXNZAR"), std::exception);
}

test_suite* FXTriangulationTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("FXTriangulation tests");

    suite->add(BOOST_TEST_CASE(&FXTriangulationTest::testFXTriangulation));

    return suite;
}
}
