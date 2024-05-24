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

#include "toplevelfixture.hpp"
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/currencies/all.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CashFlowTest)

BOOST_AUTO_TEST_CASE(testFXLinkedCashFlow) {

    BOOST_TEST_MESSAGE("Testing FX Linked CashFlow");

    // Test today = 5 Jan 2016
    Settings::instance().evaluationDate() = Date(5, Jan, 2016);
    Date today = Settings::instance().evaluationDate();

    Date cfDate1(5, Jan, 2015); // historical
    Date cfDate2(5, Jan, 2016); // today
    Date cfDate3(5, Jan, 2017); // future

    Real foreignAmount = 1000000; // 1M
    QuantLib::ext::shared_ptr<SimpleQuote> sq = QuantLib::ext::make_shared<SimpleQuote>(123.45);
    Handle<Quote> spot(sq);
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Calendar cal = TARGET();
    Handle<YieldTermStructure> domYTS(QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.005, dc))); // JPY
    Handle<YieldTermStructure> forYTS(QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.03, dc)));  // USD
    // TODO foreign/domestic vs source/target
    QuantLib::ext::shared_ptr<FxIndex> fxIndex =
        QuantLib::ext::make_shared<FxIndex>("FX::USDJPY", 0, USDCurrency(), JPYCurrency(), TARGET(), spot, domYTS, forYTS);

    FXLinkedCashFlow fxlcf1(cfDate1, cfDate1, foreignAmount, fxIndex);
    FXLinkedCashFlow fxlcf2(cfDate2, cfDate2, foreignAmount, fxIndex);
    FXLinkedCashFlow fxlcf3(cfDate3, cfDate3, foreignAmount, fxIndex);

    // Add historical and todays fixing
    fxIndex->addFixing(cfDate1, 112.0);
    fxIndex->addFixing(cfDate2, sq->value());

    BOOST_TEST_MESSAGE("Check historical flow is correct");
    BOOST_CHECK_CLOSE(fxlcf1.amount(), 112000000.0, 1e-10);

    BOOST_TEST_MESSAGE("Check todays flow is correct");
    BOOST_CHECK_CLOSE(fxlcf2.amount(), 123450000.0, 1e-10);

    BOOST_TEST_MESSAGE("Check future (expected) flow is correct");
    Real fwd = sq->value() * domYTS->discount(cfDate3) / forYTS->discount(cfDate3);
    BOOST_CHECK_CLOSE(fxlcf3.amount(), foreignAmount * fwd, 1e-10);

    // Now move forward in time, check historical value is still correct
    Settings::instance().evaluationDate() = Date(1, Feb, 2016);
    sq->setValue(150.0);
    domYTS->update();
    forYTS->update();
    BOOST_CHECK_CLOSE(fxlcf1.amount(), 112000000.0, 1e-10);

    // check forward quote is still valid
    fwd = sq->value() * domYTS->discount(cfDate3) / forYTS->discount(cfDate3);
    BOOST_CHECK_CLOSE(fxlcf3.amount(), foreignAmount * fwd, 1e-10);

    // reset
    Settings::instance().evaluationDate() = today;
}

BOOST_AUTO_TEST_CASE(testEquityCoupon) {

    BOOST_TEST_MESSAGE("Testing Equity Coupon");

    // Test today = 5 Jan 2016
    Settings::instance().evaluationDate() = Date(5, Jan, 2016);
    Date today = Settings::instance().evaluationDate();

    Date cfDate1(4, Dec, 2015);
    Date cfDate2(5, Apr, 2016); // future
    Date fixingDate1(31, Dec, 2015);
    Date fixingDate2(1, Apr, 2016);

    Real nominal = 1000000; // 1M - in USD
    string eqName = "SP5";
    QuantLib::ext::shared_ptr<SimpleQuote> sq = QuantLib::ext::make_shared<SimpleQuote>(2100);
    Handle<Quote> spot(sq);
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Calendar cal = TARGET();
    Currency ccy = USDCurrency();
    Natural fixingLag = 2;
    Real divFactor = 1.0;
    Handle<YieldTermStructure> dividend(
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.01, dc))); // Dividend Curve
    Handle<YieldTermStructure> equityforecast(
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.02, dc))); // Equity Forecast Curve

    QuantLib::ext::shared_ptr<EquityIndex2> eqIndex =
        QuantLib::ext::make_shared<EquityIndex2>(eqName, cal, ccy, spot, equityforecast, dividend);

    eqIndex->addFixing(cfDate1, 2000);
    eqIndex->addFixing(fixingDate1, 1980);

    // Price Return coupon
    EquityCoupon eq1(cfDate2, nominal, today, cfDate2, 0, eqIndex, dc, EquityReturnType::Price);
    // Total Return Coupon
    EquityCoupon eq2(cfDate2, nominal, today, cfDate2, 0, eqIndex, dc, EquityReturnType::Total, divFactor);
    // historical starting coupon
    EquityCoupon eq3(cfDate2, nominal, cfDate1, cfDate2, 0, eqIndex, dc, EquityReturnType::Price);
    // Total Return Coupon with fixing lag
    EquityCoupon eq4(cfDate2, nominal, today, cfDate2, fixingLag, eqIndex, dc, EquityReturnType::Total);

    // Fx Index, coupon and underlying have different currency
    Handle<YieldTermStructure> domYTS(QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.01, dc))); // EUR
    Handle<YieldTermStructure> forYTS(QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.02, dc))); // USD
    Handle<Quote> fxSpot(QuantLib::ext::make_shared<SimpleQuote>(1.1));
    QuantLib::ext::shared_ptr<FxIndex> fxIndex =
        QuantLib::ext::make_shared<FxIndex>("FX::EURUSD", 2, EURCurrency(), USDCurrency(), TARGET(), fxSpot, domYTS, forYTS);
    // Add historical and todays fixing
    fxIndex->addFixing(cfDate1, 1.09);

    // Total return coupon with underlying in different ccy - Base ccy EUR, and underlying SP5 in USD
    EquityCoupon eq5(cfDate2, nominal, today, cfDate2, 0, eqIndex, dc, EquityReturnType::Total, 1.0, false,
                     Null<Real>(), Null<Real>(), Date(), Date(), Date(), Date(), Date(), fxIndex);

    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer1(new EquityCouponPricer());
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer2(new EquityCouponPricer());
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer3(new EquityCouponPricer());
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer4(new EquityCouponPricer());
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer5(new EquityCouponPricer());
    eq1.setPricer(pricer1);
    eq2.setPricer(pricer2);
    eq3.setPricer(pricer3);
    eq4.setPricer(pricer4);
    eq5.setPricer(pricer5);

    // Price Return coupon
    Time dt = dc.yearFraction(today, cfDate2);
    Real forward = spot->value() * std::exp((0.02 - 0.01) * dt);
    Real expectedAmount = nominal * (forward - spot->value()) / spot->value();
    BOOST_TEST_MESSAGE("Check Price Return is correct.");
    BOOST_CHECK_CLOSE(eq1.amount(), expectedAmount, 1e-10);

    // Total Return Coupon
    forward = spot->value() * std::exp((0.02 - 0.01) * dt);
    Real div = spot->value() * std::exp((0.02) * dt) - forward;
    expectedAmount = nominal * (forward + divFactor * div - spot->value()) / spot->value();
    BOOST_TEST_MESSAGE("Check Total Return is correct");
    BOOST_CHECK_CLOSE(eq2.amount(), expectedAmount, 1e-10);

    // Historical starting Price Return coupon
    forward = spot->value() * std::exp((0.02 - 0.01) * dt);
    expectedAmount = nominal * (forward - eqIndex->fixing(cfDate1)) / eqIndex->fixing(cfDate1);
    BOOST_TEST_MESSAGE("Check Historical starting Price Return is correct.");
    BOOST_CHECK_CLOSE(eq3.amount(), expectedAmount, 1e-10);

    // Total Return Coupon with fixing lag
    dt = dc.yearFraction(today, fixingDate2);
    forward = spot->value() * std::exp(0.02 * dt);
    expectedAmount = nominal * (forward - eqIndex->fixing(fixingDate1)) / eqIndex->fixing(fixingDate1);
    BOOST_TEST_MESSAGE("Check Total Return fixing lag handling is correct.");
    BOOST_CHECK_CLOSE(eq4.amount(), expectedAmount, 1e-10);

    // Total return coupon with underlying in different ccy
    dt = dc.yearFraction(today, cfDate2);
    forward = spot->value() * std::exp((0.02 - 0.01) * dt);
    div = spot->value() * std::exp((0.02) * dt) - forward;
    expectedAmount =
        nominal * ((forward + div) * fxIndex->fixing(cfDate2) - spot->value() * 1.1) / (spot->value() * 1.1);
    BOOST_TEST_MESSAGE("Check Total Return with underlying in different ccy handling is correct.");
    BOOST_CHECK_CLOSE(eq5.amount(), expectedAmount, 1e-10);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
