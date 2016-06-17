/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "cashflow.hpp"
#include <ql/quotes/simplequote.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <boost/make_shared.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

void CashFlowTest::testFXLinkedCashFlow() {

    BOOST_TEST_MESSAGE("Testing FX Linked CashFlow");

    Date today = Settings::instance().evaluationDate();

    // Test today = 1 Jan 2016
    Settings::instance().evaluationDate() = Date(1,Jan,2016);

    Date cfDate1 (1,Jan,2015); // historical
    Date cfDate2 (1,Jan,2016); // today
    Date cfDate3 (1,Jan,2017); // future

    string index = "FX_USDJPY";
    Real foreignAmount = 1000000; // 1M
    boost::shared_ptr<SimpleQuote> sq = boost::make_shared<SimpleQuote>(123.45);
    Handle<Quote> spot(sq);
    DayCounter dc = ActualActual();
    Handle<YieldTermStructure> domYTS(boost::shared_ptr<YieldTermStructure> (
        new FlatForward (today, 0.005, dc))); // JPY
    Handle<YieldTermStructure> forYTS(boost::shared_ptr<YieldTermStructure> (
        new FlatForward (today, 0.03, dc))); // USD

    FXLinkedCashFlow fxlcf1(cfDate1, cfDate1, foreignAmount, spot, forYTS, domYTS, index);
    FXLinkedCashFlow fxlcf2(cfDate2, cfDate2, foreignAmount, spot, forYTS, domYTS, index);
    FXLinkedCashFlow fxlcf3(cfDate3, cfDate3, foreignAmount, spot, forYTS, domYTS, index);

    // Add historical fixing
    TimeSeries<Real> history = IndexManager::instance().getHistory(index);
    history[cfDate1] = 112.0;
    IndexManager::instance().setHistory(index, history);

    BOOST_TEST_MESSAGE("Check historical flow is correct");
    BOOST_CHECK_CLOSE(fxlcf1.amount(), 112000000.0, 1e-10);

    BOOST_TEST_MESSAGE("Check todays flow is correct");
    BOOST_CHECK_CLOSE(fxlcf2.amount(), 123450000.0, 1e-10);

    BOOST_TEST_MESSAGE("Check future (expected) flow is correct");
    Real fwd = sq->value() * forYTS->discount(cfDate3) / domYTS->discount(cfDate3);
    BOOST_CHECK_CLOSE(fxlcf3.amount(), foreignAmount * fwd, 1e-10);

    // Now move forward in time, check historical value is still correct
    Settings::instance().evaluationDate() = Date(1,Feb,2016);
    sq->setValue(150.0);
    BOOST_CHECK_CLOSE(fxlcf1.amount(), 112000000.0, 1e-10);

    // check foward quote is still valid
    fwd = sq->value() * forYTS->discount(cfDate3) / domYTS->discount(cfDate3);
    BOOST_CHECK_CLOSE(fxlcf3.amount(), foreignAmount * fwd, 1e-10);

    // reset
    Settings::instance().evaluationDate() = today;
}


test_suite* CashFlowTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CashFlowTests");
    suite->add(BOOST_TEST_CASE(&CashFlowTest::testFXLinkedCashFlow));
    return suite;
}
