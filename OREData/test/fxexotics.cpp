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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/builders/fxdoublebarrieroption.hpp>
#include <ored/portfolio/builders/fxdoubletouchoption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/swap.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/all.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREPlusEquityFXTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FXOptionTest)

namespace {

struct FxOptionData {
    string optionType;
    Real s;       // spot
    Real k;       // strike
    Rate q;       // dividend
    Rate r;       // risk-free rate
    string t;     // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
};

class TestMarket : public MarketImpl {
public:
    TestMarket(Real spot, Real q, Real r, Real vol, bool withFixings = false) : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        Settings::instance().evaluationDate() = asof_;

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(q);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] = flatRateYts(r);

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        quotes["EURJPY"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot));
        fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

	// add fx conventions
	auto conventions = QuantLib::ext::make_shared<Conventions>();
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));
        InstrumentConventions::instance().setConventions(conventions);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURJPY")] = flatRateFxv(vol);

        if (withFixings) {
            // add fixings
            TimeSeries<Real> pastFixings;
            pastFixings[Date(1, Feb, 2016)] = 100;
            pastFixings[Date(2, Feb, 2016)] = 90;
            IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
            TimeSeries<Real> pastFixingsInverted;
            pastFixingsInverted[Date(1, Feb, 2016)] = 1 / pastFixings[Date(1, Feb, 2016)];
            pastFixingsInverted[Date(2, Feb, 2016)] = 1 / pastFixings[Date(2, Feb, 2016)];
            IndexManager::instance().setHistory("Reuters JPY/EUR", pastFixingsInverted);
        }
    }

    void setFxSpot(string ccyPair, Real spot) {
        auto q = QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(fx_->getQuote(ccyPair).currentLink());
        QL_REQUIRE(q, "internal error: could not cast quote to SimpleQuote in TestMarket::setFxSpot()");
        q->setValue(spot);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, Actual360()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, Actual360()));
        return Handle<BlackVolTermStructure>(fxv);
    }

    QuantLib::ext::shared_ptr<SimpleQuote> fxSpot_;
};
} // namespace

// FX Digital Option test, "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 88
// type, strike,  spot,    q,    r,    t,  vol,  value
// "Put", 80.00, 100.0, 0.06, 0.06, 0.75, 0.35, 2.6710
BOOST_AUTO_TEST_CASE(testFXDigitalOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXDigitalOption Price...");

    FxOptionData fxd[] = {{"Put", 100.00, 80.0, 0.06, 0.06, "20161030", 0.35, 2.6710}};

    for (auto& f : fxd) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXDigitalOption - expiry in 9 months
        // Date exDate = today + Integer(0.75*360+0.5);
        OptionData optionData("Long", "Put", "European", true, vector<string>(1, f.t));
        Envelope env("CP1");
        FxDigitalOption fxOption(env, optionData, f.k, 10, "EUR", // foreign
                                 "JPY");                          // domestic

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDigitalOption") = "GarmanKohlhagen";
        engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxOption.build(engineFactory);

        Real npv = fxOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("FX Option, NPV Currency " << fxOption.npvCurrency());
        BOOST_TEST_MESSAGE("NPV =                     " << npv);

        // Check NPV matches expected values.
        QL_REQUIRE(fxOption.npvCurrency() == "JPY", "unexpected NPV currency ");

        BOOST_CHECK_CLOSE(npv, expectedNPV, 0.2);
        Settings::instance().evaluationDate() = today; // reset
    }
}

struct BarrierOptionData {
    string barrierType;
    Real barrier;
    Real rebate;
    string optionType; // option type
    Real k;            // spot
    Real s;            // strike
    Rate q;            // dividend
    Rate r;            // risk-free rate
    Real t;            // time to maturity
    Volatility v;      // volatility
    Real result;       // expected result
};

BOOST_AUTO_TEST_CASE(testFXBarrierOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXBarrierOption Price...");

    // Testing option values when barrier untouched
    // Values from "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 72
    BarrierOptionData fxb[] = {// barrierType, barrier, rebate,   type, strk,     s,    q,    r,    t,    v,  result
                               {"DownAndOut", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 9.0246},
                               {"DownAndOut", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 4.8759},
                               {"DownAndOut", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"UpAndOut", 105.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 2.6789},
                               {"UpAndOut", 105.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 2.3580},
                               {"UpAndOut", 105.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 2.3453},

                               {"DownAndIn", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 7.7627},
                               {"DownAndIn", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 4.0109},
                               {"DownAndIn", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 2.0576},
                               {"DownAndIn", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 13.8333},
                               {"DownAndIn", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 7.8494},
                               {"DownAndIn", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 3.9795},
                               {"UpAndIn", 105.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 14.1112},
                               {"UpAndIn", 105.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 8.4482},
                               {"UpAndIn", 105.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 4.5910},

                               {"DownAndOut", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 8.8334},
                               {"DownAndOut", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 7.0285},
                               {"DownAndOut", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 5.4137},
                               {"DownAndOut", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"UpAndOut", 105.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 2.6341},
                               {"UpAndOut", 105.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 2.4389},
                               {"UpAndOut", 105.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 2.4315},

                               {"DownAndIn", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 9.0093},
                               {"DownAndIn", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 5.1370},
                               {"DownAndIn", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 2.8517},
                               {"DownAndIn", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 14.8816},
                               {"DownAndIn", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 9.2045},
                               {"DownAndIn", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 5.3043},
                               {"UpAndIn", 105.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 15.2098},
                               {"UpAndIn", 105.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 9.7278},
                               {"UpAndIn", 105.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 5.8350},

                               // barrierType, barrier, rebate,   type, strk,     s,    q,    r,    t,    v,  result
                               {"DownAndOut", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 2.2798},
                               {"DownAndOut", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 2.2947},
                               {"DownAndOut", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 2.6252},
                               {"DownAndOut", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 3.0000},
                               {"UpAndOut", 105.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 3.7760},
                               {"UpAndOut", 105.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 5.4932},
                               {"UpAndOut", 105.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 7.5187},

                               {"DownAndIn", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 2.9586},
                               {"DownAndIn", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 6.5677},
                               {"DownAndIn", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 11.9752},
                               {"DownAndIn", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 2.2845},
                               {"DownAndIn", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 5.9085},
                               {"DownAndIn", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 11.6465},
                               {"UpAndIn", 105.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 1.4653},
                               {"UpAndIn", 105.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 3.3721},
                               {"UpAndIn", 105.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 7.0846},

                               {"DownAndOut", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 2.4170},
                               {"DownAndOut", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 2.4258},
                               {"DownAndOut", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 2.6246},
                               {"DownAndOut", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"DownAndOut", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 3.0000},
                               {"UpAndOut", 105.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 4.2293},
                               {"UpAndOut", 105.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 5.8032},
                               {"UpAndOut", 105.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 7.5649},

                               {"DownAndIn", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 3.8769},
                               {"DownAndIn", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 7.7989},
                               {"DownAndIn", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 13.3078},
                               {"DownAndIn", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 3.3328},
                               {"DownAndIn", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 7.2636},
                               {"DownAndIn", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 12.9713},
                               {"UpAndIn", 105.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 2.0658},
                               {"UpAndIn", 105.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 4.4226},
                               {"UpAndIn", 105.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 8.3686},

                               // Check 'Out' options return rebate when barrier touched
                               // barrierType, barrier, rebate,   type, strk,     s,    q,    r,    t,    v,  result
                               {"DownAndOut", 95.0, 3.0, "Call", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 95.0, 3.0, "Call", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 100, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Call", 90, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Call", 100, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Call", 110, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},

                               {"DownAndOut", 95.0, 3.0, "Put", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 95.0, 3.0, "Put", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 100, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Put", 90, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Put", 100, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},
                               {"UpAndOut", 105.0, 3.0, "Put", 110, 110.0, 0.04, 0.08, 0.50, 0.25, 3.0},

                               // Check 'In' options return fxOption npv when barrier touched
                               // barrierType, barrier, rebate,   type, strk,     s,    q,    r,    t,    v,   result
                               {"DownAndIn", 95.0, 3.0, "Call", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 7.06448},
                               {"DownAndIn", 95.0, 3.0, "Call", 100, 90.0, 0.04, 0.08, 0.50, 0.25, 3.29945},
                               {"DownAndIn", 95.0, 3.0, "Call", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 1.36007},
                               {"DownAndIn", 100.0, 3.0, "Call", 90, 90.0, 0.04, 0.08, 0.50, 0.25, 7.06448},
                               {"DownAndIn", 100.0, 3.0, "Call", 100, 90.0, 0.04, 0.08, 0.50, 0.25, 3.29945},
                               {"DownAndIn", 100.0, 3.0, "Call", 110, 90.0, 0.04, 0.08, 0.50, 0.25, 1.36007},
                               {"UpAndIn", 105.0, 3.0, "Call", 90, 110.0, 0.04, 0.08, 0.50, 0.25, 22.21500},
                               {"UpAndIn", 105.0, 3.0, "Call", 100, 110.0, 0.04, 0.08, 0.50, 0.25, 14.52180},
                               {"UpAndIn", 105.0, 3.0, "Call", 110, 110.0, 0.04, 0.08, 0.50, 0.25, 8.63437}};

    vector<string> positions = {"Long", "Short"};
    for (auto& f : fxb) {
        for (auto& position : positions) {
            // build market
            QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
            Date today = Settings::instance().evaluationDate();
            Settings::instance().evaluationDate() = market->asofDate();

            // build FXBarrierOption - expiry in 6 months
            OptionData optionData(position, f.optionType, "European", true, vector<string>(1, "20160801"));
            vector<Real> barriers = {f.barrier};
            vector<TradeBarrier> tradeBarriers;
            tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
            BarrierData barrierData(f.barrierType, barriers, f.rebate, tradeBarriers);
            Envelope env("CP1");
            FxBarrierOption fxBarrierOption(env, optionData, barrierData, Date(), "", "EUR", 1, // foreign
                                            "JPY", f.k);                                        // domestic

            // we'll check that the results scale as expected
            // scaling the notional and the rebate by a million we should get npv_scaled = 1million * npv
            Real Notional = 1000000;
            BarrierData barrierDataScaled(f.barrierType, barriers, f.rebate * Notional, tradeBarriers);
            FxBarrierOption fxBarrierOptionNotional(env, optionData, barrierDataScaled, Date(), "", "EUR", Notional, // foreign
                                                    "JPY", Notional * f.k);                                          // domestic

            Real expectedNPV = f.result;

            // Build and price
            QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
            engineData->model("FxBarrierOption") = "GarmanKohlhagen";
            engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
            engineData->model("FxOption") = "GarmanKohlhagen";
            engineData->engine("FxOption") = "AnalyticEuropeanEngine";

            QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

            fxBarrierOption.build(engineFactory);
            fxBarrierOptionNotional.build(engineFactory);

            Real npv = (position == "Long" ? 1 : -1) * fxBarrierOption.instrument()->NPV();

            BOOST_TEST_MESSAGE("NPV Currency " << fxBarrierOption.npvCurrency());
            BOOST_TEST_MESSAGE("FX Barrier Option NPV =                     " << npv);

            // Check NPV matches expected values.
            QL_REQUIRE(fxBarrierOption.npvCurrency() == "JPY", "unexpected NPV currency ");

            BOOST_CHECK_CLOSE(npv, expectedNPV, 0.2);
            BOOST_CHECK_CLOSE(fxBarrierOption.instrument()->NPV() * 1000000,
                              fxBarrierOptionNotional.instrument()->NPV(), 0.2);
            Settings::instance().evaluationDate() = today; // reset
        }
    }
}

BOOST_AUTO_TEST_CASE(testFXBarrierOptionSymmetry) {
    BOOST_TEST_MESSAGE("Testing FXBarrierOption Symmetry...");
    //"Option pricing formulas, Second Edition", E.G. Haug, McGraw-Hill 2007 - page 168
    // For single barrier options the symmetry between put and call options is:
    // c_di(spot, strike, barrier, r, q, vol) = p_ui(strike, spot, strike*spot/barrier, q, r, vol);
    // c_di(spot, strike, barrier, r, q, vol) = p_di(strike, spot, strike*spot/barrier, q, r, vol);

    BarrierOptionData fxb[] = {// barrierType, barrier, rebate, type, strk,     s,    q,    r,    t,    v,  result
                               {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 9.0246},
                               {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 7.7627},
                               {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 4.0109},
                               {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 2.0576},
                               {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 13.8333},
                               {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 7.8494},
                               {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 3.9795},
                               {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 9.0093},
                               {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 5.1370},
                               {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 2.8517},
                               {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 14.8816},
                               {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 9.2045},
                               {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 5.3043}};

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> marketCall = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        QuantLib::ext::shared_ptr<Market> marketPut = QuantLib::ext::make_shared<TestMarket>(f.k, f.r, f.q, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = marketCall->asofDate();

        // build FXBarrierOptions - expiry in 6 months
        OptionData optionCallData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        OptionData optionPutData("Long", "Put", "European", true, vector<string>(1, "20160801"));
        vector<Real> barriersCall = {f.barrier};
        vector<Real> barriersPut = {f.s * f.k / f.barrier};
        vector<TradeBarrier> tradeBarriers_call, tradeBarriers_put;
        tradeBarriers_call.push_back(TradeBarrier(f.barrier, ""));
        tradeBarriers_put.push_back(TradeBarrier(f.s * f.k / f.barrier, ""));
        BarrierData barrierCallData("DownAndIn", barriersCall, 0.0, tradeBarriers_call);
        BarrierData barrierPutData("UpAndIn", barriersPut, 0.0, tradeBarriers_put);
        Envelope env("CP1");

        FxBarrierOption fxCallOption(env, optionCallData, barrierCallData, Date(), "", "EUR", 1, // foreign
                                     "JPY", f.k);                                                // domestic
        FxBarrierOption fxPutOption(env, optionPutData, barrierPutData, Date(), "", "EUR", 1,    // foreign
                                    "JPY", f.s);                                                 // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactoryCall = QuantLib::ext::make_shared<EngineFactory>(engineData, marketCall);
        QuantLib::ext::shared_ptr<EngineFactory> engineFactoryPut = QuantLib::ext::make_shared<EngineFactory>(engineData, marketPut);

        fxCallOption.build(engineFactoryCall);
        fxPutOption.build(engineFactoryPut);

        Real npvCall = fxCallOption.instrument()->NPV();
        Real npvPut = fxPutOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("NPV Currency " << fxCallOption.npvCurrency());
        BOOST_TEST_MESSAGE("FX Barrier Option, NPV Call " << npvCall);
        BOOST_TEST_MESSAGE("FX Barrier Option, NPV Put " << npvPut);
        // Check NPV matches expected values.
        BOOST_CHECK_CLOSE(npvCall, npvPut, 0.01);

        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXBarrierOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXBarrierOption Parity...");

    // An "In" option and an "Out" option with the same strikes and expiries should have the same combined price as a
    // vanilla Call option
    BarrierOptionData fxb[] = {
        // barrierType, barrier, rebate, type, strk,     s,    q,    r,    t,    v,  result
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
    };

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrier};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
        BarrierData downInData("DownAndIn", barriers, 0.0, tradeBarriers);
        BarrierData upInData("UpAndIn", barriers, 0.0, tradeBarriers);
        BarrierData downOutData("DownAndOut", barriers, 0.0, tradeBarriers);
        BarrierData upOutData("UpAndOut", barriers, 0.0, tradeBarriers);

        Envelope env("CP1");

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);

        FxBarrierOption downInOption(env, optionData, downInData, Date(), "", "EUR", 1,   // foreign
                                     "JPY", f.k);                                         // domestic
        FxBarrierOption upInOption(env, optionData, upInData, Date(), "", "EUR", 1,       // foreign
                                   "JPY", f.k);                                           // domestic
        FxBarrierOption downOutOption(env, optionData, downOutData, Date(), "", "EUR", 1, // foreign
                                      "JPY", f.k);                                        // domestic
        FxBarrierOption upOutOption(env, optionData, upOutData, Date(), "", "EUR", 1,     // foreign
                                    "JPY", f.k);                                          // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxOption.build(engineFactory);
        downInOption.build(engineFactory);
        upInOption.build(engineFactory);
        downOutOption.build(engineFactory);
        upOutOption.build(engineFactory);

        Real npv = fxOption.instrument()->NPV();

        // Check NPV matches expected values.
        BOOST_CHECK_CLOSE(npv, downInOption.instrument()->NPV() + downOutOption.instrument()->NPV(), 0.01);
        BOOST_CHECK_CLOSE(npv, upInOption.instrument()->NPV() + upOutOption.instrument()->NPV(), 0.01);

        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXBarrierOptionTouched) {
    BOOST_TEST_MESSAGE("Testing FXBarrierOption when barrier already touched...");

    // An "In" option is equivalent to an fxOption once the barrier has been touched
    // An "Out" option has zero value once the barrier has been touched and the rebate paid
    BarrierOptionData fxb[] = {// barrierType, barrier, rebate,   type, strk,     s,    q,    r,    t,    v,  result
                               {"DownAndIn", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

                               {"DownAndIn", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndIn", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndIn", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

                               {"DownAndOut", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Call", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Call", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Call", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

                               {"DownAndOut", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"DownAndOut", 100.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Put", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Put", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
                               {"UpAndOut", 95.0, 3.0, "Put", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0}};

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrier};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
        BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);

        Envelope env("CP1");

        FxBarrierOption fxBarrierOption(env, optionData, barrierData, Date(1, Month::February, 2016), 
            "TARGET", "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY"); 
        FxOption fxOption(env, optionData, "EUR", 1,                                             // foreign
                          "JPY", f.k);                                                           // domestic

        FxBarrierOption fxBarrierOptionInverted(env, optionData, barrierData, Date(1, Month::February, 2016), 
            "TARGET", "EUR", 1,                                                // foreign
            "JPY", f.k, "FX-Reuters-JPY-EUR"); // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxOption.build(engineFactory);
        fxBarrierOption.build(engineFactory);
        fxBarrierOptionInverted.build(engineFactory);

        // Check NPV matches expected values.
        if (f.barrierType == "DownAndIn" || f.barrierType == "UpAndIn") {
            BOOST_CHECK_CLOSE(fxBarrierOption.instrument()->NPV(), fxOption.instrument()->NPV(), 0.01);
            BOOST_CHECK_CLOSE(fxBarrierOptionInverted.instrument()->NPV(), fxOption.instrument()->NPV(), 0.01);
        } else {
            BOOST_CHECK_CLOSE(fxBarrierOption.instrument()->NPV(), 0.0, 0.01);
            BOOST_CHECK_CLOSE(fxBarrierOptionInverted.instrument()->NPV(), 0.0, 0.01);
        }

        Settings::instance().evaluationDate() = today; // reset
        IndexManager::instance().clearHistories();     // reset
    }
}

struct DigitalBarrierOptionData {
    string barrierType;
    Real barrier;
    Real cash; // cash payoff for cash-or-nothing
    string optionType;
    Real k;
    Real s;      // spot
    Rate q;      // dividend
    Rate r;      // risk-free rate
    Time t;      // time to maturity
    Real v;      // volatility
    Real result; // expected result
};

BOOST_AUTO_TEST_CASE(testFXDigitalBarrierOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXDigitalBarrierOption Price...");

    // Testing option values when barrier untouched
    // Values from "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 180
    DigitalBarrierOptionData fxb[] = {
        // barrierType, barrier,  cash,   type, strike,   spot,     q,    r,   t,  vol,  value
        {"DownAndIn", 100.00, 15.00, "Call", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 4.9289},
        {"DownAndIn", 100.00, 15.00, "Call", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 6.2150},
        {"UpAndIn", 100.00, 15.00, "Call", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 5.8926}, // 5.3710 in Haug's book
        {"UpAndIn", 100.00, 15.00, "Call", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 7.4519},
        {"DownAndIn", 100.00, 15.00, "Put", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 4.4314},
        {"DownAndIn", 100.00, 15.00, "Put", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 3.1454},
        {"UpAndIn", 100.00, 15.00, "Put", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 5.3297},
        {"UpAndIn", 100.00, 15.00, "Put", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 3.7704},
        {"DownAndOut", 100.00, 15.00, "Call", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 4.8758},
        {"DownAndOut", 100.00, 15.00, "Call", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 4.9081},
        {"UpAndOut", 100.00, 15.00, "Call", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0000},
        {"UpAndOut", 100.00, 15.00, "Call", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0407},
        {"DownAndOut", 100.00, 15.00, "Put", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0323},
        {"DownAndOut", 100.00, 15.00, "Put", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0000},
        {"UpAndOut", 100.00, 15.00, "Put", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 3.0461},
        {"UpAndOut", 100.00, 15.00, "Put", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 3.0054}};

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = market->asofDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXOption - expiry in 6 months
        OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrier};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
        BarrierData barrierData(f.barrierType, barriers, 0, tradeBarriers);

        Envelope env("CP1");
        FxDigitalBarrierOption barrierOption(env, optionData, barrierData, f.k, f.cash, "EUR", // foreign
                                             "JPY");                                           // domestic

        Real expectedNPV = f.result / f.cash;

        // Build and price
        map<string, string> engineParamMap;
        engineParamMap["Scheme"] = "Douglas";
        engineParamMap["TimeGridPerYear"] = "800";
        engineParamMap["XGrid"] = "400";
        engineParamMap["DampingSteps"] = "100";

        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDigitalBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDigitalBarrierOption") = "FdBlackScholesBarrierEngine";
        engineData->engineParameters("FxDigitalBarrierOption") = engineParamMap;
        engineData->model("FxDigitalOption") = "GarmanKohlhagen";
        engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";
        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
        barrierOption.build(engineFactory);

        Real npv = barrierOption.instrument()->NPV() / f.cash;

        BOOST_TEST_MESSAGE("NPV Currency " << barrierOption.npvCurrency());

        // Check NPV matches expected values.
        // TODO: Implement analytical formula to improve accuracy
        BOOST_CHECK_SMALL(npv - expectedNPV, 1e-3);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDigitalBarrierOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXDigitalBarrierOption Price...");
    // An "In" option and an "Out" option with the same strikes and expiries should have the same combined price as a
    // vanilla Call option

    DigitalBarrierOptionData fxb[] = {
        // barrierType, barrier,  cash,   type, strike,   spot,     q,    r,   t,  vol, value
        {"", 100.00, 15.00, "Call", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, -0.14, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, 0.03, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 98.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 99.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 99.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0}};

    vector<string> payoutCcys = {"EUR", "JPY"};
    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = market->asofDate();
        Settings::instance().evaluationDate() = market->asofDate();
        for (auto& payoutCcy : payoutCcys) {
            // build FXOption - expiry in 9 months
            // Date exDate = today + Integer(f.t*360+0.5);
            OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, "20160801"));

            vector<Real> barriers = {f.barrier};
            vector<TradeBarrier> tradeBarriers;
            tradeBarriers.push_back(TradeBarrier(f.barrier, ""));

            BarrierData downInData("DownAndIn", barriers, 0.0, tradeBarriers);
            BarrierData upInData("UpAndIn", barriers, 0.0, tradeBarriers);
            BarrierData downOutData("DownAndOut", barriers, 0.0, tradeBarriers);
            BarrierData upOutData("UpAndOut", barriers, 0.0, tradeBarriers);
            BarrierData barrierData(f.barrierType, barriers, 0, tradeBarriers);

            Envelope env("CP1");
            FxDigitalOption fxOption(env, optionData, f.k, payoutCcy, f.cash, "EUR", // foreign
                                     "JPY");                                         // domestic

            FxDigitalBarrierOption downInOption(env, optionData, downInData, f.k, f.cash, "EUR",   // foreign
                                                "JPY", "", "", "", payoutCcy);                     // domestic
            FxDigitalBarrierOption upInOption(env, optionData, upInData, f.k, f.cash, "EUR",       // foreign
                                              "JPY", "", "", "", payoutCcy);                       // domestic
            FxDigitalBarrierOption downOutOption(env, optionData, downOutData, f.k, f.cash, "EUR", // foreign
                                                 "JPY", "", "", "", payoutCcy);                    // domestic
            FxDigitalBarrierOption upOutOption(env, optionData, upOutData, f.k, f.cash, "EUR",     // foreign
                                               "JPY", "", "", "", payoutCcy);                      // domestic

            // Build and price
            map<string, string> engineParamMap;
            engineParamMap["Scheme"] = "Douglas";
            engineParamMap["TimeGridPerYear"] = "400";
            engineParamMap["XGrid"] = "400";
            engineParamMap["DampingSteps"] = "100";

            QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
            engineData->model("FxDigitalBarrierOption") = "GarmanKohlhagen";
            engineData->engine("FxDigitalBarrierOption") = "FdBlackScholesBarrierEngine";
            engineData->engineParameters("FxDigitalBarrierOption") = engineParamMap;
            engineData->model("FxDigitalOption") = "GarmanKohlhagen";
            engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";
            QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

            fxOption.build(engineFactory);
            downInOption.build(engineFactory);
            upInOption.build(engineFactory);
            downOutOption.build(engineFactory);
            upOutOption.build(engineFactory);

            Real npv = fxOption.instrument()->NPV();

            BOOST_TEST_MESSAGE("NPV Currency " << fxOption.npvCurrency());

            // Check NPV matches expected values.
            // Check NPV matches expected values.
            BOOST_CHECK_CLOSE(npv, downInOption.instrument()->NPV() + downOutOption.instrument()->NPV(), 0.1);
            BOOST_CHECK_CLOSE(npv, upInOption.instrument()->NPV() + upOutOption.instrument()->NPV(), 0.1);
        }
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDigitalBarrierOptionTouched) {
    BOOST_TEST_MESSAGE("Testing FXDigitalBarrierOption Price...");

    DigitalBarrierOptionData fxb[] = {
        // barrierType, barrier,  cash,   type, strike,   spot,     q,    r,   t,  vol, value
        {"", 100.00, 15.00, "Call", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 105.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 95.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, -0.14, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 102.00, 95.00, 0.03, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 98.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 102.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 99.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Call", 98.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 99.00, 0.00, 0.10, 0.5, 0.20, 0.0},
        {"", 100.00, 15.00, "Put", 98.00, 101.00, 0.00, 0.10, 0.5, 0.20, 0.0}};

    vector<string> payoutCcys = {"EUR", "JPY"};
    vector<string> fxIndices = {"FX-Reuters-EUR-JPY", "FX-Reuters-JPY-EUR"};
    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = market->asofDate();
        Settings::instance().evaluationDate() = market->asofDate();
        for (auto& payoutCcy : payoutCcys) {
            for (auto& fxIndex : fxIndices) {
                // build FXOption - expiry in 6 months
                OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, "20160801"));

                vector<Real> barriers = {f.barrier};
                vector<TradeBarrier> tradeBarriers;
                tradeBarriers.push_back(TradeBarrier(f.barrier, ""));

                BarrierData downInData("DownAndIn", barriers, 0.0, tradeBarriers);
                BarrierData upInData("UpAndIn", barriers, 0.0, tradeBarriers);
                BarrierData downOutData("DownAndOut", barriers, 0.0, tradeBarriers);
                BarrierData upOutData("UpAndOut", barriers, 0.0, tradeBarriers);

                Envelope env("CP1");
                FxDigitalOption fxOption(env, optionData, f.k, payoutCcy, f.cash, "EUR",             // foreign
                                         "JPY");                                                     // domestic
                FxDigitalBarrierOption downInOption(env, optionData, downInData, f.k, f.cash, "EUR", // foreign
                                                    "JPY", "20160201", "TARGET", fxIndex, payoutCcy);
                FxDigitalBarrierOption upInOption(env, optionData, upInData, f.k, f.cash, "EUR", // foreign
                                                  "JPY", "20160201", "TARGET", fxIndex, payoutCcy);
                FxDigitalBarrierOption downOutOption(env, optionData, downOutData, f.k, f.cash, "EUR", // foreign
                                                     "JPY", "20160201", "TARGET", fxIndex, payoutCcy);
                FxDigitalBarrierOption upOutOption(env, optionData, upOutData, f.k, f.cash, "EUR", // foreign
                                                   "JPY", "20160201", "TARGET", fxIndex, payoutCcy);

                // Build and price
                map<string, string> engineParamMap;
                engineParamMap["Scheme"] = "Douglas";
                engineParamMap["TimeGridPerYear"] = "400";
                engineParamMap["XGrid"] = "400";
                engineParamMap["DampingSteps"] = "100";

                QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
                engineData->model("FxDigitalBarrierOption") = "GarmanKohlhagen";
                engineData->engine("FxDigitalBarrierOption") = "FdBlackScholesBarrierEngine";
                engineData->engineParameters("FxDigitalBarrierOption") = engineParamMap;
                engineData->model("FxDigitalOption") = "GarmanKohlhagen";
                engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";
                QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

                fxOption.build(engineFactory);
                downInOption.build(engineFactory);
                upInOption.build(engineFactory);
                downOutOption.build(engineFactory);
                upOutOption.build(engineFactory);

                Real npv = fxOption.instrument()->NPV();

                BOOST_TEST_MESSAGE("NPV Currency " << fxOption.npvCurrency());

                // Check NPV matches expected values.
                BOOST_CHECK_CLOSE(npv, downInOption.instrument()->NPV(), 0.01);
                BOOST_CHECK_CLOSE(npv, upInOption.instrument()->NPV(), 0.01);
                BOOST_CHECK_CLOSE(0.0, downOutOption.instrument()->NPV(), 0.01);
                BOOST_CHECK_CLOSE(0.0, upOutOption.instrument()->NPV(), 0.01);
            }
        }
        Settings::instance().evaluationDate() = today; // reset
        IndexManager::instance().clearHistories();     // reset
    }
}

struct FxTouchOptionData {
    string barrierType;
    Real barrier;
    Real cash; // cash payoff
    bool payoffAtExpiry;
    string optionType;
    bool payoffCurrencyDomestic;
    Real s;      // spot
    Rate q;      // foreign risk-free rate
    Rate r;      // domestic risk-free rate
    Time t;      // time to maturity
    Real v;      // volatility
    Real result; // expected result
};

BOOST_AUTO_TEST_CASE(testFXTouchOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXTouchOption Price...");

    // The following results are from Table 4.22, pp 180 of
    // "The Complete Guide to Option Pricing Formulas" (2nd Ed) by E. G. Haug
    FxTouchOptionData fxd[] = {
        // barrierType, barrier, cash, payAtExp,   type, payDom,     s,   q,   r,   t, vol,   value
        {"DownAndIn", 100.0, 15.0, true, "Put", true, 105.0, 0.0, 0.1, 0.5, 0.2, 9.3604},
        {"UpAndIn", 100.0, 15.0, true, "Call", true, 95.0, 0.0, 0.1, 0.5, 0.2, 11.2223},
        {"DownAndOut", 100.0, 15.0, true, "Put", true, 105.0, 0.0, 0.1, 0.5, 0.2, 4.9081},
        {"UpAndOut", 100.0, 15.0, true, "Call", true, 95.0, 0.0, 0.1, 0.5, 0.2, 3.0461},

        // payoff at hit. The following are the corresponding test cases from Haug.
        {"DownAndIn", 100.0, 15.0, false, "Put", true, 105.0, 0.0, 0.1, 0.5, 0.2, 9.3604},
        {"UpAndIn", 100.0, 15.0, false, "Call", true, 95.0, 0.0, 0.1, 0.5, 0.2, 11.2223},

        // check that if the option has already knocked in or out then the option prices appropriately.
        {"DownAndIn", 100.0, 15.0, true, "Put", true, 95.0, 0.0, 0.1, 0.5, 0.2, 14.2684},
        {"UpAndIn", 100.0, 15.0, true, "Call", true, 105.0, 0.0, 0.1, 0.5, 0.2, 14.2684},
        {"DownAndOut", 100.0, 15.0, true, "Put", true, 95.0, 0.0, 0.1, 0.5, 0.2, 0.0},
        {"UpAndOut", 100.0, 15.0, true, "Call", true, 105.0, 0.0, 0.1, 0.5, 0.2, 0.0},

        // check consistent pricing in the limit of high barrier level
        {"UpAndIn", 1000.0, 15.0, true, "Call", true, 100.0, 0.0, 0.1, 0.5, 0.2, 0.0},
        {"UpAndOut", 1000.0, 15.0, true, "Call", true, 100.0, 0.0, 0.1, 0.5, 0.2, 14.2684},
    };

    // Set engineData
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("FxTouchOption") = "GarmanKohlhagen";
    engineData->engine("FxTouchOption") = "AnalyticDigitalAmericanEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";

    for (auto& f : fxd) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);

        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FxTouchOption expiring in 6 months
        vector<Real> barriers = {f.barrier};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
        BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);
        OptionData optionData("Long", f.optionType, "American", f.payoffAtExpiry, vector<string>(1, "20160801"));
        Envelope env("CP1");
        FxTouchOption fxTouchOption(env, optionData, barrierData, "EUR", "JPY",
                                    f.payoffCurrencyDomestic ? "JPY" : "EUR", f.cash);

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxTouchOption.build(engineFactory);

        Real npv = fxTouchOption.instrument()->NPV();
        string ccy = fxTouchOption.npvCurrency();

        BOOST_TEST_MESSAGE("FX Touch Option, NPV Currency " << ccy);
        BOOST_TEST_MESSAGE("NPV =                     " << npv);
        BOOST_TEST_MESSAGE("Expected NPV =                     " << expectedNPV);

        BOOST_CHECK_SMALL(npv - expectedNPV, 0.01);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXTouchOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXTouchOption Parity...");
    // An "In" option and an "Out" option with the same strikes and expiries should have the same combined price as a
    // cashflow

    FxTouchOptionData fxb[] = {
        // barrierType, barrier, cash, payAtExp, type, payDom,     s,    q,    r,    t,  vol, value
        {"", 0.0, 1e6, true, "", true, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 1e6, true, "", true, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 1e6, true, "", true, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 105.0, 1e6, true, "", true, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 999.0, 1e6, true, "", true, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0}};

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrier};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrier, ""));

        BarrierData downInData("DownAndIn", barriers, 0.0, tradeBarriers);
        BarrierData upInData("UpAndIn", barriers, 0.0, tradeBarriers);
        BarrierData downOutData("DownAndOut", barriers, 0.0, tradeBarriers);
        BarrierData upOutData("UpAndOut", barriers, 0.0, tradeBarriers);

        Envelope env("CP1");

        vector<double> amounts = {f.cash};
        vector<string> dates = {"2016-08-01"};

        LegData legData(QuantLib::ext::make_shared<CashflowData>(amounts, dates), true,
                        f.payoffCurrencyDomestic ? "JPY" : "EUR");
        legData.isPayer() = false;
        ore::data::Swap swap(env, {legData});

        FxTouchOption downInOption(env, optionData, downInData, "EUR", "JPY", f.payoffCurrencyDomestic ? "JPY" : "EUR",
                                   f.cash);
        FxTouchOption upInOption(env, optionData, upInData, "EUR", "JPY", f.payoffCurrencyDomestic ? "JPY" : "EUR",
                                 f.cash);
        FxTouchOption downOutOption(env, optionData, downOutData, "EUR", "JPY",
                                    f.payoffCurrencyDomestic ? "JPY" : "EUR", f.cash);
        FxTouchOption upOutOption(env, optionData, upOutData, "EUR", "JPY", f.payoffCurrencyDomestic ? "JPY" : "EUR",
                                  f.cash);

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxTouchOption") = "GarmanKohlhagen";
        engineData->engine("FxTouchOption") = "AnalyticDigitalAmericanEngine";
        engineData->model("Swap") = "DiscountedCashflows";
        engineData->engine("Swap") = "DiscountingSwapEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        swap.build(engineFactory);
        downInOption.build(engineFactory);
        upInOption.build(engineFactory);
        downOutOption.build(engineFactory);
        upOutOption.build(engineFactory);

        Real npv = swap.instrument()->NPV();

        // Check NPV matches expected values.
        BOOST_CHECK_CLOSE(npv, downInOption.instrument()->NPV() + downOutOption.instrument()->NPV(), 0.01);
        BOOST_CHECK_CLOSE(npv, upInOption.instrument()->NPV() + upOutOption.instrument()->NPV(), 0.01);

        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXTouchOptionTouched) {
    BOOST_TEST_MESSAGE("Testing FXTouchOption when barrier already touched...");

    struct FxTouchOptionTouchedData {
        string barrierType;
        Real barrier;
        Real cash;
        Real s;       // spot
        Real s_1;     // spot at t-1
        Real s_2;     // spot at t-2
        Rate q;       // dividend
        Rate r;       // risk-free rate
        Real t;       // time to maturity
        Volatility v; // volatility
        Real result;  // expected result
    };

    // An "In" option is equivalent to a cashflow once the barrier has been touched
    // An "Out" option has zero value once the barrier has been touched
    FxTouchOptionTouchedData fxt[] = {
        // barrierType, barrier,   cash,     s,   s_1,   s_2,    q,    r,    t,    v,  result
        {"DownAndIn", 80.0, 1e6, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndIn", 80.0, 1e6, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndIn", 80.0, 1e6, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndIn", 80.0, 1e6, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndIn", 80.0, 1e6, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndIn", 80.0, 1e6, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"UpAndIn", 120.0, 1e6, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndIn", 120.0, 1e6, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndIn", 120.0, 1e6, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndIn", 120.0, 1e6, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndIn", 120.0, 1e6, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndIn", 120.0, 1e6, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"DownAndOut", 80.0, 1e6, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndOut", 80.0, 1e6, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndOut", 80.0, 1e6, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndOut", 80.0, 1e6, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndOut", 80.0, 1e6, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"DownAndOut", 80.0, 1e6, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"UpAndOut", 120.0, 1e6, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndOut", 120.0, 1e6, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndOut", 120.0, 1e6, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndOut", 120.0, 1e6, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndOut", 120.0, 1e6, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"UpAndOut", 120.0, 1e6, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0}};

    vector<string> payoutCcys = {"EUR", "JPY"};
    vector<string> fxIndices = {"FX-Reuters-EUR-JPY", "FX-Reuters-JPY-EUR"};
    for (auto& f : fxt) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();
        TimeSeries<Real> pastFixings;
        pastFixings[market->asofDate() - 1 * Days] = f.s_1;
        pastFixings[market->asofDate() - 2 * Days] = f.s_2;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
        TimeSeries<Real> pastFixingsInverted;
        pastFixingsInverted[market->asofDate() - 1 * Days] = 1 / pastFixings[market->asofDate() - 1 * Days];
        pastFixingsInverted[market->asofDate() - 2 * Days] = 1 / pastFixings[market->asofDate() - 2 * Days];
        IndexManager::instance().setHistory("Reuters JPY/EUR", pastFixingsInverted);
        for (auto& payoutCcy : payoutCcys) {
            for (auto& fxIndex : fxIndices) {
                // build FXBarrierOption - expiry in 6 months
                OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

                vector<Real> barriers = {f.barrier};
                vector<TradeBarrier> tradeBarriers;
                tradeBarriers.push_back(TradeBarrier(f.barrier, ""));
                BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);

                Envelope env("CP1");

                vector<double> amounts = {f.cash};
                vector<string> dates = {"2016-08-01"};

                LegData legData(QuantLib::ext::make_shared<CashflowData>(amounts, dates), true, payoutCcy);
                legData.isPayer() = false;
                ore::data::Swap swap(env, {legData});

                FxTouchOption touchOption(env, optionData, barrierData, "EUR", "JPY", payoutCcy, f.cash, "20160201",
                                          "TARGET", fxIndex);

                // Build and price
                QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
                engineData->model("FxTouchOption") = "GarmanKohlhagen";
                engineData->engine("FxTouchOption") = "AnalyticDigitalAmericanEngine";
                engineData->model("Swap") = "DiscountedCashflows";
                engineData->engine("Swap") = "DiscountingSwapEngine";

                QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

                touchOption.build(engineFactory);
                swap.build(engineFactory);

                // Check NPV matches expected values.
                if (f.barrierType == "DownAndIn" || f.barrierType == "UpAndIn")
                    BOOST_CHECK_CLOSE(touchOption.instrument()->NPV(), swap.instrument()->NPV(), 0.01);
                else
                    BOOST_CHECK_CLOSE(touchOption.instrument()->NPV(), 0.0, 0.01);
            }
        }
        Settings::instance().evaluationDate() = today; // reset
        IndexManager::instance().clearHistories();     // reset
    }
}

struct DoubleBarrierOptionData {
    string barrierType;
    Real barrierLow;
    Real barrierHigh;
    Real rebate;
    string optionType; // option type
    Real k;            // strike
    Real s;            // spot
    Rate q;            // dividend
    Rate r;            // risk-free rate
    Real t;            // time to maturity
    Volatility v;      // volatility
    Real result;       // expected result
};

struct DoubleTouchOptionData {
    string barrierType;
    Real barrierLow;
    Real barrierHigh;
    Real cash;
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Real t;       // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
};

namespace {
// Testing option values when barrier untouched
// Values from "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 158
DoubleBarrierOptionData fxdb[] = {
    // barrierType, barrierLow, barrierHigh, rebate,   type, strk,     s,   q,   r,    t,    v,  result
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.15, 4.3515},
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.25, 6.1644},
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.35, 7.0373},
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.15, 6.9853},
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.25, 7.9336},
    {"KnockOut", 50.0, 150.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.35, 6.5088},

    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.15, 4.3505},
    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.25, 5.8500},
    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.35, 5.7726},
    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.15, 6.8082},
    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.25, 6.3383},
    {"KnockOut", 60.0, 140.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.35, 4.3841},

    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.15, 4.3139},
    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.25, 4.8293},
    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.35, 3.7765},
    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.15, 5.9697},
    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.25, 4.0004},
    {"KnockOut", 70.0, 130.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.35, 2.2563},

    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.15, 3.7516},
    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.25, 2.6387},
    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.35, 1.4903},
    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.15, 3.5805},
    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.25, 1.5098},
    {"KnockOut", 80.0, 120.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.35, 0.5635},

    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.15, 1.2055},
    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.25, 0.3098},
    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.25, 0.35, 0.0477},
    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.15, 0.5537},
    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.25, 0.0441},
    // Haug's result is 0.0011. Added 1 dp to pass closeness test
    {"KnockOut", 90.0, 110.0, 0.0, "Call", 100, 100.0, 0.0, 0.1, 0.50, 0.35, 0.00109}};

// Testing option values when barrier untouched
// Values from "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 181 & 182
DoubleTouchOptionData fxdt[] = {
    // barrierType, barrierLow, barrierHigh, cash,     s,    q,    r,    t,   v, result
    {"KnockOut", 80.0, 120.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.1, 9.8716},
    {"KnockOut", 80.0, 120.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.2, 8.9307},
    {"KnockOut", 80.0, 120.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.3, 6.3272},
    {"KnockOut", 80.0, 120.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.5, 1.9094},

    {"KnockOut", 85.0, 115.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.1, 9.7961},
    {"KnockOut", 85.0, 115.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.2, 7.2300},
    {"KnockOut", 85.0, 115.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.3, 3.7100},
    {"KnockOut", 85.0, 115.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.5, 0.4271},

    {"KnockOut", 90.0, 110.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.1, 8.9054},
    {"KnockOut", 90.0, 110.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.2, 3.6752},
    {"KnockOut", 90.0, 110.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.3, 0.7960},
    {"KnockOut", 90.0, 110.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.5, 0.0059},

    {"KnockOut", 95.0, 105.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.1, 3.6323},
    {"KnockOut", 95.0, 105.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.2, 0.0911},
    {"KnockOut", 95.0, 105.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.3, 0.0002},
    {"KnockOut", 95.0, 105.0, 10.0, 100.0, 0.02, 0.05, 0.25, 0.5, 0.0000}};
} // namespace

BOOST_AUTO_TEST_CASE(testFXDoubleBarrierOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXDoubleBarrierOption Price...");
    for (auto& f : fxdb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        Date exDate = today + Integer(f.t * 360 + 0.5);
        OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, ore::data::to_string(exDate)));
        vector<Real> barriers = {f.barrierLow, f.barrierHigh};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
        tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
        BarrierData barrierData(f.barrierType, barriers, f.rebate, tradeBarriers);
        Envelope env("CP1");
        FxDoubleBarrierOption fxDoubleBarrierOption(env, optionData, barrierData, Date(), "", 
            "EUR", 1,    // foreign
            "JPY", f.k); // domestic

        // we'll check that the results scale as expected
        // scaling the notional and the rebate by a million we should get npv_scaled = 1million * npv
        Real Notional = 1000000;
        BarrierData barrierDataScaled(f.barrierType, barriers, f.rebate * Notional, tradeBarriers);
        FxDoubleBarrierOption fxDoubleBarrierOptionNotional(env, optionData, barrierDataScaled, Date(), "", 
            "EUR", Notional,        // foreign
            "JPY", Notional * f.k); // domestic

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxDoubleBarrierOption.build(engineFactory);

        fxDoubleBarrierOptionNotional.build(engineFactory);

        Real npv = fxDoubleBarrierOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("NPV Currency " << fxDoubleBarrierOption.npvCurrency());
        BOOST_TEST_MESSAGE("FX Barrier Option NPV =                     " << npv);

        BOOST_CHECK_CLOSE(npv, expectedNPV, 0.2);
        BOOST_CHECK_CLOSE(fxDoubleBarrierOption.instrument()->NPV() * 1000000,
                          fxDoubleBarrierOptionNotional.instrument()->NPV(), 0.2);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDoubleBarrierOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXDoubleBarrierOption Parity ...");
    for (auto& f : fxdb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        Date exDate = today + Integer(f.t * 360 + 0.5);
        OptionData optionData("Long", f.optionType, "European", true, vector<string>(1, ore::data::to_string(exDate)));
        vector<Real> barriers = {f.barrierLow, f.barrierHigh};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
        tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
        BarrierData barrierDataIn("KnockIn", barriers, f.rebate, tradeBarriers);
        BarrierData barrierDataOut("KnockOut", barriers, f.rebate, tradeBarriers);
        Envelope env("CP1");
        FxDoubleBarrierOption fxDoubleBarrierInOption(env, optionData, barrierDataIn, Date(), "", 
            "EUR", 1,    // foreign
            "JPY", f.k); // domestic
        FxDoubleBarrierOption fxDoubleBarrierOutOption(env, optionData, barrierDataOut, Date(), "", 
            "EUR", 1,    // foreign
            "JPY", f.k); // domestic

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);               // domestic

        // we'll check that the results scale as expected
        // scaling the notional and the rebate by a million we should get npv_scaled = 1million * npv
        Real Notional = 1000000;
        BarrierData barrierDataScaled(f.barrierType, barriers, f.rebate * Notional, tradeBarriers);
        FxDoubleBarrierOption fxDoubleBarrierOptionNotional(env, optionData, barrierDataScaled, Date(), "",
            "EUR", Notional,        // foreign
            "JPY", Notional * f.k); // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxDoubleBarrierInOption.build(engineFactory);
        fxDoubleBarrierOutOption.build(engineFactory);
        fxOption.build(engineFactory);

        Real npv = fxDoubleBarrierInOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("NPV Currency " << fxDoubleBarrierInOption.npvCurrency());
        BOOST_TEST_MESSAGE("FX Barrier Option NPV =                     " << npv);
        BOOST_TEST_MESSAGE("FX Option NPV =                     " << fxOption.instrument()->NPV());

        // Check NPV matches expected values.
        QL_REQUIRE(fxOption.npvCurrency() == "JPY", "unexpected NPV currency ");

        BOOST_CHECK_CLOSE(fxDoubleBarrierInOption.instrument()->NPV() + fxDoubleBarrierOutOption.instrument()->NPV(),
                          fxOption.instrument()->NPV(), 0.0000000002);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDoubleBarrierOptionTouched) {
    BOOST_TEST_MESSAGE("Testing FXDoubleBarrierOption when barrier already touched...");

    struct DoubleBarrierOptionTouchedData {
        string barrierType;
        Real barrierLow;
        Real barrierHigh;
        Real rebate;
        string type;
        Real k;
        Real s;       // spot
        Real s_1;     // spot at t-1
        Real s_2;     // spot at t-2
        Rate q;       // dividend
        Rate r;       // risk-free rate
        Real t;       // time to maturity
        Volatility v; // volatility
        Real result;  // expected result
    };

    // An "In" option is equivalent to a cashflow once the barrier has been touched
    // An "Out" option has zero value once the barrier has been touched
    DoubleBarrierOptionTouchedData fxdb[] = {
        // barrierType, barrierLow, barrierHigh, rebate,   type,     k,     s,   s_1,   s_2,    q,    r,    t,    v,
        // result
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 80.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 80.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 70.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Call", 100.0, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 3.0, "Put", 100.0, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Call", 100.0, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 120.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 100.0, 130.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 100.0, 130.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 3.0, "Put", 100.0, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0}};

    for (auto& f : fxdb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();
        TimeSeries<Real> pastFixings;
        pastFixings[market->asofDate() - 1 * Days] = f.s_1;
        pastFixings[market->asofDate() - 2 * Days] = f.s_2;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
        TimeSeries<Real> pastFixingsInverted;
        pastFixingsInverted[market->asofDate() - 1 * Days] = 1 / pastFixings[market->asofDate() - 1 * Days];
        pastFixingsInverted[market->asofDate() - 2 * Days] = 1 / pastFixings[market->asofDate() - 2 * Days];
        IndexManager::instance().setHistory("Reuters JPY/EUR", pastFixingsInverted);

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrierLow, f.barrierHigh};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
        tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
        BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);

        Envelope env("CP1");

        FxDoubleBarrierOption doubleBarrierOption(env, optionData, barrierData, Date(1, Month::February, 2016), "TARGET", 
            "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");
        FxDoubleBarrierOption doubleBarrierOptionInverted(env, optionData, barrierData, Date(1, Month::February, 2016), "TARGET", 
            "EUR", 1, "JPY", f.k, "FX-Reuters-JPY-EUR");
        FxOption fxOption(env, optionData, "EUR", 1, // foreign
            "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        doubleBarrierOption.build(engineFactory);
        doubleBarrierOptionInverted.build(engineFactory);
        fxOption.build(engineFactory);

        // Check NPV matches expected values.
        if (f.barrierType == "KnockIn") {
            BOOST_CHECK_CLOSE(doubleBarrierOption.instrument()->NPV(), fxOption.instrument()->NPV(), 0.01);
            BOOST_CHECK_CLOSE(doubleBarrierOptionInverted.instrument()->NPV(), fxOption.instrument()->NPV(), 0.01);
        } else {
            BOOST_CHECK_CLOSE(doubleBarrierOption.instrument()->NPV(), 0.0, 0.01);
            BOOST_CHECK_CLOSE(doubleBarrierOptionInverted.instrument()->NPV(), 0.0, 0.01);
        }

        Settings::instance().evaluationDate() = today; // reset
        IndexManager::instance().clearHistories();     // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDoubleTouchOptionPrice) {
    BOOST_TEST_MESSAGE("Testing FXDoubleTouchOption Price...");

    // Set engineData
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("FxDoubleTouchOption") = "GarmanKohlhagen";
    engineData->engine("FxDoubleTouchOption") = "AnalyticDoubleBarrierBinaryEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";

    for (auto& f : fxdt) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);

        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        Date exDate = today + Integer(f.t * 360 + 0.5);
        vector<Real> barriers = {f.barrierLow, f.barrierHigh};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
        tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
        BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, ore::data::to_string(exDate)));
        Envelope env("CP1");
        FxDoubleTouchOption fxDoubleTouchOption(env, optionData, barrierData, "EUR", "JPY", "JPY", f.cash);

        Real expectedNPV = f.result;

        // Build and price
        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        fxDoubleTouchOption.build(engineFactory);

        Real npv = fxDoubleTouchOption.instrument()->NPV();
        string ccy = fxDoubleTouchOption.npvCurrency();

        BOOST_TEST_MESSAGE("FX Double Touch Option, NPV Currency " << ccy);
        BOOST_TEST_MESSAGE("NPV =                     " << npv);
        BOOST_TEST_MESSAGE("Expected NPV =                     " << expectedNPV);

        BOOST_CHECK_SMALL(npv - expectedNPV, 0.01);
        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDoubleTouchOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXDoubleTouchOption Parity...");
    // An "In" option and an "Out" option with the same strikes and expiries should have the same combined price as a
    // cashflow

    for (auto& f : fxdt) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

        vector<Real> barriers = {f.barrierLow, f.barrierHigh};
        vector<TradeBarrier> tradeBarriers;
        tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
        tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
        BarrierData knonkOutData("KnockOut", barriers, 0.0, tradeBarriers);
        BarrierData knonkInData("KnockIn", barriers, 0.0, tradeBarriers);

        Envelope env("CP1");

        vector<double> amounts = {f.cash};
        vector<string> dates = {"2016-08-01"};

        LegData legData(QuantLib::ext::make_shared<CashflowData>(amounts, dates), true, "JPY");
        legData.isPayer() = false;
        ore::data::Swap swap(env, {legData});

        FxDoubleTouchOption knockOutOption(env, optionData, knonkOutData, "EUR", "JPY", "JPY", f.cash);
        FxDoubleTouchOption knockInOption(env, optionData, knonkInData, "EUR", "JPY", "JPY", f.cash);

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleTouchOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleTouchOption") = "AnalyticDoubleBarrierBinaryEngine";
        engineData->model("Swap") = "DiscountedCashflows";
        engineData->engine("Swap") = "DiscountingSwapEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        swap.build(engineFactory);
        knockOutOption.build(engineFactory);
        knockInOption.build(engineFactory);

        Real npv = swap.instrument()->NPV();

        // Check NPV matches expected values.
        BOOST_CHECK_CLOSE(npv, knockOutOption.instrument()->NPV() + knockInOption.instrument()->NPV(), 0.01);

        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXDoubleTouchOptionTouched) {
    BOOST_TEST_MESSAGE("Testing FXDoubleTouchOption when barrier already touched...");

    struct DoubleTouchOptionTouchedData {
        string barrierType;
        Real barrierLow;
        Real barrierHigh;
        Real cash;
        Real s;       // spot
        Real s_1;     // spot at t-1
        Real s_2;     // spot at t-2
        Rate q;       // dividend
        Rate r;       // risk-free rate
        Real t;       // time to maturity
        Volatility v; // volatility
        Real result;  // expected result
    };

    // An "In" option is equivalent to a cashflow once the barrier has been touched
    // An "Out" option has zero value once the barrier has been touched
    DoubleTouchOptionTouchedData fxdt[] = {
        // barrierType, barrierLow, barrierHigh, cash,     s,   s_1,   s_2,    q,    r,    t,    v, result
        {"KnockIn", 80.0, 120.0, 1e6, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 80.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 70.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 1e6, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 120.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 130.0, 100.0, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 1e6, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 100.0, 70.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 70.0, 70.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 100.0, 100.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 100.0, 70.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 70.0, 70.0, 70.0, 0.04, 0.08, 0.50, 0.25, 0.0},

        {"KnockIn", 80.0, 120.0, 1e6, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 100.0, 120.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockIn", 80.0, 120.0, 1e6, 120.0, 120.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 100.0, 100.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 100.0, 120.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"KnockOut", 80.0, 120.0, 1e6, 120.0, 120.0, 120.0, 0.04, 0.08, 0.50, 0.25, 0.0}};

    vector<string> payoutCcys = {"EUR", "JPY"};
    vector<string> fxIndices = {"FX-Reuters-EUR-JPY", "FX-Reuters-JPY-EUR"};
    for (auto& f : fxdt) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = market->asofDate();
        TimeSeries<Real> pastFixings;
        pastFixings[market->asofDate() - 1 * Days] = f.s_1;
        pastFixings[market->asofDate() - 2 * Days] = f.s_2;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
        TimeSeries<Real> pastFixingsInverted;
        pastFixingsInverted[market->asofDate() - 1 * Days] = 1 / pastFixings[market->asofDate() - 1 * Days];
        pastFixingsInverted[market->asofDate() - 2 * Days] = 1 / pastFixings[market->asofDate() - 2 * Days];
        IndexManager::instance().setHistory("Reuters JPY/EUR", pastFixingsInverted);

        for (auto& payoutCcy : payoutCcys) {
            for (auto& fxIndex : fxIndices) {
                // build FXBarrierOption - expiry in 6 months
                OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));

                vector<Real> barriers = {f.barrierLow, f.barrierHigh};
                vector<TradeBarrier> tradeBarriers;
                tradeBarriers.push_back(TradeBarrier(f.barrierLow, ""));
                tradeBarriers.push_back(TradeBarrier(f.barrierHigh, ""));
                BarrierData barrierData(f.barrierType, barriers, 0.0, tradeBarriers);

                Envelope env("CP1");

                vector<double> amounts = {f.cash};
                vector<string> dates = {"2016-08-01"};

                LegData legData(QuantLib::ext::make_shared<CashflowData>(amounts, dates), true, payoutCcy);
                legData.isPayer() = false;
                ore::data::Swap swap(env, {legData});
                FxDoubleTouchOption doubleTouchOption(env, optionData, barrierData, "EUR", "JPY", payoutCcy, f.cash,
                                                      "20160201", "TARGET", fxIndex);

                // Build and price
                QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
                engineData->model("FxDoubleTouchOption") = "GarmanKohlhagen";
                engineData->engine("FxDoubleTouchOption") = "AnalyticDoubleBarrierBinaryEngine";
                engineData->model("Swap") = "DiscountedCashflows";
                engineData->engine("Swap") = "DiscountingSwapEngine";

                QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

                doubleTouchOption.build(engineFactory);
                swap.build(engineFactory);

                // Check NPV matches expected values.
                if (f.barrierType == "KnockIn")
                    BOOST_CHECK_CLOSE(doubleTouchOption.instrument()->NPV(), swap.instrument()->NPV(), 0.01);
                else
                    BOOST_CHECK_CLOSE(doubleTouchOption.instrument()->NPV(), 0.0, 0.01);
            }
        }
        Settings::instance().evaluationDate() = today; // reset
        IndexManager::instance().clearHistories();     // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXEuropeanBarrierOptionSymmetry) {
    BOOST_TEST_MESSAGE("Testing FXEuropeanBarrierOption Symmetry...");
    // For single barrier options the symmetry between put and call options is:
    // c_di(spot, strike, barrier, r, q, vol) = p_ui(strike, spot, strike*spot/barrier, q, r, vol);

    BarrierOptionData fxb[] = {
        // barrierType, barrier, rebate, type, strk,     s,    q,    r,    t,    v,  result
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
    };

    for (auto& f : fxb) {
        // build market
        QuantLib::ext::shared_ptr<Market> marketCall = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
        QuantLib::ext::shared_ptr<Market> marketPut = QuantLib::ext::make_shared<TestMarket>(f.k, f.r, f.q, f.v);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = marketCall->asofDate();

        // build FXBarrierOptions - expiry in 6 months
        OptionData optionCallData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        OptionData optionPutData("Long", "Put", "European", true, vector<string>(1, "20160801"));
        vector<Real> barriersCall = {f.barrier};
        vector<TradeBarrier> tradeBarriers_call;
        tradeBarriers_call.push_back(TradeBarrier(f.barrier, ""));
        vector<Real> barriersPut = {f.s * f.k / f.barrier};
        vector<TradeBarrier> tradeBarriers_put;
        tradeBarriers_put.push_back(TradeBarrier(f.s * f.k / f.barrier, ""));
        BarrierData barrierCallData("DownAndIn", barriersCall, f.rebate, tradeBarriers_call);
        BarrierData barrierPutData("UpAndIn", barriersPut, f.rebate, tradeBarriers_put);
        Envelope env("CP1");

        FxEuropeanBarrierOption fxCallOption(env, optionCallData, barrierCallData, "EUR", 1, // foreign
                                             "JPY", f.k);                                    // domestic
        FxEuropeanBarrierOption fxPutOption(env, optionPutData, barrierPutData, "EUR", 1,    // foreign
                                            "JPY", f.s);                                     // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDigitalOption") = "GarmanKohlhagen";
        engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactoryCall = QuantLib::ext::make_shared<EngineFactory>(engineData, marketCall);
        QuantLib::ext::shared_ptr<EngineFactory> engineFactoryPut = QuantLib::ext::make_shared<EngineFactory>(engineData, marketPut);

        fxCallOption.build(engineFactoryCall);
        fxPutOption.build(engineFactoryPut);

        Real npvCall = fxCallOption.instrument()->NPV();
        Real npvPut = fxPutOption.instrument()->NPV();

        BOOST_TEST_MESSAGE("NPV Currency " << fxCallOption.npvCurrency());
        BOOST_TEST_MESSAGE("FX Barrier Option, NPV Call " << npvCall);
        BOOST_TEST_MESSAGE("FX Barrier Option, NPV Put " << npvPut);
        // Check NPV matches expected values.
        BOOST_TEST(npvCall >= 0);
        BOOST_TEST(npvPut >= 0);
        BOOST_CHECK_CLOSE(npvCall, npvPut, 0.01);

        Settings::instance().evaluationDate() = today; // reset
    }
}

BOOST_AUTO_TEST_CASE(testFXEuropeanBarrierOptionParity) {
    BOOST_TEST_MESSAGE("Testing FXEuropeanBarrierOption Parity...");

    // An "In" option and an "Out" option with the same strikes and expiries should have the same combined price as a
    // vanilla Call option
    BarrierOptionData fxb[] = {
        // barrierType, barrier, rebate, type, strk,     s,    q,    r,    t,    v,  result
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 100.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.25, 0.0},
        {"", 95.0, 0.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 95.0, 0.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 3.0, "", 90, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 3.0, "", 100, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
        {"", 100.0, 3.0, "", 110, 100.0, 0.04, 0.08, 0.50, 0.30, 0.0},
    };

    vector<string> optionTypes = {"Call", "Put"};
    for (auto& f : fxb) {
        for (auto& optionType : optionTypes) {
            // build market
            QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v);
            Date today = Settings::instance().evaluationDate();
            Settings::instance().evaluationDate() = market->asofDate();

            // build FXBarrierOption - expiry in 6 months
            OptionData optionData("Long", optionType, "European", true, vector<string>(1, "20160801"));

            vector<Real> barriers = {f.barrier};
            vector<TradeBarrier> tradeBarriers;
            tradeBarriers.push_back(TradeBarrier(f.barrier, ""));

            BarrierData downInData("DownAndIn", barriers, f.rebate, tradeBarriers);
            BarrierData upInData("UpAndIn", barriers, f.rebate, tradeBarriers);
            BarrierData downOutData("DownAndOut", barriers, f.rebate, tradeBarriers);
            BarrierData upOutData("UpAndOut", barriers, f.rebate, tradeBarriers);

            Envelope env("CP1");

            FxOption fxOption(env, optionData, "EUR", 1, // foreign
                              "JPY", f.k);

            FxEuropeanBarrierOption downInOption(env, optionData, downInData, "EUR", 1,   // foreign
                                                 "JPY", f.k);                             // domestic
            FxEuropeanBarrierOption upInOption(env, optionData, upInData, "EUR", 1,       // foreign
                                               "JPY", f.k);                               // domestic
            FxEuropeanBarrierOption downOutOption(env, optionData, downOutData, "EUR", 1, // foreign
                                                  "JPY", f.k);                            // domestic
            FxEuropeanBarrierOption upOutOption(env, optionData, upOutData, "EUR", 1,     // foreign
                                                "JPY", f.k);                              // domestic

            vector<double> amounts = {f.rebate};
            vector<string> dates = {"2016-08-01"};
            LegData legData(QuantLib::ext::make_shared<CashflowData>(amounts, dates), false, "JPY");
            ore::data::Swap swap(env, {legData});

            // Build and price
            QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
            engineData->model("FxDigitalOption") = "GarmanKohlhagen";
            engineData->engine("FxDigitalOption") = "AnalyticEuropeanEngine";
            engineData->model("FxOption") = "GarmanKohlhagen";
            engineData->engine("FxOption") = "AnalyticEuropeanEngine";
            engineData->model("Swap") = "DiscountedCashflows";
            engineData->engine("Swap") = "DiscountingSwapEngine";

            QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

            fxOption.build(engineFactory);
            downInOption.build(engineFactory);
            upInOption.build(engineFactory);
            downOutOption.build(engineFactory);
            upOutOption.build(engineFactory);
            swap.build(engineFactory);

            Real npv = fxOption.instrument()->NPV() + swap.instrument()->NPV();

            // Check NPV matches expected values.
            BOOST_TEST(downInOption.instrument()->NPV() >= 0);
            BOOST_TEST(downOutOption.instrument()->NPV() >= 0);
            BOOST_TEST(upInOption.instrument()->NPV() >= 0);
            BOOST_TEST(upOutOption.instrument()->NPV() >= 0);
            BOOST_CHECK_CLOSE(npv, downInOption.instrument()->NPV() + downOutOption.instrument()->NPV(), 0.01);
            BOOST_CHECK_CLOSE(npv, upInOption.instrument()->NPV() + upOutOption.instrument()->NPV(), 0.01);

            Settings::instance().evaluationDate() = today; // reset
        }
    }
}

BOOST_AUTO_TEST_CASE(testFXKIKOBarrierOption) {
    BOOST_TEST_MESSAGE("Testing FXDoubleBarrierOption when barrier already touched...");

    struct KIKOBarrierOptionData {
        string knockInType;
        string knockOutType;
        Real barrierKnockIn;
        Real barrierKnockOut;
        Real rebate;
        string type;
        Real k;
        Real s;       // spot
        Rate q;       // dividend
        Rate r;       // risk-free rate
        Real t;       // time to maturity
        Volatility v; // volatility
    };

    KIKOBarrierOptionData fxdb[] = {
        // knockInBarrierType, knockOutBarrierType, barrierKnockIn, barrierKnockOut, rebate,   type,     k,     s,   q,
        // r,    t, v
        {"DownAndIn", "UpAndOut", 80.0, 120.0, 0.0, "Call", 100.0, 100.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "UpAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 80.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "DownAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 80.0, 0.04, 0.08, 0.50, 0.2},
        {"DownAndIn", "DownAndOut", 100.0, 80.0, 0.0, "Call", 100.0, 120.0, 0.04, 0.08, 0.50, 0.2}};

    // we test that the trades knocks in and knocks out as expected when seasoned
    for (auto& f : fxdb) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = today; // reset
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        vector<TradeBarrier> tradeBarriers_KI;
        tradeBarriers_KI.push_back(TradeBarrier(f.barrierKnockIn, ""));
        vector<TradeBarrier> tradeBarriers_KO;
        tradeBarriers_KO.push_back(TradeBarrier(f.barrierKnockOut, ""));
        BarrierData knockInBarrierData(f.knockInType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);
        BarrierData knockOutBarrierData(f.knockOutType, {f.barrierKnockOut}, 0.0, tradeBarriers_KO);

        vector<BarrierData> barriers = {knockInBarrierData, knockOutBarrierData};
        Envelope env("CP1");

        FxKIKOBarrierOption kikoBarrierOption(env, optionData, barriers, "EUR", 1, "JPY", f.k, "20160201", "TARGET",
                                              "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption(env, optionData, knockOutBarrierData, Date(1, Month::February, 2016), "TARGET", "EUR", 1, 
            "JPY", f.k, "FX-Reuters-EUR-JPY");

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        // knocked in npv = knockOut npv
        TimeSeries<Real> pastFixings;
        pastFixings[market->asofDate() - 1 * Days] = f.barrierKnockIn;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);

        kikoBarrierOption.reset();
        kikoBarrierOption.build(engineFactory);
        koBarrierOption.build(engineFactory);
        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), koBarrierOption.instrument()->NPV(), 0.01);
        // knocked out npv = 0
        IndexManager::instance().clearHistories(); // reset
        TimeSeries<Real> pastFixings2;
        pastFixings2[market->asofDate() - 1 * Days] = f.barrierKnockIn;
        pastFixings2[market->asofDate() - 2 * Days] = f.barrierKnockOut;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings2);
        kikoBarrierOption.reset();
        kikoBarrierOption.build(engineFactory);

        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), 0.0, 0.01);

        IndexManager::instance().clearHistories(); // reset
    }

    KIKOBarrierOptionData fxdb3[] = {
        // knockInBarrierType, knockOutBarrierType, barrierKnockIn, barrierKnockOut, rebate,   type,     k,     s,   q,
        // r,    t,    v
        {"DownAndIn", "UpAndOut", 80.0, 120.0, 0.0, "Call", 100.0, 79.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "UpAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 101.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "DownAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 101.0, 0.04, 0.08, 0.50, 0.2},
        {"DownAndIn", "DownAndOut", 100.0, 80.0, 0.0, "Call", 100.0, 99.0, 0.04, 0.08, 0.50, 0.2}};

    // we test trades that are knocked in but unseasoned
    for (auto& f : fxdb3) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = today; // reset
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        vector<TradeBarrier> tradeBarriers_KI;
        tradeBarriers_KI.push_back(TradeBarrier(f.barrierKnockIn, ""));
        vector<TradeBarrier> tradeBarriers_KO;
        tradeBarriers_KO.push_back(TradeBarrier(f.barrierKnockOut, ""));
        BarrierData knockInBarrierData(f.knockInType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);
        BarrierData knockOutBarrierData(f.knockOutType, {f.barrierKnockOut}, 0.0, tradeBarriers_KO);

        vector<BarrierData> barriers = {knockInBarrierData, knockOutBarrierData};
        Envelope env("CP1");

        FxKIKOBarrierOption kikoBarrierOption(env, optionData, barriers, "EUR", 1, "JPY", f.k, "20160201", "TARGET",
                                              "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption(env, optionData, knockOutBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        kikoBarrierOption.build(engineFactory);
        koBarrierOption.build(engineFactory);
        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), koBarrierOption.instrument()->NPV(), 0.01);
    }

    KIKOBarrierOptionData fxdb4[] = {
        // knockInBarrierType, knockOutBarrierType, barrierKnockIn, barrierKnockOut, rebate,   type,     k,     s,   q,
        // r,    t,    v
        {"DownAndIn", "UpAndOut", 80.0, 120.0, 0.0, "Call", 120.0, 121.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "UpAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 121.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "DownAndOut", 100.0, 120.0, 0.0, "Call", 100.0, 119.0, 0.04, 0.08, 0.50, 0.2},
        {"DownAndIn", "DownAndOut", 100.0, 80.0, 0.0, "Call", 100.0, 79.0, 0.04, 0.08, 0.50, 0.2}};

    // we test trades that are knocked out but unseasoned
    for (auto& f : fxdb4) {
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = today; // reset
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        vector<TradeBarrier> tradeBarriers_KI;
        tradeBarriers_KI.push_back(TradeBarrier(f.barrierKnockIn, ""));
        vector<TradeBarrier> tradeBarriers_KO;
        tradeBarriers_KO.push_back(TradeBarrier(f.barrierKnockOut, ""));
        BarrierData knockInBarrierData(f.knockInType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);
        BarrierData knockOutBarrierData(f.knockOutType, {f.barrierKnockOut}, 0.0, tradeBarriers_KO);

        vector<BarrierData> barriers = {knockInBarrierData, knockOutBarrierData};
        Envelope env("CP1");

        FxKIKOBarrierOption kikoBarrierOption(env, optionData, barriers, "EUR", 1, "JPY", f.k, "20160201", "TARGET",
                                              "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption(env, optionData, knockOutBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        // knocked out npv = 0
        kikoBarrierOption.build(engineFactory);

        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), 0.0, 0.01);
    }

    // we also test the cases where the knockOut barrier is an extreme value, unlikely to be triggered.
    // In this case we expect Kiko_npv == ki_npv
    KIKOBarrierOptionData fxdb2[] = {
        // knockInBarrierType, knockOutBarrierType, barrierKnockIn, barrierKnockOut, rebate,   type,     k,     s,   q,
        // r,    t
        {"DownAndIn", "UpAndOut", 80.0, 1000000.0, 0.0, "Call", 100.0, 100.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "UpAndOut", 150.0, 1000000.0, 0.0, "Call", 100.0, 80.0, 0.04, 0.08, 0.50, 0.2},
        {"UpAndIn", "DownAndOut", 150.0, 0.000001, 0.0, "Call", 100.0, 80.0, 0.04, 0.08, 0.50, 0.2},
        {"DownAndIn", "DownAndOut", 100.0, 0.000001, 0.0, "Call", 100.0, 120.0, 0.04, 0.08, 0.50, 0.2}};

    for (auto& f : fxdb2) {
        BOOST_TEST_MESSAGE("testing " << f.knockInType << " " << f.knockOutType);
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = today; // reset
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        vector<TradeBarrier> tradeBarriers_KI;
        tradeBarriers_KI.push_back(TradeBarrier(f.barrierKnockIn, ""));
        vector<TradeBarrier> tradeBarriers_KO;
        tradeBarriers_KO.push_back(TradeBarrier(f.barrierKnockOut, ""));
        BarrierData knockInBarrierData(f.knockInType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);
        BarrierData knockOutBarrierData(f.knockOutType, {f.barrierKnockOut}, 0.0, tradeBarriers_KO);
        BarrierData knockOutBarrierData2(f.knockOutType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);

        vector<BarrierData> barriers = {knockInBarrierData, knockOutBarrierData};
        Envelope env("CP1");

        FxKIKOBarrierOption kikoBarrierOption(env, optionData, barriers, "EUR", 1, "JPY", f.k, "20160201", "TARGET",
                                              "FX-Reuters-EUR-JPY");
        FxBarrierOption kiBarrierOption(env, optionData, knockInBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k,  "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption(env, optionData, knockOutBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption2(env, optionData, knockOutBarrierData2, Date(1, Month::February, 2016),
                                         "TARGET", "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");

        vector<Real> barrier = {std::min(f.barrierKnockIn, f.barrierKnockOut),
                                std::max(f.barrierKnockIn, f.barrierKnockOut)};
        vector<TradeBarrier> tradeBarriers = {TradeBarrier(std::min(f.barrierKnockIn, f.barrierKnockOut), ""),
                                              TradeBarrier(std::max(f.barrierKnockIn, f.barrierKnockOut), "")};
        BarrierData barrierData("KnockOut", barrier, 0, tradeBarriers);

        FxDoubleBarrierOption dkoBarrierOption(env, optionData, barrierData, Date(), "", "EUR", 1, // foreign
            "JPY", f.k);                            // domestic

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
                          "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        TimeSeries<Real> pastFixings;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
        // untouched  kiko_npv = untouched ki_npv
        kikoBarrierOption.build(engineFactory);
        kiBarrierOption.build(engineFactory);
        koBarrierOption.build(engineFactory);
        koBarrierOption2.build(engineFactory);
        dkoBarrierOption.build(engineFactory);
        fxOption.build(engineFactory);

        BOOST_TEST_MESSAGE("KIKO NPV: " << kikoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KI NPV: " << kiBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockoutLevel) NPV: " << koBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockinLevel) NPV: " << koBarrierOption2.instrument()->NPV());
        BOOST_TEST_MESSAGE("DoubleKnockOut NPV: " << dkoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("FXOption NPV: " << fxOption.instrument()->NPV());

        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), kiBarrierOption.instrument()->NPV(), 0.01);

        // knocked in  kiko_npv = knocked in ki_npv
        pastFixings[market->asofDate() - 1 * Days] = f.barrierKnockIn;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);

        kikoBarrierOption.reset();
        kiBarrierOption.reset();
        dkoBarrierOption.reset();
        kikoBarrierOption.build(engineFactory);
        kiBarrierOption.build(engineFactory);
        fxOption.build(engineFactory);

        kikoBarrierOption.build(engineFactory);
        kiBarrierOption.build(engineFactory);
        koBarrierOption.build(engineFactory);
        dkoBarrierOption.build(engineFactory);
        fxOption.build(engineFactory);
        
        BOOST_TEST_MESSAGE("KIKO NPV: " << kikoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KI NPV: " << kiBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockoutLevel) NPV: " << koBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockinLevel) NPV: " << koBarrierOption2.instrument()->NPV());
        BOOST_TEST_MESSAGE("DoubleKnockOut NPV: " << dkoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("FXOption NPV: " << fxOption.instrument()->NPV());


        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), kiBarrierOption.instrument()->NPV(), 0.01);

        IndexManager::instance().clearHistories(); // reset
    }


    // we test that when the spot value is updated the trade behaves as expected. 
    KIKOBarrierOptionData fxdb5[] = {
        // knockInBarrierType, knockOutBarrierType, barrierKnockIn, barrierKnockOut, rebate,   type,     k,     s,   q,
        // r,    t
        {"UpAndIn", "UpAndOut", 80.0, 150.0, 0.0, "Call", 100.0, 70.0, 0.04, 0.08, 0.50, 0.2},
        {"DownAndIn", "DownAndOut", 150.0, 80, 0.0, "Call", 100.0, 160.0, 0.04, 0.08, 0.50, 0.2}};

    for (auto& f : fxdb5) {
        BOOST_TEST_MESSAGE("testing " << f.knockInType << " " << f.knockOutType);
        // build market
        QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(f.s, f.q, f.r, f.v, true);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().evaluationDate() = today; // reset
        Settings::instance().evaluationDate() = market->asofDate();

        // build FXBarrierOption - expiry in 6 months
        OptionData optionData("Long", "Call", "European", true, vector<string>(1, "20160801"));
        vector<TradeBarrier> tradeBarriers_KI = {TradeBarrier(f.barrierKnockIn, "")};
        vector<TradeBarrier> tradeBarriers_KO = {TradeBarrier(f.barrierKnockOut, "")};
        BarrierData knockInBarrierData(f.knockInType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);
        BarrierData knockOutBarrierData(f.knockOutType, {f.barrierKnockOut}, 0.0, tradeBarriers_KO);
        BarrierData knockOutBarrierData2(f.knockOutType, {f.barrierKnockIn}, 0.0, tradeBarriers_KI);

        vector<BarrierData> barriers = {knockInBarrierData, knockOutBarrierData};
        Envelope env("CP1");

        FxKIKOBarrierOption kikoBarrierOption(env, optionData, barriers, "EUR", 1, "JPY", f.k, "20160201", "TARGET",
                                              "FX-Reuters-EUR-JPY");
        FxBarrierOption kiBarrierOption(env, optionData, knockInBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k,  "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption(env, optionData, knockOutBarrierData, Date(1, Month::February, 2016), "TARGET",
                                        "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");
        FxBarrierOption koBarrierOption2(env, optionData, knockOutBarrierData2, Date(1, Month::February, 2016),
                                         "TARGET", "EUR", 1, "JPY", f.k, "FX-Reuters-EUR-JPY");

        vector<Real> barrier = {std::min(f.barrierKnockIn, f.barrierKnockOut),
                                std::max(f.barrierKnockIn, f.barrierKnockOut)};
        vector<TradeBarrier> tradeBarriers = {TradeBarrier(std::min(f.barrierKnockIn, f.barrierKnockOut), ""),
                                              TradeBarrier(std::max(f.barrierKnockIn, f.barrierKnockOut), "")};
        BarrierData barrierData("KnockOut", barrier, 0, tradeBarriers);

        FxDoubleBarrierOption dkoBarrierOption(env, optionData, barrierData, Date(), "",
            "EUR", 1,    // foreign
            "JPY", f.k); // domestic

        FxOption fxOption(env, optionData, "EUR", 1, // foreign
            "JPY", f.k);               // domestic

        // Build and price
        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxDoubleBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxDoubleBarrierOption") = "AnalyticDoubleBarrierEngine";
        engineData->model("FxBarrierOption") = "GarmanKohlhagen";
        engineData->engine("FxBarrierOption") = "AnalyticBarrierEngine";
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";

        QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

        TimeSeries<Real> pastFixings;
        IndexManager::instance().setHistory("Reuters EUR/JPY", pastFixings);
        
        kikoBarrierOption.build(engineFactory);
        kiBarrierOption.build(engineFactory);
        koBarrierOption.build(engineFactory);
        koBarrierOption2.build(engineFactory);
        dkoBarrierOption.build(engineFactory);
        fxOption.build(engineFactory);

        BOOST_TEST_MESSAGE("KIKO NPV: " << kikoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KI NPV: " << kiBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockoutLevel) NPV: " << koBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("KO(knockinLevel) NPV: " << koBarrierOption2.instrument()->NPV());
        BOOST_TEST_MESSAGE("DoubleKnockOut NPV: " << dkoBarrierOption.instrument()->NPV());
        BOOST_TEST_MESSAGE("FXOption NPV: " << fxOption.instrument()->NPV());

        //check trade knockedIn
        dynamic_pointer_cast<TestMarket>(market)->setFxSpot("EURJPY", f.barrierKnockIn);
        BOOST_CHECK_CLOSE(kikoBarrierOption.instrument()->NPV(), koBarrierOption.instrument()->NPV(), 0.01);
        
        //check trade knockedOut
        dynamic_pointer_cast<TestMarket>(market)->setFxSpot("EURJPY", f.barrierKnockOut);
        BOOST_CHECK_SMALL(kikoBarrierOption.instrument()->NPV(), 0.0001);

        IndexManager::instance().clearHistories(); // reset
    }

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
