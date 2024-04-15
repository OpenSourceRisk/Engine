/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
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

    TodaysMarketArguments(const Date& asof, const string& inputDir, const string& marketFile,
        const string& fixingsFile, const string& conventionsFile)
        : asof(asof) {

        Settings::instance().evaluationDate() = asof;

        string filename = inputDir + "/" + conventionsFile;
        conventions->fromFile(TEST_INPUT_FILE(filename));
        InstrumentConventions::instance().setConventions(conventions);
        
        filename = inputDir + "/curveconfig.xml";
        curveConfigs->fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/todaysmarket.xml";
        todaysMarketParameters->fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/" + marketFile;
        string fixingsFilename = inputDir + "/" + fixingsFile;
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

BOOST_AUTO_TEST_SUITE(InflationCurveTests)

// Test AU CPI bootstrap before and after index publication date in Oct 2020 (28 Oct 2020).
// Mirrors the conventions from the example in section 2.5 of AFMA Inflation Product Conventions, May 2017.

vector<Date> auCpiTestDates{
    Date(27, Oct, 2020), // Before Q3 CPI release date => swaps start on 15 Sep referencing Q2 CPI
    Date(28, Oct, 2020), // On Q3 CPI release date => swap start depends on PublicationRoll
    Date(29, Oct, 2020)  // After Q3 CPI release date => swaps start on 15 Dec referencing Q3 CPI
};

vector<string> publicationRolls{
    "on",    // Roll on the cpi release date
    "after"  // Roll after the cpi release date
};

BOOST_DATA_TEST_CASE(testAuCpiZcInflationCurve, bdata::make(auCpiTestDates) * bdata::make(publicationRolls),
    asof, publicationRoll) {

    BOOST_TEST_MESSAGE("Testing AU CPI zero coupon inflation curve bootstrap on date " << io::iso_date(asof));

    // Create the market arguments. The fixing file on the release date depends on the publication roll setting.
    string marketFile = "market_" + to_string(io::iso_date(asof)) + ".txt";
    string fixingsFile = "fixings_" + to_string(io::iso_date(asof));
    if (asof == Date(28, Oct, 2020)) {
        fixingsFile += "_" + publicationRoll;
    }
    fixingsFile += ".txt";
    string conventionsFile = "conventions_" + publicationRoll + ".xml";
    TodaysMarketArguments tma(asof, "aucpi_zc", marketFile, fixingsFile, conventionsFile);

    // Check that the market builds without error.
    QuantLib::ext::shared_ptr<TodaysMarket> market;
    BOOST_REQUIRE_NO_THROW(market = QuantLib::ext::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters,
        tma.loader, tma.curveConfigs, false, true, false));

    // Portfolio containing 2 AU CPI zero coupon swaps, AUD 10M, that should price at 0, i.e. NPV < AUD 0.01.
    // Similar to the fixing file, the helpers on the release date depend on the publication roll setting.
    string portfolioFile = "aucpi_zc/portfolio_" + to_string(io::iso_date(asof));
    if (asof == Date(28, Oct, 2020)) {
        portfolioFile += "_" + publicationRoll;
    }
    portfolioFile += ".xml";

    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("aucpi_zc/pricingengine.xml"));
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE(portfolioFile));
    portfolio->build(factory);

    BOOST_CHECK_EQUAL(portfolio->size(), 2);
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        BOOST_CHECK_SMALL(trade->instrument()->NPV(), 0.01);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

// clang-format on
