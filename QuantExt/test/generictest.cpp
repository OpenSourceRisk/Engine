/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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
#include "utilities.hpp"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(GenericTest)

BOOST_AUTO_TEST_CASE(testGeneric) {
    BOOST_TEST_MESSAGE("Generic test case ...");
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
