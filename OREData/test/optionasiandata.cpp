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
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ored/portfolio/optionasiandata.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(OptionAsianDataTests)

BOOST_AUTO_TEST_CASE(testOptionAsianDataDefaultConstruction) {

    BOOST_TEST_MESSAGE("Testing default construction...");

    OptionAsianData oad;

    BOOST_CHECK_EQUAL(oad.asianType(), OptionAsianData::AsianType::Price);
    BOOST_CHECK_EQUAL(oad.averageType(), Average::Type::Arithmetic);
    BOOST_CHECK_EQUAL(oad.fixingDates().size(), 0);
}

BOOST_AUTO_TEST_CASE(testOptionAsianDataStringConstruction) {

    BOOST_TEST_MESSAGE("Testing construction of OptionAsianData from a vector of string dates...");

    vector<string> strDates{"2021-04-01", "2021-04-30"};
    OptionAsianData oad(OptionAsianData::AsianType::Price, Average::Type::Arithmetic, strDates);

    vector<Date> expDates{Date(01, Apr, 2021), Date(30, Apr, 2021)};
    BOOST_CHECK_EQUAL(oad.asianType(), OptionAsianData::AsianType::Price);
    BOOST_CHECK_EQUAL(oad.averageType(), Average::Type::Arithmetic);
    BOOST_CHECK_EQUAL_COLLECTIONS(oad.fixingDates().begin(), oad.fixingDates().end(), expDates.begin(), expDates.end());
}

BOOST_AUTO_TEST_CASE(testOptionAsianDataDateConstruction) {

    BOOST_TEST_MESSAGE("Testing construction of OptionAsianData from a vector of dates...");

    vector<Date> dates{Date(01, Apr, 2021), Date(30, Apr, 2021)};
    OptionAsianData oad(OptionAsianData::AsianType::Strike, Average::Type::Geometric, dates);

    vector<Date> expDates{Date(01, Apr, 2021), Date(30, Apr, 2021)};
    BOOST_CHECK_EQUAL(oad.asianType(), OptionAsianData::AsianType::Strike);
    BOOST_CHECK_EQUAL(oad.averageType(), Average::Type::Geometric);
    BOOST_CHECK_EQUAL_COLLECTIONS(oad.fixingDates().begin(), oad.fixingDates().end(), expDates.begin(), expDates.end());
}

BOOST_AUTO_TEST_CASE(testOptionAsianDataConstructionFromXml) {

    BOOST_TEST_MESSAGE("Testing contruction of OptionAsianData from XML...");

    // XML input
    string xml;
    xml.append("<AsianData>");
    xml.append("  <AsianType>Strike</AsianType>");
    xml.append("  <AverageType>Geometric</AverageType>");
    xml.append("  <FixingDates>");
    xml.append("    <FixingDate>2021-04-01</FixingDate>");
    xml.append("    <FixingDate>2021-04-30</FixingDate>");
    xml.append("  </FixingDates>");
    xml.append("</AsianData>");

    // Load OptionPaymentData from XML
    OptionAsianData oad;
    oad.fromXMLString(xml);

    // Check is as expected
    vector<Date> expDates{Date(01, Apr, 2021), Date(30, Apr, 2021)};
    BOOST_CHECK_EQUAL(oad.asianType(), OptionAsianData::AsianType::Strike);
    BOOST_CHECK_EQUAL(oad.averageType(), Average::Type::Geometric);
    BOOST_CHECK_EQUAL_COLLECTIONS(oad.fixingDates().begin(), oad.fixingDates().end(), expDates.begin(), expDates.end());
}

BOOST_AUTO_TEST_CASE(testOptionAsianDataConstructionToXml) {

    BOOST_TEST_MESSAGE("Testing contruction of OptionAsianData to/from XML...");

    // Construct explicitly
    vector<Date> dates{Date(01, Apr, 2021), Date(30, Apr, 2021)};
    OptionAsianData inoad(OptionAsianData::AsianType::Strike, Average::Type::Geometric, dates);

    // Write to XML and read the result from XML to populate new object
    OptionAsianData outoad;
    outoad.fromXMLString(inoad.toXMLString());

    // Check is as expected
    BOOST_CHECK_EQUAL(inoad.asianType(), outoad.asianType());
    BOOST_CHECK_EQUAL(inoad.averageType(), outoad.averageType());
    BOOST_CHECK_EQUAL_COLLECTIONS(inoad.fixingDates().begin(), inoad.fixingDates().end(), outoad.fixingDates().begin(),
                                  outoad.fixingDates().end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
