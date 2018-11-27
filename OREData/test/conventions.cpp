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

#include <boost/test/unit_test.hpp>

#include <ored/configuration/conventions.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <ql/currencies/all.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;
using boost::algorithm::replace_all_copy;

BOOST_AUTO_TEST_SUITE(OREDataTestSuite)

BOOST_AUTO_TEST_SUITE(ConventionsTests)

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionConstruction) {

    BOOST_TEST_MESSAGE("Testing cross currency fix float convention construction");

    // Check construction raises no errors
    boost::shared_ptr<CrossCcyFixFloatSwapConvention> convention;
    BOOST_CHECK_NO_THROW(convention = boost::make_shared<CrossCcyFixFloatSwapConvention>(
        "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M"));

    // Check object
    BOOST_CHECK_EQUAL(convention->id(), "USD-TRY-XCCY-FIX-FLOAT");
    BOOST_CHECK_EQUAL(convention->settlementDays(), 2);
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), JointCalendar(UnitedStates(), UnitedKingdom(), Turkey()));
    BOOST_CHECK_EQUAL(convention->settlementConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), TRYCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), Annual);
    BOOST_CHECK_EQUAL(convention->fixedConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), Actual360());
    BOOST_CHECK_EQUAL(convention->index()->name(), "USDLibor3M Actual/360");
    BOOST_CHECK(!convention->eom());

    // Check end of month when not default
    BOOST_CHECK_NO_THROW(convention = boost::make_shared<CrossCcyFixFloatSwapConvention>(
        "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M", "false"));
    BOOST_CHECK(!convention->eom());

    BOOST_CHECK_NO_THROW(convention = boost::make_shared<CrossCcyFixFloatSwapConvention>(
        "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M", "true"));
    BOOST_CHECK(convention->eom());
}

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of cross currency fix float convention from XML");

    // XML string convention
    string xml;
    xml.append("<CrossCurrencyFixFloat>");
    xml.append("  <Id>USD-TRY-XCCY-FIX-FLOAT</Id>");
    xml.append("  <SettlementDays>2</SettlementDays>");
    xml.append("  <SettlementCalendar>US,UK,TRY</SettlementCalendar>");
    xml.append("  <SettlementConvention>F</SettlementConvention>");
    xml.append("  <FixedCurrency>TRY</FixedCurrency>");
    xml.append("  <FixedFrequency>Annual</FixedFrequency>");
    xml.append("  <FixedConvention>F</FixedConvention>");
    xml.append("  <FixedDayCounter>A360</FixedDayCounter>");
    xml.append("  <Index>USD-LIBOR-3M</Index>");
    xml.append("</CrossCurrencyFixFloat>");

    // Parse convention from XML
    boost::shared_ptr<CrossCcyFixFloatSwapConvention> convention = boost::make_shared<CrossCcyFixFloatSwapConvention>();
    BOOST_CHECK_NO_THROW(convention->fromXMLString(xml));

    // Check parsed object
    BOOST_CHECK_EQUAL(convention->id(), "USD-TRY-XCCY-FIX-FLOAT");
    BOOST_CHECK_EQUAL(convention->settlementDays(), 2);
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), JointCalendar(UnitedStates(), UnitedKingdom(), Turkey()));
    BOOST_CHECK_EQUAL(convention->settlementConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), TRYCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), Annual);
    BOOST_CHECK_EQUAL(convention->fixedConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), Actual360());
    BOOST_CHECK_EQUAL(convention->index()->name(), "USDLibor3M Actual/360");
    BOOST_CHECK(!convention->eom());

    // Check end of month also
    xml = replace_all_copy(xml, "</CrossCurrencyFixFloat>", "<EOM>false</EOM></CrossCurrencyFixFloat>");
    BOOST_TEST_MESSAGE("xml is: " << xml);
    convention->fromXMLString(xml);
    BOOST_CHECK(!convention->eom());

    xml = replace_all_copy(xml, "<EOM>false</EOM>", "<EOM>true</EOM>");
    convention->fromXMLString(xml);
    BOOST_CHECK(convention->eom());
}

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionToXml) {

    BOOST_TEST_MESSAGE("Testing writing of cross currency fix float convention to XML");

    // Construct the convention
    boost::shared_ptr<CrossCcyFixFloatSwapConvention> convention;
    BOOST_CHECK_NO_THROW(convention = boost::make_shared<CrossCcyFixFloatSwapConvention>(
        "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M"));

    // Write the convention to a string
    string xml = convention->toXMLString();

    // Read the convention back from the string using fromXMLString
    boost::shared_ptr<CrossCcyFixFloatSwapConvention> readConvention = boost::make_shared<CrossCcyFixFloatSwapConvention>();
    BOOST_CHECK_NO_THROW(readConvention->fromXMLString(xml));

    // The read convention should equal the original convention
    BOOST_CHECK_EQUAL(convention->id(), readConvention->id());
    BOOST_CHECK_EQUAL(convention->settlementDays(), readConvention->settlementDays());
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), readConvention->settlementCalendar());
    BOOST_CHECK_EQUAL(convention->settlementConvention(), readConvention->settlementConvention());
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), readConvention->fixedCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), readConvention->fixedFrequency());
    BOOST_CHECK_EQUAL(convention->fixedConvention(), readConvention->fixedConvention());
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), readConvention->fixedDayCounter());
    BOOST_CHECK_EQUAL(convention->index()->name(), readConvention->index()->name());
    BOOST_CHECK_EQUAL(convention->eom(), readConvention->eom());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
