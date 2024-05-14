/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <ored/portfolio/cbo.hpp>
#include <ored/portfolio/builders/cbo.hpp>
#include <oret/toplevelfixture.hpp>

#include <oret/datapaths.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>

#include <ored/portfolio/portfolio.hpp>

#include <iostream>
#include <iomanip>

using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREPlusCreditTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CBOTest)

BOOST_AUTO_TEST_CASE(testSimpleCBO) {
    BOOST_TEST_MESSAGE("Testing simple CBO...");

    Settings::instance().evaluationDate() = Date(31, Dec, 2018);
    Date asof = Settings::instance().evaluationDate();

    // Market
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE("conventions.xml"));
    InstrumentConventions::instance().setConventions(conventions);

    auto todaysMarketParams = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParams->fromFile(TEST_INPUT_FILE("todaysmarket.xml"));
    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE("curveconfig.xml"));
    auto loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE("market.txt"), TEST_INPUT_FILE("fixings.txt"), false);
    auto market = QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, false);

    // Portfolio to test market
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("pricingengine.xml"));
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    Portfolio p;
    BOOST_CHECK_NO_THROW(p.fromFile(TEST_INPUT_FILE("cbo.xml")));
    BOOST_CHECK_NO_THROW(p.build(factory));

    // Pricing comparison
    double expectedNpv = 3013120.939;
    const Real tol = 0.01;

    BOOST_CHECK_NO_THROW(p.get("CBO-Constellation")->instrument()->NPV());
    BOOST_TEST_MESSAGE(p.get("CBO-Constellation")->instrument()->NPV());
    BOOST_CHECK_CLOSE(p.get("CBO-Constellation")->instrument()->NPV(), expectedNpv, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
