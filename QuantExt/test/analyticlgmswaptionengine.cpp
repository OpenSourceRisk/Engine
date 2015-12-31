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
    Real fixedAmountCorrectionSettlement1 =
        swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_nocurves.setPricingEngine(engine_monocurve);
    swaption_nocurves.NPV();
    std::vector<Real> fixedAmountCorrections2 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2 =
        swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_monocurve.setPricingEngine(engine_nodisc);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections3 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement3 =
        swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_monocurve.setPricingEngine(engine_monocurve);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections4 =
        swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement4 =
        swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");

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

    if (!close_enough(fixedAmountCorrectionSettlement1, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (1) should be "
                         "zero in mono curve setup, but is "
                         << fixedAmountCorrectionSettlement1);
    }
    if (!close_enough(fixedAmountCorrectionSettlement2, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (2) should be "
                         "zero in mono curve setup, but is "
                         << fixedAmountCorrectionSettlement2);
    }
    if (!close_enough(fixedAmountCorrectionSettlement3, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (3) should be "
                         "zero in mono curve setup, but is "
                         << fixedAmountCorrectionSettlement3);
    }
    if (!close_enough(fixedAmountCorrectionSettlement4, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (4) should be "
                         "zero in mono curve setup, but is "
                         << fixedAmountCorrectionSettlement4);
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

    boost::shared_ptr<PricingEngine> engine_a =
        boost::make_shared<AnalyticLgmSwaptionEngine>(
            &irlgm1f, discCurve, AnalyticLgmSwaptionEngine::nextCoupon);

    boost::shared_ptr<PricingEngine> engine_b =
        boost::make_shared<AnalyticLgmSwaptionEngine>(
            &irlgm1f, discCurve, AnalyticLgmSwaptionEngine::proRata);

    swaption1.setPricingEngine(engine_a);
    swaption1.NPV();
    std::vector<Real> fixedAmountCorrections1a =
        swaption1.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement1a =
        swaption1.result<Real>("fixedAmountCorrectionSettlement");
    swaption2.setPricingEngine(engine_a);
    swaption2.NPV();
    std::vector<Real> fixedAmountCorrections2a =
        swaption2.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2a =
        swaption2.result<Real>("fixedAmountCorrectionSettlement");
    swaption1.setPricingEngine(engine_b);
    swaption1.NPV();
    std::vector<Real> fixedAmountCorrections1b =
        swaption1.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement1b =
        swaption1.result<Real>("fixedAmountCorrectionSettlement");
    swaption2.setPricingEngine(engine_b);
    swaption2.NPV();
    std::vector<Real> fixedAmountCorrections2b =
        swaption2.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2b =
        swaption2.result<Real>("fixedAmountCorrectionSettlement");

    // check corrections on settlement for plausibility

    Real tolerance = 0.000025; // 0.25 bp

    if (!close_enough(fixedAmountCorrectionSettlement1a, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (1) should be "
                         "0 for nextCoupon, but is "
                         << fixedAmountCorrectionSettlement1a);
    }
    if (!close_enough(fixedAmountCorrectionSettlement2a, 0.0)) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (2) should be "
                         "0 for nextCoupon, but is "
                         << fixedAmountCorrectionSettlement2a);
    }
    if (std::abs(fixedAmountCorrectionSettlement1b - 0.00025) > tolerance) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (1) should be "
                         "close to 2.5bp for proRata, but is "
                         << fixedAmountCorrectionSettlement1b);
    }
    if (std::abs(fixedAmountCorrectionSettlement2b + 0.00025) > tolerance) {
        BOOST_TEST_ERROR("fixed amount correction on settlement (1) should be "
                         "close to -2.5bp for proRata, but is "
                         << fixedAmountCorrectionSettlement2b);
    }

    // we can assume that the result vectors have the correct size, this
    // was tested above

    for (Size i = 0; i < 10; ++i) {
        // amount correction should be close to +10bp (-10bp)
        // up to conventions, check this for plausibility
        if (std::abs(fixedAmountCorrections1a[i] - 0.0010) > tolerance) {
            BOOST_TEST_ERROR("fixed coupon adjustment (1, nextCoupon) should "
                             "be close to 10bp for "
                             "a 10bp curve spread, but is "
                             << fixedAmountCorrections1a[i] << " for component "
                             << i);
        }
        if (std::abs(fixedAmountCorrections2a[i] + 0.0010) > tolerance) {
            BOOST_TEST_ERROR("fixed coupon adjustment (2, nextCoupon) should "
                             "be close to -10bp for "
                             "a -10bp curve spread, but is "
                             << fixedAmountCorrections2a[i] << " for component "
                             << i);
        }
        if (std::abs(fixedAmountCorrections1b[i] -
                     (i == 9 ? 0.00075 : 0.0010)) > tolerance) {
            BOOST_TEST_ERROR("fixed coupon adjustment (1, proRata) should "
                             "be close to 10bp (7.5bp for component 9) for "
                             "a 10bp curve spread, but is "
                             << fixedAmountCorrections1b[i] << " for component "
                             << i);
        }
        if (std::abs(fixedAmountCorrections2b[i] +
                     (i == 9 ? 0.00075 : 0.0010)) > tolerance) {
            BOOST_TEST_ERROR("fixed coupon adjustment (2, proRata) should "
                             "be close to -10bp (-7.5bp for component 9) for "
                             "a -10bp curve spread, but is "
                             << fixedAmountCorrections2b[i] << " for component "
                             << i);
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
