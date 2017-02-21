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


#include <test/bond.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <boost/make_shared.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>

#include <iostream>

using std::string;

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

// Bond test

namespace {

class TestMarket : public MarketImpl {
    public:
        TestMarket() {
            asof_ = Date(3, Feb, 2016);
            // build discount
            yieldCurves_[make_pair(Market::defaultConfiguration, "BANK_EUR_LEND")] = flatRateYts(0.02);
            defaultCurves_[make_pair(Market::defaultConfiguration, "CPTY_A")] = flatRateDcs(0.0);
            recoveryRates_[make_pair(Market::defaultConfiguration, "CPTY_A")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
            securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        }

        TestMarket(Real defaultFlatRate) {
            asof_ = Date(3, Feb, 2016);
            // build discount
            yieldCurves_[make_pair(Market::defaultConfiguration, "BANK_EUR_LEND")] = flatRateYts(0.02);
            defaultCurves_[make_pair(Market::defaultConfiguration, "CPTY_A")] = flatRateDcs(defaultFlatRate);
            recoveryRates_[make_pair(Market::defaultConfiguration, "CPTY_A")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
            securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        }

    private:
        Handle<YieldTermStructure> flatRateYts(Real forward) {
            boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
            return Handle<YieldTermStructure>(yts);
        }
        Handle<DefaultProbabilityTermStructure> flatRateDcs(Real forward) {
            boost::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, ActualActual()));
            return Handle<DefaultProbabilityTermStructure>(dcs);
        }

};

struct CommonVars {
    // global data
    string ccy;
    string securityId;
    string issuerId;
    string referenceCurveId;
    bool isPayer;
    string start;
    string end;
    string issue;
    string fixtenor;
    Calendar cal;
    string calStr;
    string conv;
    string rule;
    Size days;
    string fixDC;
    Real fixedRate;
    string settledays;
    bool isinarrears;
    Real notional;
    vector<Real> notionals;
    vector<Real> spread;

    // utilities
    boost::shared_ptr<ore::data::Bond> makeBond() {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        // build CMSSwap
        FixedLegData fixedLegRateData(vector<double>(1, fixedRate));
        LegData fixedLegData(isPayer, ccy, fixedLegRateData, fixedSchedule, fixDC, notionals);

        Envelope env("CP1");

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(env, issuerId, securityId, referenceCurveId, settledays, calStr, issue, fixedLegData));
        return bond;
    }

    boost::shared_ptr<ore::data::Bond> makeZeroBond() {
        Envelope env("CP1");

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(env, issuerId, securityId, referenceCurveId, settledays, calStr, notional, end, ccy, issue));
        return bond;
    }

    CommonVars() {
        ccy = "EUR";
        securityId = "Security1";
        issuerId = "CPTY_A";
        referenceCurveId = "BANK_EUR_LEND";
        isPayer = false;
        start = "20160203";
        end = "20210203";
        issue = "20160203";
        fixtenor = "1Y";
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/ACT";
        fixedRate = 0.05;
        settledays = "2";
        isinarrears = false;
        notional = 10000000;
        notionals.push_back(10000000);
        spread.push_back(0.0);
    }
};

}

namespace testsuite {

void BondTest::testZeroBond() {
    BOOST_TEST_MESSAGE("Testing Zero Bond...");

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    boost::shared_ptr<ore::data::Bond> bond = vars.makeZeroBond();

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    bond->build(engineFactory);

    Real npv = bond->instrument()->NPV();
    Real expectedNpv = 11403727.39;

    BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);
}

void BondTest::testBondZeroSpreadDefault() {
    BOOST_TEST_MESSAGE("Testing Bond price...");

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    boost::shared_ptr<ore::data::Bond> bond = vars.makeBond();

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    bond->build(engineFactory);

    Real npv = bond->instrument()->NPV();
    Real expectedNpv = 11403727.39;

    BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);

}

void BondTest::testBondCompareDefault() {
    BOOST_TEST_MESSAGE("Testing Bond price...");

    // build market
    boost::shared_ptr<Market> market1 = boost::make_shared<TestMarket>(0.0);
    boost::shared_ptr<Market> market2 = boost::make_shared<TestMarket>(0.5);
    boost::shared_ptr<Market> market3 = boost::make_shared<TestMarket>(0.99);
    Settings::instance().evaluationDate() = market1->asofDate();

    CommonVars vars;
    boost::shared_ptr<ore::data::Bond> bond = vars.makeBond();

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    boost::shared_ptr<EngineFactory> engineFactory1 = boost::make_shared<EngineFactory>(engineData, market1);
    boost::shared_ptr<EngineFactory> engineFactory2 = boost::make_shared<EngineFactory>(engineData, market2);
    boost::shared_ptr<EngineFactory> engineFactory3 = boost::make_shared<EngineFactory>(engineData, market3);

    bond->build(engineFactory1);
    Real npv1 = bond->instrument()->NPV();
    bond->build(engineFactory2);
    Real npv2 = bond->instrument()->NPV();
    bond->build(engineFactory3);
    Real npv3 = bond->instrument()->NPV();
    
    BOOST_CHECK((npv1 > npv2) && (npv2 > npv3));
    
 //   BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);

}


test_suite* BondTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("BondTest");
    suite->add(BOOST_TEST_CASE(&BondTest::testZeroBond));
    suite->add(BOOST_TEST_CASE(&BondTest::testBondZeroSpreadDefault));
    suite->add(BOOST_TEST_CASE(&BondTest::testBondCompareDefault));
    return suite;
}
}
