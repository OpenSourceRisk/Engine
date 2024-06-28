/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/utilities/to_string.hpp>
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

struct ZeroDatum {
    string date;
    double zero;
};

struct ExpectedResult {
    string date;
    double rate;
    double discount;
    double zero;
};

class MarketDataLoader : public Loader {
public:
    MarketDataLoader();
    MarketDataLoader(vector<string> data);
    vector<QuantLib::ext::shared_ptr<MarketDatum>> loadQuotes(const Date&) const override;
    set<Fixing> loadFixings() const override { return fixings_; }
    set<QuantExt::Dividend> loadDividends() const override { return dividends_; }
    void add(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addDividend(const QuantExt::Dividend& div) {}

private:
    map<Date, vector<QuantLib::ext::shared_ptr<MarketDatum>>> data_;
    set<Fixing> fixings_;
    set<QuantExt::Dividend> dividends_;
};

vector<QuantLib::ext::shared_ptr<MarketDatum>> MarketDataLoader::loadQuotes(const Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "Loader has no data for date " << d);
    return it->second;
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

MarketDataLoader::MarketDataLoader(vector<string> data) {

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

    TodaysMarketArguments(const Date& asof, const string& inputDir, const string& curveConfigFile = "curveconfig.xml")
        : asof(asof) {

        Settings::instance().evaluationDate() = asof;

        string filename = inputDir + "/conventions.xml";
        conventions->fromFile(TEST_INPUT_FILE(filename));
        InstrumentConventions::instance().setConventions(conventions);
        
        filename = inputDir + "/" + curveConfigFile;
        curveConfigs->fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/todaysmarket.xml";
        todaysMarketParameters->fromFile(TEST_INPUT_FILE(filename));

        filename = inputDir + "/market.txt";
        string fixingsFilename = inputDir + "/fixings.txt";
        loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE(filename), TEST_INPUT_FILE(fixingsFilename), false);
    }

    Date asof;
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    QuantLib::ext::shared_ptr<CurveConfigurations> curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    QuantLib::ext::shared_ptr<Loader> loader;
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

// Test yield curve bootstrap from overnight index futures where the first future in the list of instruments may be
// expired. We use the March 2020 SOFR future contract whose last trade date is 16 Jun 2020 with settlement date 17
// Jun 2020. A number of cases are tested:
// 1. Valuation date is 9 Jun 2020. March 2020 SOFR future should be included in bootstrap fine.
// 2. Valuation date is 16 Jun 2020. March 2020 SOFR future should be included in bootstrap. The final SOFR fixing
//    i.e. the fixing for 16 Jun 2020 will not be known on 16 Jun 2020.
// 3. Valuation date is 17 Jun 2020. March 2020 SOFR future should be excluded from the bootstrap.
// 4. Valuation date is 23 Jun 2020. March 2020 SOFR future should be excluded from the bootstrap.

struct FutureCase {
    Date date;
    string desc;
};

vector<FutureCase> oiFutureCases{{Date(9, Jun, 2020), "before_ltd"},
                                 {Date(16, Jun, 2020), "on_ltd"},
                                 {Date(17, Jun, 2020), "on_settlement"},
                                 {Date(23, Jun, 2020), "after_ltd"}};

// Test yield curve bootstrap from money market futures where the first future in the list of instruments has an ibor
// start date that is before, on and after the valuation date. We use the August 2020 Eurodollar future contract whose
// last trade date is 17 Aug 2020 with an underlying ibor start date of 19 Aug 2020. Note that the USD-LIBOR-3M fixing
// is known on 17 Aug 2020 and the future expires on this date with the associated final settlement price. A number of
// cases are tested:
// 1. Valuation date is 18 Aug 2020. August 2020 Eurodollar future should be included in bootstrap.
// 2. Valuation date is 19 Aug 2020. August 2020 Eurodollar future should be included in bootstrap.
// 3. Valuation date is 20 Aug 2020. August 2020 Eurodollar future should be excluded from the bootstrap.
vector<FutureCase> mmFutureCases{{Date(18, Aug, 2020), "before_ibor_start"},
                                 {Date(19, Aug, 2020), "on_ibor_start"},
                                 {Date(20, Aug, 2020), "after_ibor_start"}};

ostream& operator<<(ostream& os, const FutureCase& c) {
    return os << "Date is " << io::iso_date(c.date) << " and case is " << c.desc << ".";
}
    
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(YieldCurveTests)

BOOST_AUTO_TEST_CASE(testBootstrapAndFixings) {

    Date asof(31, August, 2015);
    Settings::instance().evaluationDate() = asof;

    YieldCurveSpec spec("JPY", "JPY6M");

    CurveConfigurations curveConfigs;
    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments{QuantLib::ext::make_shared<SimpleYieldCurveSegment>(
        "Swap", "JPY-SWAP-CONVENTIONS", vector<string>(1, "IR_SWAP/RATE/JPY/2D/6M/2Y"))};
    QuantLib::ext::shared_ptr<YieldCurveConfig> jpyYieldConfig =
        QuantLib::ext::make_shared<YieldCurveConfig>("JPY6M", "JPY 6M curve", "JPY", "", segments);
    curveConfigs.add(CurveSpec::CurveType::Yield, "JPY6M", jpyYieldConfig);

    MarketDataLoader loader;

    // QL >= 1.19 should not throw, no matter if the float convention has the correct calendar

    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    InstrumentConventions::instance().setConventions(conventions);

    QuantLib::ext::shared_ptr<Convention> convention =
        QuantLib::ext::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M");
    conventions->add(convention);

    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader));

    conventions->clear();
    convention = QuantLib::ext::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP,UK", "Semiannual", "MF", "A365",
                                                      "JPY-LIBOR-6M");
    conventions->add(convention);
    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader));
}

BOOST_AUTO_TEST_CASE(testBuildDiscountCurveDirectSegment) {

    Date asof(13, October, 2023);
    Settings::instance().evaluationDate() = asof;

    YieldCurveSpec spec("EUR", "EUR-CURVE");

    CurveConfigurations curveConfigs;

    vector<string> quotes;
    quotes.emplace_back("DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-14");
    quotes.emplace_back("DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-15");

    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments{QuantLib::ext::make_shared<DirectYieldCurveSegment>(
        "Discount", "", quotes)};

    QuantLib::ext::shared_ptr<YieldCurveConfig> yCConfig =
        QuantLib::ext::make_shared<YieldCurveConfig>("EUR-CURVE", "ORE YieldCurve built from EUR-CURVE", "EUR", "", segments);
    curveConfigs.add(CurveSpec::CurveType::Yield, "EUR-CURVE", yCConfig);

    vector<string> data{"2023-10-12 DISCOUNT/RATE/SEK/STINA-CURVE/2023-10-13 0.77",
                        "2023-10-12 DISCOUNT/RATE/EUR/EUR-ANOTHER-CURVE/2023-10-13 0.95",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-ANOTHER-CURVE/2023-10-14 0.95",
                        "2023-10-12 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-12 0.88",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-13 1.0",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-14 0.99",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-15 0.98",
                        "2023-10-13 COMMODITY_FWD/PRICE/GOLD/USD/2023-10-31 1158.8",
                        "2023-10-13 COMMODITY_FWD/PRICE/GOLD/USD/2023-11-01 1160.9",
                        "2023-10-13 COMMODITY_FWD/PRICE/GOLD/USD/2023-11-02 1163.4"};
    MarketDataLoader loader(data);

    BOOST_CHECK_NO_THROW(YieldCurve yieldCurve(asof, spec, curveConfigs, loader));
}

BOOST_AUTO_TEST_CASE(testBuildDiscountCurveDirectSegmentWildcard) {

    Date asof(13, October, 2023);
    Settings::instance().evaluationDate() = asof;

    YieldCurveSpec spec("EUR", "EUR-CURVE");

    CurveConfigurations curveConfigs;

    vector<string> quotes;
    quotes.emplace_back("DISCOUNT/RATE/EUR/EUR-CURVE/*");

    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments{
        QuantLib::ext::make_shared<DirectYieldCurveSegment>("Discount", "", quotes)};

    QuantLib::ext::shared_ptr<YieldCurveConfig> yCConfig = QuantLib::ext::make_shared<YieldCurveConfig>(
        "EUR-CURVE", "ORE YieldCurve built from EUR-CURVE", "EUR", "", segments);
    curveConfigs.add(CurveSpec::CurveType::Yield, "EUR-CURVE", yCConfig);

    vector<string> data{"2023-10-12 DISCOUNT/RATE/SEK/STINA-CURVE/2023-10-13 0.77",
                        "2023-10-12 DISCOUNT/RATE/EUR/EUR-ANOTHER-CURVE/2023-10-13 0.95",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-ANOTHER-CURVE/2023-10-14 0.95",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-13 1.0",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-14 0.99",
                        "2023-10-13 DISCOUNT/RATE/EUR/EUR-CURVE/2023-10-15 0.98",
                        "2023-10-13 EQUITY_FWD/PRICE/SP5/USD/1Y 1500.00",
                        "2023-10-13 EQUITY_FWD/PRICE/SP5/USD/20231014 1500.00",
                        "2023-10-13 EQUITY_DIVIDEND/RATE/SP5/USD/20231015 0.00",
                        "2023-10-13 EQUITY_DIVIDEND/RATE/SP5/USD/2Y 0.00"};
    MarketDataLoader loader(data);

    BOOST_CHECK_NO_THROW(YieldCurve yieldCurve(asof, spec, curveConfigs, loader));
}

// Test ARS-IN-USD failures using the old QuantLib::IterativeBootstrap parameters
BOOST_DATA_TEST_CASE(testBootstrapARSinUSDFailures, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing ARS-IN-USD fails with configuration file: failing/" << curveConfigFile);

    TodaysMarketArguments tma(Date(25, Sep, 2019), "ars_in_usd", "failing/" + curveConfigFile);

    QuantLib::ext::shared_ptr<TodaysMarket> todaysMarket;
    BOOST_CHECK_EXCEPTION(todaysMarket =
                              QuantLib::ext::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                               tma.curveConfigs, false, false),
                          Error, ExpErrorPred("yield curve building failed for curve ARS-IN-USD"));
}

// Test ARS-IN-USD passes using the new QuantExt::IterativeBootstrap parameters
BOOST_DATA_TEST_CASE(testBootstrapARSinUSDPasses, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing ARS-IN-USD passes with configuration file: passing/" << curveConfigFile);

    TodaysMarketArguments tma(Date(25, Sep, 2019), "ars_in_usd", "passing/" + curveConfigFile);

    QuantLib::ext::shared_ptr<TodaysMarket> todaysMarket;
    BOOST_REQUIRE_NO_THROW(todaysMarket =
                               QuantLib::ext::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                                tma.curveConfigs, false, false));

    Handle<YieldTermStructure> yts = todaysMarket->discountCurve("ARS");
    BOOST_TEST_MESSAGE("Discount: " << std::fixed << std::setprecision(14) << yts->discount(1.0));
}

BOOST_DATA_TEST_CASE(testOiFirstFutureDateVsValuationDate, bdata::make(oiFutureCases), oiFutureCase) {

    BOOST_TEST_MESSAGE("Testing OI future. " << oiFutureCase);

    BOOST_TEST_CONTEXT(oiFutureCase) {

        TodaysMarketArguments tma(oiFutureCase.date, "oi_future/" + oiFutureCase.desc);

        QuantLib::ext::shared_ptr<TodaysMarket> todaysMarket;
        BOOST_REQUIRE_NO_THROW(todaysMarket =
                                   QuantLib::ext::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                                    tma.curveConfigs, false, true));

        Handle<YieldTermStructure> yts;
        BOOST_REQUIRE_NO_THROW(yts = todaysMarket->discountCurve("USD"));
        BOOST_REQUIRE_NO_THROW(yts->discount(1.0));
    }
}

BOOST_DATA_TEST_CASE(testMmFirstFutureDateVsValuationDate, bdata::make(mmFutureCases), mmFutureCase) {

    BOOST_TEST_MESSAGE("Testing money market future. " << mmFutureCase);

    BOOST_TEST_CONTEXT(mmFutureCase) {

        TodaysMarketArguments tma(mmFutureCase.date, "mm_future/" + mmFutureCase.desc);

        QuantLib::ext::shared_ptr<TodaysMarket> todaysMarket;
        BOOST_REQUIRE_NO_THROW(todaysMarket =
                                   QuantLib::ext::make_shared<TodaysMarket>(tma.asof, tma.todaysMarketParameters, tma.loader,
                                                                    tma.curveConfigs, false, true));

        Handle<YieldTermStructure> yts;
        BOOST_REQUIRE_NO_THROW(yts = todaysMarket->discountCurve("USD"));
        BOOST_REQUIRE_NO_THROW(yts->discount(1.0));
    }
}

BOOST_AUTO_TEST_CASE(testQuadraticInterpolation) {

    Date asof(24, Mar, 2020);
    Settings::instance().evaluationDate() = asof;

    std::vector<ZeroDatum> zero_data = {
        { "2020-03-25", -0.00710652430814573 },
        { "2020-04-27", -0.00741014330032008 },
        { "2020-05-26", -0.00756626445863218 },
        { "2020-06-26", -0.00757302703270679 },
        { "2020-09-28", -0.00741005956787566 },
        { "2020-12-29", -0.00741819259807242 },
        { "2021-03-26", -0.00745035004912764 },
        { "2022-03-28", -0.00724972360299359 },
        { "2023-03-27", -0.00694809582571432 },
        { "2024-03-26", -0.00639564747668298 },
        { "2025-03-26", -0.0056924815618794 },
        { "2026-03-26", -0.00491308147033043 },
        { "2027-03-30", -0.00428289071011978 },
        { "2028-03-27", -0.00365173027918575 },
        { "2029-03-26", -0.00312018815108916 },
        { "2030-03-26", -0.00266352161484584 },
        { "2032-03-30", -0.00179856872850126 },
        { "2035-03-27", -0.000800546649163958 },
        { "2040-03-26", -0.000821931627955741 },
        { "2045-03-27", -0.00149953900205779 },
        { "2050-03-28", -0.00228805321739911 },
    };
    
    YieldCurveSpec spec("CHF", "CHF-OIS");
    
    vector<string> quotes(zero_data.size());
    for (Size i=0; i < zero_data.size(); ++i) {
        quotes[i] = "ZERO/RATE/CHF/CHF-OIS/A365/" + zero_data[i].date;
    }
    
    CurveConfigurations curveConfigs;
    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments{
        QuantLib::ext::make_shared<DirectYieldCurveSegment>(
            "Zero", "CHF-ZERO-CONVENTIONS", quotes)
    };
    QuantLib::ext::shared_ptr<YieldCurveConfig> chfYieldConfig =
        QuantLib::ext::make_shared<YieldCurveConfig>("CHF-OIS", "CHF OIS curve", "CHF",
                                             "", segments,
                                             "Discount", "LogQuadratic");
    curveConfigs.add(CurveSpec::CurveType::Yield, "CHF-OIS", chfYieldConfig);
    
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();;
    InstrumentConventions::instance().setConventions(conventions);
    
    QuantLib::ext::shared_ptr<Convention> convention =
        QuantLib::ext::make_shared<ZeroRateConvention>("CHF-ZERO-CONVENTIONS", "A365",
                                               "CHF", "Compounded", "Annual");
    conventions->add(convention);
    
    vector<string> data(zero_data.size());
    for (Size i=0; i < zero_data.size(); ++i) {
        data[i] = to_string(asof) + " " + quotes[i] + " ";
        data[i] += boost::lexical_cast<std::string>(zero_data[i].zero);
    }
    
    MarketDataLoader loader(data);
    YieldCurve chfYieldCurve(asof, spec, curveConfigs, loader);
    
    BOOST_TEST_MESSAGE("Test zeroRate from YieldCurve against input");
    for (Size i=0; i < zero_data.size(); ++i) {
        BOOST_CHECK_CLOSE(
            chfYieldCurve.handle()->zeroRate(parseDate(zero_data[i].date),
                                             Actual365Fixed(), Compounded,
                                             Annual).rate(),
            zero_data[i].zero, 1e-6
            );
    }
    
    // From Front Arena Prime
    std::vector<ExpectedResult> expected = {
        { "2020-03-25", -0.00705200739223866, 1.00001953963179, -0.00710652430814573 },
        { "2020-04-02", -0.00721390912158171, 1.0001778751147, -0.00718723002828103 },
        { "2020-04-10", -0.00738227311346984, 1.00033965887219, -0.00726491951444497 },
        { "2020-04-18", -0.00749059894111781, 1.0005044904488, -0.00733665761295088 },
        { "2020-04-27", -0.00760320907581491, 1.00069307015329, -0.00741014330031875 },
        { "2020-04-09", -0.00737384545779651, 1.00031926101022, -0.00725553005947521 },
        { "2020-04-25", -0.00758478528252393, 1.0006509032529, -0.00739447534752169 },
        { "2020-05-10", -0.00769596247521598, 1.00096977382034, -0.00749931157112393 },
        { "2020-05-26", -0.0076429042339754, 1.00131178328636, -0.0075662644586304 },
        { "2020-04-17", -0.00748320484351694, 1.00048373351725, -0.00732801995321264 },
        { "2020-05-10", -0.00769596247521598, 1.00096977382034, -0.00749931157112393 },
        { "2020-06-02", -0.00758166464153831, 1.00146009211018, -0.00757891880297334 },
        { "2020-06-26", -0.00736149127903651, 1.00195965381451, -0.00757302703270502 },
        { "2020-05-10", -0.00769596247521598, 1.00096977382034, -0.00749931157112393 },
        { "2020-06-26", -0.00736149127903651, 1.00195965381451, -0.00757302703270502 },
        { "2020-08-12", -0.00711900322939663, 1.002904625853, -0.00748005210315095 },
        { "2020-09-28", -0.00719031149285065, 1.00383824668103, -0.00741005956787366 },
        { "2020-06-02", -0.00758166464153831, 1.00146009211018, -0.00757891880297334 },
        { "2020-08-11", -0.00712099069506866, 1.00288478786826, -0.0074820952971888 },
        { "2020-10-20", -0.00728829236142925, 1.00428240351543, -0.00739981858935435 },
        { "2020-12-29", -0.00748148784771807, 1.00572822439311, -0.00741819259807019 },
        { "2020-06-24", -0.00738797434533645, 1.00191855412792, -0.00757552258803451 },
        { "2020-09-24", -0.00717170989259053, 1.00375818166182, -0.0074134982804841 },
        { "2020-12-24", -0.00747585963708053, 1.00562378840629, -0.00741575708517861 },
        { "2021-03-26", -0.00740455196858392, 1.00754755952054, -0.00745035004912542 },
        { "2020-09-24", -0.00717170989259053, 1.00375818166182, -0.0074134982804841 },
        { "2021-03-26", -0.00740455196858392, 1.00754755952054, -0.00745035004912542 },
        { "2021-09-25", -0.00694048485996968, 1.01122053890473, -0.0073775427951106 },
        { "2022-03-28", -0.0066863454350452, 1.01473957125551, -0.00724972360299103 },
        { "2020-12-24", -0.00747585963708053, 1.00562378840629, -0.00741575708517861 },
        { "2021-09-24", -0.0069414029672199, 1.01120103496917, -0.00737820251647103 },
        { "2022-06-25", -0.00656498953875317, 1.01640540717996, -0.00719077356299158 },
        { "2023-03-27", -0.00558871194802357, 1.02119585320621, -0.00694809582571021 },
        { "2021-03-25", -0.00741094632973116, 1.00752681818474, -0.00745025320516035 },
        { "2022-03-26", -0.00668750137850616, 1.01470187085395, -0.00725106794125541 },
        { "2023-03-26", -0.00559392063686381, 1.02117998518244, -0.00694927326429007 },
        { "2024-03-26", -0.00380047798675509, 1.02605103251001, -0.0063956474766812 },
        { "2021-06-24", -0.00715202046442265, 1.0093822458995, -0.00743079826559478 },
        { "2022-09-24", -0.00634232121085709, 1.01806767055031, -0.00712529508658977 },
        { "2023-12-25", -0.00422742334270421, 1.02499844543564, -0.00655192842032992 },
        { "2025-03-26", -0.00172999929889617, 1.02900328433957, -0.00569248156188507 },
        { "2021-09-24", -0.0069414029672199, 1.01120103496917, -0.00737820251647103 },
        { "2023-03-26", -0.00559392063686381, 1.02117998518244, -0.00694927326429007 },
        { "2024-09-24", -0.00286665434442224, 1.0277915286893, -0.00606391607397783 },
        { "2026-03-26", -0.000727524210795139, 1.03003380594845, -0.00491308147034686 },
        { "2021-12-25", -0.00678779318060929, 1.01297597811044, -0.00731440818226603 },
        { "2023-09-26", -0.00466377889755343, 1.02385925805364, -0.00669595300436709 },
        { "2025-06-27", -0.00119318667783475, 1.02938907952551, -0.00548847866962621 },
        { "2027-03-30", 0.000117832613426572, 1.0305853419039, -0.00428289071016552 },
        { "2022-03-26", -0.00668750137850616, 1.01470187085395, -0.00725106794125541 },
        { "2024-03-26", -0.00380047798675509, 1.02605103251001, -0.0063956474766812 },
        { "2026-03-27", -0.000729929427953913, 1.03003588754858, -0.00491118220370035 },
        { "2028-03-27", 0.00112613114121807, 1.02975141478671, -0.00365173027928756 },
        { "2022-06-25", -0.00656498953875317, 1.01640540717996, -0.00719077356299158 },
        { "2024-09-24", -0.00286665434442224, 1.0277915286893, -0.00606391607397783 },
        { "2026-12-25", -0.000343853350512902, 1.03054949871623, -0.00444232736548467 },
        { "2029-03-26", 0.00122363405024473, 1.02856007847337, -0.00312018815121784 },
        { "2022-09-24", -0.00634232121085709, 1.01806767055031, -0.00712529508658977 },
        { "2025-03-25", -0.00173674463422646, 1.02899832012865, -0.00569463011016269 },
        { "2027-09-24", 0.000888320704621748, 1.03030851958745, -0.00396957339826431 },
        { "2030-03-26", 0.0017355869817326, 1.02705961730745, -0.00266352161503269 },
        { "2023-03-27", -0.00558871194802357, 1.02119585320621, -0.00694809582571021 },
        { "2026-03-28", -0.000730525490939549, 1.0300379726323, -0.00490928522291956 },
        { "2029-03-29", 0.00122749406191502, 1.02854958382202, -0.00311622139684964 },
        { "2032-03-30", 0.00320650937346123, 1.02188263368199, -0.00179856872876094 },
        { "2023-12-25", -0.00422742334270421, 1.02499844543564, -0.00655192842032992 },
        { "2027-09-25", 0.000889730574376024, 1.03030598533069, -0.00396780181172551 },
        { "2031-06-26", 0.00265730529292796, 1.02419866540126, -0.00212067173766495 },
        { "2035-03-27", 0.00203153226992825, 1.01209877900331, -0.000800546649511458 },
        { "2025-03-25", -0.00173674463422646, 1.02899832012865, -0.00569463011016269 },
        { "2030-03-26", 0.0017355869817326, 1.02705961730745, -0.00266352161503269 },
        { "2035-03-26", 0.00203563762406489, 1.0121045019654, -0.000801068999449428 },
        { "2040-03-26", -0.00294180811111211, 1.0165973929348, -0.000821931627935868 },
        { "2026-06-25", -0.000760135138913043, 1.03023087322183, -0.00474822200738056 },
        { "2032-09-24", 0.00346788193350989, 1.02019108824174, -0.00159634484187743 },
        { "2038-12-25", -0.00217162595274267, 1.01327109910735, -0.000702248267447581 },
        { "2045-03-27", -0.00536047202242429, 1.03826766081024, -0.00149953900098565 },
        { "2027-09-25", 0.000889730574376024, 1.03030598533069, -0.00396780181172551 },
        { "2035-03-27", 0.00203153226992825, 1.01209877900331, -0.000800546649511458 },
        { "2042-09-26", -0.00416154603709362, 1.02580615300844, -0.0011305804441839 },
        { "2050-03-28", -0.00655222665784105, 1.07121045806809, -0.0022880532151871 }
    };
    
    BOOST_TEST_MESSAGE("Test rates from YieldCurve cached result");
    for (Size i=0; i < expected.size(); ++i) {
        BOOST_CHECK_CLOSE(
            chfYieldCurve.handle()->zeroRate(parseDate(expected[i].date),
                                             Actual365Fixed(), Compounded,
                                             Annual).rate(),
            expected[i].zero, 1e-7
            );
        BOOST_CHECK_CLOSE(
            chfYieldCurve.handle()->discount(parseDate(expected[i].date)),
            expected[i].discount, 1e-7
            );
    }
    BOOST_TEST_MESSAGE("Test rates from YieldCurve cached result");
    BOOST_CHECK_EQUAL(chfYieldCurve.handle()->discount(asof), 1);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
