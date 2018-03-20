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

#include <test/commodityvolcurveconfig.hpp>

#include <ored/configuration/commodityvolcurveconfig.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

namespace testsuite {

void CommodityVolatilityCurveConfigTest::testParseConstantVolFromXml() {

    BOOST_TEST_MESSAGE("Testing parsing of constant commodity vol curve configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>GOLD_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <Type>Constant</Type>");
    configXml.append("	<Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF</Quote>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityCurveConfig config;
    config.fromXML(configNode);

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "GOLD_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    BOOST_CHECK(config.type() == CommodityVolatilityCurveConfig::Type::Constant);
    BOOST_CHECK_EQUAL(config.quotes().size(), 1);
    BOOST_CHECK_EQUAL(config.quotes()[0], "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF");
    
    // Check defaults (don't matter for constant config in any case)
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.extrapolate(), true);
    BOOST_CHECK_EQUAL(config.lowerStrikeConstantExtrapolation(), false);
    BOOST_CHECK_EQUAL(config.upperStrikeConstantExtrapolation(), false);
}

void CommodityVolatilityCurveConfigTest::testParseVolCurveFromXml() {

    BOOST_TEST_MESSAGE("Testing parsing of commodity vol curve configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>GOLD_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <Type>Curve</Type>");
    configXml.append("  <Quotes>");
    configXml.append("    <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF</Quote>");
    configXml.append("    <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATMF</Quote>");
    configXml.append("    <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/10Y/ATMF</Quote>");
    configXml.append("  </Quotes>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityCurveConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATMF",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/10Y/ATMF"
    };

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "GOLD_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    BOOST_CHECK(config.type() == CommodityVolatilityCurveConfig::Type::Curve);
    BOOST_CHECK_EQUAL(config.quotes().size(), 3);
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());

    // Check defaults
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.extrapolate(), true);
    BOOST_CHECK_EQUAL(config.lowerStrikeConstantExtrapolation(), false);
    BOOST_CHECK_EQUAL(config.upperStrikeConstantExtrapolation(), false);

    // Override defaults in turn and check

    // Day counter
    XMLUtils::addChild(doc, configNode, "DayCounter", "ACT");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.dayCounter(), "ACT");

    // Calendar
    XMLUtils::addChild(doc, configNode, "Calendar", "TARGET");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.calendar(), "TARGET");

    // Extrapolation
    XMLUtils::addChild(doc, configNode, "Extrapolation", false);
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.extrapolate(), false);
}

void CommodityVolatilityCurveConfigTest::testParseVolSurfaceFromXml() {

    BOOST_TEST_MESSAGE("Testing parsing of commodity vol surface configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>WTI_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <Type>Surface</Type>");
    configXml.append("  <Surface>");
    configXml.append("    <Expiries>1Y,5Y,10Y</Expiries>");
    configXml.append("    <Strikes>30.0,40.0,60.0</Strikes>");
    configXml.append("  </Surface>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityCurveConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/30.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/40.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/60.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/30.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/40.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/60.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/30.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/40.0",
        "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/60.0"
    };

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "WTI_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    BOOST_CHECK(config.type() == CommodityVolatilityCurveConfig::Type::Surface);
    BOOST_CHECK_EQUAL(config.quotes().size(), 9);
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());

    // Check defaults
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.extrapolate(), true);
    BOOST_CHECK_EQUAL(config.lowerStrikeConstantExtrapolation(), false);
    BOOST_CHECK_EQUAL(config.upperStrikeConstantExtrapolation(), false);

    // Override defaults related to surface

    // Day counter
    XMLUtils::addChild(doc, configNode, "LowerStrikeConstantExtrapolation", true);
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.lowerStrikeConstantExtrapolation(), true);

    // Calendar
    XMLUtils::addChild(doc, configNode, "UpperStrikeConstantExtrapolation", true);
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.upperStrikeConstantExtrapolation(), true);
}

test_suite* CommodityVolatilityCurveConfigTest::suite() {
    
    test_suite* suite = BOOST_TEST_SUITE("CommodityVolatilityCurveConfigTest");

    suite->add(BOOST_TEST_CASE(&CommodityVolatilityCurveConfigTest::testParseConstantVolFromXml));
    suite->add(BOOST_TEST_CASE(&CommodityVolatilityCurveConfigTest::testParseVolCurveFromXml));
    suite->add(BOOST_TEST_CASE(&CommodityVolatilityCurveConfigTest::testParseVolSurfaceFromXml));

    return suite;
}

}
