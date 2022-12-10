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

#include <qle/models/normalsabr.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(NormalFreeBoundarySabrTest)

BOOST_AUTO_TEST_CASE(testNormalFreeBoundarySabr) {

    BOOST_TEST_MESSAGE("Testing normal free boundary SABR...");

    Real forward = 0.0, expiryTime = 5.0, alpha = 0.0050, nu = 0.52, rho = -0.23;

    BOOST_TEST_MESSAGE("vol=" << normalFreeBoundarySabrVolatility(0.0, forward, expiryTime, alpha, nu, rho));

    // WIP...

    // for (Real strike = -0.10; strike < 0.10 + 1E-5; strike += 0.0001) {
    //     BOOST_TEST_MESSAGE(strike << " " << normalSabrVolatility(strike, forward, expiryTime, alpha, nu, rho) << " "
    //                               << normalFreeBoundarySabrVolatility(strike, forward, expiryTime, alpha, nu, rho));
    // }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
