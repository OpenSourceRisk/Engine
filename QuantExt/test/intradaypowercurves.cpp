/*
 Copyright (C) 2026 AcadiaSoft Inc
*/

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>

#include <qle/termstructures/intradayshapetermstructure.hpp>
#include <qle/termstructures/intradaypowerpricetermstructure.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <ql/currencies/america.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/interpolations/all.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>


using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(IntradayPowerCurveTests)

BOOST_AUTO_TEST_CASE(testShapeFactorsWithoutDST) {
    const Date d(10, Jun, 2026);

    // Shape with day-average exactly equal to 1.0 over 24h.
    // 0-900:   0.8
    // 900-1800:1.2
    // 1800-2700:0.8
    // 2700-3600:0.8
    // 3600-7200:1.1
    // 7200-86400:1.0
    std::map<int, Real> shape = {
        {0, 0.8}, {900, 1.2}, {1800, 0.8}, {2700, 0.8}, {3600, 1.1}, {7200, 1.0}
    };

    std::map<Date, std::map<int, Real>> shapes = {{d, shape}};
    std::map<Date, std::map<int, Real>> shapesDst; // Not used when DST adjustment is 0.

    IntradayShapeTermstructure ts(shapes, shapesDst);

    const Real dayFactor = ts.dayFactor(d);
    const Real hours = ts.hoursPerDay(d);

    const Real tol = 1e-12;
    BOOST_CHECK_CLOSE(dayFactor, 1.0, tol);
    BOOST_CHECK_CLOSE(hours, 24.0, tol);
}

BOOST_AUTO_TEST_CASE(testIntradayPriceWithLoadProfilesFallbackAndConsistency) {
    const Date today(10, Jun, 2026);
    const Date todayPlus1 = today + 1 * Days;
    const Date todayPlus2 = today + 2 * Days;

    Settings::instance().evaluationDate() = today;

    // Underlying flat dummy curve: 25 at two pillars.
    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(25.0)
    };
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<int, Real> scaleShape = {{0, 0.8}, {900, 1.2}, {1800, 0.8}, {2700, 0.8}, {3600, 1.1}, {7200, 1.0}};
    std::map<Date, std::map<int, Real>> shapeMap = {
        {today, scaleShape},
        {todayPlus2, scaleShape}
    };

    std::map<Date, std::map<int, Real>> shapeMapDst;
    
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    // Same granularity for both load profiles: full-day bucket [0, 86400).
    LoadFactors loadToday = {{0, 24 * 3600, 1.0}};
    LoadFactors loadTodayPlus2 = {{0, 24 * 3600, 2.0}};
    LoadFactors loadDstEmpty;

    auto lpToday = QuantLib::ext::make_shared<IntradayLoadProfile>(loadToday, loadDstEmpty);
    auto lpTodayPlus2 = QuantLib::ext::make_shared<IntradayLoadProfile>(loadTodayPlus2, loadDstEmpty);

    std::map<Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes = {
        {today, lpToday},
        {todayPlus2, lpTodayPlus2}
    };

    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructure>(loadingShapes);

    const Real tol = 1e-12;

    // Verify fallback semantics directly on loading term structure.
    BOOST_CHECK_CLOSE(loadTs->loadProfile(today)->totalMWh(), 24.0, tol);
    BOOST_CHECK_CLOSE(loadTs->loadProfile(todayPlus1)->totalMWh(), 24.0, tol); // falls back to today
    BOOST_CHECK_CLOSE(loadTs->loadProfile(todayPlus2)->totalMWh(), 48.0, tol);

    // Price consistency checks.
    const Real pToday = intradayTs->price(today, loadTs->loadProfile(today), true);
    const Real pTodayPlus1 = intradayTs->price(todayPlus1, loadTs->loadProfile(todayPlus1), true);
    const Real pTodayPlus2 = intradayTs->price(todayPlus2, loadTs->loadProfile(todayPlus2), true);

    BOOST_CHECK_CLOSE(pToday, 25.0, tol);
    BOOST_CHECK_CLOSE(pTodayPlus1, 25.0, tol);
    BOOST_CHECK_CLOSE(pTodayPlus2, 25.0, tol);
}

BOOST_AUTO_TEST_CASE(testBackwardFlatDailyCurveWithIntradayShapesAndLoads) {
    const Date d(10, Jun, 2026);
    const Date d7 = d + 7 * Days;
    const Date d10 = d + 10 * Days;
    const Date d15 = d + 15 * Days;
    const Date d16 = d + 16 * Days;
    const Date d30 = d + 30 * Days;

    Settings::instance().evaluationDate() = d;

    std::vector<Date> dates = {d, d10, d15, d30};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(26.0),
        QuantLib::ext::make_shared<SimpleQuote>(27.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1]), Handle<Quote>(q[2]),
                                         Handle<Quote>(q[3])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<BackwardFlat>>(
        d, dates, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<int, Real> shapeD;
    for (Size i = 0; i < 96; ++i) {
        shapeD[static_cast<int>(i * 15 * 60)] = (i % 2 == 0) ? 0.9 : 1.1;
    }

    std::map<int, Real> shapeD15;
    for (Size i = 0; i < 6; ++i) {
        shapeD15[static_cast<int>(i * 3600)] = (i % 2 == 0) ? 1.0 : 1.2;
    }
    shapeD15[6 * 3600] = 1.1;
    shapeD15[12 * 3600] = 1.0;
    shapeD15[18 * 3600] = 1.2;

    std::map<int, Real> shapeD30 = {{0, 1.2}};

    std::map<Date, std::map<int, Real>> shapeMap = {
        {d, shapeD},
        {d15, shapeD15},
        {d30, shapeD30}};
    std::map<Date, std::map<int, Real>> shapeMapDst;
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    LoadFactors loadD = {
        {8 * 3600 + 15 * 60, 8 * 3600 + 30 * 60, 100.0},
        {9 * 3600, 9 * 3600 + 30 * 60, 50.0},
        {10 * 3600, 10 * 3600 + 15 * 60, 100.0}};
    LoadFactors loadD7 = {
        {20 * 15 * 60, 21 * 15 * 60, 150.0},
        {21 * 15 * 60, 22 * 15 * 60, 100.0}
    };
    LoadFactors loadD15 = {
        {8 * 3600 + 30 * 60, 9 * 3600 + 30 * 60, 100.0}};
    LoadFactors loadD16 = {
        {8 * 3600, 10 * 3600, 100.0},
        {13 * 3600, 14 * 3600, 200.0}};
    LoadFactors loadDstEmpty;

    auto lpD = QuantLib::ext::make_shared<IntradayLoadProfile>(loadD, loadDstEmpty);
    auto lpD7 = QuantLib::ext::make_shared<IntradayLoadProfile>(loadD7, loadDstEmpty);
    auto lpD15 = QuantLib::ext::make_shared<IntradayLoadProfile>(loadD15, loadDstEmpty);
    auto lpD16 = QuantLib::ext::make_shared<IntradayLoadProfile>(loadD16, loadDstEmpty);

    std::map<Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes = {
        {d, lpD},
        {d7, lpD7},
        {d15, lpD15},
        {d16, lpD16}};
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructure>(loadingShapes);

    const Real tol = 1e-12;

    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d), 1.0, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d10), 1.0, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d15), 1.1, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d16), 1.1, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d30), 1.2, tol);

    BOOST_CHECK_CLOSE(lpD->totalMWh(), 75.0, tol);
    BOOST_CHECK_CLOSE(lpD15->totalMWh(), 100.0, tol);
    BOOST_CHECK_CLOSE(lpD16->totalMWh(), 400.0, tol);

    BOOST_CHECK_CLOSE(loadTs->loadProfile(d)->totalMWh(), 75.0, tol);
    BOOST_CHECK_CLOSE(loadTs->loadProfile(d10)->totalMWh(), 62.5, tol);
    BOOST_CHECK_CLOSE(loadTs->loadProfile(d15)->totalMWh(), 100.0, tol);
    BOOST_CHECK_CLOSE(loadTs->loadProfile(d16)->totalMWh(), 400.0, tol);
    BOOST_CHECK_CLOSE(loadTs->loadProfile(d30)->totalMWh(), 400.0, tol);

    BOOST_CHECK_CLOSE(intradayTs->price(d, loadTs->loadProfile(d), true), 25.0, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10, loadTs->loadProfile(d10), true), (0.9 * 1.5 + 1.1) / 2.5 * 25.0, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d15, loadTs->loadProfile(d15), true), 26.0 * 1.1, tol);
    // 2h at 1.1 shape factor and 100MW load + 1h at 1.1 shape factor and 200MW load = 4.2
                            // load-shape factor / 4h total time
    BOOST_CHECK_CLOSE(intradayTs->price(d16, loadTs->loadProfile(d16), true), baseCurve->price(d16) * 4.2 / 4.,
                      tol); 
    BOOST_CHECK_CLOSE(intradayTs->price(d30, loadTs->loadProfile(d30), true), 27.0 * 1.2, tol);
}

BOOST_AUTO_TEST_CASE(testEmptyOrNullShape) {
    const Date d(10, Jun, 2026);
    Settings::instance().evaluationDate() = d;

    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(26.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<Date, std::map<int, Real>> shapeMap;
    std::map<Date, std::map<int, Real>> shapeMapDst;
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    BOOST_CHECK_CLOSE(intradayTs->price(d, true), 25.0, 1e-12);

    auto intradayTs2 = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, nullptr);
    BOOST_CHECK_CLOSE(intradayTs2->price(d + 30, true), 26, 1e-12);
}

BOOST_AUTO_TEST_CASE(testNullOrEmptyLoadProfile) {
    const Date d(10, Jun, 2026);
    Settings::instance().evaluationDate() = d;

    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(26.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<int, Real> scaleShape = {{0, 0.8}, {900, 1.2}, {1800, 0.8}, {2700, 0.8}, {3600, 1.1}, {7200, 1.0}};
    std::map<Date, std::map<int, Real>> shapeMap = {{d, scaleShape}};
    std::map<Date, std::map<int, Real>> shapeMapDst;
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    LoadFactors emptyLoad;
    LoadFactors emptyLoadDst;
    auto lpEmpty = QuantLib::ext::make_shared<IntradayLoadProfile>(emptyLoad, emptyLoadDst);

    std::map<Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes = {{d, lpEmpty}};
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructure>(loadingShapes);

    const Real tol = 1e-12;
    BOOST_CHECK_CLOSE(intradayTs->price(d, loadTs->loadProfile(d), true), intradayTs->price(d, true), tol);

}

BOOST_AUTO_TEST_CASE(testIntradayPriceWithOverlappingLoadProfiles) {
    const Date day1(10, Jun, 2026);
    const Date day2 = day1 + 1 * Days;

    Settings::instance().evaluationDate() = day1;

    // Flat underlying curve at 25.
    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(25.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    // Same shape as existing test.
    std::map<int, Real> scaleShape = {
        {0, 0.8}, {900, 1.2}, {1800, 0.8}, {2700, 0.8}, {3600, 1.1}, {7200, 1.0}};
    std::map<Date, std::map<int, Real>> shapeMap = {
        {day1, scaleShape},
        {day2, scaleShape}};
    std::map<Date, std::map<int, Real>> shapeMapDst;

    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    // Day 1: load only on [800,1600), else zero.
    // Day 2: load only on [1200,1500), else zero.
    LoadFactors day1Load = {{800, 1600, 1.0}};
    LoadFactors day2Load = {{1200, 1500, 1.0}};
    LoadFactors loadDstEmpty;

    auto lpDay1 = QuantLib::ext::make_shared<IntradayLoadProfile>(day1Load, loadDstEmpty);
    auto lpDay2 = QuantLib::ext::make_shared<IntradayLoadProfile>(day2Load, loadDstEmpty);

    std::map<Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes = {
        {day1, lpDay1},
        {day2, lpDay2}};
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructure>(loadingShapes);

    // Expected factors from shape:
    // [800,1600): (0.8*100 + 1.2*700) / 800 = 1.15
    // [1200,1500): 1.2
    const Real expectedDay1 = 25.0 * 1.15;
    const Real expectedDay2 = 25.0 * 1.2;

    const Real pDay1 = intradayTs->price(day1, loadTs->loadProfile(day1), true);
    const Real pDay2 = intradayTs->price(day2, loadTs->loadProfile(day2), true);

    const Real tol = 1e-12;
    BOOST_CHECK_CLOSE(pDay1, expectedDay1, tol);
    BOOST_CHECK_CLOSE(pDay2, expectedDay2, tol);
    BOOST_CHECK_CLOSE(lpDay1->totalMWh(), 0.2222222222222222, tol); // 800s at 1.0 MW load = 800/3600 MWh
    BOOST_CHECK_CLOSE(lpDay2->totalMWh(), 0.08333333333333333, tol); // 300s at 1.0 MWload = 300/3600 MWh
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()