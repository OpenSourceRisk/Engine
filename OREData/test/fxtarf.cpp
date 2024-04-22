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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/tarf.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ql/time/date.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/math/distributions/normal.hpp>

#include <iostream>
#include <iomanip>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FxTaRfTest)

BOOST_AUTO_TEST_CASE(testNPV) {
    BOOST_TEST_MESSAGE("Testing Fx TaRF...");

    ORE_REGISTER_TRADE_BUILDER("ScriptedTrade", ore::data::ScriptedTrade, true)
    ORE_REGISTER_TRADE_BUILDER("FxTaRF", ore::data::FxTaRF, true)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::ScriptedTradeEngineBuilder, true)

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

    // move cleanup to fixture once this code is in ore minus
    struct cleanup {
        ~cleanup() { ore::data::ScriptLibraryStorage::instance().clear(); }
    } cleanup;
    ore::data::ScriptLibraryData library;
    library.fromFile(TEST_INPUT_FILE("scriptlibrary.xml"));
    ore::data::ScriptLibraryStorage::instance().set(std::move(library));

    // Read in the trade
    Portfolio p;
    string trade = "FX_TaRF";
    string portfolioFile = trade + ".xml";
    p.fromFile(TEST_INPUT_FILE(portfolioFile));
    BOOST_CHECK_NO_THROW(p.build(factory));

    const Real spot = 1.1469;
    const Real tol = 0.01;

    // Test case 1: An FxTARF with no KnockOutBarrier,one fixing and one leverage range should be equivalent to an
    // FxForward
    BOOST_CHECK_NO_THROW(p.get("FX_TARF_1")->instrument()->NPV());
    BOOST_TEST_MESSAGE(p.get("FX_TARF_1")->instrument()->NPV() * spot);
    BOOST_TEST_MESSAGE(p.get("FX_FORWARD_1")->instrument()->NPV());

    BOOST_CHECK_CLOSE(p.get("FX_TARF_1")->instrument()->NPV() * spot, p.get("FX_FORWARD_1")->instrument()->NPV(), tol);

    // Test case 2: An FxTARF with no KnockOutBarrier,several fixing dates and one leverage range should be equivalent
    // to the sum of FxForwards
    BOOST_CHECK_CLOSE(p.get("FX_TARF_2")->instrument()->NPV() * spot,
                      p.get("FX_FORWARD_2A")->instrument()->NPV() + p.get("FX_FORWARD_2B")->instrument()->NPV() +
                          p.get("FX_FORWARD_2C")->instrument()->NPV(),
                      tol);

    // Test case 3: The Value of an FxTARF with varying leverage ranges should be sum of FxTARFs with those individual
    // leverages (and zero for the other ranges)
    BOOST_CHECK_CLOSE(p.get("FX_TARF_3")->instrument()->NPV() * spot,
                      (p.get("FX_TARF_3A")->instrument()->NPV() + p.get("FX_TARF_3B")->instrument()->NPV() +
                       p.get("FX_TARF_3C")->instrument()->NPV()) *
                          spot,
                      tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
