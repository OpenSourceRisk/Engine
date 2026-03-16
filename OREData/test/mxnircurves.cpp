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
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace boost::unit_test_framework;

using ore::test::TopLevelFixture;
using std::ostream;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(MXNIrCurvesTest)

// Test in-currency yield curve bootstrap
BOOST_AUTO_TEST_CASE(testSingleCurrencyYieldCurveBootstrap) {

    // Evaluation date
    Date asof(17, Apr, 2019);
    Settings::instance().evaluationDate() = asof;

    // Market
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE("conventions_01.xml"));
    InstrumentConventions::instance().setConventions(conventions);

    auto todaysMarketParams = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParams->fromFile(TEST_INPUT_FILE("todaysmarket_01.xml"));
    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE("curveconfig_01.xml"));
    auto loader =
        QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE("market_01.txt"), TEST_INPUT_FILE("fixings.txt"), false);
    QuantLib::ext::shared_ptr<TodaysMarket> market =
        QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, false);

    // Portfolio to test market
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("pricingengine_01.xml"));
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE("mxn_ir_swap.xml"));
    portfolio->build(factory);

    // The single trade in the portfolio is a MXN 10Y swap, i.e. 10 x 13 28D coupons, with nominal 100 million. The
    // rate on the swap is equal to the 10Y rate in the market file 'market_01.txt' so we should get an NPV of 0.
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    BOOST_CHECK_SMALL(portfolio->trades().begin()->second->instrument()->NPV(), 0.01);
}

// Test cross-currency yield curve bootstrap
BOOST_AUTO_TEST_CASE(testCrossCurrencyYieldCurveBootstrap) {

    // Evaluation date
    Date asof(17, Apr, 2019);
    Settings::instance().evaluationDate() = asof;

    // Market
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE("conventions_02.xml"));
    InstrumentConventions::instance().setConventions(conventions);
    
    auto todaysMarketParams = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParams->fromFile(TEST_INPUT_FILE("todaysmarket_02.xml"));
    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE("curveconfig_02.xml"));
    auto loader =
        QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE("market_02.txt"), TEST_INPUT_FILE("fixings.txt"), false);
    QuantLib::ext::shared_ptr<TodaysMarket> market =
        QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, false);

    // Portfolio to test market
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("pricingengine_02.xml"));
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE("mxn_usd_xccy_swap.xml"));
    portfolio->build(factory);

    // The single trade in the portfolio is a USD/MXN 10Y cross currency basis swap, i.e. 10 x 13 28D coupons, with
    // nominal USD 100 million. The spread on the swap is equal to the 10Y basis spread in the market file
    // 'market_02.txt' so we should get an NPV of 0.
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    BOOST_CHECK_SMALL(portfolio->trades().begin()->second->instrument()->NPV(), 0.01);
}

// Test cap floor strip
BOOST_AUTO_TEST_CASE(testCapFloorStrip) {

    // Evaluation date
    Date asof(17, Apr, 2019);
    Settings::instance().evaluationDate() = asof;

    // Market
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE("conventions_03.xml"));
    InstrumentConventions::instance().setConventions(conventions);
    
    auto todaysMarketParams = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParams->fromFile(TEST_INPUT_FILE("todaysmarket_03.xml"));
    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE("curveconfig_03.xml"));
    auto loader =
        QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE("market_03.txt"), TEST_INPUT_FILE("fixings.txt"), false);
    QuantLib::ext::shared_ptr<TodaysMarket> market =
        QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, false);

    // Portfolio to test market
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("pricingengine_03.xml"));
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE("mxn_ir_cap.xml"));
    portfolio->build(factory);

    // The single trade in the portfolio is a MXN 10Y cap, i.e. 10 x 13 28D coupons (without first caplet), with
    // nominal USD 100 million.
    BOOST_CHECK_EQUAL(portfolio->size(), 1);

    // Get the npv of the trade using the market i.e. the stripped optionlet surface from TodaysMarket
    Real npvTodaysMarket = portfolio->trades().begin()->second->instrument()->NPV();
    BOOST_TEST_MESSAGE("NPV using TodaysMarket is: " << npvTodaysMarket);

    // Price the same cap using the constant volatility from the market
    auto trade = portfolio->trades().begin()->second;
    BOOST_REQUIRE(trade);
    BOOST_REQUIRE(trade->legs().size() == 1);
    auto pricer = QuantLib::ext::make_shared<BlackIborCouponPricer>(Handle<OptionletVolatilityStructure>(
        QuantLib::ext::make_shared<ConstantOptionletVolatility>(0, NullCalendar(), Unadjusted, 0.20320, Actual365Fixed())));
    Leg leg = trade->legs().front();
    setCouponPricer(leg, pricer);
    Real npvMarketVol = CashFlows::npv(leg, **market->discountCurve("MXN"), false);
    BOOST_TEST_MESSAGE("NPV using the constant market volatility is: " << npvMarketVol);

    // Check the difference in the NPVs. Should be small if the stripping is working correctly.
    BOOST_CHECK_SMALL(fabs(npvTodaysMarket - npvMarketVol), 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
