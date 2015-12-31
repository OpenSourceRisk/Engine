/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "analyticlgmswaptionengine.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

#include <iostream> // only for debug, delete later on!

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

void AnalyticLgmSwaptionEngineTest::testMonoCurve() {

    BOOST_TEST_MESSAGE("Testing analytic LGM swaption engine coupon "
                       "adjustments in mono curve setup...");

    Handle<YieldTermStructure> flatCurve(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.02, Actual365Fixed()));

    const IrLgm1fConstantParametrization irlgm1f(EURCurrency(), flatCurve, 0.01,
                                                 0.01);

    // no curve attached
    boost::shared_ptr<SwapIndex> index_nocurves =
        boost::make_shared<EuriborSwapIsdaFixA>(10 * Years);

    // forward curve attached
    boost::shared_ptr<SwapIndex> index_monocurve =
        boost::make_shared<EuriborSwapIsdaFixA>(10 * Years, flatCurve);

    Swaption swaption_nocurves = MakeSwaption(index_nocurves, 10 * Years, 0.02);
    Swaption swaption_monocurve =
        MakeSwaption(index_monocurve, 10 * Years, 0.02);

    boost::shared_ptr<PricingEngine> engine_nodisc =
        boost::make_shared<AnalyticLgmSwaptionEngine>(&irlgm1f);
    boost::shared_ptr<PricingEngine> engine_monocurve =
        boost::make_shared<AnalyticLgmSwaptionEngine>(&irlgm1f, flatCurve);

    swaption_nocurves.setPricingEngine(engine_nodisc);
    swaption_nocurves.NPV();
    std::vector<Real> fixedAmountCorrections1 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    swaption_nocurves.setPricingEngine(engine_monocurve);
    swaption_nocurves.NPV();
    std::vector<Real> fixedAmountCorrections2 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    swaption_monocurve.setPricingEngine(engine_nodisc);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections3 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    swaption_monocurve.setPricingEngine(engine_monocurve);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections4 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");

    if (fixedAmountCorrections1.size() != 10) {
        BOOST_TEST_ERROR(
            "fixed coupon adjustment vector 1 should have size 10, "
            "but actually has size "
            << fixedAmountCorrections1.size());
    }
    if (fixedAmountCorrections2.size() != 10) {
        BOOST_TEST_ERROR(
            "fixed coupon adjustment vector 2 should have size 10, "
            "but actually has size "
            << fixedAmountCorrections2.size());
    }
    if (fixedAmountCorrections3.size() != 10) {
        BOOST_TEST_ERROR(
            "fixed coupon adjustment vector 3 should have size 10, "
            "but actually has size "
            << fixedAmountCorrections3.size());
    }
    if (fixedAmountCorrections4.size() != 10) {
        BOOST_TEST_ERROR(
            "fixed coupon adjustment vector 4 should have size 10, "
            "but actually has size "
            << fixedAmountCorrections4.size());
    }

    for (Size i = 0; i < 10; ++i) {
        if (!close_enough(fixedAmountCorrections1[i], 0.0)) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (1) should be zero in mono "
                "curve setup, but component "
                << i << " is " << fixedAmountCorrections1[i]);
        }
        if (!close_enough(fixedAmountCorrections2[i], 0.0)) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (2) should be zero in mono "
                "curve setup, but component "
                << i << " is " << fixedAmountCorrections2[i]);
        }
        if (!close_enough(fixedAmountCorrections3[i], 0.0)) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (3) should be zero in mono "
                "curve setup, but component "
                << i << " is " << fixedAmountCorrections3[i]);
        }
        if (!close_enough(fixedAmountCorrections4[i], 0.0)) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (4) should be zero in mono "
                "curve setup, but component "
                << i << " is " << fixedAmountCorrections4[i]);
        }
    }
}

void AnalyticLgmSwaptionEngineTest::testDualCurve() {

    BOOST_TEST_MESSAGE("Testing analytic LGM swaption engine coupon "
                       "adjustments in dual curve setup...");

    // discounting curve
    Handle<YieldTermStructure> discCurve(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.02, Actual365Fixed()));
    // forward (+10bp)
    Handle<YieldTermStructure> forwardCurve1(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.0210, Actual365Fixed()));
    // forward (-10bp)
    Handle<YieldTermStructure> forwardCurve2(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.0190, Actual365Fixed()));

    const IrLgm1fConstantParametrization irlgm1f(EURCurrency(), discCurve, 0.01,
                                                 0.01);

    // forward curve attached
    boost::shared_ptr<SwapIndex> index1 =
        boost::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve1);
    boost::shared_ptr<SwapIndex> index2 =
        boost::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve2);

    Swaption swaption1 = MakeSwaption(index1, 10 * Years, 0.02);
    Swaption swaption2 = MakeSwaption(index2, 10 * Years, 0.02);

    boost::shared_ptr<PricingEngine> engine =
        boost::make_shared<AnalyticLgmSwaptionEngine>(&irlgm1f, discCurve);

    swaption1.setPricingEngine(engine);
    swaption1.NPV();
    std::vector<Real> fixedAmountCorrections1 =
        swaption1.result<std::vector<Real> >("fixedAmountCorrections");
    swaption2.setPricingEngine(engine);
    swaption2.NPV();
    std::vector<Real> fixedAmountCorrections2 =
        swaption2.result<std::vector<Real> >("fixedAmountCorrections");

    // we can assume that the result vectors have the correct size, this
    // was tested above

    for (Size i = 0; i < 10; ++i) {
        // amount correction should be roughly +10bp (-10bp)
        // up to conventions
        if (std::abs(fixedAmountCorrections1[i] - 0.0010) > 0.00005) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (1) should be around 10bp for "
                "a 10bp spread, but seems off ("
                << fixedAmountCorrections1[i] << ")");
        }
        if (std::abs(fixedAmountCorrections2[i] + 0.0010) > 0.00005) {
            BOOST_TEST_ERROR(
                "fixed coupon adjustment (2) should be around -10bp for "
                "a -10bp spread, but seems off ("
                << fixedAmountCorrections2[i] << ")");
        }
    }
}

test_suite *AnalyticLgmSwaptionEngineTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("Analytic LGM swaption engine tests");
    suite->add(
        QUANTEXT_TEST_CASE(&AnalyticLgmSwaptionEngineTest::testMonoCurve));
    suite->add(
        QUANTEXT_TEST_CASE(&AnalyticLgmSwaptionEngineTest::testDualCurve));
    return suite;
}
