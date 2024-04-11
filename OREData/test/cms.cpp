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
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <iostream>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

// CMS Swap test

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);

        // build swaption vols
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.1);

        // build ibor index
        Handle<IborIndex> hEUR(ore::data::parseIborIndex(
            "EUR-EURIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;

        QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
        
        // add swap index
        QuantLib::ext::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
            "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
        conventions->add(swapEURConv);
        QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
            new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
        conventions->add(swapIndexEURLongConv);

        InstrumentConventions::instance().setConventions(conventions);

        addSwapIndex("EUR-CMS-30Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<SwaptionVolatilityStructure> flatRateSvs(Volatility forward) {
        QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> Svs(
            new ConstantSwaptionVolatility(0, NullCalendar(), ModifiedFollowing, forward, ActualActual(ActualActual::ISDA)));
        return Handle<SwaptionVolatilityStructure>(Svs);
    }
};

struct CommonVars {
    // global data
    string ccy;
    bool isPayer;
    string start;
    string end;
    string fixtenor;
    string cmstenor;
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
    string longShort;
    vector<double> notionals;
    vector<double> spread;
    vector<string> spreadDates;

    // utilities
    QuantLib::ext::shared_ptr<ore::data::Swap> makeSwap() {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        // build CMSSwap
        LegData fixedLegData(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), !isPayer, ccy,
                             fixedSchedule, fixDC, notionals);
        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread), isPayer, ccy,
                           cmsSchedule, fixDC, notionals);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, fixedLegData, cmsLegData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeSwap(Real fixedRate_, string fixtenor_) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor_, calStr, conv, conv, rule));
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        // build CMSSwap
        LegData fixedLegData(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate_)), !isPayer, ccy,
                             fixedSchedule, fixDC, notionals);
        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread), isPayer, ccy,
                           cmsSchedule, fixDC, notionals);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, fixedLegData, cmsLegData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeCmsLegSwap() {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        CMSLegData cmsLegRateData;
        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread), isPayer, ccy,
                           cmsSchedule, fixDC, notionals);
        legData.push_back(cmsLegData);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeCappedCmsLegSwap(vector<double> caps,
                                                            vector<string> capDates = vector<string>()) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        LegData cmsLegData(
            QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread, spreadDates, caps, capDates),
            isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(cmsLegData);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::CapFloor> makeCap(vector<double> caps) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread, spreadDates), isPayer,
                           ccy, cmsSchedule, fixDC, notionals);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::CapFloor> capfloor(
            new ore::data::CapFloor(env, longShort, cmsLegData, caps, vector<double>()));
        return capfloor;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeFlooredCmsLegSwap(vector<double> floors,
                                                             vector<string> floorDates = vector<string>()) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread, spreadDates,
                                                          vector<double>(), vector<string>(), floors, floorDates),
                           isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(cmsLegData);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::CapFloor> makeFloor(vector<double> floors) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        LegData cmsLegData(QuantLib::ext::make_shared<CMSLegData>(index, fixingdays, isinarrears, spread, spreadDates), isPayer,
                           ccy, cmsSchedule, fixDC, notionals);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::CapFloor> capfloor(
            new ore::data::CapFloor(env, longShort, cmsLegData, vector<double>(), floors));
        return capfloor;
    }

    CommonVars() : ccy("EUR"), isPayer(false), start("20160301"), end("20360301"), fixtenor("1Y"), cmstenor("6M") {

        longShort = isPayer ? "Short" : "Long";
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

void outputCoupons(QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap) {
    Leg leg = cmsSwap->legs().at(1);
    for (auto cf : leg) {
        QuantLib::ext::shared_ptr<FloatingRateCoupon> frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
        BOOST_TEST_MESSAGE("Coupon Date: " << frc->date() << "; Rate: " << frc->rate()
                                           << "; DayCount: " << frc->dayCounter());
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CmsTests)

BOOST_AUTO_TEST_CASE(testCMSAnalyticHagan) {
    BOOST_TEST_MESSAGE("Testing CMS Analytic Hagan price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "Hagan";
    engineData->engine("CMS") = "Analytic";
    map<string, string> engineparams;
    engineparams["YieldCurveModel"] = "Standard";
    engineparams["MeanReversion"] = "0.0";
    engineData->engineParameters("CMS") = engineparams;

    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    cmsSwap->build(engineFactory);

    Real expectedNPV = 3440673.46;
    Real npv = cmsSwap->instrument()->NPV();

    BOOST_TEST_MESSAGE("Hagan Analytic price is " << npv);
    outputCoupons(cmsSwap);

    BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
}

BOOST_AUTO_TEST_CASE(testCMSNumericalHagan) {
    BOOST_TEST_MESSAGE("Testing CMS Numerical Hagan price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "Hagan";
    engineData->engine("CMS") = "Numerical";
    map<string, string> engineparams;
    engineparams["YieldCurveModel"] = "Standard";
    engineparams["MeanReversion"] = "0.0";
    engineparams["UpperLimit"] = "0.0";
    engineparams["LowerLimit"] = "1.0";
    engineparams["Precision"] = "0.000001";
    engineData->engineParameters("CMS") = engineparams;

    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    cmsSwap->build(engineFactory);

    Real expectedNPV = 3440673.46;
    Real npv = cmsSwap->instrument()->NPV();

    BOOST_TEST_MESSAGE("Hagan Numerical price is " << npv);
    outputCoupons(cmsSwap);

    BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
}

BOOST_AUTO_TEST_CASE(testCMSLinearTsr) {
    BOOST_TEST_MESSAGE("Testing CMS Linear TSR price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "LinearTSR";
    engineData->engine("CMS") = "LinearTSRPricer";
    map<string, string> engineparams;
    engineparams["MeanReversion"] = "0.0";
    engineparams["Policy"] = "RateBound";
    engineparams["LowerRateBoundLogNormal"] = "0.0001";
    engineparams["UpperRateBoundLogNormal"] = "2.0000";
    engineData->engineParameters("CMS") = engineparams;

    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    cmsSwap->build(engineFactory);

    Real expectedNPV = 3440673.46;
    Real npv = cmsSwap->instrument()->NPV();

    BOOST_TEST_MESSAGE("Linear TSR price is " << npv);
    outputCoupons(cmsSwap);

    BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
}

BOOST_AUTO_TEST_CASE(cmsCapFloor) {
    BOOST_TEST_MESSAGE("Testing CMS CapFloor price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "Hagan";
    engineData->engine("CMS") = "Analytic";
    map<string, string> engineparams;
    engineparams["YieldCurveModel"] = "Standard";
    engineparams["MeanReversion"] = "0.0";
    engineData->engineParameters("CMS") = engineparams;

    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    BOOST_TEST_MESSAGE(
        "Comparing CMS Cap price to replication by a Single Legged CMS Swap and a Single Leg Capped CMS Swap...");
    vector<double> capRate(1, 0.021);
    QuantLib::ext::shared_ptr<ore::data::Swap> cmsLegSwap = vars.makeCmsLegSwap();
    QuantLib::ext::shared_ptr<ore::data::Swap> cappedCmsLegSwap = vars.makeCappedCmsLegSwap(capRate);
    QuantLib::ext::shared_ptr<ore::data::CapFloor> cap = vars.makeCap(capRate);

    cmsLegSwap->build(engineFactory);
    cappedCmsLegSwap->build(engineFactory);
    cap->build(engineFactory);

    Real cmsLegNpv = cmsLegSwap->instrument()->NPV();
    Real cappedCmsLegNpv = cappedCmsLegSwap->instrument()->NPV();
    Real capNpv = cap->instrument()->NPV();

    Real capBySwaps = cmsLegNpv - cappedCmsLegNpv;

    BOOST_TEST_MESSAGE("CMS Leg swap NPV is " << cmsLegNpv);
    BOOST_TEST_MESSAGE("CMS Capped Leg swap NPV is " << cappedCmsLegNpv);
    BOOST_TEST_MESSAGE("CMS Cap NPV is " << capNpv);
    BOOST_TEST_MESSAGE("CMS Cap NPV from Swap replication is " << capBySwaps);
    BOOST_CHECK_CLOSE(capNpv, capBySwaps, 1.0);

    BOOST_TEST_MESSAGE("Checking CMS Cap with high Cap is zero...");
    vector<double> capHigh(1, 1.0);

    cap = vars.makeCap(capHigh);
    cap->build(engineFactory);
    capNpv = cap->instrument()->NPV();
    BOOST_TEST_MESSAGE("CMS Cap (Cap of 100%) NPV is " << capNpv);
    BOOST_CHECK_SMALL(capNpv, 0.01);

    BOOST_TEST_MESSAGE("Checking CMS Cap with low Cap is equal to single leg swap...");
    vector<double> capLow(1, -1.0);

    cap = vars.makeCap(capLow);
    cap->build(engineFactory);
    capNpv = cap->instrument()->NPV();
    BOOST_TEST_MESSAGE("CMS Cap (Cap of -100%) NPV is " << capNpv);
    BOOST_CHECK_CLOSE(capNpv, cmsLegNpv, 1.0);

    BOOST_TEST_MESSAGE("Checking CMS Floor with low Cap is equal to zero...");
    vector<double> floorLow(1, -1.0);

    QuantLib::ext::shared_ptr<ore::data::CapFloor> floor = vars.makeFloor(floorLow);
    floor->build(engineFactory);
    Real floorNpv = floor->instrument()->NPV();
    BOOST_TEST_MESSAGE("CMS Floor (Floor of -100%) NPV is " << floorNpv);
    BOOST_CHECK_SMALL(floorNpv, 0.01);

    BOOST_TEST_MESSAGE("Checking CMS Cap + CMS Floor = Swap...");
    vector<double> floorRate(1, 0.021);

    cap = vars.makeCap(capRate);
    floor = vars.makeFloor(floorRate);
    QuantLib::ext::shared_ptr<ore::data::Swap> swap = vars.makeSwap(0.021, "6M");
    cap->build(engineFactory);
    floor->build(engineFactory);
    swap->build(engineFactory);
    capNpv = cap->instrument()->NPV();
    floorNpv = floor->instrument()->NPV();
    Real swapNpv = swap->instrument()->NPV();
    Real capFloorNpv = capNpv - floorNpv;
    BOOST_TEST_MESSAGE("CMS Cap NPV is " << capNpv);
    BOOST_TEST_MESSAGE("CMS Floor NPV is " << floorNpv);
    BOOST_TEST_MESSAGE("CMS Cap + Floor NPV is " << capFloorNpv);
    BOOST_TEST_MESSAGE("CMS Swap NPV is " << swapNpv);
    BOOST_CHECK_CLOSE(capFloorNpv, swapNpv, 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
