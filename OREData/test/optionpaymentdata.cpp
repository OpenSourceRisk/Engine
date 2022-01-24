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

#include <boost/test/unit_test.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ored/portfolio/optionpaymentdata.hpp>
#include <ql/time/calendars/unitedstates.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(OptionPaymentDataTests)

BOOST_AUTO_TEST_CASE(testDefaultConstruction) {

    BOOST_TEST_MESSAGE("Testing default construction...");

    OptionPaymentData opd;

    BOOST_CHECK(!opd.rulesBased());
    BOOST_CHECK(opd.dates().empty());
    BOOST_CHECK_EQUAL(opd.lag(), 0);
    BOOST_CHECK_EQUAL(opd.calendar(), Calendar());
    BOOST_CHECK_EQUAL(opd.convention(), Following);
    BOOST_CHECK_EQUAL(opd.relativeTo(), OptionPaymentData::RelativeTo::Expiry);
}

BOOST_AUTO_TEST_CASE(testDatesBasedConstruction) {

    BOOST_TEST_MESSAGE("Testing dates based construction...");

    vector<string> strDates{"2020-06-08", "2020-09-08"};
    OptionPaymentData opd(strDates);

    vector<Date> expDates{Date(8, Jun, 2020), Date(8, Sep, 2020)};
    BOOST_CHECK(!opd.rulesBased());
    BOOST_CHECK_EQUAL_COLLECTIONS(opd.dates().begin(), opd.dates().end(), expDates.begin(), expDates.end());
    BOOST_CHECK_EQUAL(opd.lag(), 0);
    BOOST_CHECK_EQUAL(opd.calendar(), Calendar());
    BOOST_CHECK_EQUAL(opd.convention(), Following);
    BOOST_CHECK_EQUAL(opd.relativeTo(), OptionPaymentData::RelativeTo::Expiry);
}

BOOST_AUTO_TEST_CASE(testDatesBasedFromXml) {

    BOOST_TEST_MESSAGE("Testing dates based fromXML...");

    // XML input
    string xml;
    xml.append("<PaymentData>");
    xml.append("  <Dates>");
    xml.append("    <Date>2020-06-08</Date>");
    xml.append("    <Date>2020-09-08</Date>");
    xml.append("  </Dates>");
    xml.append("</PaymentData>");

    // Load OptionPaymentData from XML
    OptionPaymentData opd;
    opd.fromXMLString(xml);

    // Check is as expected.
    vector<Date> expDates{Date(8, Jun, 2020), Date(8, Sep, 2020)};
    BOOST_CHECK(!opd.rulesBased());
    BOOST_CHECK_EQUAL_COLLECTIONS(opd.dates().begin(), opd.dates().end(), expDates.begin(), expDates.end());
    BOOST_CHECK_EQUAL(opd.lag(), 0);
    BOOST_CHECK_EQUAL(opd.calendar(), Calendar());
    BOOST_CHECK_EQUAL(opd.convention(), Following);
    BOOST_CHECK_EQUAL(opd.relativeTo(), OptionPaymentData::RelativeTo::Expiry);
}

BOOST_AUTO_TEST_CASE(testDatesBasedToXml) {

    BOOST_TEST_MESSAGE("Testing dates based toXML...");

    // Construct explicitly
    vector<string> strDates{"2020-06-08", "2020-09-08"};
    OptionPaymentData inOpd(strDates);

    // Write to XML and read the result from XML to populate new object
    OptionPaymentData outOpd;
    outOpd.fromXMLString(inOpd.toXMLString());

    // Check is as expected.
    BOOST_CHECK(!outOpd.rulesBased());
    BOOST_CHECK_EQUAL_COLLECTIONS(outOpd.dates().begin(), outOpd.dates().end(), inOpd.dates().begin(),
                                  inOpd.dates().end());
    BOOST_CHECK_EQUAL(outOpd.lag(), 0);
    BOOST_CHECK_EQUAL(outOpd.calendar(), Calendar());
    BOOST_CHECK_EQUAL(outOpd.convention(), Following);
    BOOST_CHECK_EQUAL(outOpd.relativeTo(), OptionPaymentData::RelativeTo::Expiry);
}

BOOST_AUTO_TEST_CASE(testRulesBasedConstruction) {

    BOOST_TEST_MESSAGE("Testing rules based construction...");

    OptionPaymentData opd("5", "USD", "Following", "Exercise");

    BOOST_CHECK(opd.rulesBased());
    BOOST_CHECK(opd.dates().empty());
    BOOST_CHECK_EQUAL(opd.lag(), 5);
    BOOST_CHECK_EQUAL(opd.calendar(), UnitedStates(UnitedStates::Settlement));
    BOOST_CHECK_EQUAL(opd.convention(), Following);
    BOOST_CHECK_EQUAL(opd.relativeTo(), OptionPaymentData::RelativeTo::Exercise);
}

BOOST_AUTO_TEST_CASE(testRulesBasedFromXml) {

    BOOST_TEST_MESSAGE("Testing rules based fromXML...");

    // XML input
    string xml;
    xml.append("<PaymentData>");
    xml.append("  <Rules>");
    xml.append("    <Lag>3</Lag>");
    xml.append("    <Calendar>US</Calendar>");
    xml.append("    <Convention>ModifiedFollowing</Convention>");
    xml.append("    <RelativeTo>Expiry</RelativeTo>");
    xml.append("  </Rules>");
    xml.append("</PaymentData>");

    // Load OptionPaymentData from XML
    OptionPaymentData opd;
    opd.fromXMLString(xml);

    // Check is as expected.
    BOOST_CHECK(opd.rulesBased());
    BOOST_CHECK(opd.dates().empty());
    BOOST_CHECK_EQUAL(opd.lag(), 3);
    BOOST_CHECK_EQUAL(opd.calendar(), UnitedStates(UnitedStates::Settlement));
    BOOST_CHECK_EQUAL(opd.convention(), ModifiedFollowing);
    BOOST_CHECK_EQUAL(opd.relativeTo(), OptionPaymentData::RelativeTo::Expiry);
}

BOOST_AUTO_TEST_CASE(testRulesBasedToXml) {

    BOOST_TEST_MESSAGE("Testing rules based toXML...");

    // Construct explicitly
    OptionPaymentData inOpd("3", "USD", "ModifiedFollowing", "Exercise");

    // Write to XML and read the result from XML to populate new object
    OptionPaymentData outOpd;
    outOpd.fromXMLString(inOpd.toXMLString());

    // Check is as expected.
    BOOST_CHECK(outOpd.rulesBased());
    BOOST_CHECK(outOpd.dates().empty());
    BOOST_CHECK_EQUAL(outOpd.lag(), 3);
    BOOST_CHECK_EQUAL(outOpd.calendar(), UnitedStates(UnitedStates::Settlement));
    BOOST_CHECK_EQUAL(outOpd.convention(), ModifiedFollowing);
    BOOST_CHECK_EQUAL(outOpd.relativeTo(), OptionPaymentData::RelativeTo::Exercise);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
