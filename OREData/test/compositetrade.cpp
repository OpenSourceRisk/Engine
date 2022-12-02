/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/compositetrade.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataPlusTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CompositeTradeTest)

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket(map<string, Handle<Quote>> fxRates = {{"EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2))}})
        : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.075);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.1);

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        for (auto& fxRate : fxRates) {
            quotes[fxRate.first] = fxRate.second;
        }
        fx_ = boost::make_shared<FXTriangulation>(quotes);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);

        // add equity spots
        equitySpots_[make_pair(Market::defaultConfiguration, "eurCorp")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(100));
        equitySpots_[make_pair(Market::defaultConfiguration, "usdCorp")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(100));

        // add dividend yield
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "eurCorp")] =
            flatRateYts(0.05);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "usdCorp")] =
            flatRateYts(0.05);

        // add equity curve
        equityCurves_[make_pair(Market::defaultConfiguration, "eurCorp")] =
            Handle<EquityIndex>(boost::make_shared<EquityIndex>(
                "eurCorp", TARGET(), parseCurrency("EUR"), equitySpot("eurCorp"),
                yieldCurve(YieldCurveType::Discount, "EUR"), yieldCurve(YieldCurveType::EquityDividend, "eurCorp")));
        equityCurves_[make_pair(Market::defaultConfiguration, "usdCorp")] =
            Handle<EquityIndex>(boost::make_shared<EquityIndex>(
                "usdCorp", TARGET(), parseCurrency("USD"), equitySpot("usdCorp"),
                yieldCurve(YieldCurveType::Discount, "USD"), yieldCurve(YieldCurveType::EquityDividend, "usdCorp")));

        // build equity vols
        equityVols_[make_pair(Market::defaultConfiguration, "eurCorp")] = flatRateFxv(0.20);
        equityVols_[make_pair(Market::defaultConfiguration, "usdCorp")] = flatRateFxv(0.20);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<BlackVolTermStructure>(fxv);
    }
};
} // namespace

// Synthetic Forward Test.. Per put-call parity a call - a put = a forward
BOOST_AUTO_TEST_CASE(testSyntheticForward) {
    BOOST_TEST_MESSAGE("Testing SyntheticForwardTrade...");

    SavedSettings backup;

    InstrumentConventions::instance().setConventions(boost::make_shared<Conventions>());
    
    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    OptionData putData("Short", "Put", "European", true, vector<string>(1, exp_str));
    Envelope env("CP1");
    TradeStrike tradeStrike(95.0, "EUR");
    boost::shared_ptr<Trade> eqCall =
        boost::make_shared<EquityOption>(env, callData, EquityUnderlying("eurCorp"), "EUR", 1.0, tradeStrike);
    eqCall->id() = "Long Call";
    boost::shared_ptr<Trade> eqPut =
        boost::make_shared<EquityOption>(env, putData, EquityUnderlying("eurCorp"), "EUR", 1.0, tradeStrike);
    eqPut->id() = "Short Put";
    CompositeTrade syntheticForward("EUR", {eqCall, eqPut}, "Mean", 0.0, env);
    syntheticForward.id() = "Synthetic Forward Test";
    ore::data::EquityForward eqFwd(env, "Long", EquityUnderlying("eurCorp"), "EUR", 1.0, exp_str, 95.0);

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    engineData->model("EquityForward") = "DiscountedCashflows";
    engineData->engine("EquityForward") = "DiscountingEquityForwardEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<EquityEuropeanOptionEngineBuilder>());
    engineFactory->registerBuilder(boost::make_shared<EquityForwardEngineBuilder>());

    syntheticForward.build(engineFactory);
    eqFwd.build(engineFactory);

    Real npv_composite = syntheticForward.instrument()->NPV();
    Real npv_fwd = eqFwd.instrument()->NPV();

    BOOST_CHECK_CLOSE(npv_composite, npv_fwd, 0.01);
    BOOST_CHECK_CLOSE(syntheticForward.notional(), eqFwd.notional(), 0.01);
}

// Simple combination of 2 options in different currencies
BOOST_AUTO_TEST_CASE(testMultiCcyComposite) {
    BOOST_TEST_MESSAGE("Testing SyntheticForwardTrade...");

    SavedSettings backup;

    InstrumentConventions::instance().setConventions(boost::make_shared<Conventions>());

    // build market
    boost::shared_ptr<SimpleQuote> eurusdRate(boost::make_shared<SimpleQuote>(1.2));
    map<string, Handle<Quote>> fxRates = {{"EURUSD", Handle<Quote>(eurusdRate)}};
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>(fxRates);
    Settings::instance().evaluationDate() = market->asofDate();
    Date expiry = market->asofDate() + 6 * Months + 1 * Days;
    ostringstream o;
    o << QuantLib::io::iso_date(expiry);
    string exp_str = o.str();

    // build EquityOption - expiry in 1 Year
    OptionData callData("Long", "Call", "European", true, vector<string>(1, exp_str));
    Envelope env("CP1");
    TradeStrike tradeStrike_EUR(95.0, "EUR");
    boost::shared_ptr<Trade> eurCall =
        boost::make_shared<EquityOption>(env, callData, EquityUnderlying("eurCorp"), "EUR", 1.0, tradeStrike_EUR);
    eurCall->id() = "EUR Call";
    TradeStrike tradeStrike_USD(95.0, "USD");
    boost::shared_ptr<Trade> usdCall =
        boost::make_shared<EquityOption>(env, callData, EquityUnderlying("usdCorp"), "USD", 1.0, tradeStrike_USD);
    usdCall->id() = "USD Call";
    CompositeTrade eurComp("EUR", {eurCall, usdCall}, "Sum", 0.0, env);
    CompositeTrade usdComp("USD", {eurCall, usdCall}, "Sum", 0.0, env);
    eurComp.id() = "EUR Combo Call Test";
    usdComp.id() = "USD Combo Call Test";

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<EquityEuropeanOptionEngineBuilder>());

    eurComp.build(engineFactory);
    usdComp.build(engineFactory);

    Real npv_eur_composite = eurComp.instrument()->NPV();
    Real npv_usd_composite = usdComp.instrument()->NPV();
    Real npv_eurCall = eurCall->instrument()->NPV();
    Real npv_usdCall = usdCall->instrument()->NPV();

    BOOST_CHECK_CLOSE(npv_eur_composite, npv_eurCall * 1.0 + npv_usdCall / 1.2, 0.01);
    BOOST_CHECK_CLOSE(npv_usd_composite, npv_eurCall * 1.2 + npv_usdCall / 1.0, 0.01);
    // Check that the notional is calulated correctly.
    BOOST_CHECK_CLOSE(usdComp.notional(), eurCall->notional() * 2.2, 0.01);

    // let's bump the fx to check that observation is working
    auto testMarket = boost::dynamic_pointer_cast<TestMarket>(market);
    eurusdRate->setValue(1.25);
    npv_usd_composite = usdComp.instrument()->NPV();
    BOOST_CHECK_CLOSE(npv_usd_composite, npv_eurCall * 1.25 + npv_usdCall / 1.0, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
