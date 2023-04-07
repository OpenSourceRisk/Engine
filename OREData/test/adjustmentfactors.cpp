/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
 */

#include <boost/test/unit_test.hpp>
#include <orepbase/ored/marketdata/adjustmentfactors.hpp>
#include <oret/toplevelfixture.hpp>

using namespace ore::data;
using namespace oreplus::data;
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(OREDataPlusTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AdjustmentFactorsTest)

BOOST_AUTO_TEST_CASE(testAdjustmentFactors) {
    BOOST_TEST_MESSAGE("Testing AdjustmentFactors...");

    Date asof(28, September, 2018);

    AdjustmentFactors adjFactors(asof);
    adjFactors.addFactor("Equity1", Date(1, January, 2010), 0.5);
    adjFactors.addFactor("Equity1", Date(8, November, 2013), 5);
    adjFactors.addFactor("Equity1", Date(14, October, 2017), 0.1);

    BOOST_CHECK_EQUAL(adjFactors.getFactor("Equity1", Date(10, December, 2009)), 0.25);
    BOOST_CHECK_EQUAL(adjFactors.getFactor("Equity1", Date(12, February, 2012)), 0.5);
    BOOST_CHECK_EQUAL(adjFactors.getFactor("Equity1", Date(6, October, 2015)), 0.10);
    BOOST_CHECK_EQUAL(adjFactors.getFactor("Equity1", Date(27, September, 2018)), 1.0);
}

BOOST_AUTO_TEST_CASE(testAdjustmentFactorsFromXml) {
    BOOST_TEST_MESSAGE("Testing parsing of Adjustment Factors from XML");

    // Create an XML string representation of the Adjustment Factors curve configuration
    string factorsXml;
    factorsXml.append("<AdditionalData>");
    factorsXml.append("  <AdjustmentFactors>");
    factorsXml.append("    <AdjustmentFactor>");
    factorsXml.append("      <Date>2018-09-28</Date>");
    factorsXml.append("      <Quote>Equity1</Quote>");
    factorsXml.append("      <Factor>0.5</Factor>");
    factorsXml.append("    </AdjustmentFactor>");
    factorsXml.append("  </AdjustmentFactors>");
    factorsXml.append("</AdditionalData>");

    // Create the XMLNode
    XMLDocument doc;
    doc.fromXMLString(factorsXml);
    XMLNode* factorsNode = doc.getFirstNode("AdditionalData");

    Date asof(30, September, 2018);

    // Parse Adjustment Factors curve configuration from XML
    AdjustmentFactors adjFactors(asof);
    adjFactors.fromXML(factorsNode);

    // Check fields
    BOOST_CHECK_EQUAL(adjFactors.getFactor("Equity1", Date(27, September, 2018)), 0.5);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
