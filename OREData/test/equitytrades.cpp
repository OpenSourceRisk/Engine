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

#include <boost/test/unit_test.hpp>
#include <oret/toplevelfixture.hpp>
#include <boost/make_shared.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.1);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.075);

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);

        // add equity spots
        equitySpots_[make_pair(Market::defaultConfiguration, "zzzCorp")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(100));

        // add dividend yield
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "zzzCorp")] =
            flatRateYts(0.05);

        // add forecast curve
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityForecast, "zzzCorp")] =
            flatRateYts(0.1);

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
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(EquityTradesTests)

// Equity option/forward test, example from Haug, Chapter 1
BOOST_AUTO_TEST_CASE(testEquityTradePrices) {

    BOOST_TEST_MESSAGE("Testing EquityOption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData callDataPremium("Long", "Call", "European", true, vector<string>(1, exp_str), "Cash", 1.0, "EUR",
                               exp_str);
    OptionData putData("Short", "Put", "European", true, vector<string>(1, exp_str));
    OptionData putDataPremium("Short", "Put", "European", true, vector<string>(1, exp_str), "Cash", 1.0, "EUR",
                              exp_str);
    Envelope env("CP1");
    EquityOption eqCall(env, callData, "zzzCorp", "EUR", 95.0, 1.0);
    EquityOption eqCallPremium(env, callDataPremium, "zzzCorp", "EUR", 95.0, 1.0);
    EquityOption eqPut(env, putData, "zzzCorp", "EUR", 95.0, 1.0);
    EquityOption eqPutPremium(env, putDataPremium, "zzzCorp", "EUR", 95.0, 1.0);
    EquityForward eqFwd(env, "Long", "zzzCorp", "EUR", 1.0, exp_str, 95.0);

    Real expectedNPV_Put = -2.4648;           // negative for sold option
    Real expectedNPV_Put_Premium = -1.513558; // less negative due to received premium of 1 EUR at expiry

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
    eqCallPremium.build(engineFactory);
    eqPut.build(engineFactory);
    eqPutPremium.build(engineFactory);
    eqFwd.build(engineFactory);

    Real npv_call = eqCall.instrument()->NPV();
    Real npv_call_premium = eqCallPremium.instrument()->NPV();
    Real npv_put = eqPut.instrument()->NPV();
    Real npv_put_premium = eqPutPremium.instrument()->NPV();
    Real npv_fwd = eqFwd.instrument()->NPV();

    Real put_call_sum = npv_call + npv_put;
    Real put_call_premium_sum = npv_call_premium + npv_put_premium;

    BOOST_CHECK_CLOSE(expectedNPV_Put, npv_put, 0.001);
    BOOST_CHECK_CLOSE(expectedNPV_Put_Premium, npv_put_premium, 0.001);
    BOOST_CHECK_CLOSE(npv_fwd, put_call_sum, 0.001);         // put-call parity check
    BOOST_CHECK_CLOSE(npv_fwd, put_call_premium_sum, 0.001); // put-call parity check

    Settings::instance().evaluationDate() = today; // reset
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
