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

#include <boost/test/unit_test.hpp>
#include <ored/utilities/parsers.hpp>
#include <oret/toplevelfixture.hpp>

using namespace boost::unit_test_framework;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FXDominanceTest)

BOOST_AUTO_TEST_CASE(testFXDominance) {
    BOOST_TEST_MESSAGE("Testing FXDominance...");

    // List of pairs - we split them up and try both ways
    const char* expectedPairs[] = {
        "EURUSD", "GBPUSD", "AUDUSD", "CADJPY", "AUDJPY", "EURNOK", "EURJPY", "EURSEK", "EURCHF", "EURNOK", "NOKJPY",
        "NOKSEK", "DKKSEK", "CNYJPY", "JPYIDR",

        // another longer list - might have duplicates
        "AUDCAD", "AUDCHF", "AUDJPY", "AUDNZD", "AUDUSD", "CADJPY", "CADMXN", "CADNOK", "CHFJPY", "EURAUD", "EURCAD",
        "EURCHF", "EURCZK", "EURDKK", "EURGBP", "EURHUF", "EURJPY", "EURKRW", "EURMXN", "EURNOK", "EURNZD", "EURPLN",
        "EURRUB", "EURSEK", "EURTRY", "EURUSD", "EURZAR", "GBPAUD", "GBPCAD", "GBPCHF", "GBPJPY", "GBPNOK", "GBPUSD",
        "JPYKRW", "MXNJPY", "NOKSEK", "NZDCAD", "NZDCHF", "NZDJPY", "NZDUSD", "TRYJPY", "USDCAD", "USDCHF", "USDCNH",
        "USDCZK", "USDDKK", "USDHKD", "USDHUF", "USDILS", "USDJPY", "USDMXN", "USDNOK", "USDOMR", "USDPLN", "USDRON",
        "USDRUB", "USDSEK", "USDSGD", "USDTHB", "USDTRY", "USDZAR",

        // XXX for unknown ccys
        "XXXJPY", "EURXXX", "USDXXX",

        // Some metals
        "XAUUSD", "XAGUSD", "XPTUSD", "XPDUSD", "XAUEUR", "XAGEUR", "XPTAUD"};

    for (unsigned i = 0; i < sizeof(expectedPairs) / sizeof(expectedPairs[0]); i++) {
        string expectedPair = expectedPairs[i];
        string s1 = expectedPair.substr(0, 3);
        string s2 = expectedPair.substr(3);

        BOOST_TEST_MESSAGE("Combining " << s1 << " and " << s2 << " expecting " << expectedPair);
        string pair = ore::data::fxDominance(s1, s2);
        BOOST_CHECK_EQUAL(pair, expectedPair);

        BOOST_TEST_MESSAGE("Combining " << s2 << " and " << s1 << " expecting " << expectedPair);
        pair = ore::data::fxDominance(s2, s1);
        BOOST_CHECK_EQUAL(pair, expectedPair);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
