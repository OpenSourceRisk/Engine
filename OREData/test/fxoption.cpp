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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.025);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
        fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

        // add fx conventions
        auto conventions = QuantLib::ext::make_shared<Conventions>();
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
        InstrumentConventions::instance().setConventions(conventions);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);
    }
    TestMarket(Real spot, Real q, Real r, Real vol, bool withFixings = false) : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            flatRateYts(r, Actual360());
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] =
            flatRateYts(q, Actual360());

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        quotes["JPYEUR"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot));
        fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

        // add fx conventions
        auto conventions = QuantLib::ext::make_shared<Conventions>();
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));
        InstrumentConventions::instance().setConventions(conventions);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "JPYEUR")] = flatRateFxv(vol, Actual360());

        if (withFixings) {
            // add fixings
            TimeSeries<Real> pastFixings;
            pastFixings[Date(1, Feb, 2016)] = 100;
            pastFixings[Date(2, Feb, 2016)] = 90;
            IndexManager::instance().setHistory("FX/Reuters JPY/EUR", pastFixings);
        }
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward, const DayCounter& dc = ActualActual(ActualActual::ISDA)) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, dc));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward, const DayCounter& dc = ActualActual(ActualActual::ISDA)) {
        QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, dc));
        return Handle<BlackVolTermStructure>(fxv);
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FXOptionTests)

// FX Option test, example from Wystup, section 1.2.6, page 28
BOOST_AUTO_TEST_CASE(testFXOptionPrice) {

    BOOST_TEST_MESSAGE("Testing FXOption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build FXOption - expiry in 1 Year
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20170203"));
    OptionData optionDataPremiumUSD("Long", "Call", "European", true, vector<string>(1, "20170203"), "Cash", "",
                                    PremiumData(10000.0, "USD", Date(3, Feb, 2017)));
    OptionData optionDataPremiumEUR("Long", "Call", "European", true, vector<string>(1, "20170203"), "Cash", "",
                                    PremiumData(10000.0, "EUR", Date(3, Feb, 2017)));
    Envelope env("CP1");
    FxOption fxOption(env, optionData, "EUR", 1000000, // bought
                      "USD", 1250000);                 // sold
    FxOption fxOptionPremiumUSD(env, optionDataPremiumUSD, "EUR", 1000000, "USD", 1250000);
    FxOption fxOptionPremiumEUR(env, optionDataPremiumEUR, "EUR", 1000000, "USD", 1250000);

    // NPV currency = sold currency = USD

    Real expectedNPV_USD = 29148.0;
    Real expectedNPV_EUR = 24290.0;
    Real expectedNPV_USD_Premium_USD = 19495.6;
    Real expectedNPV_USD_Premium_EUR = 17496.4;

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("FxOption") = "GarmanKohlhagen";
    engineData->engine("FxOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    fxOption.build(engineFactory);
    fxOptionPremiumUSD.build(engineFactory);
    fxOptionPremiumEUR.build(engineFactory);

    Real npv = fxOption.instrument()->NPV();
    Real npv_prem_usd = fxOptionPremiumUSD.instrument()->NPV();
    Real npv_prem_eur = fxOptionPremiumEUR.instrument()->NPV();

    BOOST_TEST_MESSAGE("FX Option, NPV Currency " << fxOption.npvCurrency());
    BOOST_TEST_MESSAGE("NPV =                     " << npv);
    BOOST_TEST_MESSAGE("NPV with premium in USD = " << npv_prem_usd);
    BOOST_TEST_MESSAGE("NPV with premium in EUR = " << npv_prem_eur);

    // Check NPV matches expected values. Expected value from Wystup is rounded at each step of
    // calculation so we get a difference of $50 here.
    QL_REQUIRE(fxOption.npvCurrency() == "USD", "unexpected NPV currency");
    if (fxOption.npvCurrency() == "EUR") {
        BOOST_CHECK_CLOSE(npv, expectedNPV_EUR, 0.2);
    } else if (fxOption.npvCurrency() == "USD") {
        BOOST_CHECK_CLOSE(npv, expectedNPV_USD, 0.2);
        BOOST_CHECK_CLOSE(npv_prem_usd, expectedNPV_USD_Premium_USD, 0.001);
        BOOST_CHECK_CLOSE(npv_prem_eur, expectedNPV_USD_Premium_EUR, 0.001);
    } else {
        BOOST_FAIL("Unexpected FX Option npv currency " << fxOption.npvCurrency());
    }

    Settings::instance().evaluationDate() = today; // reset
}

namespace {

struct AmericanOptionData {
    Option::Type type;
    Real strike;
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Time t;       // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
};

} // namespace

BOOST_AUTO_TEST_CASE(testFXAmericanOptionPrice) {

    BOOST_TEST_MESSAGE("Testing FXAmericanOption Price...");

    AmericanOptionData fxd[] = {//        type, strike,   spot,    q,    r,    t,  vol,   value
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.10, 0.15, 0.0206},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.10, 0.15, 1.8771},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.10, 0.15, 10.0089},
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.10, 0.25, 0.3159},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.10, 0.25, 3.1280},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.10, 0.25, 10.3919},
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.10, 0.35, 0.9495},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.10, 0.35, 4.3777},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.10, 0.35, 11.1679},
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.50, 0.15, 0.8208},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.50, 0.15, 4.0842},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.50, 0.15, 10.8087},
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.50, 0.25, 2.7437},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.50, 0.25, 6.8015},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.50, 0.25, 13.0170},
                                {Option::Call, 100.00, 90.00, 0.10, 0.10, 0.50, 0.35, 5.0063},
                                {Option::Call, 100.00, 100.00, 0.10, 0.10, 0.50, 0.35, 9.5106},
                                {Option::Call, 100.00, 110.00, 0.10, 0.10, 0.50, 0.35, 15.5689},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.10, 0.15, 10.0000},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.10, 0.15, 1.8770},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.10, 0.15, 0.0410},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.10, 0.25, 10.2533},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.10, 0.25, 3.1277},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.10, 0.25, 0.4562},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.10, 0.35, 10.8787},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.10, 0.35, 4.3777},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.10, 0.35, 1.2402},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.50, 0.15, 10.5595},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.50, 0.15, 4.0842},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.50, 0.15, 1.0822},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.50, 0.25, 12.4419},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.50, 0.25, 6.8014},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.50, 0.25, 3.3226},
                                {Option::Put, 100.00, 90.00, 0.10, 0.10, 0.50, 0.35, 14.6945},
                                {Option::Put, 100.00, 100.00, 0.10, 0.10, 0.50, 0.35, 9.5104},
                                {Option::Put, 100.00, 110.00, 0.10, 0.10, 0.50, 0.35, 5.8823},
                                {Option::Put, 100.00, 100.00, 0.00, 0.00, 0.50, 0.15, 4.2294}};

    for (auto& f : fxd) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXAmericanOption
        string maturityDate = to_string(market->asofDate() + Integer(f.t * 360 + 0.5));
        OptionData optionData("Long", f.type == Option::Call ? "Call" : "Put", "American", false,
                              vector<string>(1, maturityDate));
        Envelope env("CP1");
        FxOption fxOption(env, optionData, "JPY", 1, // foreign
                          "EUR", f.strike);          // domestic

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxOptionAmerican") = "GarmanKohlhagen";
        engineData->engine("FxOptionAmerican") = "BaroneAdesiWhaleyApproximationEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxOption.build(engineFactory);

        Real npv = fxOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("FX American Option, NPV Currency " << fxOption.npvCurrency());
        BOOST_TEST_MESSAGE("NPV =                     " << npv);

        // Check NPV matches expected values.
        QL_REQUIRE(fxOption.npvCurrency() == "EUR", "unexpected NPV currency ");

        BOOST_CHECK_CLOSE(npv, expectedNPV, 0.2);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFdValues) {

    BOOST_TEST_MESSAGE("Testing finite-difference engine "
                       "for American options...");

    /* The data below are from
       An Approximate Formula for Pricing American Options
       Journal of Derivatives Winter 1999
       Ju, N.
    */
    AmericanOptionData juValues[] = {//        type, strike,   spot,    q,    r,    t,     vol,   value, tol
                                     // These values are from Exhibit 3 - Short dated Put Options
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.0833, 0.2, 0.006},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.3333, 0.2, 0.201},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.5833, 0.2, 0.433},

                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.0833, 0.2, 0.851},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.3333, 0.2, 1.576},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.5833, 0.2, 1.984},

                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.0833, 0.2, 5.000},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.3333, 0.2, 5.084},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.5833, 0.2, 5.260},

                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.0833, 0.3, 0.078},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.3333, 0.3, 0.697},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.5833, 0.3, 1.218},

                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.0833, 0.3, 1.309},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.3333, 0.3, 2.477},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.5833, 0.3, 3.161},

                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.0833, 0.3, 5.059},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.3333, 0.3, 5.699},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.5833, 0.3, 6.231},

                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.0833, 0.4, 0.247},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.3333, 0.4, 1.344},
                                     {Option::Put, 35.00, 40.00, 0.0, 0.0488, 0.5833, 0.4, 2.150},

                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.0833, 0.4, 1.767},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.3333, 0.4, 3.381},
                                     {Option::Put, 40.00, 40.00, 0.0, 0.0488, 0.5833, 0.4, 4.342},

                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.0833, 0.4, 5.288},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.3333, 0.4, 6.501},
                                     {Option::Put, 45.00, 40.00, 0.0, 0.0488, 0.5833, 0.4, 7.367},

                                     // Type in Exhibits 4 and 5 if you have some spare time ;-)

                                     //        type, strike,   spot,    q,    r,    t,     vol,   value, tol
                                     // values from Exhibit 6 - Long dated Call Options with dividends
                                     {Option::Call, 100.00, 80.00, 0.07, 0.03, 3.0, 0.2, 2.605},
                                     {Option::Call, 100.00, 90.00, 0.07, 0.03, 3.0, 0.2, 5.182},
                                     {Option::Call, 100.00, 100.00, 0.07, 0.03, 3.0, 0.2, 9.065},
                                     {Option::Call, 100.00, 110.00, 0.07, 0.03, 3.0, 0.2, 14.430},
                                     {Option::Call, 100.00, 120.00, 0.07, 0.03, 3.0, 0.2, 21.398},

                                     {Option::Call, 100.00, 80.00, 0.07, 0.03, 3.0, 0.4, 11.336},
                                     {Option::Call, 100.00, 90.00, 0.07, 0.03, 3.0, 0.4, 15.711},
                                     {Option::Call, 100.00, 100.00, 0.07, 0.03, 3.0, 0.4, 20.760},
                                     {Option::Call, 100.00, 110.00, 0.07, 0.03, 3.0, 0.4, 26.440},
                                     {Option::Call, 100.00, 120.00, 0.07, 0.03, 3.0, 0.4, 32.709},

                                     {Option::Call, 100.00, 80.00, 0.07, 0.00001, 3.0, 0.3, 5.552},
                                     {Option::Call, 100.00, 90.00, 0.07, 0.00001, 3.0, 0.3, 8.868},
                                     {Option::Call, 100.00, 100.00, 0.07, 0.00001, 3.0, 0.3, 13.158},
                                     {Option::Call, 100.00, 110.00, 0.07, 0.00001, 3.0, 0.3, 18.458},
                                     {Option::Call, 100.00, 120.00, 0.07, 0.00001, 3.0, 0.3, 24.786},

                                     {Option::Call, 100.00, 80.00, 0.03, 0.07, 3.0, 0.3, 12.177},
                                     {Option::Call, 100.00, 90.00, 0.03, 0.07, 3.0, 0.3, 17.411},
                                     {Option::Call, 100.00, 100.00, 0.03, 0.07, 3.0, 0.3, 23.402},
                                     {Option::Call, 100.00, 110.00, 0.03, 0.07, 3.0, 0.3, 30.028},
                                     {Option::Call, 100.00, 120.00, 0.03, 0.07, 3.0, 0.3, 37.177}};

    Real tolerance = 8.0e-2;

    for (auto& f : juValues) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXAmericanOption
        string maturityDate = to_string(market->asofDate() + Integer(f.t * 360 + 0.5));
        OptionData optionData("Long", f.type == Option::Call ? "Call" : "Put", "American", false,
                              vector<string>(1, maturityDate));
        Envelope env("CP1");
        FxOption fxOption(env, optionData, "JPY", 1, // foreign
                          "EUR", f.strike);          // domestic

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxOptionAmerican") = "GarmanKohlhagen";
        engineData->engine("FxOptionAmerican") = "FdBlackScholesVanillaEngine";
        engineData->engineParameters("FxOptionAmerican") = {
            {"Scheme", "Douglas"}, {"TimeGridPerYear", "100"}, {"XGrid", "100"}, {"DampingSteps", "0"}};

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxOption.build(engineFactory);

        Real npv = fxOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("FX American Option, NPV Currency " << fxOption.npvCurrency());
        BOOST_TEST_MESSAGE("NPV =                     " << npv);

        // Check NPV matches expected values.
        QL_REQUIRE(fxOption.npvCurrency() == "EUR", "unexpected NPV currency ");

        BOOST_CHECK_SMALL(npv - expectedNPV, tolerance);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
