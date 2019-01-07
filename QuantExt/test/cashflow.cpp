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

#include "cashflow.hpp"
#include <boost/make_shared.hpp>
#include <ql/currencies/all.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace testsuite {
void CashFlowTest::testFXLinkedCashFlow() {

    BOOST_TEST_MESSAGE("Testing FX Linked CashFlow");

    // Test today = 5 Jan 2016
    Settings::instance().evaluationDate() = Date(5, Jan, 2016);
    Date today = Settings::instance().evaluationDate();

    Date cfDate1(5, Jan, 2015); // historical
    Date cfDate2(5, Jan, 2016); // today
    Date cfDate3(5, Jan, 2017); // future

    Real foreignAmount = 1000000; // 1M
    boost::shared_ptr<SimpleQuote> sq = boost::make_shared<SimpleQuote>(123.45);
    Handle<Quote> spot(sq);
    DayCounter dc = ActualActual();
    Calendar cal = TARGET();
    Handle<YieldTermStructure> domYTS(boost::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.005, dc))); // JPY
    Handle<YieldTermStructure> forYTS(boost::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.03, dc)));  // USD
    // TODO foreign/domestic vs source/target
    boost::shared_ptr<FxIndex> fxIndex =
        boost::make_shared<FxIndex>("FX::USDJPY", 0, USDCurrency(), JPYCurrency(), TARGET(), spot, domYTS, forYTS);

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

    // check foward quote is still valid
    fwd = sq->value() * domYTS->discount(cfDate3) / forYTS->discount(cfDate3);
    BOOST_CHECK_CLOSE(fxlcf3.amount(), foreignAmount * fwd, 1e-10);

    // reset
    Settings::instance().evaluationDate() = today;
}

void CashFlowTest::testEquityCoupon() {

    BOOST_TEST_MESSAGE("Testing Equity Coupon");

    // Test today = 5 Jan 2016
    Settings::instance().evaluationDate() = Date(5, Jan, 2016);
    Date today = Settings::instance().evaluationDate();

    Date cfDate1(4, Dec, 2015);
    Date cfDate2(5, Apr, 2016); // future

    Real nominal = 1000000; // 1M
    string eqName = "SP5";
    boost::shared_ptr<SimpleQuote> sq = boost::make_shared<SimpleQuote>(2100);
    Handle<Quote> spot(sq);
    DayCounter dc = ActualActual();
    Calendar cal = TARGET();
    Handle<YieldTermStructure> dividend(boost::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.01, dc))); // Dividend Curve
    Handle<YieldTermStructure> equityforecast(boost::shared_ptr<YieldTermStructure>(new FlatForward(0, cal, 0.02, dc)));  // Equity Forecast Curve

    boost::shared_ptr<EquityIndex> eqIndex =
        boost::make_shared<EquityIndex>(eqName, cal, spot, equityforecast, dividend);

    eqIndex->addFixing(cfDate1, 2000);

    // Price Return coupon
    EquityCoupon eq1(cfDate2, 1000000, today, cfDate2, eqIndex, dc);
    // Total Return Coupon
    EquityCoupon eq2(cfDate2, 1000000, today, cfDate2, eqIndex, dc, true);
    // historical starting coupon
    EquityCoupon eq3(cfDate2, 1000000, cfDate1, cfDate2, eqIndex, dc);

    boost::shared_ptr<EquityCouponPricer> pricer1(new EquityCouponPricer());
    boost::shared_ptr<EquityCouponPricer> pricer2(new EquityCouponPricer());
    boost::shared_ptr<EquityCouponPricer> pricer3(new EquityCouponPricer());
    eq1.setPricer(pricer1);
    eq2.setPricer(pricer2);
    eq3.setPricer(pricer3);

    // Price Return coupon
    Time dt = dc.yearFraction(today, cfDate2);
    Real forward = spot->value() * std::exp((0.02 - 0.01)*dt);
    Real expectedAmount = nominal * (forward - spot->value()) / spot->value();
    BOOST_TEST_MESSAGE("Check Price Return is correct");
    BOOST_CHECK_CLOSE(eq1.amount(), expectedAmount, 1e-10);

    // Total Return Coupon
    forward = spot->value() * std::exp(0.02*dt);
    expectedAmount = nominal * (forward - spot->value()) / spot->value();
    BOOST_TEST_MESSAGE("Check Total Return is correct");
    BOOST_CHECK_CLOSE(eq2.amount(), expectedAmount, 1e-10);
    
    // Historical starting Price Return coupon
    forward = spot->value() * std::exp((0.02 - 0.01)*dt);
    expectedAmount = nominal * (forward - eqIndex->fixing(cfDate1)) / eqIndex->fixing(cfDate1);
    BOOST_TEST_MESSAGE("Check Historical starting Price Return is correct");
    BOOST_CHECK_CLOSE(eq3.amount(), expectedAmount, 1e-10);
}

test_suite* CashFlowTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CashFlowTests");
    suite->add(BOOST_TEST_CASE(&CashFlowTest::testFXLinkedCashFlow));
    suite->add(BOOST_TEST_CASE(&CashFlowTest::testEquityCoupon));
    return suite;
}
} // namespace testsuite
