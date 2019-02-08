/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/errors.hpp>
#include <ql/types.hpp>

using namespace ore::data;
using namespace boost::unit_test_framework;
using namespace std;
using namespace QuantLib;

using ore::test::TopLevelFixture;

namespace {

// Fixture used in each test case below
class F : public TopLevelFixture {
public:
    XMLDocument testDoc;

    F() {
        // dummy xml test string
        string testXML = "<root>"
                         "<level1>"
                         "<level1a>"
                         "<data1a attr = \"0.7736\">17.5</data1a>"
                         "</level1a>"
                         "<level1b>"
                         "<vector1b>"
                         "<vector1bval>12.3</vector1bval><vector1bval>45.6</vector1bval><vector1bval>78.9</vector1bval>"
                         "</vector1b>"
                         "</level1b>"
                         "</level1>"
                         "<level2>"
                         "<level2aDuplicates></level2aDuplicates>"
                         "<level2aDuplicates></level2aDuplicates>"
                         "<level2aDuplicates></level2aDuplicates>"
                         "<level2aDuplicates></level2aDuplicates>"
                         "</level2>"
                         "</root>";

        // test creation of XML document from hardcoded string
        testDoc.fromXMLString(testXML);
    }

    ~F() {}
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(XmlManipulationTests)

BOOST_FIXTURE_TEST_CASE(testXMLDataGetters, F) {

    BOOST_TEST_MESSAGE("Testing XML (scalar) data getters");

    // check that the root node is as expected
    XMLNode* root = testDoc.getFirstNode("root");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(root, "root"));

    // check that getChildNode works as expected
    XMLNode* level1 = XMLUtils::getChildNode(root, "level1");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(level1, "level1"));

    // check that getChildValue works as expected
    XMLNode* level1a = XMLUtils::getChildNode(level1, "level1a");
    string expectedStr = "17.5";
    string data1a_str = XMLUtils::getChildValue(level1a, "data1a");
    BOOST_CHECK_EQUAL(data1a_str, expectedStr);
    // ensure that cast to double works as expected
    Real expectedReal = 17.5;
    Real data1a_real = XMLUtils::getChildValueAsDouble(level1a, "data1a");
    BOOST_CHECK_EQUAL(data1a_real, expectedReal);
    // ensure that error thrown if value cast fails
    BOOST_CHECK_THROW(XMLUtils::getChildValueAsInt(level1a, "data1a"), QuantLib::Error);
    // ensure that error thrown if mandatory element is not found
    BOOST_CHECK_THROW(XMLUtils::getChildValue(level1a, "data1b", true), QuantLib::Error);
    // ensure that no error thrown if element is not mandatory
    BOOST_CHECK_NO_THROW(XMLUtils::getChildValue(level1a, "data1b", false));
    // test that default value of mandatory is false
    BOOST_CHECK_NO_THROW(XMLUtils::getChildValue(level1a, "data1b"));
    // test getNodeValue
    XMLNode* data1a = XMLUtils::getChildNode(level1a, "data1a");
    BOOST_CHECK(data1a);
    // test getAttributeValue
    string expAttribVal = "0.7736";
    string attribVal = XMLUtils::getAttribute(data1a, "attr");
    BOOST_CHECK_EQUAL(attribVal, expAttribVal);
    BOOST_CHECK_NO_THROW(XMLUtils::getAttribute(data1a, "garbagename"));
}

BOOST_FIXTURE_TEST_CASE(testXMLVectorDataGetters, F) {

    BOOST_TEST_MESSAGE("Testing XML vector data getters");

    // check that the root node is as expected
    XMLNode* root = testDoc.getFirstNode("root");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(root, "root"));

    // check that getChildNode works as expected
    XMLNode* level1 = XMLUtils::getChildNode(root, "level1");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(level1, "level1"));

    // test getChildrenValues
    XMLNode* level1b = XMLUtils::getChildNode(level1, "level1b");
    XMLNode* vector1b = XMLUtils::getChildNode(level1b, "vector1b");
    vector<string> expVecStr = {"12.3", "45.6", "78.9"};
    vector<Real> expVecReal;
    for (auto s : expVecStr)
        expVecReal.push_back(boost::lexical_cast<Real>(s));
    vector<string> vec1b_str = XMLUtils::getChildrenValues(level1b, "vector1b", "vector1bval");
    BOOST_CHECK_EQUAL_COLLECTIONS(vec1b_str.begin(), vec1b_str.end(), expVecStr.begin(), expVecStr.end());
    vector<Real> vec1bReal = XMLUtils::getChildrenValuesAsDoubles(level1b, "vector1b", "vector1bval");
    BOOST_CHECK_EQUAL_COLLECTIONS(vec1bReal.begin(), vec1bReal.end(), expVecReal.begin(), expVecReal.end());
    // test getChildrenNodes
    vector<XMLNode*> vecNodes = XMLUtils::getChildrenNodes(vector1b, "vector1bval");
    vector<string> manualVectorString;
    for (auto c : vecNodes)
        manualVectorString.push_back(XMLUtils::getNodeValue(c));
    BOOST_CHECK_EQUAL_COLLECTIONS(manualVectorString.begin(), manualVectorString.end(), expVecStr.begin(),
                                  expVecStr.end());
}

BOOST_FIXTURE_TEST_CASE(testXMLDataSetters, F) {

    BOOST_TEST_MESSAGE("Testing XML data setters");

    // check that the root node is as expected
    XMLNode* root = testDoc.getFirstNode("root");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(root, "root"));

    string newNodeVal = "value17.3";
    XMLUtils::addChild(testDoc, root, "NewNode", newNodeVal);

    string newNodeValCheck = XMLUtils::getChildValue(root, "NewNode", true);
    BOOST_CHECK_EQUAL(newNodeValCheck, newNodeVal);

    vector<Real> nodesVec = {11.1, 22.2, 33.4, 55.6};
    XMLUtils::addChildren(testDoc, root, "nodeContainingVector", "vectorElement", nodesVec);

    vector<Real> nodesVecCheck =
        XMLUtils::getChildrenValuesAsDoubles(root, "nodeContainingVector", "vectorElement", true);
    BOOST_CHECK_EQUAL_COLLECTIONS(nodesVecCheck.begin(), nodesVecCheck.end(), nodesVec.begin(), nodesVec.end());
}

BOOST_FIXTURE_TEST_CASE(testXMLAttributes, F) {

    BOOST_TEST_MESSAGE("Testing XML attributes");

    // check that the root node is as expected
    XMLNode* root = testDoc.getFirstNode("root");
    BOOST_CHECK_NO_THROW(XMLUtils::checkNode(root, "root"));

    // check that getChildNode works as expected
    XMLNode* level1 = XMLUtils::getChildNode(root, "level1");
    XMLNode* level1a = XMLUtils::getChildNode(level1, "level1a");
    XMLNode* data1a = XMLUtils::getChildNode(level1a, "data1a");

    // test getAttributeValue
    string attrExpVal = "0.7736";
    string attrVal = XMLUtils::getAttribute(data1a, "attr");
    BOOST_CHECK_EQUAL(attrVal, attrExpVal);

    // test add attribute
    string level1aAttrName = "level1aAttrName";
    string level1aAttrVal = "14.2";
    XMLUtils::addAttribute(testDoc, level1a, level1aAttrName, level1aAttrVal);
    string level1aAttrValExract = XMLUtils::getAttribute(level1a, level1aAttrName);
    BOOST_CHECK_EQUAL(level1aAttrValExract, level1aAttrVal);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
