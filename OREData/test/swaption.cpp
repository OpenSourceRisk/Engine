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
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        asof_ = Date(2, January, 2017);

        vector<pair<string, Real>> indexData = {
            {"EUR-EONIA", 0.01},    {"EUR-EURIBOR-3M", 0.015}, {"EUR-EURIBOR-6M", 0.02}
        };
    
        for (auto id : indexData) {
            Handle<IborIndex> h(parseIborIndex(id.first, flatRateYts(id.second)));
            iborIndices_[make_pair(Market::defaultConfiguration, id.first)] = h;
    
            // set up dummy fixings for the past 400 days
            for (Date d = asof_ - 400; d < asof_; d++) {
                if (h->isValidFixingDate(d))
                    h->addFixing(d, 0.01);
            }
        }
        QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
        QuantLib::ext::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
            "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));    
        conventions->add(swapEURConv);

        QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURConv(
            new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
        QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
            new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
    
        conventions->add(swapIndexEURConv);
        conventions->add(swapIndexEURLongConv);
        InstrumentConventions::instance().setConventions(conventions);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.03);

        addSwapIndex("EUR-CMS-2Y", "EUR-EONIA", Market::defaultConfiguration);
        addSwapIndex("EUR-CMS-30Y", "EUR-EONIA", Market::defaultConfiguration);

        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = std::make_pair("EUR-CMS-2Y", "EUR-CMS-30Y");

        // build swaption vols
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatSwaptionVol(0.30);

        Handle<IborIndex> h(parseIborIndex("EUR-EURIBOR-6M", flatRateYts(0.03)));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = h;
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantLib::SwaptionVolatilityStructure>
    flatSwaptionVol(Volatility forward, VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(ActualActual::ISDA), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(EuropeanSwaptionTests)

BOOST_AUTO_TEST_CASE(testEuropeanSwaptionPrice) {

    BOOST_TEST_MESSAGE("Testing Swaption Price...");

    Date today(3, Dec, 2018);
    Settings::instance().evaluationDate() = today;

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build Swaptions - expiry 5Y, term 10Y

    Calendar calendar = TARGET();
    Date qlStartDate = calendar.adjust(today + 5 * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + 10 * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, "6M", "TARGET", "MF", "MF", "Forward"));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, "1Y", "TARGET", "MF", "MF", "Forward"));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(std::vector<Real>(1, 0.03)), true, "EUR", fixedSchedule, "30/360",
                     std::vector<Real>(1, 10000.0));
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, std::vector<Real>(1, 0.0)),
                        false, "EUR", floatSchedule, "A360", std::vector<Real>(1, 10000.0));
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);

    Envelope env("CP1");
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, startDate), "Cash");
    OptionData optionDataPhysical("Long", "Call", "European", true, vector<string>(1, startDate), "Physical");
    Real premium = 700.0;
    OptionData optionDataPremium("Long", "Call", "European", true, vector<string>(1, startDate), "Cash", "",
                                 PremiumData(premium, "EUR", qlStartDate));
    ore::data::Swaption swaptionCash(env, optionData, legs);
    ore::data::Swaption swaptionPhysical(env, optionDataPhysical, legs);
    ore::data::Swaption swaptionPremium(env, optionDataPremium, legs);

    Real expectedNpvCash = 565.19;
    Real premiumNpv = premium * market->discountCurve("EUR")->discount(calendar.adjust(qlStartDate));
    Real expectedNpvPremium = expectedNpvCash - premiumNpv;

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    swaptionCash.build(engineFactory);
    swaptionPhysical.build(engineFactory);
    swaptionPremium.build(engineFactory);

    Real npvCash = swaptionCash.instrument()->NPV();
    Real npvPhysical = swaptionPhysical.instrument()->NPV();
    Real npvPremium = swaptionPremium.instrument()->NPV();

    BOOST_TEST_MESSAGE("Swaption, NPV Currency " << swaptionCash.npvCurrency());
    BOOST_TEST_MESSAGE("NPV Cash              = " << npvCash);
    BOOST_TEST_MESSAGE("NPV Physical          = " << npvPhysical);
    BOOST_TEST_MESSAGE("NPV Cash with premium = " << npvPremium);

    BOOST_CHECK_SMALL(npvCash - expectedNpvCash, 0.01);
    BOOST_CHECK_SMALL(npvPremium - expectedNpvPremium, 0.01);
}


BOOST_AUTO_TEST_CASE(testRepresentativeSwaptionDefault) {

    BOOST_TEST_MESSAGE("Testing Representative Swaption for varying notional ...");

    Date today(2, January, 2017);
    Settings::instance().evaluationDate() = today;

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build Swaptions - expiry 5Y, term 10Y
    Calendar calendar = TARGET();
    Date qlStartDate = calendar.adjust(today + 5 * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + 10 * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, "6M", "TARGET", "MF", "MF", "Forward"));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, "1Y", "TARGET", "MF", "MF", "Forward"));

    auto notionals = std::vector<Real>(1, 1000.0);
    notionals[0]=1000;    
    
    auto notionals2 = std::vector<Real>(1, 1000.0);
    notionals2[0]=1000;    

    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(std::vector<Real>(10, 0.03)), true, "EUR", fixedSchedule, "30/360",
                     notionals2);
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, std::vector<Real>(20, 0.0)),
                        false, "EUR", floatSchedule, "A360", notionals);

    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);

    // Coupon amount



    Envelope env("CP1");
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, startDate), "Cash");
    OptionData optionDataPhysical("Long", "Call", "European", true, vector<string>(1, startDate), "Physical");
    Real premium = 700.0;
    OptionData optionDataPremium("Long", "Call", "European", true, vector<string>(1, startDate), "Cash", "",
                                 PremiumData(premium, "EUR", qlStartDate));
    ore::data::Swaption swaptionCash(env, optionData, legs);
    ore::data::Swaption swaptionPhysical(env, optionDataPhysical, legs);
    ore::data::Swaption swaptionPremium(env, optionDataPremium, legs);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    swaptionCash.build(engineFactory);
    swaptionPhysical.build(engineFactory);
    swaptionPremium.build(engineFactory);
    
    std::cout << "Swaption Cash NPV: " << swaptionCash.instrument()->NPV() << std::endl;


}

BOOST_AUTO_TEST_CASE(testRepresentativeSwaptionVaryingNotional) {

    BOOST_TEST_MESSAGE("Testing Representative Swaption for varying notional ...");

    Date today(2, January, 2017);
    Settings::instance().evaluationDate() = today;

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build Swaptions - expiry 5Y, term 10Y
    Calendar calendar = TARGET();
    Date qlStartDate = calendar.adjust(today + 5 * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + 10 * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, "6M", "TARGET", "MF", "MF", "Forward"));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, "1Y", "TARGET", "MF", "MF", "Forward"));

    auto notionals = std::vector<Real>(20, 1000.0);
    notionals[0]=1000;
    notionals[1]=100;
    notionals[2]=900;
    notionals[3]=900;
    notionals[4]=800;
    notionals[5]=800;
    notionals[6]=700;
    notionals[7]=700;
    notionals[8]=600;
    notionals[9]=600;    
    notionals[10]=500;
    notionals[11]=500;
    notionals[12]=400;
    notionals[13]=400;
    notionals[14]=300;
    notionals[15]=300;
    notionals[16]=200;
    notionals[17]=200;
    notionals[18]=100;
    notionals[19]=100;    
    
    auto notionals2 = std::vector<Real>(10, 1000.0);
    notionals2[0]=1000;
    notionals2[1]=900;
    notionals2[2]=800;
    notionals2[3]=700;
    notionals2[4]=600;
    notionals2[5]=500;
    notionals2[6]=400;
    notionals2[7]=300;
    notionals2[8]=200;
    notionals2[9]=100;

    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(std::vector<Real>(10, 0.03)), true, "EUR", fixedSchedule, "30/360",
                     notionals2);
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, std::vector<Real>(20, 0.0)),
                        false, "EUR", floatSchedule, "A360", notionals);

    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);

    Envelope env("CP1");
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, startDate), "Cash");
    OptionData optionDataPhysical("Long", "Call", "European", true, vector<string>(1, startDate), "Physical");
    Real premium = 700.0;
    OptionData optionDataPremium("Long", "Call", "European", true, vector<string>(1, startDate), "Cash", "",
                                 PremiumData(premium, "EUR", qlStartDate));
    ore::data::Swaption swaptionCash(env, optionData, legs);
    ore::data::Swaption swaptionPhysical(env, optionDataPhysical, legs);
    ore::data::Swaption swaptionPremium(env, optionDataPremium, legs);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    swaptionCash.build(engineFactory);
    swaptionPhysical.build(engineFactory);
    swaptionPremium.build(engineFactory);
}

BOOST_AUTO_TEST_CASE(testRepresentativeSwaptionVaryingRates) {

    BOOST_TEST_MESSAGE("Testing Representative Swaption for varying rates ...");

    Date today(2, January, 2017);
    Settings::instance().evaluationDate() = today;

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // build Swaptions - expiry 5Y, term 10Y
    Calendar calendar = TARGET();
    Date qlStartDate = calendar.adjust(today + 5 * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + 10 * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, "6M", "TARGET", "MF", "MF", "Forward"));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, "1Y", "TARGET", "MF", "MF", "Forward"));

    std::vector<Real> rates(10, 0.03);
    rates[0] = 0.03;
    rates[1] = 0.027;
    rates[2] = 0.024;
    rates[3] = 0.021;
    rates[4] = 0.018;
    rates[5] = 0.015;
    rates[6] = 0.012;
    rates[7] = 0.009;
    rates[8] = 0.006;
    rates[9] = 0.003;

    auto notionals = std::vector<Real>(1, 1000.0);

    // fixed leg    
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), true, "EUR", fixedSchedule, "30/360", notionals);

    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, std::vector<Real>(1, 0.0)),
                        false, "EUR", floatSchedule, "A360", notionals);

    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);

    Envelope env("CP1");
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, startDate), "Cash");
    OptionData optionDataPhysical("Long", "Call", "European", true, vector<string>(1, startDate), "Physical");
    Real premium = 700.0;
    OptionData optionDataPremium("Long", "Call", "European", true, vector<string>(1, startDate), "Cash", "",
                                 PremiumData(premium, "EUR", qlStartDate));
    ore::data::Swaption swaptionCash(env, optionData, legs);
    ore::data::Swaption swaptionPhysical(env, optionDataPhysical, legs);
    ore::data::Swaption swaptionPremium(env, optionDataPremium, legs);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    swaptionCash.build(engineFactory);
    swaptionPhysical.build(engineFactory);
    swaptionPremium.build(engineFactory);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
