/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

namespace {

// Construct and hold the arguments needed to construct a TodaysMarket.
struct TodaysMarketArguments {

    TodaysMarketArguments(const Date& asof, const string& curveconfigFile)
        : asof(asof) {

        Settings::instance().evaluationDate() = asof;

        string filename = "conventions.xml";
        conventions->fromFile(TEST_INPUT_FILE(filename));
        InstrumentConventions::instance().setConventions(conventions);

        filename = curveconfigFile;
        curveConfigs->fromFile(TEST_INPUT_FILE(filename));

        filename = "todaysmarket.xml";
        todaysMarketParameters->fromFile(TEST_INPUT_FILE(filename));

        filename = "market.txt";
        string fixingsFilename = "fixings.txt";
        loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE(filename), TEST_INPUT_FILE(fixingsFilename), false);
    }

    Date asof;
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    QuantLib::ext::shared_ptr<CurveConfigurations> curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    QuantLib::ext::shared_ptr<Loader> loader;
};

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FxVolCurveTests)

BOOST_AUTO_TEST_CASE(testFxVolWildCards) {

    BOOST_TEST_MESSAGE("Testing FxVolatility Curve Wildcards");
    
    std::vector<std::pair<std::string, std::string>> curveConfigs = {
        { "curveconfig_full.xml", "curveconfig_wc.xml" },
        { "curveconfig_full_M.xml", "curveconfig_wc_M.xml" }
    };

    for (auto& c : curveConfigs) {
        Date asof(31, Dec, 2018);
        TodaysMarketArguments tma_full(asof, c.first);
        TodaysMarketArguments tma_wc(asof, c.second);

        // Check that the market builds without error.
        QuantLib::ext::shared_ptr<TodaysMarket> market_full = QuantLib::ext::make_shared<TodaysMarket>(tma_full.asof, tma_full.todaysMarketParameters,
            tma_full.loader, tma_full.curveConfigs, false, true, false);
        QuantLib::ext::shared_ptr<TodaysMarket> market_wc = QuantLib::ext::make_shared<TodaysMarket>(tma_wc.asof, tma_wc.todaysMarketParameters,
            tma_wc.loader, tma_wc.curveConfigs, false, true, false);

        // Portfolio containing 2 AU CPI zero coupon swaps, AUD 10M, that should price at 0, i.e. NPV < AUD 0.01.
        // Similar to the fixing file, the helpers on the release date depend on the publication roll setting.

        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->model("FxOption") = "GarmanKohlhagen";
        engineData->engine("FxOption") = "AnalyticEuropeanEngine";
        QuantLib::ext::shared_ptr<EngineFactory> engineFactory_full = QuantLib::ext::make_shared<EngineFactory>(engineData, market_full);
        QuantLib::ext::shared_ptr<EngineFactory> engineFactory_wc = QuantLib::ext::make_shared<EngineFactory>(engineData, market_wc);
        
        
        string portfolioFile = "portfolio.xml";
        QuantLib::ext::shared_ptr<Portfolio> portfolio_full = QuantLib::ext::make_shared<Portfolio>();
        portfolio_full->fromFile(TEST_INPUT_FILE(portfolioFile));
        portfolio_full->build(engineFactory_full);
        
        QuantLib::ext::shared_ptr<Portfolio> portfolio_wc = QuantLib::ext::make_shared<Portfolio>();
        portfolio_wc->fromFile(TEST_INPUT_FILE(portfolioFile));
        portfolio_wc->build(engineFactory_wc);

        BOOST_CHECK_EQUAL(portfolio_full->size(), portfolio_wc->size());
        
        auto portfolioFullIt = portfolio_full->trades().begin();
        auto portfolioWCIt = portfolio_wc->trades().begin();

        for (Size i = 0; i < portfolio_full->trades().size(); i++) {
            BOOST_CHECK_CLOSE(portfolioFullIt->second->instrument()->NPV(), portfolioWCIt->second->instrument()->NPV(), 0.001);
            portfolioFullIt++;
            portfolioWCIt++;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
