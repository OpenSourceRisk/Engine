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

#include <test/equitytrades.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;

// Equity option/forward test, example from Haug, Chapter 1

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(3, Feb, 2016);

        // build discount
        discountCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateYts(0.1);
        discountCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateYts(0.075);

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);

        // add equity spots
        equitySpots_[make_pair(Market::defaultConfiguration, "zzzCorp")] = Handle<Quote>(boost::make_shared<SimpleQuote>(100));

        // add dividend yield
        equityDividendCurves_[make_pair(Market::defaultConfiguration, "zzzCorp")] = flatRateYts(0.05);

        // add dividend yield
        equityInterestRateCurves_[make_pair(Market::defaultConfiguration, "zzzCorp")] = flatRateYts(0.1);

        // build equity vols
        equityVols_[make_pair(Market::defaultConfiguration, "zzzCorp")] = flatRateFxv(0.20);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, ActualActual()));
        return Handle<BlackVolTermStructure>(fxv);
    }
};
}

namespace testsuite {

void EquityTradesTest::testEquityTradePrices() {
    BOOST_TEST_MESSAGE("Testing EquityOption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    // build FXOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData putData("Short", "Put", "European", true, vector<string>(1, exp_str));
    Envelope env("CP1");
    EquityOption eqCall(env, callData, "zzzCorp", "EUR", 95.0, 1.0);
    EquityOption eqPut(env, putData, "zzzCorp", "EUR", 95.0, 1.0);
    EquityForward eqFwd(env, "Long", "zzzCorp", "EUR", 1.0, exp_str, 95.0);

    Real expectedNPV_Put = -2.4648; // negative for sold option

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    engineData->model("EquityForward") = "DiscountedCashflows";
    engineData->engine("EquityForward") = "DiscountingEquityForwardEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<EquityOptionEngineBuilder>());
    engineFactory->registerBuilder(boost::make_shared<EquityForwardEngineBuilder>());

    eqCall.build(engineFactory);
    eqPut.build(engineFactory);
    eqFwd.build(engineFactory);

    Real npv_call = eqCall.instrument()->NPV();
    Real npv_put = eqPut.instrument()->NPV();
    Real npv_fwd = eqFwd.instrument()->NPV();

    Real put_call_sum = npv_call + npv_put;

    BOOST_CHECK_CLOSE(expectedNPV_Put, npv_put, 0.001);
    BOOST_CHECK_CLOSE(npv_fwd, put_call_sum, 0.001); // put-call parity check

    Settings::instance().evaluationDate() = today; // reset
}

test_suite* EquityTradesTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("EquityTradesTest");
    suite->add(BOOST_TEST_CASE(&EquityTradesTest::testEquityTradePrices));
    return suite;
}
}
