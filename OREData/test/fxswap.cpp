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
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>

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
    TestMarket() : MarketImpl(false) {
        asof_ = Date(3, Feb, 2015);

        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

        // add conventions
        QuantLib::ext::shared_ptr<ore::data::Convention> usdChfConv(
            new ore::data::FXConvention("USD-CHF-FX", "0", "USD", "CHF", "10000", "USD,CHF"));
        QuantLib::ext::shared_ptr<ore::data::Convention> usdGbpConv(
            new ore::data::FXConvention("USD-GBP-FX", "0", "USD", "GBP", "10000", "USD,GBP"));
        QuantLib::ext::shared_ptr<ore::data::Convention> usdEurConv(
            new ore::data::FXConvention("USD-EUR-FX", "0", "USD", "EUR", "10000", "USD,EUR"));

        conventions->add(usdChfConv);
        conventions->add(usdGbpConv);
        conventions->add(usdEurConv);
        InstrumentConventions::instance().setConventions(conventions);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "CHF")] = flatRateYts(0.04);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] = flatRateYts(0.05);

        // add fx rates
	std::map<std::string, Handle<Quote>> quotes;
	quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
	quotes["EURGBP"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.4));
	quotes["EURCHF"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.3));
	fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);
        fxVols_[make_pair(Market::defaultConfiguration, "EURCHF")] = flatRateFxv(0.20);
        fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.20);
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

namespace {

void test(string nearDate, string farDate, string nearBoughtCurrency, double nearBoughtAmount, string nearSoldCurrency,
          double nearSoldAmount, double farBoughtAmount, double farSoldAmount, const QuantLib::ext::shared_ptr<Market>& market) {

    // build market
    Settings::instance().evaluationDate() = market->asofDate();
    // fxswap's npv should equal that of two separate fxforwards
    // build first fxforward
    Envelope env1("FxForward1");

    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("FxForward") = "DiscountedCashflows";
    engineData->engine("FxForward") = "DiscountingFxForwardEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
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

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FXSwapTests)

BOOST_AUTO_TEST_CASE(testFXSwap) {

    BOOST_TEST_MESSAGE("Testing FXSwap...");

    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();

    string nearDate = "2015-10-27";
    string farDate = "2015-11-03";
    string nearBoughtCurrency = "EUR";
    Real nearBoughtAmount = 224557621.490000;
    string nearSoldCurrency = "USD";
    Real nearSoldAmount = 250000000.000000;
    Real farBoughtAmount = 250018000.00;
    Real farSoldAmount = 224552207.77;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount, market);

    nearDate = "2015-07-14";
    farDate = "2015-11-16";
    nearBoughtCurrency = "CHF";
    nearBoughtAmount = 97000000;
    nearSoldCurrency = "USD";
    nearSoldAmount = 103718911.06;
    farBoughtAmount = 103923787.150000;
    farSoldAmount = 96737000.000000;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount, market);

    nearDate = "2015-08-04";
    farDate = "2015-11-30";
    nearBoughtCurrency = "GBP";
    nearBoughtAmount = 100227439.19;
    nearSoldCurrency = "USD";
    nearSoldAmount = 156000000;
    farBoughtAmount = 156148000.000000;
    farSoldAmount = 100400372.110000;

    test(nearDate, farDate, nearBoughtCurrency, nearBoughtAmount, nearSoldCurrency, nearSoldAmount, farBoughtAmount,
         farSoldAmount, market);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
