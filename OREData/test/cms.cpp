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

#include <test/cms.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/builders/cms.hpp>
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

// CMS Swap test

namespace {

    class TestMarket : public MarketImpl {
    public:
        TestMarket() {
            asof_ = Date(3, Feb, 2016);

            // build discount
            discountCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateYts(0.02);

            // build swaption vols
            swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.1);

            // build ibor index
            Handle<IborIndex> hEUR(ore::data::parseIborIndex(
                "EUR-EURIBOR-6M", discountCurves_[make_pair(Market::defaultConfiguration, "EUR")]));
            iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;

            // add swap index
            boost::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
                "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
            conventions_.add(swapEURConv);
            boost::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
                new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
            conventions_.add(swapIndexEURLongConv);
            addSwapIndex("EUR-CMS-30Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);
        }

    private:
        Handle<YieldTermStructure> flatRateYts(Real forward) {
            boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
            return Handle<YieldTermStructure>(yts);
        }
        Handle<SwaptionVolatilityStructure> flatRateSvs(Volatility forward) {
            boost::shared_ptr<SwaptionVolatilityStructure> Svs(
                new ConstantSwaptionVolatility(0, NullCalendar(), ModifiedFollowing, forward, ActualActual()));
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
        vector<double> notionals;
        vector<double> spread;

        // utilities
        boost::shared_ptr<ore::data::Swap> makeSwap() {
            ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));
            ScheduleData cmsSchedule(ScheduleRules(start, end, cmstenor, calStr, conv, conv, rule));

            // build CMSSwap
            FixedLegData fixedLegRateData(vector<double>(1, fixedRate));
            LegData fixedLegData(isPayer, ccy, fixedLegRateData, fixedSchedule, fixDC, notionals);

            CMSLegData cmsLegRateData(index, fixingdays, isinarrears, spread);
            LegData cmsLegData(!isPayer, ccy, cmsLegRateData, cmsSchedule, fixDC, notionals);

            Envelope env("CP1");

            boost::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, fixedLegData, cmsLegData));
            return swap;
        }

        CommonVars() {
            ccy = "EUR";
            isPayer = false;
            start = "20160301";
            end = "20360301";
            fixtenor = "1Y";
            cmstenor = "6M";
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

    void outputCoupons(boost::shared_ptr<ore::data::Swap> cmsSwap) {
        Leg leg = cmsSwap->legs().at(1);
        for (auto cf : leg) {
            boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(cf);
            BOOST_TEST_MESSAGE("Coupon Date: " << frc->date() << "; Rate: " << frc->rate()
                << "; DayCount: " << frc->dayCounter());
        }
    };
}

namespace testsuite {

    void CmsTest::testCMSAnalyticHagan() {
        BOOST_TEST_MESSAGE("Testing CMS Analytic Hagan price...");

        // build market
        boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
        Settings::instance().evaluationDate() = market->asofDate();
        CommonVars vars;
        boost::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

        // Build and price
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->model("CMS") = "Hagan";
        engineData->engine("CMS") = "Analytic";
        map<string, string> engineparams;
        engineparams["YieldCurveModel"] = "Standard";
        engineparams["MeanReversion"] = "0.0";
        engineData->engineParameters("CMS") = engineparams;

        engineData->model("Swap") = "DiscountedCashflows";
        engineData->engine("Swap") = "DiscountingSwapEngineOptimised";

        boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
        engineFactory->registerBuilder(boost::make_shared<AnalyticHaganCmsCouponPricerBuilder>());

        cmsSwap->build(engineFactory);

        Real expectedNPV = -3440673.46;
        Real npv = cmsSwap->instrument()->NPV();

        BOOST_TEST_MESSAGE("Hagan Analytic price is " << npv);
        outputCoupons(cmsSwap);

        BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
    }

    void CmsTest::testCMSNumericalHagan() {
        BOOST_TEST_MESSAGE("Testing CMS Numerical Hagan price...");

        // build market
        boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
        Settings::instance().evaluationDate() = market->asofDate();

        CommonVars vars;
        boost::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

        // Build and price
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
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

        boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
        engineFactory->registerBuilder(boost::make_shared<NumericalHaganCmsCouponPricerBuilder>());

        cmsSwap->build(engineFactory);

        Real expectedNPV = -3440673.46;
        Real npv = cmsSwap->instrument()->NPV();

        BOOST_TEST_MESSAGE("Hagan Numerical price is " << npv);
        outputCoupons(cmsSwap);

        BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
    }

    void CmsTest::testCMSLinearTsr() {
        BOOST_TEST_MESSAGE("Testing CMS Linear TSR price...");

        // build market
        boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
        Settings::instance().evaluationDate() = market->asofDate();

        CommonVars vars;
        boost::shared_ptr<ore::data::Swap> cmsSwap = vars.makeSwap();

        // Build and price
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
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

        boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
        engineFactory->registerBuilder(boost::make_shared<LinearTSRCmsCouponPricerBuilder>());

        cmsSwap->build(engineFactory);

        Real expectedNPV = -3440673.46;
        Real npv = cmsSwap->instrument()->NPV();

        BOOST_TEST_MESSAGE("Linear TSR price is " << npv);
        outputCoupons(cmsSwap);

        BOOST_CHECK_CLOSE(npv, expectedNPV, 1.0);
    }

    test_suite* CmsTest::suite() {
        test_suite* suite = BOOST_TEST_SUITE("CmsTest");
        suite->add(BOOST_TEST_CASE(&CmsTest::testCMSAnalyticHagan));
        suite->add(BOOST_TEST_CASE(&CmsTest::testCMSNumericalHagan));
        suite->add(BOOST_TEST_CASE(&CmsTest::testCMSLinearTsr));
        return suite;
    }
}
