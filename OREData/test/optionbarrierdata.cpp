/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/portfolio/optionbarrierdata.hpp>
#include <oret/toplevelfixture.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(OptionBarrierDataTests)

BOOST_AUTO_TEST_CASE(testDefaultConstruction) {
    BOOST_TEST_MESSAGE("Testing default construction...");

    OptionBarrierData obd;

    BOOST_CHECK(!obd.barrierType());
    BOOST_CHECK(!obd.doubleBarrierType());
    BOOST_CHECK(obd.levels().empty());
    BOOST_CHECK_EQUAL(obd.rebate(), 0.0);
}

BOOST_AUTO_TEST_CASE(testBarrierTypeConstruction) {
    BOOST_TEST_MESSAGE("Testing single barrier type construction...");

    OptionBarrierData obd(Barrier::Type::DownIn, {90}, "American", 0.3);

    BOOST_REQUIRE(obd.barrierType());
    BOOST_CHECK_EQUAL(*obd.barrierType(), Barrier::Type::DownIn);
    BOOST_CHECK(!obd.doubleBarrierType());
    BOOST_REQUIRE_EQUAL(obd.levels().size(), 1);
    BOOST_CHECK_EQUAL(obd.levels()[0], 90);
    BOOST_CHECK_EQUAL(obd.rebate(), 0.3);
}

BOOST_AUTO_TEST_CASE(testDoubleBarrierTypeConstruction) {
    BOOST_TEST_MESSAGE("Testing double barrier type construction...");

    OptionBarrierData obd(DoubleBarrier::Type::KnockOut, {90, 110}, "American");

    BOOST_REQUIRE(obd.doubleBarrierType());
    BOOST_CHECK_EQUAL(*obd.doubleBarrierType(), DoubleBarrier::Type::KnockOut);
    BOOST_CHECK(!obd.barrierType());
    BOOST_REQUIRE_EQUAL(obd.levels().size(), 2);
    BOOST_CHECK_EQUAL(obd.levels()[0], 90);
    BOOST_CHECK_EQUAL(obd.levels()[1], 110);
    BOOST_CHECK_EQUAL(obd.rebate(), 0.0);
}

BOOST_AUTO_TEST_CASE(testBarrierTypeConstructionFromXml) {
    BOOST_TEST_MESSAGE("Testing single barrier type construction based on fromXML...");

    OptionBarrierData obd;
    // XML input
    string xml;
    xml.append("<BarrierData>");
    xml.append("  <Type>UpAndIn</Type>");
    xml.append("  <Style>American</Style>");
    xml.append("  <Levels>");
    xml.append("    <Level>90</Level>");
    xml.append("  </Levels>");
    xml.append("  <Rebate>0.3</Rebate>");
    xml.append("</BarrierData>");

    // Load OptionPaymentData from XML
    obd.fromXMLString(xml);

    BOOST_REQUIRE(obd.barrierType());
    BOOST_CHECK_EQUAL(*obd.barrierType(), Barrier::Type::UpIn);
    BOOST_CHECK(!obd.doubleBarrierType());
    BOOST_CHECK_EQUAL(obd.windowStyle(), "American");
    BOOST_REQUIRE_EQUAL(obd.levels().size(), 1);
    BOOST_CHECK_EQUAL(obd.levels()[0], 90);
    BOOST_CHECK_EQUAL(obd.rebate(), 0.3);
}

BOOST_AUTO_TEST_CASE(testDoubleBarrierTypeConstructionFromXml) {
    BOOST_TEST_MESSAGE("Testing double barrier type construction based on fromXML...");

    OptionBarrierData obd;
    // XML input
    string xml;
    xml.append("<BarrierData>");
    xml.append("  <Type>KnockIn</Type>");
    xml.append("  <Levels>");
    xml.append("    <Level>90</Level>");
    xml.append("    <Level>110</Level>");
    xml.append("  </Levels>");
    xml.append("</BarrierData>");

    // Load OptionPaymentData from XML
    obd.fromXMLString(xml);

    BOOST_REQUIRE(obd.doubleBarrierType());
    BOOST_CHECK_EQUAL(*obd.doubleBarrierType(), DoubleBarrier::Type::KnockIn);
    BOOST_CHECK(!obd.barrierType());
    BOOST_CHECK_EQUAL(obd.windowStyle(), "American");
    BOOST_REQUIRE_EQUAL(obd.levels().size(), 2);
    BOOST_CHECK_EQUAL(obd.levels()[0], 90);
    BOOST_CHECK_EQUAL(obd.levels()[1], 110);
    BOOST_CHECK_EQUAL(obd.rebate(), 0.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
