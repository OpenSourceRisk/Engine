/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore::data;
using std::vector;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        // valuation date
        asof_ = Date(18, July, 2016);

        spotSP5 = Handle<Quote>(boost::shared_ptr<SimpleQuote>(new SimpleQuote(2100)));
        forecastSP5 = flatRateYts(0.03);
        dividendSP5 = flatRateYts(0.01);

        // build vectors with dates and discount factors for GBP
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = forecastSP5;
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "SP5")] = forecastSP5;
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "SP5")] = dividendSP5;

        // build USD Libor index
        hUSD = Handle<IborIndex>(parseIborIndex("USD-LIBOR-3M", flatRateYts(0.035)));
        iborIndices_[make_pair(Market::defaultConfiguration, "USD-LIBOR-3M")] = hUSD;

        // build SP5 Equity Curve
        hSP5 = Handle<EquityIndex>(boost::shared_ptr<EquityIndex>(
            new EquityIndex("SP5", UnitedStates(), parseCurrency("USD"), spotSP5, forecastSP5, dividendSP5)));
        equityCurves_[make_pair(Market::defaultConfiguration, "SP5")] = hSP5;

        // add fixings
        hUSD->addFixing(Date(14, July, 2016), 0.035);
        hUSD->addFixing(Date(18, October, 2016), 0.037);
    }

    Handle<IborIndex> hUSD;
    Handle<EquityIndex> hSP5;
    Handle<Quote> spotSP5;
    Handle<YieldTermStructure> forecastSP5;
    Handle<YieldTermStructure> dividendSP5;

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(new FlatForward(0, UnitedStates(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
};

struct CommonVars {
    // global data
    string ccy;
    bool isPayer;
    string start;
    string end;
    string floattenor;
    string eqtenor;
    string calStr;
    string conv;
    string rule;
    Natural days;
    string fixDC;
    string index;
    string eqName;
    Real dividendFactor;
    Real initialPrice;
    Integer settlementDays;
    bool isinarrears;
    vector<double> notionals;
    vector<double> spread;

    // utilities
    boost::shared_ptr<ore::data::Swap> makeEquitySwap(string returnType, bool notionalReset = false) {
        ScheduleData floatSchedule(ScheduleRules(start, end, floattenor, calStr, conv, conv, rule));
        ScheduleData eqSchedule(ScheduleRules(start, end, eqtenor, calStr, conv, conv, rule));

        // build EquitySwap
        LegData floatLegData(boost::make_shared<FloatingLegData>(index, days, isinarrears, spread), !isPayer, ccy,
                             floatSchedule, fixDC, notionals);
        LegData eqLegData(boost::make_shared<EquityLegData>(returnType, dividendFactor, EquityUnderlying(eqName),
                                                            initialPrice, notionalReset, settlementDays),
                          isPayer, ccy, eqSchedule, fixDC, notionals);

        Envelope env("CP1");
        boost::shared_ptr<ore::data::Swap> swap(new ore::data::Swap(env, eqLegData, floatLegData));
        return swap;
    }

    boost::shared_ptr<QuantLib::Swap> qlEquitySwap(string returnType, bool notionalReset = false) {

        boost::shared_ptr<TestMarket> market = boost::make_shared<TestMarket>();
        // check Equity swap NPV against pure QL pricing
        Schedule floatSchedule(parseDate(start), parseDate(end), parsePeriod(floattenor), parseCalendar(calStr),
                               parseBusinessDayConvention(conv), parseBusinessDayConvention(conv),
                               parseDateGenerationRule(rule), false);
        Schedule eqSchedule(parseDate(start), parseDate(end), parsePeriod(eqtenor), parseCalendar(calStr),
                            parseBusinessDayConvention(conv), parseBusinessDayConvention(conv),
                            parseDateGenerationRule(rule), false);
        Leg floatLeg = IborLeg(floatSchedule, *market->hUSD)
                           .withNotionals(notionals)
                           .withFixingDays(days)
                           .withSpreads(spread)
                           .withPaymentDayCounter(parseDayCounter(fixDC))
                           .withPaymentAdjustment(parseBusinessDayConvention(conv));
        Leg eqLeg = EquityLeg(eqSchedule, *market->equityCurve("SP5"))
                        .withNotionals(notionals)
                        .withPaymentDayCounter(parseDayCounter(fixDC))
                        .withPaymentAdjustment(parseBusinessDayConvention(conv))
                        .withTotalReturn(returnType == "Total")
                        .withInitialPrice(initialPrice)
                        .withNotionalReset(notionalReset);

        boost::shared_ptr<QuantLib::Swap> swap(new QuantLib::Swap(floatLeg, eqLeg));
        return swap;
    }

    CommonVars() {
        ccy = "USD";
        isPayer = false;
        start = "20160718";
        end = "20210718";
        floattenor = "3M";
        eqtenor = "3M";
        calStr = "USD";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/ACT";
        index = "USD-LIBOR-3M";
        eqName = "SP5";
        dividendFactor = 1.0;
        initialPrice = 2100.0;
        settlementDays = 0;
        days = 0;
        isinarrears = false;
        notionals.push_back(10000000);
        spread.push_back(0.0);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(EquitySwapTests)

BOOST_AUTO_TEST_CASE(testEquitySwapPriceReturn) {

    BOOST_TEST_MESSAGE("Testing Equity Swap Price Return...");

    // build market
    boost::shared_ptr<TestMarket> market = boost::make_shared<TestMarket>();
    Date today = market->asofDate();
    Settings::instance().evaluationDate() = today;

    CommonVars vars;
    boost::shared_ptr<ore::data::Swap> eqSwap = vars.makeEquitySwap("Price");

    // engine data and factory
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<SwapEngineBuilder>());

    // build swaps and portfolio
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    eqSwap->id() = "EQ_Swap";

    portfolio->add(eqSwap);
    portfolio->build(engineFactory);

    boost::shared_ptr<QuantLib::Swap> qlSwap = vars.qlEquitySwap("Price");

    auto dscEngine = boost::make_shared<DiscountingSwapEngine>(market->discountCurve("USD"));
    qlSwap->setPricingEngine(dscEngine);
    BOOST_TEST_MESSAGE("Leg 1 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(0)
                       << " QL = " << qlSwap->legNPV(0));
    BOOST_TEST_MESSAGE("Leg 2 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(1)
                       << " QL = " << qlSwap->legNPV(1));
    BOOST_CHECK_CLOSE(eqSwap->instrument()->NPV(), qlSwap->NPV(), 1E-8); // this is 1E-10 rel diff
}

BOOST_AUTO_TEST_CASE(testEquitySwapTotalReturn) {

    BOOST_TEST_MESSAGE("Testing Equity Swap Total Return...");

    // build market
    boost::shared_ptr<TestMarket> market = boost::make_shared<TestMarket>();
    Date today = market->asofDate();
    Settings::instance().evaluationDate() = today;

    CommonVars vars;
    boost::shared_ptr<ore::data::Swap> eqSwap = vars.makeEquitySwap("Total");

    // engine data and factory
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<SwapEngineBuilder>());

    // build swaps and portfolio
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    eqSwap->id() = "EQ_Swap";

    portfolio->add(eqSwap);
    portfolio->build(engineFactory);

    boost::shared_ptr<QuantLib::Swap> qlSwap = vars.qlEquitySwap("Total");

    auto dscEngine = boost::make_shared<DiscountingSwapEngine>(market->discountCurve("USD"));
    qlSwap->setPricingEngine(dscEngine);
    BOOST_TEST_MESSAGE("Leg 1 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(0)
                       << " QL = " << qlSwap->legNPV(0));
    BOOST_TEST_MESSAGE("Leg 2 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(1)
                       << " QL = " << qlSwap->legNPV(1));
    BOOST_CHECK_CLOSE(eqSwap->instrument()->NPV(), qlSwap->NPV(), 1E-8);
}

BOOST_AUTO_TEST_CASE(testEquitySwapNotionalReset) {

    BOOST_TEST_MESSAGE("Testing Equity Swap Notional Reset...");

    // build market
    boost::shared_ptr<TestMarket> market = boost::make_shared<TestMarket>();
    Date today = market->asofDate();
    // Move on 4 months so we during next period, check we can get a notional
    Settings::instance().evaluationDate() = today + Period(4, Months);

    CommonVars vars;
    boost::shared_ptr<ore::data::Swap> eqSwap = vars.makeEquitySwap("Total", true);

    // engine data and factory
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    engineFactory->registerBuilder(boost::make_shared<SwapEngineBuilder>());

    // build swaps and portfolio
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    eqSwap->id() = "EQ_Swap";

    portfolio->add(eqSwap);
    portfolio->build(engineFactory);

    boost::shared_ptr<QuantLib::Swap> qlSwap = vars.qlEquitySwap("Total", true);

    BOOST_TEST_MESSAGE("Initial notional = " << eqSwap->notional());

    // Add equity fixing after portfolio build, this allows us to test notional without fixing
    market->equityCurve("SP5")->addFixing(Date(18, October, 2016), 2100);

    auto dscEngine = boost::make_shared<DiscountingSwapEngine>(market->discountCurve("USD"));
    qlSwap->setPricingEngine(dscEngine);
    BOOST_TEST_MESSAGE("Leg 1 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(0)
                       << " QL = " << qlSwap->legNPV(0));
    BOOST_TEST_MESSAGE("Leg 2 NPV: ORE = "
                       << boost::static_pointer_cast<QuantLib::Swap>(eqSwap->instrument()->qlInstrument())->legNPV(1)
                       << " QL = " << qlSwap->legNPV(1));
    BOOST_CHECK_CLOSE(eqSwap->instrument()->NPV(), qlSwap->NPV(), 1E-8);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
