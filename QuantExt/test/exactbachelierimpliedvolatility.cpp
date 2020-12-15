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

#include <qle/models/exactbachelierimpliedvolatility.hpp>

#include <ql/pricingengines/blackformula.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ExactBachelierImpliedVolatilityTest)

BOOST_AUTO_TEST_CASE(testExactBachelierImpliedVolatility) {

    BOOST_TEST_MESSAGE("Testing exact Bachelier implied volatility...");

    Real tolerance = 1E-4; // percent, i.e. we test for 1E-6 relative error
    Real forward = 0.05;   // fix the forward, only the difference forward - strike matters

    for (Real strikeSpread = -0.10; strikeSpread < 0.10 + 1E-5; strikeSpread += 0.001) {
        Real strike = forward + strikeSpread;
        for (Real vol = 0.0; vol < 0.02 + 1E-5; vol += 0.001) {
            for (Real tte = 0.001; tte < 51.0; tte += 0.1) {
                Real stdDev = std::sqrt(tte) * vol;
                Real call = bachelierBlackFormula(Option::Call, strike, forward, stdDev);
                Real put = bachelierBlackFormula(Option::Put, strike, forward, stdDev);
                if (std::abs(call) < 1E-12 || std::abs(put) < 1E-12)
                    continue;
                Real impliedVolCall = exactBachelierImpliedVolatility(Option::Call, strike, forward, tte, call);
                Real impliedVolPut = exactBachelierImpliedVolatility(Option::Put, strike, forward, tte, put);
                BOOST_CHECK_CLOSE(vol, impliedVolCall, tolerance);
                BOOST_CHECK_CLOSE(vol, impliedVolPut, tolerance);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
