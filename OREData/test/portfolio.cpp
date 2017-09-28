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

#include "portfolio.hpp"
#include <ored/portfolio/portfolio.hpp>
#include <boost/make_shared.hpp>
#include <test/portfolio.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <iostream>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;


namespace testsuite {

void PortfolioTest::testConstructor() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    //BOOST_CHECK_EQUAL(portfolio, true);
}

void PortfolioTest::testAddTrade() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    boost::shared_ptr<FxForward> trade = boost::make_shared<FxForward>();
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), true);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
}

void PortfolioTest::testAddTradeWithExistingId() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    boost::shared_ptr<FxForward> trade1 = boost::make_shared<FxForward>();
    boost::shared_ptr<FxForward> trade2 = boost::make_shared<FxForward>();
    portfolio->add(trade1);
    BOOST_CHECK_THROW(portfolio->add(trade2), exception);
}

void PortfolioTest::testClear() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    boost::shared_ptr<FxForward> trade = boost::make_shared<FxForward>();
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
    portfolio->clear();
    BOOST_CHECK_EQUAL(portfolio->size(), 0);
}

void PortfolioTest::testSize() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    boost::shared_ptr<FxForward> trade = boost::make_shared<FxForward>();
    BOOST_CHECK_EQUAL(portfolio->size(), 0);
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->size(), 1);
}

void PortfolioTest::testRemove() {
    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
    boost::shared_ptr<FxForward> trade = boost::make_shared<FxForward>();
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), false);
    portfolio->add(trade);
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), true);
    portfolio->remove(trade->id());
    BOOST_CHECK_EQUAL(portfolio->has(trade->id()), false);
}

test_suite* PortfolioTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Portfolio Unittests");
    BOOST_TEST_MESSAGE("Testing Portfolio...");

    suite->add(BOOST_TEST_CASE(&PortfolioTest::testConstructor));
    suite->add(BOOST_TEST_CASE(&PortfolioTest::testAddTrade));
    suite->add(BOOST_TEST_CASE(&PortfolioTest::testAddTradeWithExistingId));
    suite->add(BOOST_TEST_CASE(&PortfolioTest::testClear));
    suite->add(BOOST_TEST_CASE(&PortfolioTest::testSize));
    suite->add(BOOST_TEST_CASE(&PortfolioTest::testRemove));
    return suite;
}
} // namespace testsuite
