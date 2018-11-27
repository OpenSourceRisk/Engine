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

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/make_shared.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/simpledaycounter.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket(Real hazardRate, Real recoveryRate, Real liborRate) {
        asof_ = Date(3, Feb, 2016);
        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            flatRateYts(liborRate);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(hazardRate);
        recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(recoveryRate));
        // build ibor index
        Handle<IborIndex> hEUR(ore::data::parseIborIndex(
            "EUR-EURIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
        yts->enableExtrapolation();
        return Handle<YieldTermStructure>(yts);
    }
    Handle<DefaultProbabilityTermStructure> flatRateDcs(Real forward) {
        boost::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, SimpleDayCounter()));
        return Handle<DefaultProbabilityTermStructure>(dcs);
    }
};

struct CommonVars {
    // global data
    string ccy;
    string creditCurveId;
    string issuerId;
    bool isPayer;
    string start;
    string issue;
    string fixtenor;
    Calendar cal;
    string calStr;
    string conv;
    string rule;
    Size days;
    string fixDC;
    string settledays;
    bool isinarrears;
    Real notional;
    vector<Real> notionals;
    vector<Real> spread;

    // utilities
    boost::shared_ptr<ore::data::CreditDefaultSwap> makeCDS(string end, Real rate) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        // build CDS
        boost::shared_ptr<FixedLegData> fixedLegRateData = boost::make_shared<FixedLegData>(vector<double>(1, rate));
        LegData fixedLegData(fixedLegRateData, isPayer, ccy, fixedSchedule, fixDC, notionals);

        CreditDefaultSwapData cd(issuerId, creditCurveId, fixedLegData, false, true);
        Envelope env("CP1");

        boost::shared_ptr<ore::data::CreditDefaultSwap> cds(new ore::data::CreditDefaultSwap(env, cd));
        return cds;
    }

    CommonVars()
        : ccy("EUR"), creditCurveId("CreditCurve_A"), issuerId("CPTY_A"), isPayer(false), start("20160203"),
          issue("20160203"), fixtenor("1Y") {
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/365";
        settledays = "2";
        isinarrears = false;
        notional = 1;
        notionals.push_back(1);
        spread.push_back(0.0);
    }
};

struct MarketInputs {
    Real hazardRate;
    Real recoveryRate;
    Real liborRate;
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the MarketInputs
ostream& operator<<(ostream& os, const MarketInputs& m) {
    return os << "[" << m.hazardRate << "," << m.recoveryRate << "," << m.liborRate << "]";
}

struct TradeInputs {
    string endDate;
    Real fixedRate;
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the TradeInputs
ostream& operator<<(ostream& os, const TradeInputs& t) {
    return os << "[" << t.endDate << "," << t.fixedRate << "]";
}

Real cdsNpv(const MarketInputs& m, const TradeInputs& t) {

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>(m.hazardRate, m.recoveryRate, m.liborRate);
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    boost::shared_ptr<ore::data::CreditDefaultSwap> cds = vars.makeCDS(t.endDate, t.fixedRate);

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("CreditDefaultSwap") = "DiscountedCashflows";
    engineData->engine("CreditDefaultSwap") = "MidPointCdsEngine";

    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    cds->build(engineFactory);

    return cds->instrument()->NPV();
}

// Cases below:
// 1) HazardRate = 0, couponRate = 0. ExpectedNpv = 0
// 2) RecoveryRate = 1, couponRate = 0. ExpectedNpv = 0
// 3) Example from Hull, Ch 21 (pp. 510 - 513). Take RR = 1 to show only couponNPV.
// 4) Example from Hull, Ch 21 (pp. 510 - 513). Take coupon rate = 0 to show only defaultNPV.

// Market inputs used in the test below
MarketInputs marketInputs[] = {
    { 0, 1, 0 },
    { 1, 1, 0 },
    { 0.02, 1, 0.05 },
    { 0.02, 0.4, 0.05 }
};

// Trade inputs used in the test below
TradeInputs tradeInputs[] = {
    { "20170203", 0 },
    { "20170203", 0 },
    { "20210203", 0.0124248849209095 },
    { "20210203", 0.0 }
};

// Expected NPVs given the market and trade inputs above
Real expNpvs[] = { 0, 0, 0.050659, -0.05062 };

}

BOOST_AUTO_TEST_SUITE(OREDataTestSuite)

BOOST_AUTO_TEST_SUITE(CreditDefaultSwapTests)

BOOST_DATA_TEST_CASE(testCreditDefaultSwap, 
    bdata::make(marketInputs) ^ bdata::make(tradeInputs) ^ bdata::make(expNpvs), mkt, trd, exp) {
    
    BOOST_CHECK_CLOSE(cdsNpv(mkt, trd), exp, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
