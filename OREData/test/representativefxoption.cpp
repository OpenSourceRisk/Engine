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

#include <qle/models/representativefxoption.hpp>

#include <oret/toplevelfixture.hpp>

#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/fxindex.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

struct RepresentativeFxOptionFixture : public ore::test::TopLevelFixture {
public:
    RepresentativeFxOptionFixture() { Settings::instance().evaluationDate() = today; }
    Date today = Date(20, Apr, 2021);
    Handle<Quote> eurUsdSpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
    Actual365Fixed dc;
    Handle<YieldTermStructure> eurCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, dc));
    Handle<YieldTermStructure> usdCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.03, dc));
};

BOOST_AUTO_TEST_SUITE(OREDataTestSuite)

BOOST_FIXTURE_TEST_SUITE(RepresentativeFxOptionTest, RepresentativeFxOptionFixture)

BOOST_AUTO_TEST_CASE(testSimpleCashflows) {

    BOOST_TEST_MESSAGE("test reproducing simple cashflows");

    Real tol = 1E-12, tol2 = 1E-10;

    Date refDate = today + 5 * Years;

    Real eurAmount = 35000.0;
    Real usdAmount = 14222.0;
    Leg eurLeg = {QuantLib::ext::make_shared<SimpleCashFlow>(eurAmount, refDate)};
    Leg usdLeg = {QuantLib::ext::make_shared<SimpleCashFlow>(usdAmount, refDate)};

    RepresentativeFxOptionMatcher matcher1({eurLeg, usdLeg}, {true, false}, {"EUR", "USD"}, refDate, "EUR", "USD",
                                           eurCurve, usdCurve, eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher1.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher1.currency2(), "USD");
    BOOST_CHECK_CLOSE(matcher1.amount1(), -eurAmount, tol);
    BOOST_CHECK_CLOSE(matcher1.amount2(), usdAmount, tol);

    RepresentativeFxOptionMatcher matcher2({eurLeg, usdLeg}, {false, true}, {"EUR", "USD"}, refDate, "EUR", "USD",
                                           eurCurve, usdCurve, eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher2.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher2.currency2(), "USD");
    BOOST_CHECK_CLOSE(matcher2.amount1(), eurAmount, tol);
    BOOST_CHECK_CLOSE(matcher2.amount2(), -usdAmount, tol);

    RepresentativeFxOptionMatcher matcher3({eurLeg, usdLeg}, {false, false}, {"EUR", "USD"}, refDate, "EUR", "USD",
                                           eurCurve, usdCurve, eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher3.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher3.currency2(), "USD");
    BOOST_CHECK_CLOSE(matcher3.amount1(), eurAmount, tol);
    BOOST_CHECK_CLOSE(matcher3.amount2(), usdAmount, tol);

    RepresentativeFxOptionMatcher matcher4({eurLeg, usdLeg}, {true, true}, {"EUR", "USD"}, refDate, "EUR", "USD",
                                           eurCurve, usdCurve, eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher4.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher4.currency2(), "USD");
    BOOST_CHECK_CLOSE(matcher4.amount1(), -eurAmount, tol);
    BOOST_CHECK_CLOSE(matcher4.amount2(), -usdAmount, tol);

    RepresentativeFxOptionMatcher matcher5({eurLeg}, {true}, {"EUR"}, refDate, "EUR", "USD", eurCurve, usdCurve,
                                           eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher5.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher5.currency2(), "USD");
    BOOST_CHECK_CLOSE(matcher5.amount1(), -eurAmount, tol);
    BOOST_CHECK_SMALL(matcher5.amount2(), tol2);

    RepresentativeFxOptionMatcher matcher6({}, {}, {}, refDate, "EUR", "USD", eurCurve, usdCurve, eurUsdSpot, true);

    BOOST_CHECK_EQUAL(matcher6.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher6.currency2(), "USD");
    BOOST_CHECK_SMALL(matcher6.amount1(), tol2);
    BOOST_CHECK_SMALL(matcher6.amount2(), tol2);
}

BOOST_AUTO_TEST_CASE(testFxLinkedCashflow) {

    BOOST_TEST_MESSAGE("test fx linked cashflow");

    Real tol = 1E-12;

    Date refDate = today + 5 * Years;
    Date fixDate = UnitedStates(UnitedStates::Settlement).advance(refDate, -2 * Days);

    auto fxSpotScen = QuantLib::ext::make_shared<SimpleQuote>(eurUsdSpot->value());
    auto fxIndex = QuantLib::ext::make_shared<FxIndex>("dummy", 2, EURCurrency(), USDCurrency(), UnitedStates(UnitedStates::Settlement),
                                               Handle<Quote>(fxSpotScen), eurCurve, usdCurve);
    Real forAmount = 100000.0;

    Leg leg = {QuantLib::ext::make_shared<FXLinkedCashFlow>(refDate, fixDate, forAmount, fxIndex)};

    RepresentativeFxOptionMatcher matcher({leg}, {false}, {"USD"}, today, "EUR", "USD", eurCurve, usdCurve, eurUsdSpot);

    BOOST_CHECK_EQUAL(matcher.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher.currency2(), "USD");

    Real eurAmount = matcher.amount1();
    Real usdAmount = matcher.amount2();

    // check npv matches

    BOOST_CHECK_CLOSE(eurAmount * eurUsdSpot->value() + usdAmount, leg.front()->amount() * usdCurve->discount(refDate),
                      tol);

    // check fx delta matches

    fxSpotScen->setValue(eurUsdSpot->value() * 1.01);
    Real upNpv = leg.front()->amount() * usdCurve->discount(refDate);

    fxSpotScen->setValue(eurUsdSpot->value() * 0.99);
    Real downNpv = leg.front()->amount() * usdCurve->discount(refDate);

    BOOST_CHECK_CLOSE(eurAmount, (upNpv - downNpv) / (eurUsdSpot->value() * 0.02), tol);
}

BOOST_AUTO_TEST_CASE(testFxLinkedFloatingRateCoupon) {

    BOOST_TEST_MESSAGE("test fx linked floating rate coupon");

    Real tol = 1E-12;

    Date refDate = today + 5 * Years;
    Date fixDate = UnitedStates(UnitedStates::Settlement).advance(refDate, -2 * Days);

    auto fxSpotScen = QuantLib::ext::make_shared<SimpleQuote>(eurUsdSpot->value());
    auto fxIndex = QuantLib::ext::make_shared<FxIndex>("dummy", 2, EURCurrency(), USDCurrency(), UnitedStates(UnitedStates::Settlement),
                                               Handle<Quote>(fxSpotScen), eurCurve, usdCurve);
    Real forAmount = 100000.0;

    auto iborIndex = QuantLib::ext::make_shared<IborIndex>("dummyIbor", 6 * Months, 2, EURCurrency(), TARGET(), Following,
                                                   false, Actual360(), eurCurve);
    Date start(refDate - 6 * Months), end(refDate);
    auto iborCoupon = QuantLib::ext::make_shared<IborCoupon>(refDate, 1.0, start, end, 2, iborIndex, 1.0, 0.0);
    iborCoupon->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>());

    Leg leg = {QuantLib::ext::make_shared<FloatingRateFXLinkedNotionalCoupon>(fixDate, forAmount, fxIndex, iborCoupon)};

    RepresentativeFxOptionMatcher matcher({leg}, {false}, {"USD"}, today, "EUR", "USD", eurCurve, usdCurve, eurUsdSpot);

    BOOST_CHECK_EQUAL(matcher.currency1(), "EUR");
    BOOST_CHECK_EQUAL(matcher.currency2(), "USD");

    Real eurAmount = matcher.amount1();
    Real usdAmount = matcher.amount2();

    // check npv matches

    BOOST_CHECK_CLOSE(eurAmount * eurUsdSpot->value() + usdAmount, leg.front()->amount() * usdCurve->discount(refDate),
                      tol);

    // check fx delta matches

    fxSpotScen->setValue(eurUsdSpot->value() * 1.01);
    Real upNpv = leg.front()->amount() * usdCurve->discount(refDate);

    fxSpotScen->setValue(eurUsdSpot->value() * 0.99);
    Real downNpv = leg.front()->amount() * usdCurve->discount(refDate);

    BOOST_CHECK_CLOSE(eurAmount, (upNpv - downNpv) / (eurUsdSpot->value() * 0.02), tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
