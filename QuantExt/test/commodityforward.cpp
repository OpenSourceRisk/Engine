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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/currencies/america.hpp>
#include <ql/settings.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <qle/instruments/commodityforward.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {
class CommonData {
public:
    // Variables
    QuantLib::ext::shared_ptr<CommodityIndex> index;
    USDCurrency currency;
    Position::Type position;
    Real quantity;
    Date maturity;
    Real strike;

    // cleanup
    SavedSettings backup;

    // Default constructor
    CommonData() : index(QuantLib::ext::make_shared<CommoditySpotIndex>("GOLD_USD", NullCalendar())),
        currency(USDCurrency()), position(Position::Long), quantity(100), maturity(19, Feb, 2019), strike(50.0) {}
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityForwardTests)

BOOST_AUTO_TEST_CASE(testConstructor) {

    BOOST_TEST_MESSAGE("Testing commodity forward constructor");

    CommonData td;

    CommodityForward forward(td.index, td.currency, td.position, td.quantity, td.maturity, td.strike);

    BOOST_CHECK_EQUAL(forward.index()->name(), td.index->name());
    BOOST_CHECK_EQUAL(forward.currency(), td.currency);
    BOOST_CHECK_EQUAL(forward.position(), td.position);
    BOOST_CHECK_EQUAL(forward.quantity(), td.quantity);
    BOOST_CHECK_EQUAL(forward.maturityDate(), td.maturity);
    BOOST_CHECK_EQUAL(forward.strike(), td.strike);
}

BOOST_AUTO_TEST_CASE(testIsExpired) {

    BOOST_TEST_MESSAGE("Testing commodity forward expiry logic");

    CommonData td;

    CommodityForward forward(td.index, td.currency, td.position, td.quantity, td.maturity, td.strike);

    Settings::instance().evaluationDate() = td.maturity - 1 * Days;
    Settings::instance().includeReferenceDateEvents() = true;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().evaluationDate() = td.maturity;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().includeReferenceDateEvents() = false;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);
}

BOOST_AUTO_TEST_CASE(testIsExpiredCashSettledMaturityEqualsPayment) {

    BOOST_TEST_MESSAGE("Testing commodity forward expiry logic for cash-settled forward" <<
        " with payment equal to maturity");

    CommonData td;

    CommodityForward forward(td.index, td.currency, td.position, td.quantity, td.maturity, td.strike, false);

    Settings::instance().evaluationDate() = td.maturity - 1 * Days;
    Settings::instance().includeReferenceDateEvents() = true;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().evaluationDate() = td.maturity;
    BOOST_CHECK_EQUAL(forward.isExpired(), false);

    Settings::instance().includeReferenceDateEvents() = false;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);
}

BOOST_AUTO_TEST_CASE(testIsExpiredCashSettledPaymentGtMaturity) {

    BOOST_TEST_MESSAGE("Testing commodity forward expiry logic for cash-settled forward" <<
        " with payment date strictly greater than maturity date.");

    CommonData td;

    Date payment(21, Feb, 2019);
    CommodityForward forward(td.index, td.currency, td.position, td.quantity, td.maturity, td.strike,
        false, payment);

    // Check not expired right up to payment date when includeReferenceDateEvents is true.
    Settings::instance().includeReferenceDateEvents() = true;
    Date tmpDate = td.maturity - 1 * Days;
    while (tmpDate <= payment) {
        Settings::instance().evaluationDate() = tmpDate;
        BOOST_CHECK_EQUAL(forward.isExpired(), false);
        tmpDate++;
    }

    // Is expired if we set includeReferenceDateEvents to false.
    Settings::instance().includeReferenceDateEvents() = false;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);

    // Is expired always when valuation date is greater than payment.
    Settings::instance().evaluationDate() = payment + 1 * Days;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);
    Settings::instance().includeReferenceDateEvents() = true;
    BOOST_CHECK_EQUAL(forward.isExpired(), true);

}

BOOST_AUTO_TEST_CASE(testNegativeQuantityThrows) {

    BOOST_TEST_MESSAGE("Test that using a negative quantity in the constructor causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.index, td.currency, td.position, -10.0, td.maturity, td.strike),
                      QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNegativeStrikeThrows) {

    BOOST_TEST_MESSAGE("Test that using a negative strike in the constructor causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.index, td.currency, td.position, td.quantity, td.maturity, -50.0),
                      QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testPaymentDateLtMaturityCashSettledThrows) {

    BOOST_TEST_MESSAGE("Test that using a payment date less than maturity for cash settled causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.index, td.currency, td.position, td.quantity, td.maturity, -50.0,
        false, td.maturity - 1 * Days), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNonNullPaymentDatePhysicallySettledThrows) {

    BOOST_TEST_MESSAGE("Test that using a payment date for physically settled causes an exception");

    CommonData td;

    BOOST_CHECK_THROW(CommodityForward(td.index, td.currency, td.position, td.quantity, td.maturity, -50.0,
        true, td.maturity + 2 * Days), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
