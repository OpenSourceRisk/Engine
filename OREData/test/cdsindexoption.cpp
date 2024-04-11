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

#include <oret/toplevelfixture.hpp>
#include <boost/test/unit_test.hpp>
#include <oret/datapaths.hpp>

#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

using ore::data::BlackIndexCdsOptionEngineBuilder;
using ore::data::MidPointIndexCdsEngineBuilder;

using std::fixed;
using std::setprecision;
using std::string;
using std::vector;


namespace {

// Create portfolio from input files.
QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(const Date& asof, const string& inputDir) {

    Settings::instance().evaluationDate() = asof;

    auto conventions = QuantLib::ext::make_shared<Conventions>();
    conventions->fromFile(TEST_INPUT_FILE(string(inputDir + "/conventions.xml")));
    InstrumentConventions::instance().setConventions(conventions);

    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE(string(inputDir + "/curveconfig.xml")));

    auto todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParameters->fromFile(TEST_INPUT_FILE(string(inputDir + "/todaysmarket.xml")));

    auto loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE(string(inputDir + "/market.txt")),
        TEST_INPUT_FILE(string(inputDir + "/fixings.txt")), false);

    auto tm = QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParameters, loader, curveConfigs);

    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->fromFile(TEST_INPUT_FILE(string(inputDir + "/pricingengine.xml")));

    auto rdm = QuantLib::ext::make_shared<BasicReferenceDataManager>(
        TEST_INPUT_FILE(string(inputDir + "/reference_data.xml")));

    map<MarketContext, string> configurations;
    vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders{
        QuantLib::ext::make_shared<MidPointIndexCdsEngineBuilder>(),
        QuantLib::ext::make_shared<BlackIndexCdsOptionEngineBuilder>()
    };
    vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders;
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(data, tm, configurations, rdm);

    auto portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE(string(inputDir + "/portfolio.xml")));
    portfolio->build(engineFactory);

    return portfolio;
}

// Check portfolio prices are within tolerance
void checkNpvs(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, Real tol, Real relTol = Null<Real>()) {
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        auto npv = trade->instrument()->NPV();
        if (relTol != Null<Real>()) {
            auto opt = QuantLib::ext::dynamic_pointer_cast<ore::data::IndexCreditDefaultSwapOption>(trade);
            Real expNpv = opt->option().premiumData().premiumData().front().amount;
            Real relDiff = npv / expNpv;
            BOOST_TEST_CONTEXT("trade_id,npv,expNpv,relDiff:" << tradeId << fixed << setprecision(2) <<
                "," << npv << "," << expNpv << "," << setprecision(6) << relDiff) {
                BOOST_CHECK(std::abs(npv) < tol || std::abs(relDiff) < relTol);
            }
        } else {
            BOOST_TEST_CONTEXT("trade_id,npv:" << tradeId << fixed << setprecision(2) << "," << npv) {
                BOOST_CHECK_SMALL(npv, tol);
            }
        }
    }
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CdsIndexOptionTest)

/* The 4 test cases below perform the same steps to check index CDS option pricing for different scenarios against 
   Markit data for the given valuation date. The differing scenarios are:
   - strike is quoted in terms of spread or price
   - pricing engine uses the index CDS spread curve or the underlying CDS spread curves (without bias correction)

   The portfolio is built from data in the given directories. Markit CDS spreads and volatilities are used to price 
   the index CDS options across a range of strikes, including deeply in-the-money and out-of-the-money strikes, and 
   a range of option expiries from 3M to 12M. The trades in the portfolio have a notional of 10K and the associated 
   Markit premium in the `PremiumAmount` field. The NPV of the trade is therefore the difference between the Markit 
   premium and our calculated value in bps. We check this difference against a tolerance.
*/
BOOST_AUTO_TEST_CASE(testSpreadStrikeNoDefaultsIndexCurve) {
    BOOST_TEST_MESSAGE("Testing pricing for spread strike, no existing defaults, using index curve ...");
    checkNpvs(buildPortfolio(Date(22, Apr, 2021), "cdx_ig_36_v1_2021-04-22_index"), 6.5);
}

BOOST_AUTO_TEST_CASE(testSpreadStrikeNoDefaultsUnderlyingCurves) {
    BOOST_TEST_MESSAGE("Testing pricing for spread strike, no existing defaults, using underlying curves ...");
    checkNpvs(buildPortfolio(Date(22, Apr, 2021), "cdx_ig_36_v1_2021-04-22_underlyings"), 12.0);
}

BOOST_AUTO_TEST_CASE(testPriceStrikeNoDefaultsIndexCurve) {
    BOOST_TEST_MESSAGE("Testing pricing for price strike, no existing defaults, using index curve ...");
    checkNpvs(buildPortfolio(Date(22, Apr, 2021), "cdx_hy_36_v1_2021-04-22_index"), 10, 0.105);
}

// Large relative tolerance used here. We see large differences with Markit due to a difference in the front end 
// adjusted forward price that we calculate vs. the forward price that they use.
BOOST_AUTO_TEST_CASE(testPriceStrikeNoDefaultsUnderlyingCurves) {
    BOOST_TEST_MESSAGE("Testing pricing for price strike, no existing defaults, using underlying curves ...");
    checkNpvs(buildPortfolio(Date(22, Apr, 2021), "cdx_hy_36_v1_2021-04-22_underlyings"), 20, 0.25);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
