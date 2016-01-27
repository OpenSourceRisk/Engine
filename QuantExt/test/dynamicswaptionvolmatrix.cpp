/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "dynamicswaptionvolmatrix.hpp"

#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>

#include <qle/termstructures/swaptionvolmatrix.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/quotes/simplequote.hpp>

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
        atmVols[0][0] = 0.0050; atmVols[0][1] = 0.0060; // opt 3m
        atmVols[1][0] = 0.0100; atmVols[1][1] = 0.0160; // opt 2y

        // atm surface
        atmSurface = boost::make_shared<SwaptionVolatilityMatrix>(
            origRefDate, TARGET(), Following, optionTenors, swapTenors, atmVols,
            Actual365Fixed(), false, Normal);
    }

    SavedSettings backup;
    const Date origRefDate;
    boost::shared_ptr<SwaptionVolatilityStructure> atmSurface;
    std::vector<Period> optionTenors, swapTenors;
    Matrix atmVols;
};

} // anonymous namespace

void DynamicSwaptionVolMatrixTest::testConstantVariance() {

    BOOST_TEST_MESSAGE("Testing constant variance dynamics of "
                       "DynamicSwaptionVolatilityMatrix...");

    TestData d;

    boost::shared_ptr<SwaptionVolatilityStructure> dyn =
        boost::make_shared<DynamicSwaptionVolatilityMatrix>(
            d.atmSurface, 0, TARGET(), ConstantVariance);

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    Real strike = 0.05; // dummy strike

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 1 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 2 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 1 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 2 * Years, strike),
                      tol);

    // move forward in time, we expect a constant surface

    Settings::instance().evaluationDate() =
        TARGET().advance(d.origRefDate, 5 * Months);

    BOOST_CHECK_CLOSE(dyn->volatility(0.1, 1.0, strike),
                      d.atmSurface->volatility(0.1, 1.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(0.1, 2.0, strike),
                      d.atmSurface->volatility(0.1, 2.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1.0, 1.0, strike),
                      d.atmSurface->volatility(1.0, 1.0, strike), tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1.0, 2.0, strike),
                      d.atmSurface->volatility(1.0, 2.0, strike), tol);

} // testConstantVariance

void DynamicSwaptionVolMatrixTest::testForwardForwardVariance() {

    BOOST_TEST_MESSAGE("Testing forward forward variance dynamics of "
                       "DynamicSwaptionVolatilityMatrix");
    TestData d;

    boost::shared_ptr<SwaptionVolatilityStructure> dyn =
        boost::make_shared<DynamicSwaptionVolatilityMatrix>(
            d.atmSurface, 0, TARGET(), ForwardForwardVariance);

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    Real strike = 0.05; // dummy strike

    // initially we should get the same volatilities again

    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 1 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Months, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Months, 2 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 1 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 1 * Years, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->volatility(1 * Years, 2 * Years, strike),
                      d.atmSurface->volatility(1 * Years, 2 * Years, strike),
                      tol);

    // move forward in time, we expect the forward forward variance

    Settings::instance().evaluationDate() =
        TARGET().advance(d.origRefDate, 5 * Months);
    Real tf =
        d.atmSurface->timeFromReference(Settings::instance().evaluationDate());

    BOOST_CHECK_CLOSE(dyn->blackVariance(0.1, 1.0, strike),
                      d.atmSurface->blackVariance(tf + 0.1, 1.0, strike) -
                          d.atmSurface->blackVariance(tf, 1.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(0.1, 2.0, strike),
                      d.atmSurface->blackVariance(tf + 0.1, 2.0, strike) -
                          d.atmSurface->blackVariance(tf, 2.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(1.0, 1.0, strike),
                      d.atmSurface->blackVariance(tf + 1.0, 1.0, strike) -
                          d.atmSurface->blackVariance(tf, 1.0, strike),
                      tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(1.0, 2.0, strike),
                      d.atmSurface->blackVariance(tf + 1.0, 2.0, strike) -
                          d.atmSurface->blackVariance(tf, 2.0, strike),
                      tol);

} // testForwardForwardVariance

test_suite *DynamicSwaptionVolMatrixTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("DynamicSwaptionVolMatrix tests");
    suite->add(
        BOOST_TEST_CASE(&DynamicSwaptionVolMatrixTest::testConstantVariance));
    suite->add(BOOST_TEST_CASE(
        &DynamicSwaptionVolMatrixTest::testForwardForwardVariance));
    return suite;
}
