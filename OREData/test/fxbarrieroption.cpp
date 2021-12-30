/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
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

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(01, March, 2021);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.025);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);
    }
    TestMarket(Real spot, Real q, Real r, Real vol, bool withFixings = false) {
        asof_ = Date(01, March, 2021);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            flatRateYts(r, Actual360());
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] =
            flatRateYts(q, Actual360());

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("JPYEUR", Handle<Quote>(boost::make_shared<SimpleQuote>(spot)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "JPYEUR")] = flatRateFxv(vol, Actual360());

        //if (withFixings) {
        //    // add fixings
        //    TimeSeries<Real> pastFixings;
        //    pastFixings[Date(1, Feb, 2016)] = 100;
        //    pastFixings[Date(2, Feb, 2016)] = 90;
        //    IndexManager::instance().setHistory("FX/Reuters JPY/EUR", pastFixings);
        //}
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward, const DayCounter& dc = ActualActual()) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, dc));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility vol, const DayCounter& dc = ActualActual()) {
        boost::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), vol, dc));
        return Handle<BlackVolTermStructure>(fxv);
    }
};

struct FxBarrierOptionData {
    // Non-changing parameters
    Option::Type type;
    Real s;           // spot
    Real k;           // rebate
    Real t;           // time to maturity
    Rate rf;          // domestic rate
    Rate rd;          // foreign rate (=b+r)
    // Custom parameters
    Barrier::Type bt; // barrier type
    Real x;           // strike
    Real h;           // barrier
    Volatility v;     // volatility
    Real result;      // expected result
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FxBarrierOptionTests)

// Standard FX Barrier Option test, examples from Haug, The Complete Guide to Option Pricing Formulas, 2007, 2nd ed, p. 154, Table 4-13
BOOST_AUTO_TEST_CASE(testStandardFxBarrierOptionPrice) {

    BOOST_TEST_MESSAGE("Testing Standard FxBarrierOption Price...");

    FxBarrierOptionData fxbd[] = {
        // Option type, strike, rebate, t, rf, rd, barrier type, strike, barrier, volatility, expected result
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 90, 95, 0.25, 9.0246},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 90, 95, 0.30, 8.8334},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 100, 95, 0.25, 6.7924},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 100, 95, 0.30, 7.0285},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 110, 95, 0.25, 4.8759},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 110, 95, 0.30, 5.4137},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 90, 100, 0.25, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 90, 100, 0.30, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 100, 100, 0.25, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 100, 100, 0.30, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 110, 100, 0.25, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownOut, 110, 100, 0.30, 3.0000},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 90, 105, 0.25, 2.6789},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 90, 105, 0.30, 2.6341},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 100, 105, 0.25, 2.3580},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 100, 105, 0.30, 2.4389},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 110, 105, 0.25, 2.3453},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpOut, 110, 105, 0.30, 2.4315},
        // ---
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 90, 95, 0.25, 7.7627},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 90, 95, 0.30, 9.0093},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 100, 95, 0.25, 4.0109},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 100, 95, 0.30, 5.1370},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 110, 95, 0.25, 2.0576},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 110, 95, 0.30, 2.8517},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 90, 100, 0.25, 13.8333},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 90, 100, 0.30, 14.8816},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 100, 100, 0.25, 7.8494},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 100, 100, 0.30, 9.2045},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 110, 100, 0.25, 3.9795},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::DownIn, 110, 100, 0.30, 5.3043},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 90, 105, 0.25, 14.1112},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 90, 105, 0.30, 15.2098},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 100, 105, 0.25, 8.4482},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 100, 105, 0.30, 9.7278},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 110, 105, 0.25, 4.5910},
        {Option::Call, 100, 3, 0.5, 0.08, 0.04 + 0.08, Barrier::UpIn, 110, 105, 0.30, 5.8350},
        // ---
    };

    Date asof = Date(01, Feb, 2021);
    Date today = Settings::instance().evaluationDate();
    Envelope env("CP1");
    boost::shared_ptr<EngineFactory> engineFactory;
    boost::shared_ptr<Market> market;

    for (const auto& fxb : fxbd) {
        // build market
        boost::shared_ptr<Market> market = boost::make_shared<TestMarket>(fxb.s, fxb.rf, fxb.rd, fxb.v);
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption
        string maturityDate = to_string(market->asofDate() + Integer(fxb.t * 360 + 0.5));
        OptionData optionData("Long", to_string(fxb.type), "European", false,
                              vector<string>(1, maturityDate));

        OptionBarrierData obd(fxb.bt, {fxb.h}, "American", fxb.k);
        FxBarrierOption fxBarrierOption(env, optionData, obd, "EUR", 1, // foreign
                                        "JPY", fxb.x);                  // domestic
        
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        std::string productName = "FxBarrierOption";
        engineData->model(productName) = "GarmanKohlhagen";
        engineData->engine(productName) = "AnalyticBarrierEngine";
        engineFactory = boost::make_shared<EngineFactory>(engineData, market);
        BOOST_REQUIRE_NO_THROW(fxBarrierOption.build(engineFactory));

        Real expectedNPV = fxb.result;

        BOOST_CHECK_SMALL(fxBarrierOption.instrument()->NPV() - expectedNPV, 2e-2);
    }

    Settings::instance().evaluationDate() = today; // reset
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
