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
#include <ored/configuration/correlationcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <oret/toplevelfixture.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CorrelationCurveConfigTests)

BOOST_AUTO_TEST_CASE(testParseCMSSpreadPriceQuoteCorrelationFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of correlation curve configuration from XML");

    // Create an XML string representation of the correlation curve configuration
    string configXml;
    configXml.append("<Correlation>");
    configXml.append("  <CurveId>EUR-CMS-10Y/EUR-CMS-1Y</CurveId>");
    configXml.append("  <CurveDescription>EUR CMS correlations</CurveDescription>");
    configXml.append("  <CorrelationType>CMSSpread</CorrelationType>");
    configXml.append("  <Currency>EUR</Currency>");
    configXml.append("  <Dimension>ATM</Dimension>");
    configXml.append("  <QuoteType>PRICE</QuoteType>");
    configXml.append("  <Extrapolation>true</Extrapolation>");
    configXml.append("  <Conventions>EUR-CMS-10Y-1Y-CONVENTION</Conventions>");
    configXml.append("  <SwaptionVolatility>EUR</SwaptionVolatility>");
    configXml.append("  <DiscountCurve>EUR-EONIA</DiscountCurve>");
    configXml.append("  <Calendar>TARGET</Calendar>");
    configXml.append("  <DayCounter>A365</DayCounter>");
    configXml.append("  <BusinessDayConvention>Following</BusinessDayConvention>");
    configXml.append("  <OptionTenors>1Y,2Y</OptionTenors>");
    configXml.append("  <Index1>EUR-CMS-10Y</Index1>");
    configXml.append("  <Index2>EUR-CMS-1Y</Index2>");
    configXml.append("</CorrelationCurve>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("Correlation");

    // Parse correlation curve configuration from XML
    CorrelationCurveConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {"CORRELATION/PRICE/EUR-CMS-10Y/EUR-CMS-1Y/1Y/ATM",
                             "CORRELATION/PRICE/EUR-CMS-10Y/EUR-CMS-1Y/2Y/ATM"};

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "EUR-CMS-10Y/EUR-CMS-1Y");
    BOOST_CHECK_EQUAL(config.curveDescription(), "EUR CMS correlations");
    BOOST_CHECK_EQUAL(config.index1(), "EUR-CMS-10Y");
    BOOST_CHECK_EQUAL(config.index2(), "EUR-CMS-1Y");
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());
    BOOST_CHECK_EQUAL(config.extrapolate(), true);
    BOOST_CHECK_EQUAL(config.conventions(), "EUR-CMS-10Y-1Y-CONVENTION");
    BOOST_CHECK_EQUAL(config.swaptionVolatility(), "EUR");
    BOOST_CHECK_EQUAL(config.discountCurve(), "EUR-EONIA");
    BOOST_CHECK_EQUAL(config.dayCounter().name(), "Actual/365 (Fixed)");
    BOOST_CHECK_EQUAL(config.calendar().name(), "TARGET");
    BOOST_CHECK_EQUAL(config.businessDayConvention(), BusinessDayConvention::Following);

    vector<string> p;
    p.push_back("1Y");
    p.push_back("2Y");

    BOOST_CHECK_EQUAL_COLLECTIONS(p.begin(), p.end(), config.optionTenors().begin(), config.optionTenors().end());
}

BOOST_AUTO_TEST_CASE(testParseGenericCorrelationFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of correlation curve configuration from XML");

    // Create an XML string representation of the correlation curve configuration
    string configXml;
    configXml.append("<Correlation>");
    configXml.append("  <CurveId>EUR-CMS-10Y/EUR-CMS-1Y</CurveId>");
    configXml.append("  <CurveDescription>EUR CMS correlations</CurveDescription>");
    configXml.append("  <CorrelationType>Generic</CorrelationType>");
    configXml.append("  <Dimension>ATM</Dimension>");
    configXml.append("  <QuoteType>RATE</QuoteType>");
    configXml.append("  <Extrapolation>true</Extrapolation>");
    configXml.append("  <Calendar>TARGET</Calendar>");
    configXml.append("  <DayCounter>A365</DayCounter>");
    configXml.append("  <BusinessDayConvention>Following</BusinessDayConvention>");
    configXml.append("  <OptionTenors>1Y,2Y</OptionTenors>");
    configXml.append("  <Index1/>");
    configXml.append("  <Index2/>");
    configXml.append("  <Currency/>");
    configXml.append("</CorrelationCurve>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("Correlation");

    // Parse correlation curve configuration from XML
    CorrelationCurveConfig config;
    config.fromXML(configNode);

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "EUR-CMS-10Y/EUR-CMS-1Y");
    BOOST_CHECK_EQUAL(config.curveDescription(), "EUR CMS correlations");
    BOOST_CHECK_EQUAL(config.extrapolate(), true);

    BOOST_CHECK_EQUAL(config.dayCounter().name(), "Actual/365 (Fixed)");
    BOOST_CHECK_EQUAL(config.calendar().name(), "TARGET");
    BOOST_CHECK_EQUAL(config.businessDayConvention(), BusinessDayConvention::Following);

    vector<string> p;
    p.push_back("1Y");
    p.push_back("2Y");

    BOOST_CHECK_EQUAL_COLLECTIONS(p.begin(), p.end(), config.optionTenors().begin(), config.optionTenors().end());
}

BOOST_AUTO_TEST_CASE(testParseGenericCorrelationNullQuoteFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of correlation curve configuration from XML");

    // Create an XML string representation of the correlation curve configuration
    string configXml;
    configXml.append("<Correlation>");
    configXml.append("  <CurveId>EUR-CMS-10Y/EUR-CMS-1Y</CurveId>");
    configXml.append("  <CurveDescription>EUR CMS correlations</CurveDescription>");
    configXml.append("  <CorrelationType>Generic</CorrelationType>");
    configXml.append("  <QuoteType>NULL</QuoteType>");
    configXml.append("  <Dimension/>");
    configXml.append("  <Extrapolation/>");
    configXml.append("  <Calendar>TARGET</Calendar>");
    configXml.append("  <DayCounter>A365</DayCounter>");
    configXml.append("  <BusinessDayConvention/>");
    configXml.append("  <OptionTenors/>");
    configXml.append("  <Index1/>");
    configXml.append("  <Index2/>");
    configXml.append("  <Currency/>");
    configXml.append("</CorrelationCurve>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("Correlation");

    // Parse correlation curve configuration from XML
    CorrelationCurveConfig config;
    config.fromXML(configNode);

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "EUR-CMS-10Y/EUR-CMS-1Y");
    BOOST_CHECK_EQUAL(config.curveDescription(), "EUR CMS correlations");

    BOOST_CHECK_EQUAL(config.dayCounter().name(), "Actual/365 (Fixed)");
    BOOST_CHECK_EQUAL(config.calendar().name(), "TARGET");
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
