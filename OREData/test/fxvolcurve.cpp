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
#include <ored/marketdata/inmemoryloader.hpp>
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
#include <oret/util/datapaths.hpp>
#include <ored/utilities/toplevelfixture.hpp>

#include <qle/termstructures/blackvolsurfacebfrr.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>

#include <ql/pricingengines/blackdeltacalculator.hpp>
#include <ql/time/calendars/target.hpp>

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
        auto csvLoader = QuantLib::ext::make_shared<CSVLoader>();
        csvLoader->fromFiles(TEST_INPUT_FILE(filename), TEST_INPUT_FILE(fixingsFilename));
        loader = csvLoader;
    }

    Date asof;
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    QuantLib::ext::shared_ptr<CurveConfigurations> curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    QuantLib::ext::shared_ptr<Loader> loader;
};

const Date marketDate(31, Dec, 2018);

// Today's market from the test inputs at asof, with the EURUSD vol curve built from the given FXVolatility
// configuration and FX option quotes FX_OPTION/RATE_LNVOL/EUR/USD/<expiry>/<strike> as the only option quotes
QuantLib::ext::shared_ptr<TodaysMarket> fxVolMarket(const Date& asof, const string& fxVolConfigXml,
                                                    const vector<pair<string, Real>>& fxOptionQuotes) {
    TodaysMarketArguments tma(asof, "curveconfig_wc.xml");
    auto loader = QuantLib::ext::make_shared<InMemoryLoader>();
    for (auto const& md : tma.loader->loadQuotes(marketDate)) {
        if (md->instrumentType() != MarketDatum::InstrumentType::FX_OPTION)
            loader->add(asof, md->name(), md->quote()->value());
    }
    for (auto const& f : tma.loader->loadFixings())
        loader->addFixing(f.date, f.name, f.fixing);
    for (auto const& q : fxOptionQuotes)
        loader->add(asof, "FX_OPTION/RATE_LNVOL/EUR/USD/" + q.first, q.second);

    auto config = QuantLib::ext::make_shared<FXVolatilityCurveConfig>();
    config->fromXMLString(fxVolConfigXml);
    tma.curveConfigs->add(CurveSpec::CurveType::FXVolatility, "EURUSD", config);

    return QuantLib::ext::make_shared<TodaysMarket>(asof, tma.todaysMarketParameters, loader, tma.curveConfigs, false,
                                                    true, false);
}

string atmConfig(const string& expiries, const string& extra = "") {
    return "<FXVolatility><CurveId>EURUSD</CurveId><CurveDescription/><Dimension>ATM</Dimension><Expiries>" +
           expiries +
           "</Expiries><FXSpotID>FX/EUR/USD</FXSpotID><Calendar>TARGET</Calendar><DayCounter>A365</DayCounter>" +
           extra + "</FXVolatility>";
}

string smileConfig(const string& smile) {
    return "<FXVolatility><CurveId>EURUSD</CurveId><CurveDescription/><Dimension>Smile</Dimension>" + smile +
           "<Expiries>*</Expiries><FXSpotID>FX/EUR/USD</FXSpotID><FXForeignCurveID>Yield/EUR/EUR-IN-USD"
           "</FXForeignCurveID><FXDomesticCurveID>Yield/USD/USD-FedFunds</FXDomesticCurveID><Calendar>TARGET"
           "</Calendar><DayCounter>A365</DayCounter></FXVolatility>";
}

// Collects the structured messages logged while it lives
class StructuredMessages {
public:
    StructuredMessages() { Log::instance().registerIndependentLogger(logger_); }
    ~StructuredMessages() { Log::instance().removeIndependentLogger(StructuredLogger::name); }
    bool contains(const string& text) const {
        return std::any_of(logger_->messages().begin(), logger_->messages().end(),
                           [&text](const string& m) { return m.find(text) != string::npos; });
    }

private:
    QuantLib::ext::shared_ptr<StructuredLogger> logger_ = QuantLib::ext::make_shared<StructuredLogger>();
};

bool nonDecreasingVarianceError(const Error& e) {
    BOOST_TEST_MESSAGE(e.what());
    return string(e.what()).find("variance must be non-decreasing") != string::npos;
}

// Checks the ATM vol at each pillar equals its quote, with the pillar a tenor from asof or a date
void checkAtmPillars(const Handle<BlackVolTermStructure>& vol, const Date& asof,
                     const vector<pair<Date, Real>>& pillars) {
    for (auto const& p : pillars) {
        BOOST_TEST_MESSAGE("pillar " << p.first << " expected vol " << p.second);
        BOOST_CHECK_CLOSE(vol->blackVol(p.first, 1.0), p.second, 1e-10);
    }
}

// ATM vol of a delta surface at a pillar: the vol of its smile at the strike of the ATM node equals the ATM quote
void checkDeltaSurfaceAtm(const Handle<BlackVolTermStructure>& vol, const Date& pillar, Real atmQuote) {
    auto surface = QuantLib::ext::dynamic_pointer_cast<QuantExt::BlackVolatilitySurfaceDelta>(*vol);
    BOOST_REQUIRE(surface);
    auto smile = QuantLib::ext::dynamic_pointer_cast<QuantExt::InterpolatedSmileSection>(surface->blackVolSmile(pillar));
    BOOST_REQUIRE(smile);
    BOOST_REQUIRE_EQUAL(smile->volatilities().size(), 3);
    BOOST_CHECK_CLOSE(smile->volatilities()[1], atmQuote, 1e-10);
    BOOST_CHECK_CLOSE(vol->blackVol(pillar, smile->strikes()[1]), atmQuote, 1e-10);
}

// ATM vol of a BFRR surface at a pillar: the vol at the delta neutral ATM strike equals the ATM quote, within
// tolerance (percent)
void checkBfrrSurfaceAtm(const QuantLib::ext::shared_ptr<TodaysMarket>& market, const Date& asof, const Date& pillar,
                         Real atmQuote, Real tolerance = 1e-6) {
    auto vol = market->fxVol("EURUSD");
    auto surface = QuantLib::ext::dynamic_pointer_cast<QuantExt::BlackVolatilitySurfaceBFRR>(*vol);
    BOOST_REQUIRE(surface);
    auto it = std::find(surface->dates().begin(), surface->dates().end(), pillar);
    BOOST_REQUIRE(it != surface->dates().end());
    BOOST_CHECK_CLOSE(surface->atmQuotes()[it - surface->dates().begin()], atmQuote, 1e-10);

    // the conventions default to spot delta, delta neutral ATM, settlement two EUR,USD business days after expiry
    Calendar spotCal = parseCalendar("EUR,USD");
    Date settlAsof = spotCal.advance(asof, 2 * Days), settl = spotCal.advance(pillar, 2 * Days);
    auto dom = market->discountCurve("USD"), fgn = market->discountCurve("EUR");
    Real t = vol->timeFromReference(pillar);
    BlackDeltaCalculator c(Option::Call, DeltaVolQuote::Spot, market->fxSpot("EURUSD")->value(),
                           dom->discount(settl) / dom->discount(settlAsof),
                           fgn->discount(settl) / fgn->discount(settlAsof), atmQuote * std::sqrt(t));
    BOOST_CHECK_CLOSE(vol->blackVol(pillar, c.atmStrike(DeltaVolQuote::AtmDeltaNeutral)), atmQuote, tolerance);
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::data::TopLevelFixture)

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

BOOST_AUTO_TEST_CASE(testFxVolAtmDatePillars) {

    BOOST_TEST_MESSAGE("Testing FX ATM vol curve with tenor and date pillars");

    Date asof = marketDate;
    Date datePillar(18, Jan, 2019);
    vector<pair<string, Real>> quotes = {{"1W/ATM", 0.0575},
                                         {"2019-01-18/ATM", 0.065},
                                         {"1M/ATM", 0.07117},
                                         {"2018-12-28/ATM", 0.5},
                                         {"2018-12-31/ATM", 0.5}};
    vector<pair<Date, Real>> pillars = {{TARGET().advance(asof, 1 * Weeks), 0.0575},
                                        {datePillar, 0.065},
                                        {TARGET().advance(asof, 1 * Months), 0.07117}};

    for (auto const& expiries : {string("*"), string("1M,2019-01-18,1W,2018-12-28,2018-12-31")}) {
        BOOST_TEST_MESSAGE("Expiries " << expiries);
        auto market = fxVolMarket(asof, atmConfig(expiries), quotes);
        checkAtmPillars(market->fxVol("EURUSD"), asof, pillars);
    }
}

BOOST_AUTO_TEST_CASE(testFxVolSmileDatePillars) {

    BOOST_TEST_MESSAGE("Testing FX delta and BFRR vol surfaces with a date pillar");

    Date asof = marketDate;
    Date datePillar(18, Jan, 2019);

    auto delta = fxVolMarket(asof, smileConfig("<SmileType>Delta</SmileType><Deltas>25P,ATM,25C</Deltas>"),
                             {{"1M/25P", 0.075}, {"1M/ATM", 0.07117}, {"1M/25C", 0.07},
                              {"2019-01-18/25P", 0.07}, {"2019-01-18/ATM", 0.065}, {"2019-01-18/25C", 0.064},
                              {"2018-12-28/25P", 0.5}, {"2018-12-28/ATM", 0.5}, {"2018-12-28/25C", 0.5},
                              {"3M/25P", 0.078}, {"3M/ATM", 0.0715}, {"3M/25C", 0.07}});
    checkDeltaSurfaceAtm(delta->fxVol("EURUSD"), datePillar, 0.065);
    checkDeltaSurfaceAtm(delta->fxVol("EURUSD"), TARGET().advance(asof, 1 * Months), 0.07117);

    auto bfrr = fxVolMarket(asof, smileConfig("<SmileType>BFRR</SmileType><SmileDelta>25</SmileDelta>"),
                            {{"1M/ATM", 0.07117}, {"1M/25RR", -0.005}, {"1M/25BF", 0.002},
                             {"2019-01-18/ATM", 0.065}, {"2019-01-18/25RR", -0.004}, {"2019-01-18/25BF", 0.0015},
                             {"2018-12-28/ATM", 0.5}, {"2018-12-28/25RR", 0.1}, {"2018-12-28/25BF", 0.1},
                             {"3M/ATM", 0.0715}, {"3M/25RR", -0.006}, {"3M/25BF", 0.0025}});
    checkBfrrSurfaceAtm(bfrr, asof, datePillar, 0.065);
    checkBfrrSurfaceAtm(bfrr, asof, TARGET().advance(asof, 1 * Months), 0.07117);
}

BOOST_AUTO_TEST_CASE(testFxVolEnforceMonotoneVarianceConfig) {

    BOOST_TEST_MESSAGE("Testing the EnforceMonotoneVariance FX vol curve configuration");

    for (bool enforce : {false, true}) {
        string extra = enforce ? "<EnforceMonotoneVariance>true</EnforceMonotoneVariance>" : "";
        FXVolatilityCurveConfig config;
        config.fromXMLString(atmConfig("1M,2M", extra));
        BOOST_CHECK_EQUAL(config.enforceMonotoneVariance(), enforce);

        string xml = config.toXMLString();
        BOOST_CHECK_EQUAL(xml.find("EnforceMonotoneVariance") != string::npos, enforce);
        FXVolatilityCurveConfig roundTrip;
        roundTrip.fromXMLString(xml);
        BOOST_CHECK_EQUAL(roundTrip.enforceMonotoneVariance(), enforce);
    }
}

BOOST_AUTO_TEST_CASE(testFxVolDecreasingAtmVariance) {

    BOOST_TEST_MESSAGE("Testing FX vol curves with a decreasing ATM total variance");

    Date asof = marketDate;
    string enforce = "<EnforceMonotoneVariance>true</EnforceMonotoneVariance>";
    // total variance 0.1^2 / 12 at 1M is above 0.06^2 * 2 / 12 at 2M
    vector<pair<string, Real>> atm = {{"1M/ATM", 0.10}, {"2M/ATM", 0.06}, {"3M/ATM", 0.08}};
    vector<pair<string, Real>> bfrr = {{"1M/ATM", 0.10}, {"1M/25RR", -0.005}, {"1M/25BF", 0.002},
                                       {"2M/ATM", 0.06}, {"2M/25RR", -0.005}, {"2M/25BF", 0.002},
                                       {"3M/ATM", 0.08}, {"3M/25RR", -0.005}, {"3M/25BF", 0.002}};
    string bfrrSmile = "<SmileType>BFRR</SmileType><SmileDelta>25</SmileDelta>";

    BOOST_CHECK_EXCEPTION(fxVolMarket(asof, atmConfig("*", enforce), atm), Error, nonDecreasingVarianceError);
    BOOST_CHECK_EXCEPTION(fxVolMarket(asof, smileConfig(bfrrSmile + enforce), bfrr), Error,
                          nonDecreasingVarianceError);

    {
        StructuredMessages messages;
        auto market = fxVolMarket(asof, atmConfig("*"), atm);
        checkAtmPillars(market->fxVol("EURUSD"), asof,
                        {{TARGET().advance(asof, 1 * Months), 0.10}, {TARGET().advance(asof, 2 * Months), 0.06}});
        BOOST_CHECK(messages.contains("Decreasing ATM total variance"));
        BOOST_CHECK(messages.contains("at FX_OPTION/RATE_LNVOL/EUR/USD/1M/ATM to"));
        BOOST_CHECK(messages.contains("at FX_OPTION/RATE_LNVOL/EUR/USD/2M/ATM, set EnforceMonotoneVariance"));
        BOOST_CHECK(messages.contains("FXVolatility/EUR/USD/EURUSD"));
    }
    {
        StructuredMessages messages;
        fxVolMarket(asof, smileConfig(bfrrSmile), bfrr);
        BOOST_CHECK(messages.contains("Decreasing ATM total variance"));
        BOOST_CHECK(messages.contains("at 1M to"));
    }
    {
        // increasing variance, also with the flag set: no warning
        StructuredMessages messages;
        fxVolMarket(asof, atmConfig("*", enforce), {{"1M/ATM", 0.07}, {"2M/ATM", 0.06}, {"3M/ATM", 0.08}});
        BOOST_CHECK(!messages.contains("Decreasing ATM total variance"));
    }
}

#ifdef QL_HIGH_RESOLUTION_DATE
BOOST_AUTO_TEST_CASE(testFxVolIntradayCuts) {

    BOOST_TEST_MESSAGE("Testing FX vol curves with pillars at two cuts on the asof's day");

    Date asof(31, Dec, 2018, 9, 0, 0);
    Date ecb(31, Dec, 2018, 13, 30, 0), ny(31, Dec, 2018, 15, 0, 0);

    auto atm = fxVolMarket(asof, atmConfig("*"),
                           {{"2018-12-31T08:00Z/ATM", 0.9},
                            {"2018-12-31T13:30Z/ATM", 0.05},
                            {"2018-12-31T15:00Z/ATM", 0.055},
                            {"1M/ATM", 0.07117}});
    checkAtmPillars(atm->fxVol("EURUSD"), asof,
                    {{ecb, 0.05}, {ny, 0.055}, {TARGET().advance(asof, 1 * Months), 0.07117}});

    auto explicitAtm = fxVolMarket(asof, atmConfig("2018-12-31T08:00Z,2018-12-31T13:30Z,2018-12-31T15:00Z,1M"),
                                   {{"2018-12-31T13:30Z/ATM", 0.05},
                                    {"2018-12-31T15:00Z/ATM", 0.055},
                                    {"1M/ATM", 0.07117}});
    checkAtmPillars(explicitAtm->fxVol("EURUSD"), asof,
                    {{ecb, 0.05}, {ny, 0.055}, {TARGET().advance(asof, 1 * Months), 0.07117}});

    auto delta = fxVolMarket(asof, smileConfig("<SmileType>Delta</SmileType><Deltas>25P,ATM,25C</Deltas>"),
                             {{"2018-12-31T13:30Z/25P", 0.052}, {"2018-12-31T13:30Z/ATM", 0.05},
                              {"2018-12-31T13:30Z/25C", 0.049}, {"2018-12-31T15:00Z/25P", 0.057},
                              {"2018-12-31T15:00Z/ATM", 0.055}, {"2018-12-31T15:00Z/25C", 0.054},
                              {"1M/25P", 0.075}, {"1M/ATM", 0.07117}, {"1M/25C", 0.07}});
    checkDeltaSurfaceAtm(delta->fxVol("EURUSD"), ecb, 0.05);
    checkDeltaSurfaceAtm(delta->fxVol("EURUSD"), ny, 0.055);

    auto bfrr = fxVolMarket(asof, smileConfig("<SmileType>BFRR</SmileType><SmileDelta>25</SmileDelta>"),
                            {{"2018-12-31T13:30Z/ATM", 0.05}, {"2018-12-31T13:30Z/25RR", -0.003},
                             {"2018-12-31T13:30Z/25BF", 0.001}, {"2018-12-31T15:00Z/ATM", 0.055},
                             {"2018-12-31T15:00Z/25RR", -0.003}, {"2018-12-31T15:00Z/25BF", 0.001},
                             {"1M/ATM", 0.07117}, {"1M/25RR", -0.005}, {"1M/25BF", 0.002}});
    // The BFRR surface rebuilds the smile at t with settlement from lowerDate(t), a whole number of days after the
    // asof, so at a pillar hours away its delta neutral strike is off by the carry over those hours.
    checkBfrrSurfaceAtm(bfrr, asof, ecb, 0.05, 0.1);
    checkBfrrSurfaceAtm(bfrr, asof, ny, 0.055, 0.1);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
