/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/marketdata/strike.hpp>
#include <oret/toplevelfixture.hpp>

using namespace QuantLib;
using namespace ore::data;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(StrikeTests)

BOOST_AUTO_TEST_CASE(testAbsoluteStrike) {

    BOOST_TEST_MESSAGE("Testing absolute strike...");

    Real inputStrike = 2.0;
    Real tolerance = 1e-12;

    // Construct an AbsoluteStrike directly
    AbsoluteStrike strike(inputStrike);
    BOOST_CHECK_CLOSE(strike.strike(), inputStrike, tolerance);

    // Write AbsoluteStrike to string
    string strStrike = strike.toString();

    // Parse AbsoluteStrike from string
    QuantLib::ext::shared_ptr<BaseStrike> parsedStrike;
    BOOST_REQUIRE_NO_THROW(parsedStrike = parseBaseStrike(strStrike));

    // Check that we get back an AbsoluteStrike
    QuantLib::ext::shared_ptr<AbsoluteStrike> castStrike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(parsedStrike);
    BOOST_CHECK(castStrike);

    // Check its members
    BOOST_CHECK_CLOSE(castStrike->strike(), inputStrike, tolerance);
}

BOOST_AUTO_TEST_CASE(testDeltaStrike) {

    BOOST_TEST_MESSAGE("Testing delta strike...");

    DeltaVolQuote::DeltaType inputDeltaType = DeltaVolQuote::Spot;
    Option::Type inputOptionType = Option::Call;
    Real inputDelta = 0.25;
    Real tolerance = 1e-12;

    // Construct a DeltaStrike directly
    DeltaStrike strike(inputDeltaType, inputOptionType, inputDelta);
    BOOST_CHECK_EQUAL(strike.deltaType(), inputDeltaType);
    BOOST_CHECK_EQUAL(strike.optionType(), inputOptionType);
    BOOST_CHECK_CLOSE(strike.delta(), inputDelta, tolerance);

    // Write DeltaStrike to string
    string strStrike = strike.toString();

    // Parse DeltaStrike from string
    QuantLib::ext::shared_ptr<BaseStrike> parsedStrike;
    BOOST_REQUIRE_NO_THROW(parsedStrike = parseBaseStrike(strStrike));

    // Check that we get back a DeltaStrike
    QuantLib::ext::shared_ptr<DeltaStrike> castStrike = QuantLib::ext::dynamic_pointer_cast<DeltaStrike>(parsedStrike);
    BOOST_CHECK(castStrike);

    // Check its members
    BOOST_CHECK_EQUAL(castStrike->deltaType(), inputDeltaType);
    BOOST_CHECK_EQUAL(castStrike->optionType(), inputOptionType);
    BOOST_CHECK_CLOSE(castStrike->delta(), inputDelta, tolerance);
}

BOOST_AUTO_TEST_CASE(testAtmStrikeNoDelta) {

    BOOST_TEST_MESSAGE("Testing ATM strike without delta...");

    DeltaVolQuote::AtmType inputAtmType = DeltaVolQuote::AtmFwd;

    // Construct an AtmStrike directly
    AtmStrike strike(inputAtmType);
    BOOST_CHECK_EQUAL(strike.atmType(), inputAtmType);
    BOOST_CHECK(!strike.deltaType());

    // Write AtmStrike to string
    string strStrike = strike.toString();

    // Parse AtmStrike from string
    QuantLib::ext::shared_ptr<BaseStrike> parsedStrike;
    BOOST_REQUIRE_NO_THROW(parsedStrike = parseBaseStrike(strStrike));

    // Check that we get back an AtmStrike
    QuantLib::ext::shared_ptr<AtmStrike> castStrike = QuantLib::ext::dynamic_pointer_cast<AtmStrike>(parsedStrike);
    BOOST_CHECK(castStrike);

    // Check its members
    BOOST_CHECK_EQUAL(castStrike->atmType(), inputAtmType);
    BOOST_CHECK(!castStrike->deltaType());
}

BOOST_AUTO_TEST_CASE(testAtmStrikeNoDeltaEquality) {

     BOOST_TEST_MESSAGE("Testing equality operator for two ATM strikes without delta...");
     // Checks for failure in operator== if delta type is not given

     DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmFwd;
     boost::optional<DeltaVolQuote::DeltaType> atmDeltaType;

     vector<QuantLib::ext::shared_ptr<BaseStrike>> strikes;
     strikes.push_back(QuantLib::ext::make_shared<AtmStrike>(atmType, atmDeltaType));
     strikes.push_back(QuantLib::ext::make_shared<AtmStrike>(DeltaVolQuote::AtmFwd));
     BOOST_CHECK(*strikes[0] == *strikes[1]);
}

BOOST_AUTO_TEST_CASE(testAtmStrikeWithDelta) {

    BOOST_TEST_MESSAGE("Testing ATM strike with delta...");

    DeltaVolQuote::AtmType inputAtmType = DeltaVolQuote::AtmDeltaNeutral;
    DeltaVolQuote::DeltaType inputDeltaType = DeltaVolQuote::Fwd;

    // Construct an AtmStrike directly
    AtmStrike strike(inputAtmType, inputDeltaType);
    BOOST_CHECK_EQUAL(strike.atmType(), inputAtmType);
    BOOST_CHECK(strike.deltaType());
    BOOST_CHECK_EQUAL(*strike.deltaType(), inputDeltaType);

    // Write AtmStrike to string
    string strStrike = strike.toString();

    // Parse AtmStrike from string
    QuantLib::ext::shared_ptr<BaseStrike> parsedStrike;
    BOOST_REQUIRE_NO_THROW(parsedStrike = parseBaseStrike(strStrike));

    // Check that we get back an AtmStrike
    QuantLib::ext::shared_ptr<AtmStrike> castStrike = QuantLib::ext::dynamic_pointer_cast<AtmStrike>(parsedStrike);
    BOOST_CHECK(castStrike);

    // Check its members
    BOOST_CHECK_EQUAL(castStrike->atmType(), inputAtmType);
    BOOST_CHECK(castStrike->deltaType());
    BOOST_CHECK_EQUAL(*castStrike->deltaType(), inputDeltaType);
}

BOOST_AUTO_TEST_CASE(testMoneynessStrike) {

    BOOST_TEST_MESSAGE("Testing moneyness strike ...");

    MoneynessStrike::Type inputMoneynessType = MoneynessStrike::Type::Forward;
    Real inputMoneyness = 1.10;
    Real tolerance = 1e-12;

    // Construct an MoneynessStrike directly
    MoneynessStrike strike(inputMoneynessType, inputMoneyness);
    BOOST_CHECK_EQUAL(strike.type(), inputMoneynessType);
    BOOST_CHECK_CLOSE(strike.moneyness(), inputMoneyness, tolerance);

    // Write MoneynessStrike to string
    string strStrike = strike.toString();

    // Parse MoneynessStrike from string
    QuantLib::ext::shared_ptr<BaseStrike> parsedStrike;
    BOOST_REQUIRE_NO_THROW(parsedStrike = parseBaseStrike(strStrike));

    // Check that we get back an MoneynessStrike
    QuantLib::ext::shared_ptr<MoneynessStrike> castStrike = QuantLib::ext::dynamic_pointer_cast<MoneynessStrike>(parsedStrike);
    BOOST_CHECK(castStrike);

    // Check its members
    BOOST_CHECK_EQUAL(castStrike->type(), inputMoneynessType);
    BOOST_CHECK_CLOSE(castStrike->moneyness(), inputMoneyness, tolerance);
}

BOOST_AUTO_TEST_CASE(testAtmStrikeExceptions) {

    DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmNull;
    BOOST_CHECK_THROW(AtmStrike tmp(atmType), Error);

    atmType = DeltaVolQuote::AtmDeltaNeutral;
    BOOST_CHECK_THROW(AtmStrike tmp(atmType), Error);

    atmType = DeltaVolQuote::AtmSpot;
    DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::Spot;
    BOOST_CHECK_THROW(AtmStrike tmp(atmType, deltaType), Error);

    atmType = DeltaVolQuote::AtmPutCall50;
    BOOST_CHECK_THROW(AtmStrike tmp(atmType, deltaType), Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
