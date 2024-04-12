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

#include <ored/configuration/commodityvolcurveconfig.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityVolatilityConfigTests)

BOOST_AUTO_TEST_CASE(testParseConstantVolFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of constant commodity vol curve configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>GOLD_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <Constant>");
    configXml.append("    <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd</Quote>");
    configXml.append("  </Constant>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityConfig config;
    config.fromXML(configNode);

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "GOLD_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    // Check that we have a constant volatility config.
    QuantLib::ext::shared_ptr<ConstantVolatilityConfig> vc;
    for (auto v : config.volatilityConfig()) {
        if ((vc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(v)))
            break;
    }
    BOOST_CHECK(vc);
    BOOST_CHECK_EQUAL(config.quotes().size(), 1);
    BOOST_CHECK_EQUAL(config.quotes()[0], "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd");

    // Check defaults (don't matter for constant config in any case)
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.futureConventionsId(), "");
    BOOST_CHECK_EQUAL(config.optionExpiryRollDays(), 0);
}

BOOST_AUTO_TEST_CASE(testParseVolCurveFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity vol curve configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>GOLD_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <Curve>");
    configXml.append("    <Quotes>");
    configXml.append("      <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd</Quote>");
    configXml.append("      <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATM/AtmFwd</Quote>");
    configXml.append("      <Quote>COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/10Y/ATM/AtmFwd</Quote>");
    configXml.append("    </Quotes>");
    configXml.append("    <Interpolation>Linear</Interpolation>");
    configXml.append("    <Extrapolation>Flat</Extrapolation>");
    configXml.append("  </Curve>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {"COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd",
                             "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATM/AtmFwd",
                             "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/10Y/ATM/AtmFwd"};

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "GOLD_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    // Check that we have a volatility curve config.
    QuantLib::ext::shared_ptr<VolatilityCurveConfig> vc;
    for (auto v : config.volatilityConfig()) {
        if ((vc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(v)))
            break;
    }

    BOOST_REQUIRE(vc);
    BOOST_CHECK_EQUAL(vc->interpolation(), "Linear");
    BOOST_CHECK_EQUAL(vc->extrapolation(), "Flat");
    BOOST_CHECK_EQUAL(config.quotes().size(), 3);
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());

    // Check defaults
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.futureConventionsId(), "");
    BOOST_CHECK_EQUAL(config.optionExpiryRollDays(), 0);

    // Override defaults in turn and check

    // Day counter
    XMLUtils::addChild(doc, configNode, "DayCounter", "ACT");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.dayCounter(), "ACT");

    // Calendar
    XMLUtils::addChild(doc, configNode, "Calendar", "TARGET");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.calendar(), "TARGET");

    // Future conventions Id
    XMLUtils::addChild(doc, configNode, "FutureConventions", "NYMEX:CL");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.futureConventionsId(), "NYMEX:CL");

    // Option expiry roll days
    XMLUtils::addChild(doc, configNode, "OptionExpiryRollDays", "2");
    config.fromXML(configNode);
    BOOST_CHECK_EQUAL(config.optionExpiryRollDays(), 2);
}

BOOST_AUTO_TEST_CASE(testParseVolSurfaceFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity vol surface configuration from XML");

    // Create an XML string representation of the commodity volatility curve configuration
    string configXml;
    configXml.append("<CommodityVolatility>");
    configXml.append("  <CurveId>WTI_USD_VOLS</CurveId>");
    configXml.append("  <CurveDescription/>");
    configXml.append("  <Currency>USD</Currency>");
    configXml.append("  <StrikeSurface>");
    configXml.append("    <Strikes>30.0,40.0,60.0</Strikes>");
    configXml.append("    <Expiries>1Y,5Y,10Y</Expiries>");
    configXml.append("    <TimeInterpolation>Linear</TimeInterpolation>");
    configXml.append("    <StrikeInterpolation>Linear</StrikeInterpolation>");
    configXml.append("    <Extrapolation>true</Extrapolation>");
    configXml.append("    <TimeExtrapolation>Flat</TimeExtrapolation>");
    configXml.append("    <StrikeExtrapolation>Flat</StrikeExtrapolation>");
    configXml.append("  </StrikeSurface>");
    configXml.append("</CommodityVolatility>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(configXml);
    XMLNode* configNode = doc.getFirstNode("CommodityVolatility");

    // Parse commodity volatility curve configuration from XML
    CommodityVolatilityConfig config;
    config.fromXML(configNode);

    // Expected vector of quotes
    vector<string> quotes = {"COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/30.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/40.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/1Y/60.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/30.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/40.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/5Y/60.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/30.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/40.0",
                             "COMMODITY_OPTION/RATE_LNVOL/WTI_USD_VOLS/USD/10Y/60.0"};

    // Check fields
    BOOST_CHECK_EQUAL(config.curveID(), "WTI_USD_VOLS");
    BOOST_CHECK_EQUAL(config.currency(), "USD");
    // Check that we have a volatility strike surface config.
    QuantLib::ext::shared_ptr<VolatilityStrikeSurfaceConfig> vc;
    for (auto v : config.volatilityConfig()) {
        if ((vc = QuantLib::ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(v)))
            break;
    }
    BOOST_REQUIRE(vc);
    BOOST_CHECK_EQUAL(vc->timeInterpolation(), "Linear");
    BOOST_CHECK_EQUAL(vc->strikeInterpolation(), "Linear");
    BOOST_CHECK(vc->extrapolation());
    BOOST_CHECK_EQUAL(vc->timeExtrapolation(), "Flat");
    BOOST_CHECK_EQUAL(vc->strikeExtrapolation(), "Flat");
    BOOST_CHECK_EQUAL(config.quotes().size(), 9);
    BOOST_CHECK_EQUAL_COLLECTIONS(quotes.begin(), quotes.end(), config.quotes().begin(), config.quotes().end());

    // Check defaults
    BOOST_CHECK_EQUAL(config.dayCounter(), "A365");
    BOOST_CHECK_EQUAL(config.calendar(), "NullCalendar");
    BOOST_CHECK_EQUAL(config.futureConventionsId(), "");
    BOOST_CHECK_EQUAL(config.optionExpiryRollDays(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
