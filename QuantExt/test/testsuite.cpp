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

/*! \file testsuite.cpp
    \brief wrapper calling all individual test cases
    \ingroup
*/

#include <iostream>
#include <iomanip>
using namespace std;

// Boost
#include <boost/timer.hpp>
using namespace boost;

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
using boost::unit_test::test_suite;

#ifdef BOOST_MSVC
#include <qle/auto_link.hpp>
#include <ql/auto_link.hpp>
#endif

// Lib test suites
#include "analyticlgmswaptionengine.hpp"
#include "crossassetmodel.hpp"
#include "crossassetmodel2.hpp"
#include "equitycrossassetmodel.hpp"
#include "crossassetmodelparametrizations.hpp"
#include "currency.hpp"
#include "discountcurve.hpp"
#include "dynamicblackvoltermstructure.hpp"
#include "dynamicswaptionvolmatrix.hpp"
#include "index.hpp"
#include "logquote.hpp"
#include "staticallycorrectedyieldtermstructure.hpp"
#include "blackvariancecurve.hpp"
#include "cashflow.hpp"
#include "swaptionvolatilityconverter.hpp"
#include "optionletstripper.hpp"
#include "ratehelpers.hpp"

namespace {

boost::timer t;

void startTimer() {
    BOOST_CHECK(true);
    t.restart();
}
void stopTimer() {
    BOOST_CHECK(true);
    double seconds = t.elapsed();
    int hours = int(seconds / 3600);
    seconds -= hours * 3600;
    int minutes = int(seconds / 60);
    seconds -= minutes * 60;
    std::cout << endl << " QuantExt tests completed in ";
    if (hours > 0)
        cout << hours << " h ";
    if (hours > 0 || minutes > 0)
        cout << minutes << " m ";
    cout << fixed << setprecision(0) << seconds << " s" << endl << endl;
}
}

test_suite* init_unit_test_suite(int, char* []) {

    test_suite* test = BOOST_TEST_SUITE("QuantExtTestSuite");

    test->add(BOOST_TEST_CASE(startTimer));

    test->add(testsuite::CashFlowTest::suite());
    test->add(testsuite::AnalyticLgmSwaptionEngineTest::suite());
    test->add(testsuite::CrossAssetModelTest::suite());
    test->add(testsuite::CrossAssetModelTest2::suite());
    test->add(testsuite::CrossAssetModelParametrizationsTest::suite());
    test->add(testsuite::DiscountCurveTest::suite());
    test->add(testsuite::DynamicBlackVolTermStructureTest::suite());
    test->add(testsuite::DynamicSwaptionVolMatrixTest::suite());
    test->add(testsuite::CurrencyTest::suite());
    test->add(testsuite::IndexTest::suite());
    test->add(testsuite::LogQuoteTest::suite());
    test->add(testsuite::StaticallyCorrectedYieldTermStructureTest::suite());
    test->add(testsuite::BlackVarianceCurveTest::suite());
    test->add(testsuite::SwaptionVolatilityConverterTest::suite());
    test->add(testsuite::OptionletStripperTest::suite());
    test->add(testsuite::RateHelpersTest::suite());
    test->add(testsuite::EquityCrossAssetModelTest::suite());

    test->add(BOOST_TEST_CASE(stopTimer));

    return test;
}
