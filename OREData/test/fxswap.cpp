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

#include <boost/make_shared.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <test/fxswap.hpp>
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;
using namespace ore::data;

// FX Swap test
namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(3, Feb, 2015);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "CHF")] = flatRateYts(0.04);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] = flatRateYts(0.05);

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));
        fxSpots_[Market::defaultConfiguration].addQuote("EURCHF", Handle<Quote>(boost::make_shared<SimpleQuote>(1.3)));
        fxSpots_[Market::defaultConfiguration].addQuote("EURGBP", Handle<Quote>(boost::make_shared<SimpleQuote>(1.4)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);
        fxVols_[make_pair(Market::defaultConfiguration, "EURCHF")] = flatRateFxv(0.20);
        fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.20);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(4, NullCalendar(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, ActualActual()));
        return Handle<BlackVolTermStructure>(fxv);
    }
};
} // namespace

namespace {

void test(string nearDate, string farDate, string nearBoughtCurrency, double nearBoughtAmount, string nearSoldCurrency,
          double nearSoldAmount, double farBoughtAmount, double farSoldAmount) {

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    // fxswap's npv should equal that of two separate fxforwards
    // build first fxforward
    Envelope env1("FxForward1");

    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("FxForward") = "DiscountedCashflows";
    engineData->engine("FxForward") = "DiscountingFxForwardEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    // this trade has buyer and seller switched so that it returns it's npv in the same currency as the second forward
    // fxswap_npv= - fxfor1_npv + fxfor2_npv
    FxForward fxFor1(env1, nearDate, nearSoldCurrency, nearSoldAmount, nearBoughtCurrency, nearBoughtAmount);

    fxFor1.build(engineFactory);

    // build second fxforward
    Envelope env2("FxForward2");
    FxForward fxFor2(env2, farDate, nearSoldCurrency, farBoughtAmount, nearBoughtCurrency, farSoldAmount);
    fxFor2.build(engineFactory);

    // build fx swap

    Envelope env3("FxSwap");

    FxSwap fxswap(env3, nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount,
                  farBoughtAmount, farSoldAmount);
    fxswap.build(engineFactory);
    fxswap.instrument()->NPV();
    Real fx1 = fxFor1.instrument()->NPV();
    Real fx2 = fxFor2.instrument()->NPV();
    Real npvForward = fx2 - fx1;
    if (fxswap.instrument()->NPV() != npvForward)
        BOOST_FAIL("The FxSwap has NPV: " << fxswap.instrument()->NPV()
                                          << ", which does not equal the sum of two Fxforwards: " << npvForward);
}
} // namespace

namespace testsuite {

void FXSwapTest::testFXSwap() { BOOST_TEST_MESSAGE("Testing FXSwap..."); }

test_suite* FXSwapTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("FXSwapTest");
    string nearDate = "2015-10-27";
    string farDate = "2015-11-03";
    string nearBoughtCurrency = "EUR";
    Real nearBoughtAmount = 224557621.490000;
    string nearSoldCurrency = "USD";
    Real nearSoldAmount = 250000000.000000;
    Real farBoughtAmount = 250018000.00;
    Real farSoldAmount = 224552207.77;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount);

    nearDate = "2015-07-14";
    farDate = "2015-11-16";
    nearBoughtCurrency = "CHF";
    nearBoughtAmount = 97000000;
    nearSoldCurrency = "USD";
    nearSoldAmount = 103718911.06;
    farBoughtAmount = 103923787.150000;
    farSoldAmount = 96737000.000000;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount);

    nearDate = "2015-08-04";
    farDate = "2015-11-30";
    nearBoughtCurrency = "GBP";
    nearBoughtAmount = 100227439.19;
    nearSoldCurrency = "USD";
    nearSoldAmount = 156000000;
    farBoughtAmount = 156148000.000000;
    farSoldAmount = 100400372.110000;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount);

    suite->add(BOOST_TEST_CASE(&FXSwapTest::testFXSwap));
    return suite;
}
} // namespace testsuite
