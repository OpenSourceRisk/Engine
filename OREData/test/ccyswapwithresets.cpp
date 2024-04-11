/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace ore::data;
using std::vector;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        // valuation date
        asof_ = Date(22, Aug, 2016);

        QuantLib::ext::shared_ptr<ore::data::Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
        conventions->add(QuantLib::ext::make_shared<ore::data::FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000",
                                                                     "USD,EUR", "true"));
        InstrumentConventions::instance().setConventions(conventions);

        // build vectors with dates and discount factors
        vector<Date> datesEUR = {asof_,
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
                                 asof_ + 2 * Years,
                                 asof_ + 3 * Years,
                                 asof_ + 4 * Years,
                                 asof_ + 5 * Years,
                                 asof_ + 6 * Years};
        vector<Date> datesUSD = {asof_,
                                 asof_ + 3 * Months,
                                 asof_ + 4 * Months,
                                 asof_ + 7 * Months,
                                 asof_ + 10 * Months,
                                 asof_ + 13 * Months,
                                 asof_ + 16 * Months,
                                 asof_ + 19 * Months,
                                 asof_ + 2 * Years,
                                 asof_ + 3 * Years,
                                 asof_ + 4 * Years,
                                 asof_ + 5 * Years,
                                 asof_ + 6 * Years};
        vector<DiscountFactor> dfsEUR = {1.0,      1.000972, 1.001138, 1.001309, 1.001452, 1.001663, 1.001826,
                                         1.002005, 1.002196, 1.002369, 1.002554, 1.00275,  1.002918, 1.003114,
                                         1.004134, 1.006005, 1.007114, 1.006773, 1.004282};
        vector<DiscountFactor> dfsUSD = {1.0,      0.997872, 0.997147, 0.99499, 0.992416, 0.989948, 0.987405,
                                         0.984774, 0.980358, 0.96908,  0.95704, 0.944041, 0.93004};

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            intDiscCurve(datesEUR, dfsEUR, Actual360(), TARGET());
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] =
            intDiscCurve(datesUSD, dfsUSD, Actual360(), UnitedStates(UnitedStates::Settlement));

        // build ibor index
        Handle<IborIndex> hEUR(parseIborIndex("EUR-EURIBOR-6M", intDiscCurve(datesEUR, dfsEUR, Actual360(), TARGET())));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;

        // add Eurib 6M fixing
        hEUR->addFixing(Date(18, Aug, 2016), -0.00191);
        Handle<IborIndex> hUSD(
            parseIborIndex("USD-LIBOR-3M", intDiscCurve(datesUSD, dfsUSD, Actual360(), UnitedStates(UnitedStates::Settlement))));
        iborIndices_[make_pair(Market::defaultConfiguration, "USD-LIBOR-3M")] = hUSD;

        // add Libor 3M fixing
        hUSD->addFixing(Date(18, Aug, 2016), 0.00811);

        // add fx rates
	std::map<std::string, Handle<Quote>> quotes;
	quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.1306));
	fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);
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

BOOST_AUTO_TEST_SUITE(CcySwapWithResetsTest)

// Ccy Swap with notional resets test, example from Bloomberg
BOOST_AUTO_TEST_CASE(testCcySwapWithResetsPrice) {

    BOOST_TEST_MESSAGE("Testing CcySwapWithResets Price...");

    // Bloomberg CCYswap and CCYswapReset prices (in USD)
    Real npvCCYswap = -349.69;
    Real npvCCYswapReset = 0;

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // test asof date
    Date today = market->asofDate();
    BOOST_CHECK_EQUAL(today, Date(22, Aug, 2016));

    // Test if EUR discount curve is empty
    Handle<YieldTermStructure> dts = market->discountCurve("EUR");
    QL_REQUIRE(!dts.empty(), "EUR discount curve not found");

    // Test EUR discount(today+3*Years)) against base value
    BOOST_CHECK_CLOSE(market->discountCurve("EUR")->discount(today + 3 * Years), 1.006005, 0.0001);

    // Test USD discount(today+3*Years)) against base value
    BOOST_CHECK_CLOSE(market->discountCurve("USD")->discount(today + 3 * Years), 0.96908, 0.0001);

    // Test EURUSD fx spot object against base value
    BOOST_CHECK_EQUAL(market->fxSpot("EURUSD")->value(), 1.1306);

    // envelope
    Envelope env("CP");

    // Start/End date
    Date startDate = today;
    Date endDate = today + 5 * Years;

    // date 2 string
    std::ostringstream oss;
    oss << io::iso_date(startDate);
    string start(oss.str());
    oss.str("");
    oss.clear();
    oss << io::iso_date(endDate);
    string end(oss.str());

    // Schedules
    string conv = "MF";
    string rule = "Forward";
    ScheduleData scheduleEUR(ScheduleRules(start, end, "6M", "TARGET", conv, conv, rule));
    ScheduleData scheduleUSD(ScheduleRules(start, end, "3M", "US", conv, conv, rule));

    // EUR Leg without and with Notional resets
    bool isPayerEUR = true;
    string indexEUR = "EUR-EURIBOR-6M";
    bool isInArrears = false;
    int days = 2;
    vector<Real> spreadEUR(1, 0.000261);
    string dc = "ACT/360";
    vector<Real> notionalEUR(1, 8833141.95);
    string paymentConvention = "F";
    bool notionalInitialXNL = true;
    bool notionalFinalXNL = true;
    bool notionalAmortizingXNL = false;
    string foreignCCY = "USD";
    Real foreignAmount = 10000000;
    string fxIndex = "FX-ECB-EUR-USD";
    auto legdataEUR = QuantLib::ext::make_shared<FloatingLegData>(indexEUR, days, isInArrears, spreadEUR);
    LegData legEUR1(legdataEUR, isPayerEUR, "EUR", scheduleEUR, dc, notionalEUR, vector<string>(), paymentConvention,
                    notionalInitialXNL, notionalFinalXNL, notionalAmortizingXNL, notionalFinalXNL, foreignCCY,
                    foreignAmount, fxIndex);
    LegData legEUR2(legdataEUR, isPayerEUR, "EUR", scheduleEUR, dc, notionalEUR, vector<string>(), paymentConvention,
                    notionalInitialXNL, notionalFinalXNL, notionalAmortizingXNL, false, foreignCCY, foreignAmount,
                    fxIndex);

    // USD Leg without notional resets
    bool isPayerUSD = false;
    string indexUSD = "USD-LIBOR-3M";
    vector<Real> spreadUSD(1, 0);
    vector<Real> notionalUSD(1, 10000000);
    auto legdataUSD = QuantLib::ext::make_shared<FloatingLegData>(indexUSD, days, isInArrears, spreadUSD);
    LegData legUSD(legdataUSD, isPayerUSD, "USD", scheduleUSD, dc, notionalUSD, vector<string>(), paymentConvention,
                   notionalInitialXNL, notionalFinalXNL, notionalAmortizingXNL);

    // Build swap trades
    QuantLib::ext::shared_ptr<Trade> swap1(new ore::data::Swap(env, legUSD, legEUR1));
    QuantLib::ext::shared_ptr<Trade> swap2(new ore::data::Swap(env, legUSD, legEUR2));

    // engine data and factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CrossCurrencySwap") = "DiscountedCashflows";
    engineData->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // build swaps and portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    swap1->id() = "XCCY_Swap_EUR_USD";
    swap2->id() = "XCCY_Swap_EUR_USD_RESET";

    portfolio->add(swap1);
    portfolio->add(swap2);
    portfolio->build(engineFactory);

    // check CCYswap NPV
    BOOST_TEST_MESSAGE("CcySwap Price = " << swap1->instrument()->NPV() << " " << swap1->npvCurrency()
                                          << ". BBG Price = " << npvCCYswap << " USD");
    // check if difference between ore and bbg NPV does not exceed 250 USD (on 10M notional)
    BOOST_CHECK_CLOSE(std::abs(swap1->instrument()->NPV() - npvCCYswap), 250, 5);

    // check CCYswapReset NPV
    BOOST_TEST_MESSAGE("CcySwapReset Price = " << swap2->instrument()->NPV() << " " << swap2->npvCurrency()
                                               << ". BBG Price = " << npvCCYswapReset << " USD");
    // check if difference between ore and bbg NPV does not exceed 250 USD (on 10M notional)
    BOOST_CHECK_CLOSE(std::abs(swap2->instrument()->NPV() - npvCCYswapReset), 250, 5);

    // check if sum(XNL) on resetting leg is zero
    Real sumXNL = 0;
    const vector<Leg>& legs = swap2->legs();
    for (Size i = 3; i < legs.size(); i++) {
        const QuantLib::Leg& leg = legs[i];
        for (Size j = 0; j < leg.size(); j++) {
            QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
            sumXNL += ptrFlow->amount();
        }
    }
    BOOST_CHECK_EQUAL(sumXNL, 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
