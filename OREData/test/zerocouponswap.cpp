/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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
#include <ored/utilities/log.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/time/yearcounter.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace ore::data;
using std::vector;

// Inflation CPI Swap paying fixed * CPI(t)/baseCPI * N
// with last XNL N * (CPI(T)/baseCPI - 1)

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        // valuation date
        asof_ = Date(28, August, 2018);

        // build vectors with dates and discount factors for GBP
        vector<Date> datesGBP = {asof_,
                                 asof_ + 6 * Months,
                                 asof_ + 7 * Months,
                                 asof_ + 8 * Months,
                                 asof_ + 9 * Months,
                                 asof_ + 10 * Months,
                                 asof_ + 11 * Months,
                                 asof_ + 12 * Months,
                                 asof_ + 13 * Months,
                                 asof_ + 14 * Months,
                                 asof_ + 15 * Months,
                                 asof_ + 16 * Months,
                                 asof_ + 17 * Months,
                                 asof_ + 18 * Months,
                                 asof_ + 19 * Months,
                                 asof_ + 20 * Months,
                                 asof_ + 21 * Months,
                                 asof_ + 22 * Months,
                                 asof_ + 23 * Months,
                                 asof_ + 2 * Years,
                                 asof_ + 3 * Years,
                                 asof_ + 4 * Years,
                                 asof_ + 5 * Years,
                                 asof_ + 6 * Years,
                                 asof_ + 7 * Years,
                                 asof_ + 8 * Years,
                                 asof_ + 9 * Years,
                                 asof_ + 10 * Years,
                                 asof_ + 15 * Years,
                                 asof_ + 20 * Years};

        vector<DiscountFactor> dfsGBP = {1,      0.9955, 0.9953, 0.9947, 0.9941, 0.9933, 0.9924, 0.9914,
                                         0.9908, 0.9901, 0.9895, 0.9888, 0.9881, 0.9874, 0.9868, 0.9862,
                                         0.9855, 0.9849, 0.9842, 0.9836, 0.9743, 0.9634, 0.9510, 0.9361,
                                         0.9192, 0.9011, 0.8822, 0.8637, 0.7792, 0.7079};

        // build GBP discount curve
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] =
            intDiscCurve(datesGBP, dfsGBP, ActualActual(ActualActual::ISDA), UnitedKingdom());

        // build GBP Libor index
        Handle<IborIndex> hGBP = Handle<IborIndex>(
            parseIborIndex("GBP-LIBOR-6M", intDiscCurve(datesGBP, dfsGBP, ActualActual(ActualActual::ISDA), UnitedKingdom())));
        iborIndices_[make_pair(Market::defaultConfiguration, "GBP-LIBOR-6M")] = hGBP;
    }

private:
    Handle<YieldTermStructure> intDiscCurve(vector<Date> dates, vector<DiscountFactor> dfs, DayCounter dc,
                                            Calendar cal) {
        QuantLib::ext::shared_ptr<YieldTermStructure> idc(
            new QuantLib::InterpolatedDiscountCurve<LogLinear>(dates, dfs, dc, cal));
        return Handle<YieldTermStructure>(idc);
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ZeroCouponSwapTests)

BOOST_AUTO_TEST_CASE(testZeroCouponSwapPrice) {

    BOOST_TEST_MESSAGE("Testing Zero Coupon Swap Price...");

    // build market
    Date today(28, August, 2018);
    Settings::instance().evaluationDate() = today;
    QuantLib::ext::shared_ptr<TestMarket> market = QuantLib::ext::make_shared<TestMarket>();
    Date marketDate = market->asofDate();
    BOOST_CHECK_EQUAL(today, marketDate);
    Settings::instance().evaluationDate() = marketDate;

    // Test if GBP discount curve is empty
    Handle<YieldTermStructure> dts = market->discountCurve("GBP");
    QL_REQUIRE(!dts.empty(), "GBP discount curve not found");
    BOOST_CHECK_CLOSE(market->discountCurve("GBP")->discount(today + 1 * Years), 0.9914, 0.0001);

    // Test if GBP Libor curve is empty
    Handle<IborIndex> iis = market->iborIndex("GBP-LIBOR-6M");
    QL_REQUIRE(!iis.empty(), "GBP LIBOR 6M ibor Index not found");
    BOOST_TEST_MESSAGE(
        "CPISwap: Projected Libor fixing: " << market->iborIndex("GBP-LIBOR-6M")->forecastFixing(today + 1 * Years));

    // envelope
    Envelope env("CP");

    // Start/End date
    Size years = 5;
    Date startDate = today;
    Date endDate = today + years * Years;

    // date 2 string
    std::ostringstream oss;
    oss << io::iso_date(startDate);
    string start(oss.str());
    oss.str("");
    oss.clear();
    oss << io::iso_date(endDate);
    string end(oss.str());

    // Leg variables
    vector<Real> notional(1, 1000000);
    string paymentConvention = "MF";
    Real rate = 0.02;
    vector<Real> rates;
    rates.push_back(rate);

    // Schedule
    string conv = "MF";
    string rule = "Zero";
    ScheduleData scheduleData(ScheduleRules(start, end, "5y", "UK", conv, conv, rule));
    Schedule schedule = makeSchedule(scheduleData);
    BOOST_CHECK_EQUAL(schedule.dates().size(), 2);

    // GBP Libor Leg
    bool isPayerLibor = true;
    string indexLibor = "GBP-LIBOR-6M";
    LegData leg(QuantLib::ext::make_shared<ZeroCouponFixedLegData>(rates), isPayerLibor, "GBP", scheduleData, "Year", notional,
                vector<string>(), paymentConvention);

    // Build swap trades
    vector<LegData> legData;
    legData.push_back(leg);
    QuantLib::ext::shared_ptr<Trade> swap(new ore::data::Swap(env, legData));

    // engine data and factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // build swaps and portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    swap->id() = "Swap";

    portfolio->add(swap);
    portfolio->build(engineFactory);

    Date maturity = schedule.endDate();
    Date fixedPayDate = schedule.calendar().adjust(maturity, parseBusinessDayConvention(paymentConvention));
    Real df = market->discountCurve("GBP")->discount(fixedPayDate);
    Real expectedNPV = -1 * notional.back() * (pow(1.0 + rate, years) - 1.0) * df;
    BOOST_CHECK_CLOSE(swap->instrument()->NPV(), expectedNPV, 1E-8); // this is 1E-10 rel diff
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
