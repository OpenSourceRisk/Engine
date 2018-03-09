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

#include "dynamicswaptionvolmatrix.hpp"
#include "utilities.hpp"

#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/calendars/target.hpp>

#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {

struct TestData {

    TestData() : origRefDate(20, Jan, 2016), atmVols(2, 2) {

        Settings::instance().evaluationDate() = origRefDate;

        optionTenors.push_back(3 * Months);
        optionTenors.push_back(5 * Years);
        swapTenors.push_back(1 * Years);
        swapTenors.push_back(2 * Years);
        //    sw 1Y                   sw 2Y
        atmVols[0][0] = 0.0050;
        atmVols[0][1] = 0.0060; // opt 3m
        atmVols[1][0] = 0.0100;
        atmVols[1][1] = 0.0160; // opt 2y

        // atm surface
        atmSurface = boost::make_shared<SwaptionVolatilityMatrix>(origRefDate, TARGET(), Following, optionTenors,
                                                                  swapTenors, atmVols, Actual365Fixed(), false, Normal);
    }

    SavedSettings backup;
    const Date origRefDate;
    boost::shared_ptr<SwaptionVolatilityStructure> atmSurface;
    std::vector<Period> optionTenors, swapTenors;
    Matrix atmVols;
};

} // anonymous namespace

namespace testsuite {

void DynamicSwaptionVolMatrixTest::testConstantVariance() {

    BOOST_TEST_MESSAGE("Testing constant variance dynamics of "
                       "DynamicSwaptionVolatilityMatrix...");

    TestData d;

    boost::shared_ptr<SwaptionVolatilityStructure> dyn =
        boost::make_shared<DynamicSwaptionVolatilityMatrix>(d.atmSurface, 0, TARGET(), ConstantVariance);

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    Real strike = 0.05; // dummy strike

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 1 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 2 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 1 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 2 * Years, strike), tol);

    // move forward in time, we expect a constant surface

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 5 * Months);

    BOOST_CHECK_CLOSE(dyn->volatility(0.1, 1.0, strike), d.atmSurface->volatility(0.1, 1.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(0.1, 2.0, strike), d.atmSurface->volatility(0.1, 2.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1.0, 1.0, strike), d.atmSurface->volatility(1.0, 1.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1.0, 2.0, strike), d.atmSurface->volatility(1.0, 2.0, strike), tol);

} // testConstantVariance

void DynamicSwaptionVolMatrixTest::testForwardForwardVariance() {

    BOOST_TEST_MESSAGE("Testing forward forward variance dynamics of "
                       "DynamicSwaptionVolatilityMatrix");
    TestData d;

    boost::shared_ptr<SwaptionVolatilityStructure> dyn =
        boost::make_shared<DynamicSwaptionVolatilityMatrix>(d.atmSurface, 0, TARGET(), ForwardForwardVariance);

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    Real strike = 0.05; // dummy strike

    // initially we should get the same volatilities again

    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 1 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 2 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 1 * Years, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 2 * Years, strike), tol);

    // move forward in time, we expect the forward forward variance

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 5 * Months);
    Real tf = d.atmSurface->timeFromReference(Settings::instance().evaluationDate());

    BOOST_CHECK_CLOSE(dyn->blackVariance(0.1, 1.0, strike),
                      d.atmSurface->blackVariance(tf + 0.1, 1.0, strike) - d.atmSurface->blackVariance(tf, 1.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(0.1, 2.0, strike),
                      d.atmSurface->blackVariance(tf + 0.1, 2.0, strike) - d.atmSurface->blackVariance(tf, 2.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(1.0, 1.0, strike),
                      d.atmSurface->blackVariance(tf + 1.0, 1.0, strike) - d.atmSurface->blackVariance(tf, 1.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(1.0, 2.0, strike),
                      d.atmSurface->blackVariance(tf + 1.0, 2.0, strike) - d.atmSurface->blackVariance(tf, 2.0, strike),
                      tol);

} // testForwardForwardVariance

test_suite* DynamicSwaptionVolMatrixTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("DynamicSwaptionVolMatrix tests");
    suite->add(BOOST_TEST_CASE(&DynamicSwaptionVolMatrixTest::testConstantVariance));
    suite->add(BOOST_TEST_CASE(&DynamicSwaptionVolMatrixTest::testForwardForwardVariance));
    return suite;
}
} // namespace testsuite
