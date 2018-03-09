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
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <test/swaption.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using namespace ore::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        asof_ = Date(2, January, 2017);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.03);

        // build swaption vols
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatSwaptionVol(0.30);

        Handle<IborIndex> h(parseIborIndex("EUR-EURIBOR-6M", flatRateYts(0.03)));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = h;
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantLib::SwaptionVolatilityStructure>
    flatSwaptionVol(Volatility forward, VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }
};
} // namespace

namespace testsuite {

void SwaptionTest::testEuropeanSwaptionPrice() {
    BOOST_TEST_MESSAGE("Testing Swaption Price...");

    Date today = Settings::instance().evaluationDate();

    // build market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
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
    LegData fixedLeg(boost::make_shared<FixedLegData>(std::vector<Real>(1, 0.03)), true, "EUR", fixedSchedule, "30/360",
                     std::vector<Real>(1, 10000.0));
    // float leg
    LegData floatingLeg(boost::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, std::vector<Real>(1, 0.0)),
                        false, "EUR", floatSchedule, "A360", std::vector<Real>(1, 10000.0));
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);

    Envelope env("CP1");
    OptionData optionData("Long", "Call", "European", true, vector<string>(1, startDate), "Cash");
    OptionData optionDataPhysical("Long", "Call", "European", true, vector<string>(1, startDate), "Physical");
    Real premium = 700.0;
    OptionData optionDataPremium("Long", "Call", "European", true, vector<string>(1, startDate), "Cash", premium, "EUR",
                                 startDate);
    ore::data::Swaption swaptionCash(env, optionData, legs);
    ore::data::Swaption swaptionPhysical(env, optionDataPhysical, legs);
    ore::data::Swaption swaptionPremium(env, optionDataPremium, legs);

    Real expectedNpvCash = 615.03;
    Real premiumNpv = premium * market->discountCurve("EUR")->discount(calendar.adjust(qlStartDate));
    Real expectedNpvPremium = expectedNpvCash - premiumNpv;

    // Build and price
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<EuropeanSwaptionEngineBuilder>());
    engineFactory->registerBuilder(boost::make_shared<SwapEngineBuilder>());

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

    Settings::instance().evaluationDate() = today; // reset
}

test_suite* SwaptionTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("EuropeanSwaptionTest");
    suite->add(BOOST_TEST_CASE(&SwaptionTest::testEuropeanSwaptionPrice));
    return suite;
}
} // namespace testsuite
