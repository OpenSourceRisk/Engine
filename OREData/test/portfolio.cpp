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
#include <boost/test/unit_test.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <oret/toplevelfixture.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PortfolioTests)

BOOST_AUTO_TEST_CASE(testAddTrades) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade1 = QuantLib::ext::make_shared<FxForward>();
    QuantLib::ext::shared_ptr<FxForward> trade2 = QuantLib::ext::make_shared<FxForward>();
    trade1->id() = "1";
    trade2->id() = "2";
    BOOST_CHECK_EQUAL(portfolio->has(trade1->id()), false);
    BOOST_CHECK_EQUAL(portfolio->size(), 0);
    portfolio->add(trade1);
    BOOST_CHECK_EQUAL(portfolio->has(trade1->id()), true);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    portfolio->add(trade2);
    BOOST_CHECK_EQUAL(portfolio->has(trade2->id()), true);
    BOOST_CHECK_EQUAL(portfolio->size(), 2);
}

BOOST_AUTO_TEST_CASE(testAddTradeWithExistingId) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade1 = QuantLib::ext::make_shared<FxForward>();
    QuantLib::ext::shared_ptr<FxForward> trade2 = QuantLib::ext::make_shared<FxForward>();
    portfolio->add(trade1);
    BOOST_CHECK_THROW(portfolio->add(trade2), exception);
}

BOOST_AUTO_TEST_CASE(testClear) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade = QuantLib::ext::make_shared<FxForward>();
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    portfolio->clear();
    BOOST_CHECK_EQUAL(portfolio->size(), 0);
}

BOOST_AUTO_TEST_CASE(testSize) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade1 = QuantLib::ext::make_shared<FxForward>();
    QuantLib::ext::shared_ptr<FxForward> trade2 = QuantLib::ext::make_shared<FxForward>();
    trade1->id() = "1";
    trade2->id() = "2";
    BOOST_CHECK_EQUAL(portfolio->size(), 0);
    portfolio->add(trade1);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    portfolio->add(trade2);
    BOOST_CHECK_EQUAL(portfolio->size(), 2);
}

BOOST_AUTO_TEST_CASE(testRemove) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade = QuantLib::ext::make_shared<FxForward>();
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), false);
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), true);
    portfolio->remove(trade->id());
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), false);
}

BOOST_AUTO_TEST_CASE(testTrades) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade1 = QuantLib::ext::make_shared<FxForward>();
    QuantLib::ext::shared_ptr<FxForward> trade2 = QuantLib::ext::make_shared<FxForward>();
    std::map<std::string, QuantLib::ext::shared_ptr<Trade>> trade_list;
    trade1->id() = "1";
    trade2->id() = "2";
    BOOST_CHECK(portfolio->trades() == trade_list);
    portfolio->add(trade1);
    trade_list["1"]  = trade1;
    BOOST_CHECK(portfolio->trades() == trade_list);
    portfolio->add(trade2);
    trade_list["2"] = trade2;
    BOOST_CHECK(portfolio->trades() == trade_list);
}

BOOST_AUTO_TEST_CASE(testIds) {
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<FxForward> trade1 = QuantLib::ext::make_shared<FxForward>();
    QuantLib::ext::shared_ptr<FxForward> trade2 = QuantLib::ext::make_shared<FxForward>();
    trade1->id() = "1";
    trade2->id() = "2";
    std::set<std::string> trade_ids;
    BOOST_CHECK(portfolio->ids() == trade_ids);
    portfolio->add(trade1);
    trade_ids.insert("1");
    BOOST_CHECK(portfolio->ids() == trade_ids);
    portfolio->add(trade2);
    trade_ids.insert("2");
    BOOST_CHECK(portfolio->ids() == trade_ids);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
