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

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/make_shared.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/fixings.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <oret/datapaths.hpp>
#include <tuple>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

using ore::test::TopLevelFixture;

namespace bdata = boost::unit_test::data;

namespace {

// Give back the expected results for the tests on the various trade types below
// The results are read in from the file "test_trade_types_expected.csv"
map<tuple<string, string, bool, bool>, map<string, set<Date>>> tradeTypeExpected() {
    
    static map<tuple<string, string, bool, bool>, map<string, set<Date>>> exp;

    string expResultsFile = TEST_INPUT_FILE("test_trade_types_expected.csv");
    CSVFileReader reader(expResultsFile, true, ",");

    while (reader.next()) {
        string tradeType = reader.get("trade_type");
        string tradeCase = reader.get("trade_case");
        bool includeSettlementDateFlows = parseBool(reader.get("isdf"));
        bool enforcesTodaysHistoricFixings = parseBool(reader.get("ethf"));
        string indexName = reader.get("index_name");
        
        string datesList = reader.get("dates");
        vector<string> strDates;
        boost::split(strDates, datesList, boost::is_any_of("|"));
        set<Date> dates;
        for (const auto& strDate : strDates) {
            dates.insert(parseDate(strDate));
        }
        
        auto key = make_tuple(tradeType, tradeCase, includeSettlementDateFlows, enforcesTodaysHistoricFixings);
        if (exp.count(key) == 0) {
            exp[key] = {};
        }
        exp[key][indexName] = dates;
    }

    return exp;
}

// Give back the dummy fixings keyed on [Index Name, Date] pair. We will load chosen elements 
// from this map at the end of the trade tests below to check that the trade prices.
map<tuple<string, Date>, Fixing> dummyFixings() {

    static map<tuple<string, Date>, Fixing> result;

    string dummyFixingsFile = TEST_INPUT_FILE("test_trade_types_dummy_fixings.csv");
    CSVFileReader reader(dummyFixingsFile, true, ",");

    while (reader.next()) {
        string name = reader.get("name");
        Date date = parseDate(reader.get("date"));
        Real fixing = parseReal(reader.get("value"));

        result.emplace(make_pair(name, date), Fixing(date, name, fixing));
    }

    return result;
}

// Load the requested fixings
void loadFixings(const map<string, set<Date>>& requestedFixings, const Conventions& conventions) {

    // Get the dummy fixings that we have provided in the input directory
    auto fixingValues = dummyFixings();

    // Fetch the relevant fixings using the requestedFixings argument
    vector<Fixing> relevantFixings;
    for (const auto& kv : requestedFixings) {
        for (const auto& d : kv.second) {
            relevantFixings.push_back(fixingValues.at(make_pair(kv.first, d)));
        }
    }

    // Add the fixings in QuantLib index manager
    applyFixings(relevantFixings, conventions);
}

// Fixture used in test case below:
// - sets a specific valuation date for the test
// - provides conventions
// - provides and engine factory for the test
class F : public TopLevelFixture {
public:
    Date today;
    Conventions conventions;
    boost::shared_ptr<EngineFactory> engineFactory;

    F() {
        today = Date(12, Feb, 2019);
        Settings::instance().evaluationDate() = today;

        conventions.fromFile(TEST_INPUT_FILE("market/conventions.xml"));

        TodaysMarketParameters todaysMarketParams;
        todaysMarketParams.fromFile(TEST_INPUT_FILE("market/todaysmarket.xml"));

        CurveConfigurations curveConfigs;
        curveConfigs.fromFile(TEST_INPUT_FILE("market/curveconfig.xml"));

        string marketFile = TEST_INPUT_FILE("market/market.txt");
        string fixingsFile = TEST_INPUT_FILE("market/fixings.txt");
        CSVLoader loader(marketFile, fixingsFile, false);

        bool continueOnError = false;
        boost::shared_ptr<TodaysMarket> market = boost::make_shared<TodaysMarket>(
            today, todaysMarketParams, loader, curveConfigs, conventions, continueOnError);

        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->fromFile(TEST_INPUT_FILE("market/pricingengine.xml"));

        engineFactory = boost::make_shared<EngineFactory>(engineData, market);
    }

    ~F() {}
};

// List of trades that will feed the data-driven test below. This is a list of input folder names
// under OREData/test/input/fixings. In each folder, there are three test portfolio files containing a 
// trade of the given type. The three files cover three cases:
// - simple case where fixing date < today < payment date
// - payment today where a coupon that relies on an index has payment date == today
// - fixing today where a coupon that relies on an index has fixing date == today
vector<string> tradeTypes = {
    "fixed_float_swap",
    "in_ccy_basis_swap"
};

vector<string> tradeCases = { "simple_case", "payment_today", "fixing_today" };

vector<bool> bools = { true, false };

vector<bool> enforcesTodaysHistoricFixings = { true, false };

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FixingsTests)

BOOST_DATA_TEST_CASE_F(F, testTradeTypes, bdata::make(tradeTypes) * bdata::make(tradeCases) * bdata::make(bools) * bdata::make(bools), 
    tradeType, tradeCase, includeSettlementDateFlows, enforcesTodaysHistoricFixings) {

    // Set the flag determining what happens if fixings are required today
    Settings::instance().enforcesTodaysHistoricFixings() = enforcesTodaysHistoricFixings;

    // Read in the trade
    Portfolio p;
    string portfolioFile = tradeType + "/" + tradeCase + ".xml";
    p.load(TEST_INPUT_FILE(portfolioFile));
    BOOST_REQUIRE_MESSAGE(p.size() == 1, "Expected portfolio to contain a single trade");

    // Ask for fixings before trades are built should return empty set
    auto m = p.fixings(includeSettlementDateFlows, today);
    BOOST_CHECK_MESSAGE(m.empty(), "Expected fixings to be empty when trades not built");

    // Build the portfolio and retrieve the fixings
    p.build(engineFactory);
    m = p.fixings(includeSettlementDateFlows, today);

    // Check the retrieved fixings against the expected results
    auto exp = tradeTypeExpected();
    auto key = make_tuple(tradeType, tradeCase, includeSettlementDateFlows, enforcesTodaysHistoricFixings);
    if (exp.count(key) == 0) {
        // Expected result is no required fixings
        BOOST_CHECK_MESSAGE(m.empty(), "Expected no required fixings for [" << tradeType << ", " << tradeCase << ", " << 
            ore::data::to_string(includeSettlementDateFlows) << ", " << ore::data::to_string(enforcesTodaysHistoricFixings) << 
            "] but got a map containing " << m.size() << " indices");

        // Trade should not throw if we ask for NPV
        BOOST_CHECK_NO_THROW(p.trades()[0]->instrument()->NPV());

    } else {
        // Check the retrieved fixings against the expected fixings
        auto expMap = exp.at(key);
        BOOST_CHECK_EQUAL(expMap.size(), m.size());
        for (const auto& kv : expMap) {
            BOOST_CHECK_MESSAGE(m.count(kv.first), "Could not find index " << kv.first << " in retrieved fixings");
            BOOST_CHECK_EQUAL_COLLECTIONS(kv.second.begin(), kv.second.end(), m.at(kv.first).begin(), m.at(kv.first).end());
        }

        // Trade should throw if we ask for NPV and have not added the fixings
        BOOST_CHECK_THROW(p.trades()[0]->instrument()->NPV(), Error);

        // Add the fixings
        loadFixings(m, conventions);

        // Trade should now not throw when we try to price it
        BOOST_CHECK_NO_THROW(p.trades()[0]->instrument()->NPV());
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
