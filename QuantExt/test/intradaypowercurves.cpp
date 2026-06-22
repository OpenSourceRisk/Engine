/*
 Copyright (C) 2026 AcadiaSoft Inc
*/

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>

#include <qle/termstructures/intradayshapetermstructure.hpp>
#include <qle/termstructures/intradaypowerpricetermstructure.hpp>
#include <qle/termstructures/intradaypowerloadtermstructure.hpp>
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
    std::map<int, Real> shape = {{0, 0.8}, {900, 1.2}, {1800, 0.8}, {2700, 0.8}, {3600, 1.1}, {7200, 1.0}};

    std::map<Date, std::map<int, Real>> shapes = {{d, shape}};
    std::map<Date, std::map<int, Real>> shapesDst; // Not used when DST adjustment is 0.

    IntradayShapeTermstructure ts(shapes, shapesDst);

    const Real dayFactor = ts.dayFactor(d);
    const Real hours = ts.hoursPerDay(d);

    const Real tol = 1e-12;
    BOOST_CHECK_CLOSE(dayFactor, 1.0, tol);
    BOOST_CHECK_CLOSE(hours, 24.0, tol);
}

BOOST_AUTO_TEST_CASE(testIntradayPricesNoShape) {
    const Date today(10, Jun, 2026);

    Settings::instance().evaluationDate() = today;

    // Underlying flat dummy curve: 25 at two pillars.
    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {QuantLib::ext::make_shared<SimpleQuote>(25.0),
                                                             QuantLib::ext::make_shared<SimpleQuote>(25.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, nullptr);
    BOOST_CHECK_CLOSE(intradayTs->price(today), underlying->price(today), 1e-12);
}

BOOST_AUTO_TEST_CASE(testIntradayPricesEmptyShape) {
    const Date today(10, Jun, 2026);

    Settings::instance().evaluationDate() = today;

    // Underlying flat dummy curve: 25 at two pillars.
    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {QuantLib::ext::make_shared<SimpleQuote>(25.0),
                                                             QuantLib::ext::make_shared<SimpleQuote>(25.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);
    std::map<Date, std::map<int, Real>> shapeMap;
    std::map<Date, std::map<int, Real>> shapeMapDst;
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);
    BOOST_CHECK_CLOSE(intradayTs->price(today), underlying->price(today), 1e-12);
}

BOOST_AUTO_TEST_CASE(testIntradayPricesWithShape) {
    const Date today(10, Jun, 2026);

    Settings::instance().evaluationDate() = today;

    // Underlying flat dummy curve: 25 at two pillars.
    std::vector<Period> tenors = {0 * Days, 30 * Days};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {QuantLib::ext::make_shared<SimpleQuote>(25.0),
                                                             QuantLib::ext::make_shared<SimpleQuote>(25.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1])};

    auto baseCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(tenors, quotes, Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::vector<std::pair<int, Real>> shapeFactors = {{0, 1.0},    {900, 1.2},  {1800, 0.9},
                                                      {2700, 1.2}, {3600, 1.1}, {7200, 1.0}};

    std::map<int, Real> scaleShape;

    for (const auto& [start, factor] : shapeFactors) {
        scaleShape[start] = factor;
    }

    std::map<Date, std::map<int, Real>> shapeMap = {{today, scaleShape}};
    std::map<Date, std::map<int, Real>> shapeMapDst;

    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    
    const Real tol = 1e-12;
    // Test day average price 
    auto dayAveragePrice = intradayTs->price(today, 0, 86400, false, true);
    auto expectedDayAveragePrice = underlying->price(today) * shapeTs->dayFactor(today);
    BOOST_CHECK_CLOSE(dayAveragePrice, expectedDayAveragePrice, tol);

    // Test day average price from date only
    auto dayAveragePriceWithoutTime = intradayTs->price(today, true);
    BOOST_CHECK_CLOSE(dayAveragePriceWithoutTime, expectedDayAveragePrice, tol);

    // Test bucket prices
    for (size_t i = 1; i < shapeFactors.size(); ++i) {
        auto start = shapeFactors[i - 1].first;
        auto end = shapeFactors[i].first;
        auto expectedFactor = shapeFactors[i - 1].second;
        auto price = intradayTs->price(today, start, end, false, true);
        auto expectedPrice = underlying->price(today) * expectedFactor;
        BOOST_CHECK_CLOSE(price, expectedPrice, tol);
    }

    // Test smaller buckets
    for (size_t i = 1; i < shapeFactors.size(); ++i) {
        auto start = shapeFactors[i - 1].first + 5 * 60;
        auto end = shapeFactors[i].first - 5 * 60;
        auto expectedFactor = shapeFactors[i - 1].second;
        auto price = intradayTs->price(today, start, end, false, true);
        auto expectedPrice = underlying->price(today) * expectedFactor;
        BOOST_CHECK_CLOSE(price, expectedPrice, tol);
    }

    // Test overlapping buckets
    for (size_t i = 2; i < shapeFactors.size(); ++i) {
        auto start = shapeFactors[i - 1].first - 5 * 60;
        auto end = shapeFactors[i].first + 5 * 60;
        auto expectedFactor = (shapeFactors[i - 2].second * (shapeFactors[i - 1].first - start) +
                               shapeFactors[i - 1].second * (shapeFactors[i].first - shapeFactors[i-1].first) +
                               shapeFactors[i].second * (end - shapeFactors[i].first)) /
                              (end - start);
        auto price = intradayTs->price(today, start, end, false, true);
        auto expectedPrice = underlying->price(today) * expectedFactor;
        BOOST_CHECK_CLOSE(price, expectedPrice, tol);
    }
}

BOOST_AUTO_TEST_CASE(testIntradayPricesWithShapeTermStructure) {
    const Date d(10, Jun, 2026);
    const Date d5 = d + 5 * Days;
    const Date d10 = d + 10 * Days;
    const Date d15 = d + 15 * Days;
    const Real tol = 1e-12;
    Settings::instance().evaluationDate() = d;

    std::vector<Date> dates = {d, d5, d10, d15};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0), QuantLib::ext::make_shared<SimpleQuote>(26.0),
        QuantLib::ext::make_shared<SimpleQuote>(28.0), QuantLib::ext::make_shared<SimpleQuote>(31.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1]), Handle<Quote>(q[2]),
                                         Handle<Quote>(q[3])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<BackwardFlat>>(d, dates, quotes,
                                                                                      Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<int, Real> shapeD;
    for (Size i = 0; i < 96; ++i) {
        shapeD[static_cast<int>(i * 15 * 60)] = (i % 2 == 0) ? 0.9 : 1.1;
    }

    std::map<int, Real> shapeD5;
    for (Size i = 0; i < 24; ++i) {
        shapeD5[static_cast<int>(i * 3600)] = (i % 2 == 0) ? 0.8 : 1.2;
    }

    std::map<int, Real> shapeD10 = {{0, 1.2}};

    std::map<Date, std::map<int, Real>> shapeMap = {
        {d, shapeD},
        {d5, shapeD5},
        {d10, shapeD10}};
    std::map<Date, std::map<int, Real>> shapeMapDst;
    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst);
    // Test that the correct shape is picked for each date and that the day average price is correct.
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d), 1.0, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d+1*Days), 1.0, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d5), 1.0, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d10 ), 1.2, tol);
    BOOST_CHECK_CLOSE(shapeTs->dayFactor(d10+1*Days), 1.2, tol);

    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);
    // Check day average prices for each day
    BOOST_CHECK_CLOSE(intradayTs->price(d), 25, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d+1), underlying->price(d+1) * shapeTs->dayFactor(d+1), tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d5), 26, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10), 28 * 1.2, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10+1*Days), underlying->price(d10+1*Days) * shapeTs->dayFactor(d10+1*Days), tol);

    // Check pricing 0-15 min bucket on day d
    BOOST_CHECK_CLOSE(intradayTs->price(d, 0, 15 * 60, false, true), 25 * 0.9, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d + 1, 0, 15 * 60, false, true), underlying->price(d + 1 * Days) * 0.9, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d5, 0, 15 * 60, false, true), underlying->price(d5) * 0.8, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10-1, 0, 15 * 60, false, true), underlying->price(d10 - 1 * Days) * 0.8, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10, 0, 15 * 60, false, true), underlying->price(d10) * 1.2, tol);
    BOOST_CHECK_CLOSE(intradayTs->price(d10 + 1 * Days, 0, 15 * 60, false, true), underlying->price(d10 + 1 * Days) * 1.2, tol);
}

/*
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
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructureExplicit>(loadingShapes);

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
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructureExplicit>(loadingShapes);

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
    auto loadTs = QuantLib::ext::make_shared<IntradayPowerLoadTermStructureExplicit>(loadingShapes);

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
*/

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()