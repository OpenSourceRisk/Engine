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
#include <ored/marketdata/expiry.hpp>
#include <oret/toplevelfixture.hpp>

using namespace QuantLib;
using namespace ore::data;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ExpiryTests)

BOOST_AUTO_TEST_CASE(testExpiryDate) {

    BOOST_TEST_MESSAGE("Testing expiry date...");

    Date inputDate(13, Jan, 2020);

    // Construct an ExpiryDate directly
    ExpiryDate expiry(inputDate);
    BOOST_CHECK_EQUAL(expiry.expiryDate(), inputDate);

    // Write ExpiryDate to string
    string strExpiry = expiry.toString();

    // Parse ExpiryDate from string
    QuantLib::ext::shared_ptr<Expiry> parsedExpiry;
    BOOST_REQUIRE_NO_THROW(parsedExpiry = parseExpiry(strExpiry));

    // Check that we get back an ExpiryDate
    QuantLib::ext::shared_ptr<ExpiryDate> castExpiry = QuantLib::ext::dynamic_pointer_cast<ExpiryDate>(parsedExpiry);
    BOOST_CHECK(castExpiry);

    // Check its members
    BOOST_CHECK_EQUAL(castExpiry->expiryDate(), inputDate);
}

BOOST_AUTO_TEST_CASE(testExpiryPeriod) {

    BOOST_TEST_MESSAGE("Testing expiry period...");

    Period inputPeriod(3, Months);

    // Construct an ExpiryPeriod directly
    ExpiryPeriod expiry(inputPeriod);
    BOOST_CHECK_EQUAL(expiry.expiryPeriod(), inputPeriod);

    // Write ExpiryPeriod to string
    string strExpiry = expiry.toString();

    // Parse ExpiryPeriod from string
    QuantLib::ext::shared_ptr<Expiry> parsedExpiry;
    BOOST_REQUIRE_NO_THROW(parsedExpiry = parseExpiry(strExpiry));

    // Check that we get back an ExpiryDate
    QuantLib::ext::shared_ptr<ExpiryPeriod> castExpiry = QuantLib::ext::dynamic_pointer_cast<ExpiryPeriod>(parsedExpiry);
    BOOST_CHECK(castExpiry);

    // Check its members
    BOOST_CHECK_EQUAL(castExpiry->expiryPeriod(), inputPeriod);
}

BOOST_AUTO_TEST_CASE(testContinuationExpiry) {

    BOOST_TEST_MESSAGE("Testing future continuation expiry...");

    Natural inputIndex = 2;

    // Construct a FutureContinuationExpiry directly
    FutureContinuationExpiry expiry(inputIndex);
    BOOST_CHECK_EQUAL(expiry.expiryIndex(), inputIndex);

    // Write FutureContinuationExpiry to string
    string strExpiry = expiry.toString();

    // Parse FutureContinuationExpiry from string
    QuantLib::ext::shared_ptr<Expiry> parsedExpiry;
    BOOST_REQUIRE_NO_THROW(parsedExpiry = parseExpiry(strExpiry));

    // Check that we get back a FutureContinuationExpiry
    QuantLib::ext::shared_ptr<FutureContinuationExpiry> castExpiry =
        QuantLib::ext::dynamic_pointer_cast<FutureContinuationExpiry>(parsedExpiry);
    BOOST_CHECK(castExpiry);

    // Check its members
    BOOST_CHECK_EQUAL(castExpiry->expiryIndex(), inputIndex);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
