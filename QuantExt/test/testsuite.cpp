/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file testsuite.cpp
    \brief wrapper calling all individual test cases
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
#include "discountcurve.hpp"
#include "dynamicblackvoltermstructure.hpp"
#include "logquote.hpp"
#include "currency.hpp"
#include "index.hpp"
#include "xassetmodel.hpp"
#include "xassetmodel2.hpp"
#include "xassetmodelparametrizations.hpp"

namespace {

    boost::timer t;

    void startTimer() { t.restart(); }
    void stopTimer() {
        double seconds = t.elapsed();
        int hours = int(seconds/3600);
        seconds -= hours * 3600;
        int minutes = int(seconds/60);
        seconds -= minutes * 60;
        std::cout << endl << " QuantExt tests completed in ";
        if (hours > 0) cout << hours << " h ";
        if (hours > 0 || minutes > 0) cout << minutes << " m ";
        cout << fixed << setprecision(0) << seconds << " s" << endl << endl;
    }

}

test_suite* init_unit_test_suite(int, char* []) {

    test_suite* test = BOOST_TEST_SUITE("QuantExtTestSuite");

    test->add(BOOST_TEST_CASE(startTimer));

    test->add(AnalyticLgmSwaptionEngineTest::suite());
    test->add(CurrencyTest::suite());
    test->add(DiscountCurveTest::suite());
    test->add(DynamicBlackVolTermStructureTest::suite());
    test->add(IndexTest::suite());
    test->add(LogQuoteTest::suite());
    test->add(XAssetModelTest::suite());
    test->add(XAssetModelTest2::suite());
    test->add(XAssetModelParametrizationsTest::suite());

    test->add(BOOST_TEST_CASE(stopTimer));

    return test;
}
