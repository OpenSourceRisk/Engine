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
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

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
        QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURLongConv1(
            new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
        QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURLongConv2(
            new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
        conventions->add(swapIndexEURLongConv1);
        conventions->add(swapIndexEURLongConv2);

        InstrumentConventions::instance().setConventions(conventions);

        addSwapIndex("EUR-CMS-2Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);
        addSwapIndex("EUR-CMS-30Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);

        correlationCurves_[make_tuple(Market::defaultConfiguration, "EUR-CMS-30Y", "EUR-CMS-2Y")] = flatCorr(0.8);
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
    Handle<QuantExt::CorrelationTermStructure> flatCorr(Real corr) {
        QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> cs(
            new QuantExt::FlatCorrelation(0, NullCalendar(), corr, ActualActual(ActualActual::ISDA)));

        return Handle<QuantExt::CorrelationTermStructure>(cs);
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
    string index1;
    string index2;
    int fixingdays;
    bool isinarrears;
    double notional;
    string longShort;
    vector<double> notionals;

    QuantLib::ext::shared_ptr<ore::data::Swap> makeDigitalCmsSpreadOption(bool call, vector<double> strikes,
                                                                  vector<double> payoffs) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        QuantLib::ext::shared_ptr<DigitalCMSSpreadLegData> cmsLegData;
        if (call) {
            cmsLegData = QuantLib::ext::make_shared<DigitalCMSSpreadLegData>(
                QuantLib::ext::make_shared<CMSSpreadLegData>(
                    index1, index2, fixingdays, isinarrears, vector<double>(), vector<string>(), vector<double>(),
                    vector<string>(), vector<double>(), vector<string>(), vector<double>(), vector<string>(), false),
                Position::Long, false, strikes, vector<string>(), payoffs, vector<string>());
        } else {
            cmsLegData = QuantLib::ext::make_shared<DigitalCMSSpreadLegData>(
                QuantLib::ext::make_shared<CMSSpreadLegData>(
                    index1, index2, fixingdays, isinarrears, vector<double>(), vector<string>(), vector<double>(),
                    vector<string>(), vector<double>(), vector<string>(), vector<double>(), vector<string>(), false),
                Position::Long, false, vector<double>(), vector<string>(), vector<double>(), vector<string>(),
                Position::Long, false, strikes, vector<string>(), payoffs);
        }

        LegData leg(cmsLegData, isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(leg);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeCmsSpreadOption(vector<double> spreads) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        CMSSpreadLegData cmsLegData(index1, index2, fixingdays, isinarrears, spreads, vector<string>(),
                                    vector<double>(), vector<string>(), vector<double>(), vector<string>(),
                                    vector<double>(), vector<string>(), false);

        LegData leg(QuantLib::ext::make_shared<CMSSpreadLegData>(cmsLegData), isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(leg);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeCmsSpreadFloor(vector<double> floors) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        CMSSpreadLegData cmsLegData(index1, index2, fixingdays, isinarrears, vector<double>(), vector<string>(),
                                    vector<double>(), vector<string>(), floors, vector<string>(), vector<double>(),
                                    vector<string>(), false);

        LegData leg(QuantLib::ext::make_shared<CMSSpreadLegData>(cmsLegData), isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(leg);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    QuantLib::ext::shared_ptr<ore::data::Swap> makeCmsSpreadCap(vector<double> caps) {
        ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

        vector<LegData> legData;
        CMSSpreadLegData cmsLegData(index1, index2, fixingdays, isinarrears, vector<double>(), vector<string>(), caps,
                                    vector<string>(), vector<double>(), vector<string>(), vector<double>(),
                                    vector<string>(), false);

        LegData leg(QuantLib::ext::make_shared<CMSSpreadLegData>(cmsLegData), isPayer, ccy, cmsSchedule, fixDC, notionals);
        legData.push_back(leg);

        Envelope env("CP1");
        QuantLib::ext::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, legData));
        return swap;
    }

    CommonVars() : ccy("EUR"), isPayer(false), start("20160301"), end("20360301"), fixtenor("1Y"), cmstenor("6M") {

        longShort = isPayer ? "Short" : "Long";
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/360";
        fixedRate = 0.0;
        index1 = "EUR-CMS-30Y";
        index2 = "EUR-CMS-2Y";
        fixingdays = 2;
        isinarrears = false;
        notional = 10000000;
        notionals.push_back(10000000);
    }
};

void outputCoupons(QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap) {
    Leg leg = cmsSwap->legs().at(0);
    for (auto cf : leg) {
        QuantLib::ext::shared_ptr<FloatingRateCoupon> frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
        if (frc) {
            BOOST_TEST_MESSAGE("Coupon Date: " << frc->date() << "; Rate: " << frc->rate()
                                               << "; DayCount: " << frc->dayCounter());
        } else {
            BOOST_TEST_MESSAGE("Coupon Date: " << cf->date() << " - not a floating rate coupon!");
        }
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DigitalCmsTests)

// TODO FIXME
BOOST_AUTO_TEST_CASE(testDigitalCMSSpreadCoupon) {
    BOOST_TEST_MESSAGE("Testing CMS Digital CMS Spread coupon...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();
    CommonVars vars;

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "LinearTSR";
    engineData->engine("CMS") = "LinearTSRPricer";

    map<string, string> engineparams1;
    engineparams1["MeanReversion"] = "0.0";
    engineparams1["Policy"] = "RateBound";
    engineparams1["LowerRateBoundLogNormal"] = "0.0001";
    engineparams1["UpperRateBoundLogNormal"] = "2";
    engineparams1["LowerRateBoundNormal"] = "-2";
    engineparams1["UpperRateBoundNormal"] = "2";
    engineparams1["VegaRatio"] = "0.01";
    engineparams1["PriceThreshold"] = "0.0000001";
    engineparams1["BsStdDev"] = "3";
    engineData->engineParameters("CMS") = engineparams1;

    engineData->model("CMSSpread") = "BrigoMercurio";
    engineData->engine("CMSSpread") = "Analytic";
    map<string, string> engineparams2;
    engineparams2["IntegrationPoints"] = "16";
    engineData->engineParameters("CMSSpread") = engineparams2;
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // test edge cases
    // If strike >> rate then NPV(digital call option) == NPV(option with spread = 0)
    // and NPV(digital put option == NPV(option with spread = payoff)

    {
        double strike = 1.0;
        double pay = 0.0001;
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapCall =
            vars.makeDigitalCmsSpreadOption(true, vector<double>(1, strike), vector<double>(1, pay));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapPut =
            vars.makeDigitalCmsSpreadOption(false, vector<double>(1, strike), vector<double>(1, pay));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap1 = vars.makeCmsSpreadOption(vector<double>(1, pay));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap2 = vars.makeCmsSpreadOption(vector<double>(1, 0.0));
        cmsDigitalSwapCall->build(engineFactory);
        cmsDigitalSwapPut->build(engineFactory);
        cmsSwap1->build(engineFactory);
        cmsSwap2->build(engineFactory);

        BOOST_TEST_MESSAGE("digital call coupons");
        outputCoupons(cmsDigitalSwapCall);
        BOOST_TEST_MESSAGE("digital put coupons");
        outputCoupons(cmsDigitalSwapPut);
        BOOST_TEST_MESSAGE("coupon 1");
        outputCoupons(cmsSwap1);
        BOOST_TEST_MESSAGE("coupon 2");
        outputCoupons(cmsSwap2);

        BOOST_TEST_MESSAGE("NPV Call = " << cmsDigitalSwapCall->instrument()->NPV());
        BOOST_TEST_MESSAGE("NPV Put = " << cmsDigitalSwapPut->instrument()->NPV());
        BOOST_TEST_MESSAGE("NPV1 = " << cmsSwap1->instrument()->NPV());
        BOOST_TEST_MESSAGE("NPV2 = " << cmsSwap2->instrument()->NPV());

        BOOST_CHECK_CLOSE(cmsDigitalSwapCall->instrument()->NPV(), cmsSwap2->instrument()->NPV(), 0.1);
        BOOST_CHECK_CLOSE(cmsDigitalSwapPut->instrument()->NPV(), cmsSwap1->instrument()->NPV(), 0.1);
    }

    // check put cash-or-nothing payoffs
    {
        double strike = 0.0001;
        double pay = 0.0001;
        double eps = 1e-4;
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapPut =
            vars.makeDigitalCmsSpreadOption(false, vector<double>(1, strike), vector<double>(1, pay));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap1 = vars.makeCmsSpreadFloor(vector<double>(1, strike + eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap2 = vars.makeCmsSpreadFloor(vector<double>(1, strike - eps / 2));
        cmsDigitalSwapPut->build(engineFactory);
        cmsSwap1->build(engineFactory);
        cmsSwap2->build(engineFactory);

        Leg leg = cmsDigitalSwapPut->legs().at(0);
        Leg leg1 = cmsSwap1->legs().at(0);
        Leg leg2 = cmsSwap2->legs().at(0);

        for (Size i = 0; i < leg.size(); i++) {
            QuantLib::ext::shared_ptr<DigitalCoupon> dc = QuantLib::ext::dynamic_pointer_cast<DigitalCoupon>(leg[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc1 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg1[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc2 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg2[i]);

            double r = pay * (frc1->rate() - frc2->rate()) / eps;

            BOOST_CHECK_CLOSE(r, dc->putOptionRate(), 0.1);
        }
    }

    // check call cash-or-nothing payoffs
    {
        double strike = 0.0001;
        double pay = 0.0001;
        double eps = 1e-4;
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapCall =
            vars.makeDigitalCmsSpreadOption(true, vector<double>(1, strike), vector<double>(1, pay));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap1 = vars.makeCmsSpreadCap(vector<double>(1, strike + eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap2 = vars.makeCmsSpreadCap(vector<double>(1, strike - eps / 2));
        cmsDigitalSwapCall->build(engineFactory);
        cmsSwap1->build(engineFactory);
        cmsSwap2->build(engineFactory);

        Leg leg = cmsDigitalSwapCall->legs().at(0);
        Leg leg1 = cmsSwap1->legs().at(0);
        Leg leg2 = cmsSwap2->legs().at(0);

        for (Size i = 0; i < leg.size(); i++) {
            QuantLib::ext::shared_ptr<DigitalCoupon> dc = QuantLib::ext::dynamic_pointer_cast<DigitalCoupon>(leg[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc1 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg1[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc2 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg2[i]);

            double r = pay * (frc1->rate() - frc2->rate()) / eps;

            BOOST_CHECK_CLOSE(r, dc->callOptionRate(), 0.1);
        }
    }
    // check put asset-or-nothing payoffs
    {
        double strike = 0.0001;
        double eps = 1e-4;
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapPut =
            vars.makeDigitalCmsSpreadOption(false, vector<double>(1, strike), vector<double>());
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap1 = vars.makeCmsSpreadFloor(vector<double>(1, strike + eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap2 = vars.makeCmsSpreadFloor(vector<double>(1, strike - eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap3 = vars.makeCmsSpreadFloor(vector<double>(1, strike));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap4 = vars.makeCmsSpreadFloor(vector<double>());
        cmsDigitalSwapPut->build(engineFactory);
        cmsSwap1->build(engineFactory);
        cmsSwap2->build(engineFactory);
        cmsSwap3->build(engineFactory);
        cmsSwap4->build(engineFactory);

        Leg leg = cmsDigitalSwapPut->legs().at(0);
        Leg leg1 = cmsSwap1->legs().at(0);
        Leg leg2 = cmsSwap2->legs().at(0);
        Leg leg3 = cmsSwap3->legs().at(0);
        Leg leg4 = cmsSwap4->legs().at(0);

        for (Size i = 0; i < leg.size(); i++) {
            QuantLib::ext::shared_ptr<DigitalCoupon> dc = QuantLib::ext::dynamic_pointer_cast<DigitalCoupon>(leg[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc1 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg1[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc2 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg2[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc3 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg3[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc4 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg4[i]);

            double r = strike * (frc1->rate() - frc2->rate()) / eps;
            double put = -frc4->rate() + frc3->rate();
            BOOST_TEST_MESSAGE(frc4->rate() << " " << frc3->rate() << " " << r << " " << put);

            BOOST_CHECK_CLOSE(r - put, dc->putOptionRate(), 0.1);
        }
    }

    // check call asset-or-nothing payoffs
    {
        double strike = 0.0001;
        double eps = 1e-4;
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsDigitalSwapCall =
            vars.makeDigitalCmsSpreadOption(true, vector<double>(1, strike), vector<double>());
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap1 = vars.makeCmsSpreadCap(vector<double>(1, strike + eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap2 = vars.makeCmsSpreadCap(vector<double>(1, strike - eps / 2));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap3 = vars.makeCmsSpreadCap(vector<double>(1, strike));
        QuantLib::ext::shared_ptr<ore::data::Swap> cmsSwap4 = vars.makeCmsSpreadCap(vector<double>());
        cmsDigitalSwapCall->build(engineFactory);
        cmsSwap1->build(engineFactory);
        cmsSwap2->build(engineFactory);
        cmsSwap3->build(engineFactory);
        cmsSwap4->build(engineFactory);

        Leg leg = cmsDigitalSwapCall->legs().at(0);
        Leg leg1 = cmsSwap1->legs().at(0);
        Leg leg2 = cmsSwap2->legs().at(0);
        Leg leg3 = cmsSwap3->legs().at(0);
        Leg leg4 = cmsSwap4->legs().at(0);

        for (Size i = 0; i < leg.size(); i++) {
            QuantLib::ext::shared_ptr<DigitalCoupon> dc = QuantLib::ext::dynamic_pointer_cast<DigitalCoupon>(leg[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc1 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg1[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc2 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg2[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc3 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg3[i]);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> frc4 = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg4[i]);

            double r = strike * (frc1->rate() - frc2->rate()) / eps;
            double call = frc4->rate() - frc3->rate();

            BOOST_CHECK_CLOSE(r + call, dc->callOptionRate(), 0.1);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
