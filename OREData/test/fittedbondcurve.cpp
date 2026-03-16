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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ored/marketdata/fittedbondcurvehelpermarket.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>

#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/termstructures/yield/bondhelpers.hpp>
#include <ql/termstructures/yield/nonlinearfittingmethods.hpp>

using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FittedBondCurveTests)

BOOST_AUTO_TEST_CASE(testCurveFromFixedRateBonds) {

    Date asof(6, April, 2020);
    Settings::instance().evaluationDate() = asof;

    BOOST_TEST_MESSAGE("read pricing engine config");
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->fromFile(TEST_INPUT_FILE("pricingengine.xml"));

    BOOST_TEST_MESSAGE("read portfolio of bonds");
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->fromFile(TEST_INPUT_FILE("portfolio1.xml"));

    BOOST_TEST_MESSAGE("build portfolio against FittedBondCurveHelperMarket");
    QuantLib::ext::shared_ptr < EngineFactory> engineFactory =
        QuantLib::ext::make_shared<EngineFactory>(engineData, QuantLib::ext::make_shared<FittedBondCurveHelperMarket>());
    portfolio->build(engineFactory);

    BOOST_TEST_MESSAGE("set up bond helpers");
    std::vector<QuantLib::ext::shared_ptr<Bond>> bonds;
    std::vector<QuantLib::ext::shared_ptr<BondHelper>> helpers;
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        QuantLib::ext::shared_ptr<QuantLib::Bond> bond =
            QuantLib::ext::dynamic_pointer_cast<Bond>(trade->instrument()->qlInstrument());
        BOOST_REQUIRE(bond);
        bonds.push_back(bond);
        helpers.push_back(QuantLib::ext::make_shared<BondHelper>(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0)), bond));
    }

    BOOST_TEST_MESSAGE("build fitted bond curve");
    Array guess({0.03, 0.03, 0.03, 0.5});
    NelsonSiegelFitting method((Array(), QuantLib::ext::shared_ptr<OptimizationMethod>(), Array()));
    auto curve =
        QuantLib::ext::make_shared<FittedBondDiscountCurve>(asof, helpers, Actual365Fixed(), method, 1E-10, 10000, guess);

    BOOST_TEST_MESSAGE("cost = " << curve->fitResults().minimumCostValue());

    auto engine = QuantLib::ext::make_shared<DiscountingBondEngine>(Handle<YieldTermStructure>(curve));
    for (auto const& b : bonds) {
        b->setPricingEngine(engine);
        BOOST_TEST_MESSAGE("bond helper maturity " << b->maturityDate() << " has clean price " << b->cleanPrice()
                                                   << " discount factor is " << curve->discount(b->maturityDate()));
        BOOST_CHECK_CLOSE(b->cleanPrice(), 100.0, 0.01); // 1bp tolerance in absolute price
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
