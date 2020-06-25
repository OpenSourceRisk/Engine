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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/parsers.hpp>
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

class MarketDataLoader : public Loader {
public:
    MarketDataLoader();
    const vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const Date&) const;
    const boost::shared_ptr<MarketDatum>& get(const string& name, const Date&) const;
    const vector<Fixing>& loadFixings() const { return fixings_; }
    const vector<Fixing>& loadDividends() const { return dividends_; }
    void add(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addDividend(QuantLib::Date date, const string& name, QuantLib::Real value) {}

private:
    map<Date, vector<boost::shared_ptr<MarketDatum>>> data_;
    vector<Fixing> fixings_;
    vector<Fixing> dividends_;
};

const vector<boost::shared_ptr<MarketDatum>>& MarketDataLoader::loadQuotes(const Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "Loader has no data for date " << d);
    return it->second;
}

const boost::shared_ptr<MarketDatum>& MarketDataLoader::get(const string& name, const Date& d) const {
    for (auto& md : loadQuotes(d)) {
        if (md->name() == name)
            return md;
    }
    QL_FAIL("No MarketDatum for name " << name << " and date " << d);
}

MarketDataLoader::MarketDataLoader() {

    vector<string> data{"20150831 IR_SWAP/RATE/JPY/2D/6M/2Y 0.0022875"};

    for (auto s : data) {
        vector<string> tokens;
        boost::trim(s);
        boost::split(tokens, s, boost::is_any_of(" "), boost::token_compress_on);

        QL_REQUIRE(tokens.size() == 3, "Invalid market data line, 3 tokens expected " << s);
        Date date = parseDate(tokens[0]);
        const string& key = tokens[1];
        Real value = parseReal(tokens[2]);
        data_[date].push_back(parseMarketDatum(date, key, value));
    }
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(YieldCurveTests)

BOOST_AUTO_TEST_CASE(testBootstrapAndFixings) {

    Date asof(31, August, 2015);
    Settings::instance().evaluationDate() = asof;

    YieldCurveSpec spec("JPY", "JPY6M");

    CurveConfigurations curveConfigs;
    vector<boost::shared_ptr<YieldCurveSegment>> segments{boost::make_shared<SimpleYieldCurveSegment>(
        "Swap", "JPY-SWAP-CONVENTIONS", vector<string>(1, "IR_SWAP/RATE/JPY/2D/6M/2Y"))};
    boost::shared_ptr<YieldCurveConfig> jpyYieldConfig =
        boost::make_shared<YieldCurveConfig>("JPY6M", "JPY 6M curve", "JPY", "", segments);
    curveConfigs.yieldCurveConfig("JPY6M") = jpyYieldConfig;

    Conventions conventions;
    boost::shared_ptr<Convention> convention =
        boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M");
    conventions.add(convention);

    MarketDataLoader loader;

    // Construction should fail due to missing fixing on August 28th, 2015
    auto checker = [](const Error& ex) -> bool {
        return string(ex.what()).find("Missing JPYLibor6M Actual/360 fixing for August 28th, 2015") != string::npos;
    };
    BOOST_CHECK_EXCEPTION(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions), Error, checker);

    // Change calendar in conventions to Japan & London and the curve building should not throw an exception
    conventions.clear();
    convention = boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP,UK", "Semiannual", "MF", "A365",
                                                      "JPY-LIBOR-6M");
    conventions.add(convention);
    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions));

    // Reapply old convention but load necessary fixing and the curve building should still not throw an exception
    conventions.clear();
    convention =
        boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M");
    conventions.add(convention);

    vector<Real> fixings{0.0013086};
    TimeSeries<Real> fixingHistory(Date(28, August, 2015), fixings.begin(), fixings.end());
    IndexManager::instance().setHistory("JPYLibor6M Actual/360", fixingHistory);

    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions));
}

namespace {

// List of curve configuration files that set up an ARS-IN-USD curve with various interpolation methods and variables.
// We have a set of files under ars_in_usd/failing and a set under ars_in_usd/passing:
// - failing: has the old QuantLib::IterativeBootstrap parameters i.e. 1 attempt with hard bounds
// - passing: has the default QuantExt::IterativeBootstrap parameters i.e. 5 attempts with widening bounds
vector<string> curveConfigFiles = {"discount_linear.xml",
                                   "discount_loglinear.xml",
                                   "discount_natural_cubic.xml",
                                   "discount_financial_cubic.xml",
                                   "zero_linear.xml",
                                   "zero_natural_cubic.xml",
                                   "zero_financial_cubic.xml",
                                   "forward_linear.xml",
                                   "forward_natural_cubic.xml",
                                   "forward_financial_cubic.xml",
                                   "forward_convex_monotone.xml"};

// Construct and hold the arguments needed to construct a TodaysMarket.
struct TodaysMarketArguments {
    TodaysMarketArguments(const string& inputDir, const string& curveConfigFile) {

        asof = Date(25, Sep, 2019);
        Settings::instance().evaluationDate() = asof;

        string filename = inputDir + "/conventions.xml";
        conventions.fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/" + curveConfigFile;
        curveConfigs.fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/todaysmarket.xml";
        todaysMarketParameters.fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/market.txt";
        string fixingsFilename = inputDir + "/fixings.txt";
        loader = CSVLoader(TEST_INPUT_FILE(filename), TEST_INPUT_FILE(fixingsFilename), false);
    }

    Date asof;
    Conventions conventions;
    CurveConfigurations curveConfigs;
    TodaysMarketParameters todaysMarketParameters;
    CSVLoader loader;
};

// Used to check that the exception message contains the expected message string, expMsg.
struct ExpErrorPred {

    ExpErrorPred(const string& msg) : expMsg(msg) {}

    bool operator()(const Error& ex) {
        string errMsg(ex.what());
        return errMsg.find(expMsg) != string::npos;
    }

    string expMsg;
};

} // namespace

// Test ARS-IN-USD failures using the old QuantLib::IterativeBootstrap parameters
BOOST_DATA_TEST_CASE(testBootstrapARSinUSDFailures, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing ARS-IN-USD fails with configuration file: failing/" << curveConfigFile);

    TodaysMarketArguments tma("ars_in_usd", "failing/" + curveConfigFile);

    boost::shared_ptr<TodaysMarket> todaysMarket;
    BOOST_CHECK_EXCEPTION(todaysMarket =
                              boost::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                               tma.curveConfigs, tma.conventions, false, false),
                          Error, ExpErrorPred("yield curve building failed for curve ARS-IN-USD"));
}

// Test ARS-IN-USD passes using the new QuantExt::IterativeBootstrap parameters
BOOST_DATA_TEST_CASE(testBootstrapARSinUSDPasses, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing ARS-IN-USD passes with configuration file: passing/" << curveConfigFile);

    TodaysMarketArguments tma("ars_in_usd", "passing/" + curveConfigFile);

    boost::shared_ptr<TodaysMarket> todaysMarket;
    BOOST_REQUIRE_NO_THROW(todaysMarket =
                               boost::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                                tma.curveConfigs, tma.conventions, false, false));

    Handle<YieldTermStructure> yts = todaysMarket->discountCurve("ARS");
    BOOST_TEST_MESSAGE("Discount: " << std::fixed << std::setprecision(14) << yts->discount(1.0));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
