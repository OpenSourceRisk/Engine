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

#include <test/fxoption.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;

// FX Option test, example from Wystup, section 1.2.6, page 28

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(3, Feb, 2016);

        // build discount
        discountCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateYts(0.025);
        discountCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateYts(0.03);

        // add fx rates
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.10);
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

void FXOptionTest::testFXOptionPrice() {
    BOOST_TEST_MESSAGE("Testing FXOption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build FXOption - expiry in 1 Year
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20170203"));
    Envelope env("CP1");
    FxOption fxOption(env, optionData, "EUR", 1000000, // bought
                      "USD", 1250000);                 // sold

    Real expectedNPV_USD = 29148.0;
    Real expectedNPV_EUR = 24290.0;

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("FxOption") = "GarmanKohlhagen";
    engineData->engine("FxOption") = "AnalyticEuropeanEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<FxOptionEngineBuilder>());

    fxOption.build(engineFactory);

    Real npv = fxOption.instrument()->NPV();

    // Check NPV matches expected values. Expected value from Wystup is rounded at each step of
    // calculation so we get a difference of $50 here.
    if (fxOption.npvCurrency() == "EUR") {
        BOOST_CHECK_CLOSE(npv, expectedNPV_EUR, 0.2);
    } else if (fxOption.npvCurrency() == "USD") {
        BOOST_CHECK_CLOSE(npv, expectedNPV_USD, 0.2);
    } else {
        BOOST_FAIL("Unexpected FX Option npv currency " << fxOption.npvCurrency());
    }

    Settings::instance().evaluationDate() = today; // reset
}

test_suite* FXOptionTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("FXOptionTest");
    suite->add(BOOST_TEST_CASE(&FXOptionTest::testFXOptionPrice));
    return suite;
}
}
