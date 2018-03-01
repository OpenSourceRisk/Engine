/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include "commodityforward.hpp"

#include <ql/currencies/america.hpp>
#include <ql/settings.hpp>

#include <qle/instruments/commodityforward.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {
class CommonData {
public:
    // Variables
    string name;
    USDCurrency currency;
    Position::Type position;
    Real quantity;
    Date maturity;
    Real strike;

    // cleanup
    SavedSettings backup;

    // Default constructor
    CommonData() : name("GOLD_USD"),
        currency(USDCurrency()),
        position(Position::Long),
        quantity(100),
        maturity(19, Feb, 2019),
        strike(50.0) {}
};
}

namespace testsuite {

void CommodityForwardTest::testConstructor() {
    
    BOOST_TEST_MESSAGE("Testing commodity forward constructor");

    CommonData td;

    CommodityForward forward(td.name, td.currency, td.position, 
        td.quantity, td.maturity, td.strike);

    BOOST_CHECK_EQUAL(forward.name(), td.name);
    BOOST_CHECK_EQUAL(forward.currency(), td.currency);
    BOOST_CHECK_EQUAL(forward.position(), td.position);
    BOOST_CHECK_EQUAL(forward.quantity(), td.quantity);
    BOOST_CHECK_EQUAL(forward.maturityDate(), td.maturity);
    BOOST_CHECK_EQUAL(forward.strike(), td.strike);
}

void CommodityForwardTest::testIsExpired() {

    BOOST_TEST_MESSAGE("Testing commodity forward expiry logic");

    CommonData td;

    CommodityForward forward(td.name, td.currency, td.position,
        td.quantity, td.maturity, td.strike);

    Settings::instance().evaluationDate() = td.maturity - 1 * Days;
    Settings::instance().includeReferenceDateEvents() = true;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().evaluationDate() = td.maturity;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().includeReferenceDateEvents() = false;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);
}

void CommodityForwardTest::testNegativeQuantityThrows() {

    BOOST_TEST_MESSAGE("Test that using a negative quantity in the constructor causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.name, td.currency, 
        td.position, -10.0, td.maturity, td.strike), QuantLib::Error);
}

void CommodityForwardTest::testNegativeStrikeThrows() {

    BOOST_TEST_MESSAGE("Test that using a negative strike in the constructor causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.name, td.currency,
        td.position, td.quantity, td.maturity, -50.0), QuantLib::Error);
}

test_suite* CommodityForwardTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CommodityForwardTests");
    
    suite->add(BOOST_TEST_CASE(&CommodityForwardTest::testConstructor));
    suite->add(BOOST_TEST_CASE(&CommodityForwardTest::testIsExpired));
    suite->add(BOOST_TEST_CASE(&CommodityForwardTest::testNegativeQuantityThrows));
    suite->add(BOOST_TEST_CASE(&CommodityForwardTest::testNegativeStrikeThrows));
    
    return suite;
}

}
