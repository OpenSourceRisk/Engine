/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/indexparser.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/simpledaycounter.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;
using ore::test::TopLevelFixture;

namespace bdata = boost::unit_test::data;

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket(Real hazardRate, Real recoveryRate, Real liborRate) : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);
        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] =
            flatRateYts(liborRate);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(hazardRate);
        recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate));
        // build ibor index
        Handle<IborIndex> hEUR(ore::data::parseIborIndex(
            "EUR-EURIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        yts->enableExtrapolation();
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantExt::CreditCurve> flatRateDcs(Volatility forward) {
        QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(
            new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
        return Handle<QuantExt::CreditCurve>(
            QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs)));
    }
};

struct CommonVars {
    // global data
    string ccy;
    string creditCurveId;
    string issuerId;
    bool isPayer;
    string start;
    string issue;
    string fixtenor;
    Calendar cal;
    string calStr;
    string conv;
    string rule;
    Size days;
    string fixDC;
    string settledays;
    bool isinarrears;
    Real notional;
    vector<Real> notionals;
    vector<Real> spread;

    // utilities
    QuantLib::ext::shared_ptr<ore::data::CreditDefaultSwap> makeCDS(string end, Real rate) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        // build CDS
        QuantLib::ext::shared_ptr<FixedLegData> fixedLegRateData = QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, rate));
        LegData fixedLegData(fixedLegRateData, isPayer, ccy, fixedSchedule, fixDC, notionals);

        CreditDefaultSwapData cd(issuerId, creditCurveId, fixedLegData, false,
                                 QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault);
        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::CreditDefaultSwap> cds(new ore::data::CreditDefaultSwap(env, cd));
        return cds;
    }

    CommonVars()
        : ccy("EUR"), creditCurveId("CreditCurve_A"), issuerId("CPTY_A"), isPayer(false), start("20160203"),
          issue("20160203"), fixtenor("1Y") {
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/365";
        settledays = "2";
        isinarrears = false;
        notional = 1;
        notionals.push_back(1);
        spread.push_back(0.0);
    }
};

struct MarketInputs {
    Real hazardRate;
    Real recoveryRate;
    Real liborRate;
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the MarketInputs
ostream& operator<<(ostream& os, const MarketInputs& m) {
    return os << "[" << m.hazardRate << "," << m.recoveryRate << "," << m.liborRate << "]";
}

struct TradeInputs {
    string endDate;
    Real fixedRate;
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the TradeInputs
ostream& operator<<(ostream& os, const TradeInputs& t) { return os << "[" << t.endDate << "," << t.fixedRate << "]"; }

Real cdsNpv(const MarketInputs& m, const TradeInputs& t) {

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(m.hazardRate, m.recoveryRate, m.liborRate);
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::CreditDefaultSwap> cds = vars.makeCDS(t.endDate, t.fixedRate);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CreditDefaultSwap") = "DiscountedCashflows";
    engineData->engine("CreditDefaultSwap") = "MidPointCdsEngine";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    cds->build(engineFactory);

    return cds->instrument()->NPV();
}

// Cases below:
// 1) HazardRate = 0, couponRate = 0. ExpectedNpv = 0
// 2) RecoveryRate = 1, couponRate = 0. ExpectedNpv = 0
// 3) Example from Hull, Ch 21 (pp. 510 - 513). Take RR = 1 to show only couponNPV.
// 4) Example from Hull, Ch 21 (pp. 510 - 513). Take coupon rate = 0 to show only defaultNPV.

// Market inputs used in the test below
MarketInputs marketInputs[] = {{0, 1, 0}, {1, 1, 0}, {0.02, 1, 0.05}, {0.02, 0.4, 0.05}};

// Trade inputs used in the test below
TradeInputs tradeInputs[] = {{"20170203", 0}, {"20170203", 0}, {"20210203", 0.0124248849209095}, {"20210203", 0.0}};

// Expected NPVs given the market and trade inputs above
Real expNpvs[] = {0, 0, 0.050659, -0.05062};

// List of trades that will feed the data-driven test below to check CDS trade building.
vector<string> trades = {"cds_minimal_with_rules", "cds_minimal_with_dates"};

struct TodaysMarketFiles {
    string todaysMarket = "todaysmarket.xml";
    string conventions = "conventions.xml";
    string curveConfig = "curveconfig.xml";
    string market = "market.txt";
    string fixings = "fixings.txt";
};

// Create todaysmarket from input files.
QuantLib::ext::shared_ptr<TodaysMarket> createTodaysMarket(const Date& asof, const string& inputDir,
                                                   const TodaysMarketFiles& tmf) {

    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE(string(inputDir + "/" + tmf.conventions)));
    InstrumentConventions::instance().setConventions(conventions);

    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE(string(inputDir + "/" + tmf.curveConfig)));

    auto todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParameters->fromFile(TEST_INPUT_FILE(string(inputDir + "/" + tmf.todaysMarket)));

    auto loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE(string(inputDir + "/" + tmf.market)),
                                                TEST_INPUT_FILE(string(inputDir + "/" + tmf.fixings)), false);

    return QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParameters, loader, curveConfigs);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CreditDefaultSwapTests)

BOOST_DATA_TEST_CASE(testCreditDefaultSwap, bdata::make(marketInputs) ^ bdata::make(tradeInputs) ^ bdata::make(expNpvs),
                     mkt, trd, exp) {

    BOOST_CHECK_CLOSE(cdsNpv(mkt, trd), exp, 0.01);
}

BOOST_DATA_TEST_CASE_F(TopLevelFixture, testCreditDefaultSwapBuilding, bdata::make(trades), trade) {

    BOOST_TEST_MESSAGE("Test the building of various CDS trades from XML");

    Settings::instance().evaluationDate() = Date(3, Feb, 2016);

    // Read in the trade
    Portfolio p;
    string portfolioFile = "trades/" + trade + ".xml";
    p.fromFile(TEST_INPUT_FILE(portfolioFile));
    BOOST_REQUIRE_MESSAGE(p.size() == 1, "Expected portfolio to contain a single trade");

    // Use the test market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>(0.02, 0.4, 0.05);

    // Engine data
    QuantLib::ext::shared_ptr<EngineData> ed = QuantLib::ext::make_shared<EngineData>();
    ed->model("CreditDefaultSwap") = "DiscountedCashflows";
    ed->engine("CreditDefaultSwap") = "MidPointCdsEngine";

    // Test that the trade builds and prices without error
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(ed, market);
    BOOST_CHECK_NO_THROW(p.build(engineFactory));
    Real npv;
    BOOST_CHECK_NO_THROW(npv = p.trades().begin()->second->instrument()->NPV());
    BOOST_TEST_MESSAGE("CDS NPV is: " << npv);
}

// Various file combinations for the upfront consistency test below.
struct UpfrontFiles {
    string market;
    string curveConfig;
};

ostream& operator<<(ostream& os, const UpfrontFiles& upfrontFiles) {
    return os << "[" << upfrontFiles.market << "," << upfrontFiles.curveConfig << "]";
}

UpfrontFiles upfrontFiles[] = {{"market.txt", "curveconfig.xml"}, {"market_rs.txt", "curveconfig_rs.xml"}};

// Create CDS curve from upfront quotes. Price portfolio of CDS that match the curve instruments. Check that the
// value of each instrument is zero as expected. Notional is $10M and bootstrap accuracy is 1e-12 => tol of 1e-4
// should be adequate.
BOOST_DATA_TEST_CASE(testUpfrontDefaultCurveConsistency, bdata::make(upfrontFiles), files) {

    BOOST_TEST_MESSAGE("Testing upfront default curve consistency ...");

    Date asof(6, Nov, 2020);
    Settings::instance().evaluationDate() = asof;
    string dir("upfront");
    Real tol = 0.0001;

    TodaysMarketFiles tmf;
    tmf.market = files.market;
    tmf.curveConfig = files.curveConfig;

    QuantLib::ext::shared_ptr<TodaysMarket> tm;
    BOOST_REQUIRE_NO_THROW(tm = createTodaysMarket(asof, dir, tmf));

    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->fromFile(TEST_INPUT_FILE(string(dir + "/pricingengine.xml")));
    QuantLib::ext::shared_ptr<EngineFactory> ef = QuantLib::ext::make_shared<EngineFactory>(data, tm);

    Portfolio portfolio;
    portfolio.fromFile(TEST_INPUT_FILE(string(dir + "/portfolio.xml")));
    portfolio.build(ef);

    for (const auto& [tradeId, trade] : portfolio.trades()) {
        auto npv = trade->instrument()->NPV();
        BOOST_TEST_CONTEXT("NPV of trade " << tradeId << " is " << fixed << setprecision(12) << npv) {
            BOOST_CHECK_SMALL(trade->instrument()->NPV(), tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testUpfrontCurveBuildFailsIfNoRunningSpread) {

    BOOST_TEST_MESSAGE("Testing upfront failure when no running spread ...");

    TodaysMarketFiles tmf;
    tmf.curveConfig = "curveconfig_no_rs.xml";

    Date asof(6, Nov, 2020);
    Settings::instance().evaluationDate() = Date(6, Nov, 2020);
    BOOST_CHECK_THROW(createTodaysMarket(asof, "upfront", tmf), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testSimultaneousUsageCdsQuoteTypes) {

    BOOST_TEST_MESSAGE("Testing different CDS quote types can be used together ...");

    TodaysMarketFiles tmf;
    tmf.todaysMarket = "todaysmarket_all_cds_quote_types.xml";

    Date asof(6, Nov, 2020);
    Settings::instance().evaluationDate() = Date(6, Nov, 2020);

    // Check that todaysmarket instance is created without error.
    QuantLib::ext::shared_ptr<TodaysMarket> tm;
    BOOST_CHECK_NO_THROW(tm = createTodaysMarket(asof, "upfront", tmf));

    // Check that each of the three expected curves exist and give survival probability.
    vector<string> curveNames = {
        "RED:8B69AP|SNRFOR|USD|CR-UPFRONT",
        "RED:8B69AP|SNRFOR|USD|CR-PAR_SPREAD",
        "RED:8B69AP|SNRFOR|USD|CR-CONV_SPREAD",
    };

    for (const string& curveName : curveNames) {
        BOOST_TEST_CONTEXT("Checking default curve " << curveName) {
            Handle<DefaultProbabilityTermStructure> dpts;
            BOOST_CHECK_NO_THROW(dpts = tm->defaultCurve(curveName)->curve());
            BOOST_CHECK_NO_THROW(dpts->survivalProbability(1.0));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
