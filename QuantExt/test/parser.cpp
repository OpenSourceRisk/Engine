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

#include <iostream>
#include <qle/utilities/parsers.hpp>
#include <test/parser.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {

void ParseTest::testIMMDateParsing() {
    BOOST_TEST_MESSAGE("Testing IMM Date parsing...");

    Date asof = Date(5, March, 2018);
    
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "1"), Date(21, March, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "2"), Date(20, June, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "3"), Date(19, September, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "4"), Date(19, December, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "5"), Date(20, March, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "6"), Date(19, June, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "7"), Date(18, September, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "8"), Date(18, December, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "9"), Date(18, March, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "A"), Date(17, June, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "B"), Date(16, September, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "C"), Date(16, December, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "D"), Date(17, March, 2021));

    asof = Date(20, December, 2017); //last IMM date

    BOOST_CHECK_EQUAL(parseIMMDate(asof, "1"), Date(21, March, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "2"), Date(20, June, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "3"), Date(19, September, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "4"), Date(19, December, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "5"), Date(20, March, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "6"), Date(19, June, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "7"), Date(18, September, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "8"), Date(18, December, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "9"), Date(18, March, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "A"), Date(17, June, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "B"), Date(16, September, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "C"), Date(16, December, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "D"), Date(17, March, 2021));

    //
    asof = Date(19, June, 2018);
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "1"), Date(20, June, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "2"), Date(19, September, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "3"), Date(19, December, 2018));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "4"), Date(20, March, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "5"), Date(19, June, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "6"), Date(18, September, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "7"), Date(18, December, 2019));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "8"), Date(18, March, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "9"), Date(17, June, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "A"), Date(16, September, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "B"), Date(16, December, 2020));
    BOOST_CHECK_EQUAL(parseIMMDate(asof, "C"), Date(17, March, 2021));
        
}

test_suite* ParseTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("ParseTest");
    suite->add(BOOST_TEST_CASE(&ParseTest::testIMMDateParsing));
    return suite;
}
} // namespace testsuite