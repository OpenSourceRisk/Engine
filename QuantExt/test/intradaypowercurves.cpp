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

    const Real tol = 1e-12;
    BOOST_CHECK_CLOSE(dayFactor, 1.0, tol);
}

BOOST_AUTO_TEST_CASE(testShapeFactorsWithSpringDST) {
    const Date d(29, March, 2026); // Last Sunday of March, DST starts at 2am, 1 hour is skipped.

    // Shape with day-average exactly equal to 1.0 over 23h (since there is a DST change that makes the day 1 hour shorter) and it should ignore the 1.2 factor between 2 and 3 am since that hour does not exist on that day.
    std::map<int, Real> shape = {{0, 1.1}, {2 * 3600, 1.2}, {3 * 3600, 0.9}, {5 * 3600, 1.0}};

    std::map<Date, std::map<int, Real>> shapes = {{d, shape}};
    std::map<Date, std::map<int, Real>> shapesDst; // Only used for the extra hour in autumn DST change, not for spring DST change.

    IntradayShapeTermstructure ts(shapes, shapesDst, "EU");

    const Real dayFactor = ts.dayFactor(d);

    const Real tol = 1e-12;
    
    BOOST_CHECK_CLOSE(dayFactor, 1.0, tol);

    // Test the intraday shape factor for the 2-3am hour
    Real factor = ts.intradayShapeFactor(d, 2 * 3600, 3 * 3600, false);
    BOOST_CHECK_CLOSE(factor, 0.0, tol); // Should be zero since that hour does not exist on that day due to DST change.

    // Test the intraday shape factor for the 0-2:30am hour
    factor = ts.intradayShapeFactor(d, 0, 2 * 3600 + 1800, false);
    BOOST_CHECK_CLOSE(factor, 1.1, tol); // Should be 1.1 

    // Test the intraday shape factor for the 2-5am hour
    factor = ts.intradayShapeFactor(d, 2 * 3600, 5 * 3600, false);
    BOOST_CHECK_CLOSE(factor, 0.9, tol); // Should be 0.9 
}

BOOST_AUTO_TEST_CASE(testShapeFactorsWithBackwardDST) {
    const Date d(25, October, 2026); // Last Sunday of October, DST ends at 3am, 1 hour is repeated.

    // Shape with day-average exactly equal to 1.0 over 25h (since there is a DST change that makes the day 1 hour longer) and it should consider the extra hour between 2 and 3 am.
    std::map<int, Real> shape = {{0, 1.1}, {2 * 3600, 1.2}, {3 * 3600, 0.9}, {5 * 3600, 1.0}};
    std::map<int, Real> shapeDst = {{2 * 3600, 1.2}, {2.5 * 3600, 0.8}}; 
    
    std::map<Date, std::map<int, Real>> shapes = {{d-10*Days, shape}};
    std::map<Date, std::map<int, Real>> shapesDst = {{d, shapeDst}}; // Only used for the extra hour in autumn DST change, not for spring DST change.

    IntradayShapeTermstructure ts(shapes, shapesDst, "EU");

    const Real dayFactor = ts.dayFactor(d);

    const Real tol = 1e-12;
    
    BOOST_CHECK_CLOSE(dayFactor, 25.2/25.0, tol);

    // Test the intraday shape factor for the 2-3am hour
    Real factor = ts.intradayShapeFactor(d, 2 * 3600, 3 * 3600, false);
    BOOST_CHECK_CLOSE(factor, 1.2, tol); // Should be 1.2 is present

    factor = ts.intradayShapeFactor(d, 2 * 3600, 3 * 3600, true);
    BOOST_CHECK_CLOSE(factor, 1.0, tol); // Should be one, since its the dst extra hour

    IntradayShapeTermstructure ts2(shapes, std::map<Date, std::map<int, Real>>(), "EU");
    factor = ts2.intradayShapeFactor(d, 2 * 3600, 3 * 3600, true);
    BOOST_CHECK_CLOSE(factor, 1.2, tol); // Should be 1.2 since the DST shape factors are not present, so it should use the regular shape factors for the dst extra hour

    const Date d2(26, October, 2026); // Last Sunday of October, DST ends at 3am, 1 hour is repeated.
    factor = ts2.intradayShapeFactor(d2, 2 * 3600, 3 * 3600, true);
    BOOST_CHECK_CLOSE(factor, 0, tol); // Should be 0 since its not a dst day

}

BOOST_AUTO_TEST_CASE(testTotalLoadComputationWithDST) {
    LoadFactor loadFactor{0, 4 * 3600, 1.0,
                          false}; // Load factor of 1.0 from 0 to 4am, which includes the DST change at 3am.
    {
        // We on a forward dst day, remove the 2-3am time of it
        auto adjustedLoad = dstAdjustedTotalLoad(loadFactor, QuantExt::IntradayPowerDSTAdjustment::Forward);
        BOOST_CHECK_EQUAL(adjustedLoad.startTime, 0);
        BOOST_CHECK_EQUAL(adjustedLoad.endTime, 4 * 3600);
        BOOST_CHECK_CLOSE(adjustedLoad.load, 1.0, 1e-12);
        BOOST_CHECK_EQUAL(
            adjustedLoad.totalMWh,
            3.0); // Total MWh should be reduced by the load factor for the 1 hour, since 2-3am doesnt exists
    }
    {
        // No adjutment, all hours count
        auto adjustedLoad = dstAdjustedTotalLoad(loadFactor, QuantExt::IntradayPowerDSTAdjustment::NoAdjustment);
        BOOST_CHECK_EQUAL(adjustedLoad.totalMWh, 4.0);
    }
    {
        // We have extra hour and we have a backward day, load should be returned
        LoadFactor loadFactor2{2 * 3600, 2 * 3600 + 1800, 1.0, true};
        auto adjustedLoad = dstAdjustedTotalLoad(loadFactor2, QuantExt::IntradayPowerDSTAdjustment::Backward);
        BOOST_CHECK_EQUAL(adjustedLoad.totalMWh, 0.5);
    }
    {
        // Handle case that we have a DST extra hour load, but its not a backward day, so the load should be ignored since it doesnt exist on that day
        LoadFactor loadFactor2{2 * 3600,  2 * 3600 + 1800, 1.0, true};
        auto adjustedLoad = dstAdjustedTotalLoad(loadFactor2, QuantExt::IntradayPowerDSTAdjustment::NoAdjustment);
        BOOST_CHECK_EQUAL(adjustedLoad.totalMWh, 0);
    }
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
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying);
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
        BOOST_CHECK_CLOSE(shapeTs->intradayShapeFactor(today, start, end, false), expectedFactor, tol);
        auto price = intradayTs->price(today, start, end, false, true);
        auto expectedPrice = underlying->price(today) * expectedFactor;
        BOOST_CHECK_CLOSE(price, expectedPrice, tol);
    }

    // Test smaller buckets
    for (size_t i = 1; i < shapeFactors.size(); ++i) {
        auto start = shapeFactors[i - 1].first + 5 * 60;
        auto end = shapeFactors[i].first - 5 * 60;
        auto expectedFactor = shapeFactors[i - 1].second;
        BOOST_CHECK_CLOSE(shapeTs->intradayShapeFactor(today, start, end, false), expectedFactor, tol);
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

BOOST_AUTO_TEST_CASE(testIntradayPricesWithShapeTermStructureAndLoadProfile){
    const Date d(28, Mar, 2026); // day before the DST forward change
    const Date dForward = d + 1 * Days; // DST forward change day
    const Date dNormal(1, June, 2026);
    const Date dBackward(25, October, 2026); // DST backward change day
    const Real tol = 1e-12;

    Settings::instance().evaluationDate() = d;

    std::vector<Date> dates = {d, dForward, dNormal, dBackward};
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> q = {
        QuantLib::ext::make_shared<SimpleQuote>(25.0), QuantLib::ext::make_shared<SimpleQuote>(25.0),
        QuantLib::ext::make_shared<SimpleQuote>(25.0), QuantLib::ext::make_shared<SimpleQuote>(25.0)};
    std::vector<Handle<Quote>> quotes = {Handle<Quote>(q[0]), Handle<Quote>(q[1]), Handle<Quote>(q[2]),
                                         Handle<Quote>(q[3])};

    auto baseCurve = QuantLib::ext::make_shared<InterpolatedPriceCurve<BackwardFlat>>(d, dates, quotes,
                                                                                      Actual365Fixed(), USDCurrency());
    Handle<PriceTermStructure> underlying(baseCurve);

    std::map<int, Real> shapeD;
    for (Size i = 0; i < 96; ++i) {
        shapeD[static_cast<int>(i * 15 * 60)] = (i % 2 == 0) ? 0.9 : 1.1;
    }

    std::map<Date, std::map<int, Real>> shapeMap = {
        {d, shapeD}};

    std::map<Date, std::map<int, Real>> shapeMapDst { 
        {d, {{2 * 3600, 1.2}}}};

    auto shapeTs = QuantLib::ext::make_shared<IntradayShapeTermstructure>(shapeMap, shapeMapDst, "EU");
    auto intradayTs = QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(underlying, shapeTs);

    QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> loadingShape;
    loadingShape = QuantLib::ext::make_shared<IntradayPowerLoadProfile>(IntradayPowerLoadProfile{{7200, 8100, 100.0, false}, // 2-2:15am, should ignored on forward date
                                                                                                     {8100, 9000, 200.0, false}, // 2:15-2:30am, should ignored on forward date
                                                                                                     {9000, 9900, 100.0, false}, // 2:30-2:45am, should ignored on forward date
                                                                                                     {9900, 10800, 50.0, false}, // 2:45-3am, should ignored on forward date
                                                                                                     {10800, 14400, 100.0, false},
                                                                                                     {2*3600, 3*3600, 200.0, true} // DST extra hour, should only used when the day is a backward
                                                                                                    });

    std::map<Date, std::pair<Real, Real>> expectedPrices{
        {d, {213.75 / 212.50,  212.50}}, {dForward, {1.0, 100}}, // All load factors are ignored since they are in the 2-3am
        {dBackward, {1.1,  412.50   }}                                      // range which is skipped on forward DST change
    };

    for (const auto& [deliveryDate, value] : expectedPrices) {
        const auto& [expectedPrice, expectedMWh] = value;
        ext::shared_ptr<IntradayPowerLoadProfileWithMWh> adjustedProfile =
            ext::make_shared<IntradayPowerLoadProfileWithMWh>();

        adjustedProfile->reserve(loadingShape->size());
        auto totalMWh = 0.0;
        for(const auto& loadFactor : *loadingShape) {
            auto adjustment = dayTimeSavingsAdjustment(deliveryDate, "EU");
            adjustedProfile->emplace_back(dstAdjustedTotalLoad(loadFactor, adjustment));
            totalMWh += adjustedProfile->back().totalMWh; // Set the total MWh for the load factor
        }
        auto price = intradayTs->price(deliveryDate, adjustedProfile, true);
        
        BOOST_CHECK_CLOSE(price, 25 * expectedPrice, tol);
        BOOST_CHECK_CLOSE(totalMWh, expectedMWh, tol);
    }

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()