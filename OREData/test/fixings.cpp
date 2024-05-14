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
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/fixings.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/indexes/dividendmanager.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <tuple>

using namespace QuantLib;
using namespace QuantExt;
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
void loadFixings(const map<string, RequiredFixings::FixingDates>& requestedFixings) {

    // Get the dummy fixings that we have provided in the input directory
    auto fixingValues = dummyFixings();

    // Fetch the relevant fixings using the requestedFixings argument
    set<Fixing> relevantFixings;
    for (const auto& [indexName, fixingDates] : requestedFixings) {
        for (const auto& [d, mandatory] : fixingDates) {
            relevantFixings.insert(fixingValues.at(make_pair(indexName, d)));
        }
    }

    // Add the fixings in QuantLib index manager
    applyFixings(relevantFixings);
}

// Fixture used in test case below:
// - sets a specific valuation date for the test
// - provides conventions
// - provides and engine factory for the test
class F : public TopLevelFixture {
public:
    Date today;
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory;

    F() {
        today = Date(12, Feb, 2019);
        Settings::instance().evaluationDate() = today;

        conventions->fromFile(TEST_INPUT_FILE("market/conventions.xml"));
        InstrumentConventions::instance().setConventions(conventions);
        
        auto todaysMarketParams = QuantLib::ext::make_shared<TodaysMarketParameters>();
        todaysMarketParams->fromFile(TEST_INPUT_FILE("market/todaysmarket.xml"));

        auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
        curveConfigs->fromFile(TEST_INPUT_FILE("market/curveconfig.xml"));

        string marketFile = TEST_INPUT_FILE("market/market.txt");
        string fixingsFile = TEST_INPUT_FILE("market/fixings_for_bootstrap.txt");
        string dividendsFile = TEST_INPUT_FILE("market/dividends.txt");
        auto loader = QuantLib::ext::make_shared<CSVLoader>(marketFile, fixingsFile, dividendsFile, false);

        bool continueOnError = false;
        QuantLib::ext::shared_ptr<TodaysMarket> market = QuantLib::ext::make_shared<TodaysMarket>(
            today, todaysMarketParams, loader, curveConfigs, continueOnError);

        QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
        engineData->fromFile(TEST_INPUT_FILE("market/pricingengine.xml"));

        engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    }

    ~F() {}
};

// List of trades that will feed the data-driven test below. This is a list of input folder names
// under OREData/test/input/fixings. In each folder, there are three test portfolio files containing a
// trade of the given type. The three files cover three cases:
// - simple case where fixing date < today < payment date
// - payment today where a coupon that relies on an index has payment date == today
// - fixing today where a coupon that relies on an index has fixing date == today
vector<string> tradeTypes = {"fixed_float_swap",     "in_ccy_basis_swap",       "zciis_with_interp",
                             "cpi_swap_with_interp", "yoy_swap_without_interp", "xccy_resetting_swap",
                             "equity_swap",          "cms_spread_swap"};

vector<string> tradeCases = {"simple_case", "payment_today", "fixing_today"};

vector<bool> bools = {true, false};

vector<bool> enforcesTodaysHistoricFixings = {true, false};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FixingsTests)

BOOST_DATA_TEST_CASE_F(F, testTradeTypes,
                       bdata::make(tradeTypes) * bdata::make(tradeCases) * bdata::make(bools) * bdata::make(bools),
                       tradeType, tradeCase, includeSettlementDateFlows, enforcesTodaysHistoricFixings) {

    // Set the flag determining what happens if fixings are required today
    Settings::instance().enforcesTodaysHistoricFixings() = enforcesTodaysHistoricFixings;

    // Set the flag determining what happens when cashflows happen today
    Settings::instance().includeTodaysCashFlows() = includeSettlementDateFlows;

    // Read in the trade
    Portfolio p;
    string portfolioFile = "trades/" + tradeType + "/" + tradeCase + ".xml";
    p.fromFile(TEST_INPUT_FILE(portfolioFile));
    BOOST_REQUIRE_MESSAGE(p.size() == 1, "Expected portfolio to contain a single trade");

    // Ask for fixings before trades are built should return empty set
    auto m = p.fixings(today);
    BOOST_CHECK_MESSAGE(m.empty(), "Expected fixings to be empty when trades not built");

    // Build the portfolio and retrieve the fixings
    p.build(engineFactory);
    m = p.fixings(today);

    // Check the retrieved fixings against the expected results
    auto exp = tradeTypeExpected();
    auto key = make_tuple(tradeType, tradeCase, includeSettlementDateFlows, enforcesTodaysHistoricFixings);
    if (exp.count(key) == 0) {
        // Expected result is no required fixings
        BOOST_CHECK_MESSAGE(m.empty(), "Expected no required fixings for ["
                                           << tradeType << ", " << tradeCase << ", "
                                           << ore::data::to_string(includeSettlementDateFlows) << ", "
                                           << ore::data::to_string(enforcesTodaysHistoricFixings)
                                           << "] but got a map containing " << m.size() << " indices");

        // Trade should not throw if we ask for NPV
        BOOST_CHECK_NO_THROW(p.trades().begin()->second->instrument()->NPV());

    } else {
        // Check the retrieved fixings against the expected fixings
        auto expMap = exp.at(key);
        BOOST_CHECK_EQUAL(expMap.size(), m.size());
        for (const auto& [indexName, expectedDates] : expMap) {
            BOOST_CHECK_MESSAGE(m.count(indexName), "Could not find index " <<indexName << " in retrieved fixings");
            std::set<QuantLib::Date> actualDates;
            for (const auto& [d, _] : m.at(indexName)) {
                actualDates.insert(d);
            }
            BOOST_CHECK_EQUAL_COLLECTIONS(expectedDates.begin(), expectedDates.end(), actualDates.begin(),
                                          actualDates.end());
        }

        // Trade should throw if we ask for NPV and have not added the fixings
        // If it is the zciis trade, it won't throw because the fixings were added for the bootstrap
        if (tradeType != "zciis_with_interp" && tradeType != "cpi_swap_with_interp") {
            BOOST_CHECK_THROW(p.trades().begin()->second->instrument()->NPV(), Error);
        }

        // Add the fixings
        loadFixings(m);

        // Trade should now not throw when we try to price it
        BOOST_CHECK_NO_THROW(p.trades().begin()->second->instrument()->NPV());
    }
}

BOOST_AUTO_TEST_CASE(testModifyInflationFixings) {

    // Original fixings
    map<string, RequiredFixings::FixingDates> fixings = {
        {"EUHICP", RequiredFixings::FixingDates({Date(1, Jan, 2019), Date(1, Dec, 2018), Date(1, Nov, 2018)}, true)},
        {"USCPI", RequiredFixings::FixingDates({Date(1, Dec, 2018), Date(1, Nov, 2018), Date(22, Oct, 2018),
                                                Date(1, Feb, 2018), Date(1, Feb, 2016)},
                                               true)},
        {"EUR-EURIBOR-3M", RequiredFixings::FixingDates({Date(18, Dec, 2018), Date(13, Feb, 2019)}, true)}};

    // Expected fixings after inflation modification
    map<string, RequiredFixings::FixingDates> expectedFixings = {
        {"EUHICP", RequiredFixings::FixingDates({Date(31, Jan, 2019), Date(31, Dec, 2018), Date(30, Nov, 2018)}, true)},
        {"USCPI", RequiredFixings::FixingDates({Date(31, Dec, 2018), Date(30, Nov, 2018), Date(22, Oct, 2018),
                                                Date(28, Feb, 2018), Date(29, Feb, 2016)},
                                               true)},
        {"EUR-EURIBOR-3M", RequiredFixings::FixingDates({Date(18, Dec, 2018), Date(13, Feb, 2019)}, true)}};

    // Amend the inflation portion of the fixings
    amendInflationFixingDates(fixings);

    // Compare contents of the output files
    BOOST_CHECK_EQUAL(expectedFixings.size(), fixings.size());
    for (const auto& [indexname, expectedFixingDates] : expectedFixings) {
        BOOST_CHECK_MESSAGE(fixings.count(indexname), "Could not find index " << indexname << " in retrieved fixings");
        std::set<QuantLib::Date> expectedDates;
        for (const auto& [d, _] : expectedFixingDates) {
            expectedDates.insert(d);
        }

        std::set<QuantLib::Date> actualDates;
        for (const auto& [d, _] : fixings[indexname]) {
            actualDates.insert(d);
        }
        BOOST_CHECK_EQUAL_COLLECTIONS(expectedDates.begin(), expectedDates.end(), actualDates.begin(),
                                      actualDates.end());
    }
}

BOOST_AUTO_TEST_CASE(testAddMarketFixings) {

    // Set the evaluation date
    Date asof(21, Feb, 2019);
    Settings::instance().evaluationDate() = asof;

    // Set up a simple TodaysMarketParameters
    TodaysMarketParameters mktParams;
    mktParams.addConfiguration(Market::defaultConfiguration, MarketConfiguration());

    // Add discount curves, we expect market fixings for EUR-EONIA
    map<string, string> m = {{"EUR", "Yield/EUR/EUR-EONIA"}, {"USD", "Yield/USD/USD-IN-EUR"}};
    mktParams.addMarketObject(MarketObject::DiscountCurve, Market::defaultConfiguration, m);

    // Add ibor index curves
    m = {{"EUR-EURIBOR-3M", "Yield/EUR/EUR-EURIBOR-3M"},
         {"USD-FedFunds", "Yield/USD/USD-FedFunds"},
         {"USD-LIBOR-3M", "Yield/USD/USD-LIBOR-3M"}};
    mktParams.addMarketObject(MarketObject::IndexCurve, Market::defaultConfiguration, m);

    // Add zero inflation curves
    m = {{"EUHICPXT", "Inflation/EUHICPXT/EUHICPXT_ZC_Swaps"}, {"USCPI", "Inflation/USCPI/USCPI_ZC_Swaps"}};
    mktParams.addMarketObject(MarketObject::ZeroInflationCurve, Market::defaultConfiguration, m);

    // Add yoy inflation curves
    m = {{"EUHICPXT", "Inflation/EUHICPXT/EUHICPXT_YOY_Swaps"}, {"UKRPI", "Inflation/UKRPI/UKRPI_YOY_Swaps"}};
    mktParams.addMarketObject(MarketObject::YoYInflationCurve, Market::defaultConfiguration, m);

    // Expected additional market fixings
    set<Date> inflationDates = {Date(1, Feb, 2019), Date(1, Jan, 2019), Date(1, Dec, 2018), Date(1, Nov, 2018),
                                Date(1, Oct, 2018), Date(1, Sep, 2018), Date(1, Aug, 2018), Date(1, Jul, 2018),
                                Date(1, Jun, 2018), Date(1, May, 2018), Date(1, Apr, 2018), Date(1, Mar, 2018),
                                Date(1, Feb, 2018)};
    set<Date> iborDates = {Date(21, Feb, 2019), Date(20, Feb, 2019), Date(19, Feb, 2019),
                           Date(18, Feb, 2019), Date(15, Feb, 2019), Date(14, Feb, 2019)};

    // Default for OIS dates is a lookback of 4 months on weekend only calendar => 21 Feb 2019 -> 21 Oct 2018.
    // 21 Oct 2018 is a Sunday => 22 Oct 2018 is the start of the lookback.
    set<Date> oisDates;
    Date oisDate(22, Oct, 2018);
    WeekendsOnly cal;
    while (oisDate <= asof) {
        oisDates.insert(oisDate);
        oisDate = cal.advance(oisDate, 1 * Days);
    }

    map<string, RequiredFixings::FixingDates> expectedFixings = {
        {"EUHICPXT", RequiredFixings::FixingDates(inflationDates, false)},
        {"USCPI", RequiredFixings::FixingDates(inflationDates, false)},
        {"UKRPI", RequiredFixings::FixingDates(inflationDates, false)},
        {"EUR-EURIBOR-3M", RequiredFixings::FixingDates(iborDates, false)},
        {"USD-FedFunds", RequiredFixings::FixingDates(oisDates, false)},
        {"USD-LIBOR-3M", RequiredFixings::FixingDates(iborDates, false)},
        {"EUR-EONIA", RequiredFixings::FixingDates(oisDates, false)}};

    // Populate empty fixings map using the function to be tested
    map<string, RequiredFixings::FixingDates> fixings;
    addMarketFixingDates(asof, fixings, mktParams);

    // Check the results
    BOOST_CHECK_EQUAL(expectedFixings.size(), fixings.size());
    for (const auto& [indexName, expectedFixingDates] : expectedFixings) {
        BOOST_CHECK_MESSAGE(fixings.count(indexName), "Could not find index " << indexName << " in retrieved fixings");
        std::set<QuantLib::Date> expectedDates;
        for (const auto& [d, _] : expectedFixingDates) {
            expectedDates.insert(d);
        }

        std::set<QuantLib::Date> actualDates;
        for (const auto& [d, _] : fixings[indexName]) {
            actualDates.insert(d);
        }
        BOOST_CHECK_EQUAL_COLLECTIONS(expectedDates.begin(), expectedDates.end(), actualDates.begin(),
                                      actualDates.end());
    }
}

BOOST_FIXTURE_TEST_CASE(testFxNotionalResettingSwapFirstCoupon, F) {

    // Set the flag determining what happens if fixings are required today
    Settings::instance().enforcesTodaysHistoricFixings() = true;

    // Set the flag determining what happens when cashflows happen today
    Settings::instance().includeTodaysCashFlows() = true;

    // Read in the trade
    Portfolio p;
    string portfolioFile = "trades/xccy_resetting_swap/simple_case_in_first_coupon.xml";
    p.fromFile(TEST_INPUT_FILE(portfolioFile));
    BOOST_REQUIRE_MESSAGE(p.size() == 1, "Expected portfolio to contain a single trade");

    // Ask for fixings before trades are built should return empty set
    auto m = p.fixings(today);
    BOOST_CHECK_MESSAGE(m.empty(), "Expected fixings to be empty when trades not built");

    // Build the portfolio and retrieve the fixings
    p.build(engineFactory);
    m = p.fixings(today);

    // Expected results
    map<string, Date> exp = {{"USD-LIBOR-3M", Date(5, Feb, 2019)}, {"EUR-EURIBOR-3M", Date(5, Feb, 2019)}};

    // Check the expected results against the actual results
    BOOST_CHECK_EQUAL(m.size(), exp.size());
    for (const auto& kv : exp) {
        BOOST_CHECK_MESSAGE(m.count(kv.first) == 1, "Could not find index " << kv.first << " in retrieved fixings");
        if (m.count(kv.first) == 1) {
            BOOST_CHECK_EQUAL(m.at(kv.first).size(), 1);
            BOOST_CHECK_EQUAL(kv.second, m.at(kv.first).begin()->first);
        }
    }

    // Trade should throw if we ask for NPV and have not added the fixings
    BOOST_CHECK_THROW(p.trades().begin()->second->instrument()->NPV(), Error);

    // Add the fixings
    loadFixings(m);

    // Trade should now not throw when we try to price it
    BOOST_CHECK_NO_THROW(p.trades().begin()->second->instrument()->NPV());
}

BOOST_FIXTURE_TEST_CASE(testDividends, F) {

    const string equityName = "RIC:DMIWO00000GUS";

    auto eq = parseEquityIndex("EQ-" + equityName);
    BOOST_REQUIRE_MESSAGE(eq, "Could not parse equity index EQ-" + equityName);

    BOOST_REQUIRE_MESSAGE(DividendManager::instance().hasHistory(eq->name()),
                          "Could not find index " << eq->name() << " in DividendManager");
    map<Date, QuantExt::Dividend> divMap;
    const set<QuantExt::Dividend>& dividends = eq->dividendFixings();
    for (const auto& d : dividends)
        divMap[d.exDate] = d;

    // Expected results
    map<Date, Real> exp = {{Date(1, Nov, 2018), 25.313}, {Date(1, Dec, 2018), 15.957}};

    BOOST_CHECK_EQUAL(dividends.size(), exp.size());
    for (const auto& kv : exp) {
        BOOST_CHECK_EQUAL(divMap[kv.first].rate, kv.second);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
