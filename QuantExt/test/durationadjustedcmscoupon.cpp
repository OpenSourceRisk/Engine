/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/durationadjustedcmscoupontsrpricer.hpp>
#include <qle/models/linearannuitymapping.hpp>

#include "toplevelfixture.hpp"

#include <ql/time/date.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/indexes/swap/euriborswap.hpp>

using namespace QuantExt;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DurationAdjustedCouponTest)

BOOST_AUTO_TEST_CASE(testAgainstCmsCoupon) {

    BOOST_TEST_MESSAGE("Testing duration adjusted cms coupons vs. vanilla cms coupon...");

    Date today(25, January, 2021);
    Settings::instance().evaluationDate() = today;

    Handle<YieldTermStructure> discountCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> forwardCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<SwaptionVolatilityStructure> swaptionVol(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
        0, NullCalendar(), Unadjusted, 0.0050, Actual365Fixed(), Normal));
    Handle<Quote> reversion(QuantLib::ext::make_shared<SimpleQuote>(0.01));

    Date startDate = Date(25, January, 2025);
    Date endDate = Date(25, January, 2026);
    Date payDate = Date(27, January, 2026);
    Size fixingDays = 2;
    auto index = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve, discountCurve);

    CmsCoupon cmsCoupon(payDate, 1.0, startDate, endDate, fixingDays, index);
    DurationAdjustedCmsCoupon durationAdjustedCmsCoupon(payDate, 1.0, startDate, endDate, fixingDays, index, 0);

    auto cmsPricer = QuantLib::ext::make_shared<LinearTsrPricer>(swaptionVol, reversion, discountCurve,
                                                         LinearTsrPricer::Settings().withRateBound(-2.0, 2.0));

    auto durationAdjustedCmsPricer = QuantLib::ext::make_shared<DurationAdjustedCmsCouponTsrPricer>(
        swaptionVol, QuantLib::ext::make_shared<LinearAnnuityMappingBuilder>(reversion), -2.0, 2.0);

    cmsCoupon.setPricer(cmsPricer);
    durationAdjustedCmsCoupon.setPricer(durationAdjustedCmsPricer);

    BOOST_TEST_MESSAGE("cms coupon rate                   = " << cmsCoupon.rate());
    BOOST_TEST_MESSAGE("cms coupon convexity adj          = " << cmsCoupon.convexityAdjustment());
    BOOST_TEST_MESSAGE("duration adjusted cms coupon rate = " << durationAdjustedCmsCoupon.rate());
    BOOST_TEST_MESSAGE("dur adj cms coupon convexity adj  = " << durationAdjustedCmsCoupon.convexityAdjustment());

    Real tol = 1E-6; // percentage tolerance, i.e. we have 1E-8 effectively

    BOOST_CHECK_CLOSE(cmsCoupon.rate(), durationAdjustedCmsCoupon.rate(), tol);
    BOOST_CHECK_CLOSE(cmsCoupon.convexityAdjustment(), durationAdjustedCmsCoupon.convexityAdjustment(), tol);
}

BOOST_AUTO_TEST_CASE(testHistoricalValues) {

    BOOST_TEST_MESSAGE("Testing duration adjusted cms coupon historical rates...");

    Date today(25, January, 2021);
    Settings::instance().evaluationDate() = today;

    Date startDate = Date(25, June, 2020);
    Date endDate = Date(25, June, 2021);
    Date payDate = Date(27, June, 2021);
    Size fixingDays = 2;

    Handle<YieldTermStructure> discountCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> forwardCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    auto index = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve, discountCurve);

    Date fixingDate = index->fixingCalendar().advance(startDate, -(fixingDays * Days), Preceding);
    Real fixingValue = 0.01;
    index->addFixing(fixingDate, fixingValue);

    // we do not need a vol surface or an annuity mapping builder, since the coupon amount is deterministic
    auto pricer =
        QuantLib::ext::make_shared<DurationAdjustedCmsCouponTsrPricer>(Handle<SwaptionVolatilityStructure>(), nullptr);

    DurationAdjustedCmsCoupon cpn0(payDate, 1.0, startDate, endDate, fixingDays, index, 0);
    DurationAdjustedCmsCoupon cpn1(payDate, 1.0, startDate, endDate, fixingDays, index, 1);
    DurationAdjustedCmsCoupon cpn10(payDate, 1.0, startDate, endDate, fixingDays, index, 10);

    cpn0.setPricer(pricer);
    cpn1.setPricer(pricer);
    cpn10.setPricer(pricer);

    BOOST_TEST_MESSAGE("duration = 0  : rate = " << cpn0.rate());
    BOOST_TEST_MESSAGE("duration = 1  : rate = " << cpn1.rate());
    BOOST_TEST_MESSAGE("duration = 10 : rate = " << cpn10.rate());

    BOOST_TEST_MESSAGE("duration = 0  : indexFixing = " << cpn0.indexFixing());
    BOOST_TEST_MESSAGE("duration = 1  : indexFixing = " << cpn1.indexFixing());
    BOOST_TEST_MESSAGE("duration = 10 : indexFixing = " << cpn10.indexFixing());

    auto durationAdjustment = [](const Real S, const Size i) {
        if (i == 0)
            return 1.0;
        Real tmp = 0.0;
        for (Size j = 0; j < i; ++j)
            tmp += 1.0 / std::pow(1.0 + S, j + 1);
        return tmp;
    };

    Real tol = 1.0E-6;

    BOOST_CHECK_CLOSE(cpn0.rate(), fixingValue * durationAdjustment(fixingValue, 0), tol);
    BOOST_CHECK_CLOSE(cpn1.rate(), fixingValue * durationAdjustment(fixingValue, 1), tol);
    BOOST_CHECK_CLOSE(cpn10.rate(), fixingValue * durationAdjustment(fixingValue, 10), tol);

    BOOST_CHECK_CLOSE(cpn0.indexFixing(), fixingValue * durationAdjustment(fixingValue, 0), tol);
    BOOST_CHECK_CLOSE(cpn1.indexFixing(), fixingValue * durationAdjustment(fixingValue, 1), tol);
    BOOST_CHECK_CLOSE(cpn10.indexFixing(), fixingValue * durationAdjustment(fixingValue, 10), tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
