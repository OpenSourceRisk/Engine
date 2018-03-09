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

#include "dynamicblackvoltermstructure.hpp"
#include "utilities.hpp"

#include <qle/termstructures/dynamicblackvoltermstructure.hpp>

#include <ql/math/matrix.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {

struct TestData {

    TestData() : origRefDate(20, Jan, 2016) {

        Settings::instance().evaluationDate() = origRefDate;

        // set up the reference vol term structure
        refDates.push_back(TARGET().advance(origRefDate, 1 * Years));
        refDates.push_back(TARGET().advance(origRefDate, 2 * Years));
        strikes.push_back(0.90);
        strikes.push_back(1.00);
        strikes.push_back(1.10);

        vol = Matrix(3, 2);
        vol[0][0] = 0.12;
        vol[0][1] = 0.10;
        vol[1][0] = 0.10;
        vol[1][1] = 0.08;
        vol[2][0] = 0.11;
        vol[2][1] = 0.09;

        refVol = Handle<BlackVolTermStructure>(
            boost::make_shared<BlackVarianceSurface>(origRefDate, TARGET(), refDates, strikes, vol, Actual365Fixed()));
        refVol->enableExtrapolation();

        // set up floating spot and term structures
        spot = boost::make_shared<SimpleQuote>(1.00);
        spot_q = Handle<Quote>(spot);
        rate = boost::make_shared<SimpleQuote>(0.02);
        rate_q = Handle<Quote>(rate);
        div = boost::make_shared<SimpleQuote>(0.02);
        div_q = Handle<Quote>(div);

        riskfreeTs = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, TARGET(), rate_q, Actual365Fixed()));
        dividendTs = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, TARGET(), div_q, Actual365Fixed()));
    }

    SavedSettings backup;
    const Date origRefDate;
    std::vector<Date> refDates;
    std::vector<Real> strikes;
    Matrix vol;
    Handle<BlackVolTermStructure> refVol;
    boost::shared_ptr<SimpleQuote> spot, rate, div;
    Handle<Quote> spot_q, rate_q, div_q;
    Handle<YieldTermStructure> riskfreeTs, dividendTs;
};

} // anonymous namespace

namespace testsuite {

void DynamicBlackVolTermStructureTest::testConstantVarianceStickyStrike() {

    BOOST_TEST_MESSAGE("Testing constant variance, sticky strike dynamics of "
                       "DynamicBlackVolTermStructure...");

    TestData d;

    Handle<DynamicBlackVolTermStructure<tag::surface> > dyn(
        boost::make_shared<DynamicBlackVolTermStructure<tag::surface> >(
            d.refVol, 0, TARGET(), ConstantVariance, StickyStrike, d.riskfreeTs, d.dividendTs, d.spot_q));

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.80), d.refVol->blackVol(0.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.90), d.refVol->blackVol(0.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.95), d.refVol->blackVol(0.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 1.15), d.refVol->blackVol(0.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackVol(1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackVol(1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackVol(1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackVol(1.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.80), d.refVol->blackVol(5.0, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.90), d.refVol->blackVol(5.0, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.95), d.refVol->blackVol(5.0, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 1.15), d.refVol->blackVol(5.0, 1.15), tol);

    // check atm vol retrieval via null strike (atm is spot here)

    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, Null<Real>()), d.refVol->blackVol(0.5, d.spot->value()), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, Null<Real>()), d.refVol->blackVol(1.5, d.spot->value()), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, Null<Real>()), d.refVol->blackVol(5.0, d.spot->value()), tol);

    // move forward in time, we expect a constant surface in every respect
    // even when atm is changing via spot or rates

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 6 * Months);
    d.spot->setValue(0.9);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, 0.80), d.refVol->blackVol(0.7, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, 0.90), d.refVol->blackVol(0.7, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, 0.95), d.refVol->blackVol(0.7, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, 1.15), d.refVol->blackVol(0.7, 1.15), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 18 * Months);
    d.rate->setValue(0.01);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, 0.80), d.refVol->blackVol(1.7, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, 0.90), d.refVol->blackVol(1.7, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, 0.95), d.refVol->blackVol(1.7, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, 1.15), d.refVol->blackVol(1.7, 1.15), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 3 * Years);
    d.div->setValue(0.03);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.71, 0.80), d.refVol->blackVol(1.71, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.71, 0.90), d.refVol->blackVol(1.71, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.71, 0.95), d.refVol->blackVol(1.71, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.71, 1.15), d.refVol->blackVol(1.71, 1.15), tol);

} // testConstantVarianceStickyStrike

void DynamicBlackVolTermStructureTest::testConstantVarianceStickyLogMoneyness() {

    BOOST_TEST_MESSAGE("Testing constant variance, sticky log-moneyness "
                       "dynamics of DynamicBlackVolTermStructure...");

    TestData d;

    Handle<DynamicBlackVolTermStructure<tag::surface> > dyn(
        boost::make_shared<DynamicBlackVolTermStructure<tag::surface> >(
            d.refVol, 0, TARGET(), ConstantVariance, StickyLogMoneyness, d.riskfreeTs, d.dividendTs, d.spot_q));

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.80), d.refVol->blackVol(0.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.90), d.refVol->blackVol(0.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.95), d.refVol->blackVol(0.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 1.15), d.refVol->blackVol(0.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackVol(1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackVol(1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackVol(1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackVol(1.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.80), d.refVol->blackVol(5.0, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.90), d.refVol->blackVol(5.0, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.95), d.refVol->blackVol(5.0, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 1.15), d.refVol->blackVol(5.0, 1.15), tol);

    // move forward in time, in time direction we expect a constant
    // surface, but the strike range is constant in log money now
    // instead of absolute strike

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 6 * Months);

    Real atm0 = d.spot->value(); // original atm value
    d.spot->setValue(0.9);
    Real atm = d.spot->value(); // new atm value

    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, atm * std::exp(-0.25)), d.refVol->blackVol(0.7, atm0 * std::exp(-0.25)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, atm * std::exp(0.00)), d.refVol->blackVol(0.7, atm0 * std::exp(0.00)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.7, atm * std::exp(0.25)), d.refVol->blackVol(0.7, atm0 * std::exp(0.25)), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, atm * std::exp(-0.25)), d.refVol->blackVol(1.7, atm0 * std::exp(-0.25)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, atm * std::exp(0.00)), d.refVol->blackVol(1.7, atm0 * std::exp(0.00)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.7, atm * std::exp(0.25)), d.refVol->blackVol(1.7, atm0 * std::exp(0.25)), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(3.0, atm * std::exp(-0.25)), d.refVol->blackVol(3.0, atm0 * std::exp(-0.25)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(3.0, atm * std::exp(0.00)), d.refVol->blackVol(3.0, atm0 * std::exp(0.00)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(3.0, atm * std::exp(0.25)), d.refVol->blackVol(3.0, atm0 * std::exp(0.25)), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 18 * Months);
    d.rate->setValue(0.01);
    // new atm for t=1.8
    atm = d.spot->value() / d.riskfreeTs->discount(1.8) * d.dividendTs->discount(1.8);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.8, atm * std::exp(-0.25)), d.refVol->blackVol(1.8, atm0 * std::exp(-0.25)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.8, atm * std::exp(0.00)), d.refVol->blackVol(1.8, atm0 * std::exp(0.00)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.8, atm * std::exp(0.25)), d.refVol->blackVol(1.8, atm0 * std::exp(0.25)), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 3 * Years);
    d.div->setValue(0.03);
    // new atm for t=3.5
    atm = d.spot->value() / d.riskfreeTs->discount(3.5) * d.dividendTs->discount(3.5);

    BOOST_CHECK_CLOSE(dyn->blackVol(3.5, atm * std::exp(-0.25)), d.refVol->blackVol(3.5, atm0 * std::exp(-0.25)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(3.5, atm * std::exp(0.00)), d.refVol->blackVol(3.5, atm0 * std::exp(0.00)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(3.5, atm * std::exp(0.25)), d.refVol->blackVol(3.5, atm0 * std::exp(0.25)), tol);

} // testConstantVarianceStickyLogMoneyness

void DynamicBlackVolTermStructureTest::testForwardVarianceStickyStrike() {

    BOOST_TEST_MESSAGE("Testing forward-forward variance, sticky strike "
                       "dynamics of DynamicBlackVolTermStructure...");

    TestData d;

    Handle<DynamicBlackVolTermStructure<tag::surface> > dyn(
        boost::make_shared<DynamicBlackVolTermStructure<tag::surface> >(
            d.refVol, 0, TARGET(), ForwardForwardVariance, StickyStrike, d.riskfreeTs, d.dividendTs, d.spot_q));

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.80), d.refVol->blackVol(0.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.90), d.refVol->blackVol(0.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.95), d.refVol->blackVol(0.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 1.15), d.refVol->blackVol(0.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackVol(1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackVol(1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackVol(1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackVol(1.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.80), d.refVol->blackVol(5.0, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.90), d.refVol->blackVol(5.0, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.95), d.refVol->blackVol(5.0, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 1.15), d.refVol->blackVol(5.0, 1.15), tol);

    // move forward in time, in time direction we expect to roll
    // down the curve (i.e. forward vols from the original vol ts
    // are realized), with the strike range held constant

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 6 * Months);
    Real t = d.refVol->timeFromReference(Settings::instance().evaluationDate());
    d.spot->setValue(0.9);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackForwardVol(t, t + 1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackForwardVol(t, t + 1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackForwardVol(t, t + 1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackForwardVol(t, t + 1.5, 1.15), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 18 * Months);
    t = d.refVol->timeFromReference(Settings::instance().evaluationDate());
    d.rate->setValue(0.01);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackForwardVol(t, t + 1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackForwardVol(t, t + 1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackForwardVol(t, t + 1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackForwardVol(t, t + 1.5, 1.15), tol);

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 3 * Years);
    t = d.refVol->timeFromReference(Settings::instance().evaluationDate());
    d.div->setValue(0.03);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackForwardVol(t, t + 1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackForwardVol(t, t + 1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackForwardVol(t, t + 1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackForwardVol(t, t + 1.5, 1.15), tol);

} // testForwardVarianceStickyLogMoneyness

void DynamicBlackVolTermStructureTest::testForwardVarianceStickyLogMoneyness() {

    BOOST_TEST_MESSAGE("Testing forward-forward variance, sticky log-moneyness "
                       "dynamics of DynamicBlackVolTermStructure...");

    TestData d;

    Handle<DynamicBlackVolTermStructure<tag::surface> > dyn(
        boost::make_shared<DynamicBlackVolTermStructure<tag::surface> >(
            d.refVol, 0, TARGET(), ForwardForwardVariance, StickyLogMoneyness, d.riskfreeTs, d.dividendTs, d.spot_q));

    dyn->enableExtrapolation();

    Real tol = 1.0E-8;

    // initially we should get the same volatilities

    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.80), d.refVol->blackVol(0.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.90), d.refVol->blackVol(0.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 0.95), d.refVol->blackVol(0.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(0.5, 1.15), d.refVol->blackVol(0.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.80), d.refVol->blackVol(1.5, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.90), d.refVol->blackVol(1.5, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 0.95), d.refVol->blackVol(1.5, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(1.5, 1.15), d.refVol->blackVol(1.5, 1.15), tol);

    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.80), d.refVol->blackVol(5.0, 0.80), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.90), d.refVol->blackVol(5.0, 0.90), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 0.95), d.refVol->blackVol(5.0, 0.95), tol);
    BOOST_CHECK_CLOSE(dyn->blackVol(5.0, 1.15), d.refVol->blackVol(5.0, 1.15), tol);

    // move forward in time, in time direction we expect to roll down
    // the curve with the forward variance being taken at strikes
    // constant in moneyness

    Settings::instance().evaluationDate() = TARGET().advance(d.origRefDate, 18 * Months);
    Real t = d.refVol->timeFromReference(Settings::instance().evaluationDate());
    Real atm0 = d.spot->value(); // original atm value
    d.spot->setValue(0.9);
    Real atm = d.spot->value(); // new atm value

    BOOST_CHECK_CLOSE(dyn->blackVariance(1.5, atm * std::exp(-0.25)),
                      d.refVol->blackVariance(t + 1.5, atm0 * std::exp(-0.25)) -
                          d.refVol->blackVariance(t, atm0 * std::exp(-0.25)),
                      tol);
    BOOST_CHECK_CLOSE(
        dyn->blackVariance(1.5, atm * std::exp(0.0)),
        d.refVol->blackVariance(t + 1.5, atm0 * std::exp(0.0)) - d.refVol->blackVariance(t, atm0 * std::exp(0.0)), tol);
    BOOST_CHECK_CLOSE(dyn->blackVariance(1.5, atm * std::exp(0.25)),
                      d.refVol->blackVariance(t + 1.5, atm0 * std::exp(0.25)) -
                          d.refVol->blackVariance(t, atm0 * std::exp(0.25)),
                      tol);

} // testForwardVarianceStickyLogMoneyness

test_suite* DynamicBlackVolTermStructureTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("DynamicBlackVolTermStructure tests");
    suite->add(BOOST_TEST_CASE(&DynamicBlackVolTermStructureTest::testConstantVarianceStickyStrike));
    suite->add(BOOST_TEST_CASE(&DynamicBlackVolTermStructureTest::testConstantVarianceStickyLogMoneyness));
    suite->add(BOOST_TEST_CASE(&DynamicBlackVolTermStructureTest::testForwardVarianceStickyStrike));
    suite->add(BOOST_TEST_CASE(&DynamicBlackVolTermStructureTest::testForwardVarianceStickyLogMoneyness));
    return suite;
}
} // namespace testsuite
