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
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/equityfuturesoption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.1);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.075);

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
        fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);

        // add equity spots
        equitySpots_[make_pair(Market::defaultConfiguration, "zzzCorp")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100));

        // add dividend yield
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "zzzCorp")] =
            flatRateYts(0.05);

        // add equity curve
        equityCurves_[make_pair(Market::defaultConfiguration, "zzzCorp")] =
            Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(
                "zzzCorp", TARGET(), parseCurrency("EUR"), equitySpot("zzzCorp"),
                yieldCurve(YieldCurveType::Discount, "EUR"), yieldCurve(YieldCurveType::EquityDividend, "zzzCorp")));

        // build equity vols
        equityVols_[make_pair(Market::defaultConfiguration, "zzzCorp")] = flatRateFxv(0.20);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
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
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData callDataPremium("Long", "Call", "European", true, vector<string>(1, exp_str), "Cash", "",
                               PremiumData{1.0, "EUR", expiry});
    OptionData putData("Short", "Put", "European", true, vector<string>(1, exp_str));
    OptionData putDataPremium("Short", "Put", "European", true, vector<string>(1, exp_str), "Cash", "",
                              PremiumData{1.0, "EUR", expiry});
    Envelope env("CP1");
    TradeStrike tradeStrike(95.0, "EUR");
    EquityOption eqCall(env, callData, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqCallPremium(env, callDataPremium, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqPut(env, putData, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqPutPremium(env, putDataPremium, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    ore::data::EquityForward eqFwd(env, "Long", EquityUnderlying("zzzCorp"), "EUR", 1.0, exp_str, 95.0);

    Real expectedNPV_Put = -2.4648;           // negative for sold option
    Real expectedNPV_Put_Premium = -1.513558; // less negative due to received premium of 1 EUR at expiry

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    engineData->model("EquityForward") = "DiscountedCashflows";
    engineData->engine("EquityForward") = "DiscountingEquityForwardEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

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

// when futureExpiryDate == optionExpiryDate then the trade should behave like an EquityOption
BOOST_AUTO_TEST_CASE(testEquityFutureOptionPrices) {

    BOOST_TEST_MESSAGE("Testing EquityFutureOption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying = QuantLib::ext::make_shared<ore::data::EquityUnderlying>("zzzCorp");
    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData callDataPremium("Long", "Call", "European", true, vector<string>(1, exp_str), "Physical", "",
                               PremiumData(1.0, "EUR", expiry));
    OptionData putData("Short", "Put", "European", true, vector<string>(1, exp_str));
    OptionData putDataPremium("Short", "Put", "European", true, vector<string>(1, exp_str), "Physical", "",
                              PremiumData(1.0, "EUR", expiry));
    Envelope env("CP1");
    TradeStrike strike(TradeStrike::Type::Price, 95.0);
    EquityFutureOption eqFwdCall(env, callData, "EUR", 1.0, underlying, strike, expiry);
    EquityFutureOption eqFwdCallPremium(env, callDataPremium, "EUR", 1.0, underlying, strike, expiry);
    EquityFutureOption eqFwdPut(env, putData, "EUR", 1.0, underlying, strike, expiry);
    EquityFutureOption eqFwdPutPremium(env, putDataPremium, "EUR", 1.0, underlying, strike, expiry);

    TradeStrike tradeStrike(95.0, "EUR");
    EquityOption eqCall(env, callData, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqCallPremium(env, callDataPremium, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqPut(env, putData, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    EquityOption eqPutPremium(env, putDataPremium, EquityUnderlying("zzzCorp"), "EUR", 1.0, tradeStrike);
    
    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    engineData->model("EquityFutureOption") = "BlackScholes";
    engineData->engine("EquityFutureOption") = "AnalyticEuropeanForwardEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    eqFwdCall.build(engineFactory);
    eqFwdCallPremium.build(engineFactory);
    eqFwdPut.build(engineFactory);
    eqFwdPutPremium.build(engineFactory);
    
    eqCall.build(engineFactory);
    eqCallPremium.build(engineFactory);
    eqPut.build(engineFactory);
    eqPutPremium.build(engineFactory);

    BOOST_CHECK_CLOSE(eqCall.instrument()->NPV(), eqFwdCall.instrument()->NPV(), 0.001);
    BOOST_CHECK_CLOSE(eqCallPremium.instrument()->NPV(), eqFwdCallPremium.instrument()->NPV(), 0.001);
    BOOST_CHECK_CLOSE(eqPut.instrument()->NPV(), eqFwdPut.instrument()->NPV(), 0.001);
    BOOST_CHECK_CLOSE(eqPutPremium.instrument()->NPV(), eqFwdPutPremium.instrument()->NPV(), 0.001);

    Settings::instance().evaluationDate() = today; // reset
}

BOOST_AUTO_TEST_CASE(testEquityFutureParity) {

    BOOST_TEST_MESSAGE("Testing EquityFutureOption Put-Call parity...");

    Date today = Settings::instance().evaluationDate();

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    Date futureExpiry = market->asofDate() + 12 * Months + 1 * Days;
    ostringstream o_2;
    o_2 << QuantLib::io::iso_date(futureExpiry);
    string f_exp_str = o_2.str();
    
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying = QuantLib::ext::make_shared<ore::data::EquityUnderlying>("zzzCorp");
    double spot = 100;
    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData callDataPremium("Long", "Call", "European", true, vector<string>(1, exp_str), "Physical", "",
                               PremiumData(1.0, "EUR", expiry));
    OptionData putData("Long", "Put", "European", true, vector<string>(1, exp_str));
    OptionData putDataPremium("Long", "Put", "European", true, vector<string>(1, exp_str), "Physical", "",
                              PremiumData(1.0, "EUR", expiry));
    Envelope env("CP1");
    TradeStrike strike(TradeStrike::Type::Price, 95.0);
    EquityFutureOption eqCall(env, callData, "EUR", 1.0, underlying, strike, futureExpiry);
    EquityFutureOption eqCallPremium(env, callDataPremium, "EUR", 1.0, underlying, strike, futureExpiry);
    EquityFutureOption eqPut(env, putData, "EUR", 1.0, underlying, strike, futureExpiry);
    EquityFutureOption eqPutPremium(env, putDataPremium, "EUR", 1.0, underlying, strike, futureExpiry);
    ore::data::EquityForward eqFwd(env, "Long", EquityUnderlying("zzzCorp"), "EUR", 1.0, f_exp_str, 0);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EquityFutureOption") = "BlackScholes";
    engineData->engine("EquityFutureOption") = "AnalyticEuropeanForwardEngine";
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    engineData->model("EquityForward") = "DiscountedCashflows";
    engineData->engine("EquityForward") = "DiscountingEquityForwardEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    eqCall.build(engineFactory);
    eqCallPremium.build(engineFactory);
    eqPut.build(engineFactory);
    eqPutPremium.build(engineFactory);
    eqFwd.build(engineFactory);

    Handle<YieldTermStructure> discountCurve =
            market->discountCurve("EUR", Market::defaultConfiguration);

    Handle<YieldTermStructure> dividend = market->equityDividendCurve("zzzCorp", Market::defaultConfiguration);
    Handle<YieldTermStructure> forecast = market->equityForecastCurve("zzzCorp", Market::defaultConfiguration);

    Real npv_call = eqCall.instrument()->NPV();
    Real npv_call_premium = eqCallPremium.instrument()->NPV();
    Real npv_put = eqPut.instrument()->NPV();
    Real npv_put_premium = eqPutPremium.instrument()->NPV();

    Real npv_fwd = spot * dividend->discount(futureExpiry) / forecast->discount(futureExpiry);;

    Real put_sum = npv_put + ( npv_fwd - strike.value() ) * discountCurve->discount(expiry);
    Real put_premium_sum = npv_put_premium + ( npv_fwd - strike.value() ) * discountCurve->discount(expiry);

    BOOST_CHECK_CLOSE(npv_call, put_sum, 0.001);         // put-call parity check
    BOOST_CHECK_CLOSE(npv_call_premium, put_premium_sum, 0.001); // put-call parity check

    Settings::instance().evaluationDate() = today; // reset
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
