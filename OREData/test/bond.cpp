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
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <boost/make_shared.hpp>
#include <ored/utilities/indexparser.hpp>

#include <iostream>

using std::string;

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace oreplus::data;

// Bond test

namespace {

class TestMarket : public MarketImpl {
    public:
        TestMarket() {
            asof_ = Date(3, Feb, 2016);
            // build discount
            discountCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateYts(0.02);
        }

    private:
        Handle<YieldTermStructure> flatRateYts(Real forward) {
            boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
            return Handle<YieldTermStructure>(yts);
        }
};

struct CommonVars {
    // global data
    string ccy;
    bool isPayer;
    string start;
    string end;
    string fixtenor;
    Calendar cal;
    string calStr;
    string conv;
    string rule;
    Size days;
    string fixDC;
    Real fixedRate;
    string index;
    int fixingdays;
    bool isinarrears;
    double notional;
    vector<double> notionals;
    vector<double> spread;

    // utilities
    boost::shared_ptr<Bond> makeBond() {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        // build CMSSwap
        FixedLegData fixedLegRateData(vector<double>(1, fixedRate));
        LegData fixedLegData(isPayer, ccy, fixedLegRateData, fixedSchedule, fixDC, notionals);

        Envelope env("CP1");

        boost::shared_ptr<Bond> swap(new Bond(env, fixedLegData));
        return swap;
    }

    CommonVars() {
        ccy = "EUR";
        isPayer = false;
        start = "20160301";
        end = "20360301";
        fixtenor = "1Y";
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/360";
        fixedRate = 0.0;
        index = "EUR-CMS-30Y";
        fixingdays = 2;
        isinarrears = false;
        notional = 10000000;
        notionals.push_back(10000000);
        spread.push_back(0.0);
    }
};

}

namespace testsuite {

void BondTest::testBondPrice() {
    BOOST_TEST_MESSAGE("Testing Bond price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
}

test_suite* BondTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("BondTest");
    suite->add(BOOST_TEST_CASE(&BondTest::testBondPrice));
    return suite;
}
}
