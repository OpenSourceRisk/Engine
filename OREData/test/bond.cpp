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
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
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
#include <test/bond.hpp>

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
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BANK_EUR_LEND")] = flatRateYts(0.02);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            flatRateYts(0.02);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(0.0);
        // recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
        //     Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        recoveryRates_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        // build ibor index
        Handle<IborIndex> hEUR(ore::data::parseIborIndex(
            "EUR-EURIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;

        // add Eurib 6M fixing
        hEUR->addFixing(Date(1, Feb, 2016), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2017), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2018), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2019), -0.00191);
        hEUR->addFixing(Date(31, Jan, 2019), -0.00191);
        hEUR->addFixing(Date(30, Jan, 2020), -0.00191);
    }

    TestMarket(Real defaultFlatRate) {
        asof_ = Date(3, Feb, 2016);
        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BANK_EUR_LEND")] = flatRateYts(0.02);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(defaultFlatRate);
        // recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
        //     Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
        recoveryRates_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.00));
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
        yts->enableExtrapolation();
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
    string creditCurveId;
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

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(
            env, issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr, issue, fixedLegData));
        return bond;
    }
    
    boost::shared_ptr<ore::data::Bond> makeAmortizingFixedBond(string amortType, Real value, bool underflow) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        FixedLegData fixedLegRateData(vector<double>(1, fixedRate));
        AmortizationData amortizationData(amortType, value, start, end, fixtenor, underflow);
        LegData fixedLegData(isPayer, ccy, fixedLegRateData, fixedSchedule, fixDC, notionals, vector<string>(), conv, 
                false, false, false, true, "", 0, "", 0, amortizationData);

        Envelope env("CP1");

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(
            env, issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr, issue, fixedLegData));
        return bond;
    }

    boost::shared_ptr<ore::data::Bond> makeAmortizingFloatingBond(string amortType, Real value, bool underflow) {
        ScheduleData floatingSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        FloatingLegData floatingLegRateData("EUR-EURIBOR-6M", 2, false, spread);
        AmortizationData amortizationData(amortType, value, start, end, fixtenor, underflow);
        LegData floatingLegData(isPayer, ccy, floatingLegRateData, floatingSchedule, fixDC, notionals, vector<string>(), conv, 
                false, false, false, true, "", 0, "", 0, amortizationData);
                
        Envelope env("CP1");

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(
            env, issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr, issue, floatingLegData));
        return bond;
    }

    boost::shared_ptr<ore::data::Bond> makeZeroBond() {
        Envelope env("CP1");

        boost::shared_ptr<ore::data::Bond> bond(new ore::data::Bond(
            env, issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr, notional, end, ccy, issue));
        return bond;
    }

    CommonVars() {
        ccy = "EUR";
        securityId = "Security1";
        creditCurveId = "CreditCurve_A";
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
} // namespace

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

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    bond->build(engineFactory);

    Real npv = bond->instrument()->NPV();
    Real expectedNpv = 9048374.18;

    BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);
}


void BondTest::testAmortizingBond() {
    BOOST_TEST_MESSAGE("Testing Zero Bond...");

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Date today = Date(30, Jan, 2021);
    Settings::instance().evaluationDate() = today;//market->asofDate();

    CommonVars vars;
    vector<boost::shared_ptr<ore::data::Bond>> bonds;
    boost::shared_ptr<ore::data::Bond> bondFixedAmount = vars.makeAmortizingFixedBond("FixedAmount", 2500000, true);
    bonds.push_back(bondFixedAmount);

    boost::shared_ptr<ore::data::Bond> bondRelativeInitial = vars.makeAmortizingFixedBond("RelativeToInitialNotional", 0.25, true);
    bonds.push_back(bondRelativeInitial);




    
    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    Real npvTol = 0.5;

    for( auto& b: bonds) {
        b->build(engineFactory);

        Real npv = b->instrument()->NPV();
        Real expectedNpv = 0.0;

        BOOST_CHECK(std::fabs(npv - expectedNpv) < npvTol);
    }

  
    boost::shared_ptr<ore::data::Bond> bondRelativePrevious = vars.makeAmortizingFixedBond("RelativeToPreviousNotional", 0.25, true);
    bondRelativePrevious->build(engineFactory);

    boost::shared_ptr<QuantLib::Instrument> inst1 = bondRelativePrevious->instrument()->qlInstrument();
    boost::shared_ptr<QuantLib::Bond> qlBond1 = boost::dynamic_pointer_cast<QuantLib::Bond>(inst1);
    Real expectedNotional = 3164062.5;

    Real notional = qlBond1->notionals()[qlBond1->notionals().size()-2];
    
    BOOST_CHECK_CLOSE(notional, expectedNotional, 1);

    boost::shared_ptr<ore::data::Bond> bondFixedAnnuity = vars.makeAmortizingFixedBond("Annuity", 2500000, true);
    bondFixedAnnuity->build(engineFactory);

    boost::shared_ptr<QuantLib::Instrument> inst2 = bondFixedAnnuity->instrument()->qlInstrument();
    boost::shared_ptr<QuantLib::Bond> qlBond2 = boost::dynamic_pointer_cast<QuantLib::Bond>(inst2);
    expectedNotional = 1380908.447;

    notional = qlBond2->notionals()[qlBond2->notionals().size()-2];

    BOOST_CHECK(std::fabs(notional - expectedNotional) < npvTol);

    boost::shared_ptr<ore::data::Bond> bondFloatingAnnuity = vars.makeAmortizingFloatingBond("Annuity", 2500000, true);
    bondFloatingAnnuity->build(engineFactory);

    boost::shared_ptr<QuantLib::Instrument> inst3 = bondFloatingAnnuity->instrument()->qlInstrument();
    boost::shared_ptr<QuantLib::Bond> qlBond3 = boost::dynamic_pointer_cast<QuantLib::Bond>(inst3);
    Real expectedAmount = 93.41;

    Real amount = qlBond3->cashflows()[qlBond3->cashflows().size()-2]->amount();
    
    BOOST_CHECK(std::fabs(amount - expectedAmount) < npvTol);



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

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

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
    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

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
    // TODO: work out expected value properly
    // suite->add(BOOST_TEST_CASE(&BondTest::testZeroBond));
    suite->add(BOOST_TEST_CASE(&BondTest::testBondZeroSpreadDefault));
    suite->add(BOOST_TEST_CASE(&BondTest::testBondCompareDefault));
    suite->add(BOOST_TEST_CASE(&BondTest::testAmortizingBond));
    return suite;
}
} // namespace testsuite
