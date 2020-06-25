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
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>
#include <tuple>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

using ore::test::TopLevelFixture;

namespace bdata = boost::unit_test::data;

namespace {

// Fixture used in test case below:
// - sets a specific valuation date for the test
// - provides conventions
// - provides an engine factory for the test
class F : public TopLevelFixture {
public:
    Date today;
    Conventions conventions;
    boost::shared_ptr<EngineFactory> engineFactory;

    F() {
        today = Date(31, Dec, 2018);
        Settings::instance().evaluationDate() = today;

        conventions.fromFile(TEST_INPUT_FILE("conventions.xml"));

        TodaysMarketParameters todaysMarketParams;
        todaysMarketParams.fromFile(TEST_INPUT_FILE("todaysmarket.xml"));

        CurveConfigurations curveConfigs;
        curveConfigs.fromFile(TEST_INPUT_FILE("curveconfig.xml"));

        string marketFile = TEST_INPUT_FILE("market.txt");
        string fixingsFile = TEST_INPUT_FILE("fixings.txt");
        CSVLoader loader(marketFile, fixingsFile, false);

        bool continueOnError = false;
        boost::shared_ptr<TodaysMarket> market = boost::make_shared<TodaysMarket>(
            today, todaysMarketParams, loader, curveConfigs, conventions, continueOnError);

        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->fromFile(TEST_INPUT_FILE("pricingengine.xml"));

        engineFactory = boost::make_shared<EngineFactory>(engineData, market);

        // Log::instance().registerLogger(boost::make_shared<FileLogger>(TEST_OUTPUT_FILE("log.txt")));
        // Log::instance().setMask(255);
        // Log::instance().switchOn();
    }

    ~F() {}
};

// Portfolios, designed such that trade NPVs should add up to zero
// The first two cases consist of three trades:
// 1) CPI Swap receiving a single zero coupon fixed flow and paying a single indexed redemption flow (resp. CPI cpupons
//    plus indexed redemption)
// 2) CPI Swap as above with capped indexed flow and flipped legs: pay zero coupon fixed, receive
//    capped indexed redemption (resp. capped CPI coupons plus capped indexed redemption), i.e. short embedded cap(s)
// 3) standalone long CPI cap with indexed flow(s) above as underlying
// The third portfolio has two trades
// 1) A CPI Cap as CapFloor instrument
// 2) A CPI Cap as Swap with a single CPI leg and "naked" option set to "Y"

vector<string> testCases = {"portfolio_singleflow.xml", "portfolio_multiflow.xml", "portfolio_multiflow_naked.xml"};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CPICapFloorTests)

BOOST_DATA_TEST_CASE_F(F, testCapConsistency, bdata::make(testCases), testCase) {

    BOOST_TEST_MESSAGE("Testing " << testCase);

    Portfolio p;
    p.load(TEST_INPUT_FILE(testCase));
    Size n = p.size();

    // Build the portfolio
    p.build(engineFactory);
    BOOST_CHECK_MESSAGE(p.size() == n, n << " trades expected after build, found " << p.size());
    for (Size i = 0; i < p.size(); ++i) {
        BOOST_TEST_MESSAGE("trade " << p.trades()[i]->id());
    }

    // Portfolios are designed such that trade NPVs should add up to zero
    Real sum = 0.0;
    Real minimumNPV = QL_MAX_REAL;
    for (Size i = 0; i < p.size(); ++i) {
        BOOST_CHECK_NO_THROW(p.trades()[i]->instrument()->NPV());
        BOOST_TEST_MESSAGE("trade " << p.trades()[i]->id() << " npv " << p.trades()[i]->instrument()->NPV());
        sum += p.trades()[i]->instrument()->NPV();
        minimumNPV = std::min(minimumNPV, fabs(p.trades()[i]->instrument()->NPV()));
    }
    BOOST_TEST_MESSAGE("mininim absolute NPV = " << minimumNPV);
    Real tolerance = 1.0e-8 * minimumNPV;
    BOOST_TEST_MESSAGE("tolerance = " << tolerance);
    BOOST_TEST_MESSAGE("NPV sum = " << sum);
    BOOST_CHECK_MESSAGE(fabs(sum) < tolerance, "portfolio NPV should be zero, found " << sum);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
