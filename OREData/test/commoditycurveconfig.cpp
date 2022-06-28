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
#include <oret/toplevelfixture.hpp>

#include <ored/configuration/commoditycurveconfig.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityCurveConfigTests)

BOOST_AUTO_TEST_CASE(testConstructionQuotes) {

    BOOST_TEST_MESSAGE("Testing commodity curve configuration quote vector construction");

    // Main thing to check here is that the spot quote gets
    // inserted at the beginning of the vector of quotes
    string curveId = "GOLD_USD";
    string curveDescription = "Value of troy ounce of gold in USD";
    string currency = "USD";
    string commoditySpotQuote = "COMMODITY/PRICE/GOLD/USD";
    vector<string> quotes = {"COMMODITY_FWD/PRICE/GOLD/USD/2016-02-29", "COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28"};

    // Create configuration
    CommodityCurveConfig config(curveId, curveDescription, currency, quotes, commoditySpotQuote);

    // Check quotes vector from config (none of the other members have logic)
    quotes.insert(quotes.begin(), commoditySpotQuote);
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());
}

BOOST_AUTO_TEST_CASE(testParseFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity curve configuration from XML");

    // Create an XML string representation of the commodity curve configuration
    string configXml;
    configXml.append("<CommodityCurve>");
    configXml.append("  <CurveId>GOLD_USD</CurveId>");
    configXml.append("  <CurveDescription>Gold USD price curve</CurveDescription>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <SpotQuote>COMMODITY/PRICE/GOLD/USD</SpotQuote>");
    configXml.append("  <Quotes>");
    configXml.append("    <Quote>COMMODITY_FWD/PRICE/GOLD/USD/2016-02-29</Quote>");
    configXml.append("    <Quote>COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28</Quote>");
    configXml.append("  </Quotes>");
    configXml.append("  <DayCounter>A365</DayCounter>");
    configXml.append("  <InterpolationMethod>Linear</InterpolationMethod>");
    configXml.append("  <Extrapolation>true</Extrapolation>");
    configXml.append("</CommodityCurve>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityCurve");

    // Parse commodity curve configuration from XML
    CommodityCurveConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {"COMMODITY/PRICE/GOLD/USD", "COMMODITY_FWD/PRICE/GOLD/USD/2016-02-29",
                             "COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28"};

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "GOLD_USD");
    BOOST_CHECK_EQUAL(config.curveDescription(), "Gold USD price curve");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    BOOST_CHECK_EQUAL(config.commoditySpotQuoteId(), "COMMODITY/PRICE/GOLD/USD");
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());
    BOOST_CHECK_EQUAL(config.dayCountId(), "A365");
    BOOST_CHECK_EQUAL(config.interpolationMethod(), "Linear");
    BOOST_CHECK_EQUAL(config.extrapolation(), true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
