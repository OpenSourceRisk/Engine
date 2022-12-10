/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/curvespecparser.hpp>

#include <oret/toplevelfixture.hpp>

using std::string;

using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

namespace {
 
void checkCurveSpec(const string& spec, CurveSpec::CurveType curveType, const string& curveId) {
    auto curveSpec = parseCurveSpec(spec);

    BOOST_CHECK_EQUAL(curveSpec->baseType(), curveType);
    BOOST_CHECK_EQUAL(curveSpec->curveConfigID(), curveId);
}

}

// CurveSpecParser test

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CurveSpecParserTests) 

BOOST_AUTO_TEST_CASE(testCurveSpecParsing) {
    checkCurveSpec("Equity/USD/.SPX", CurveSpec::CurveType::Equity, ".SPX");
    checkCurveSpec("Equity/USD/BBG:BRK\\/B UN Equity", CurveSpec::CurveType::Equity, "BBG:BRK/B UN Equity");
    checkCurveSpec("Yield/USD/USD-FedFunds", CurveSpec::CurveType::Yield, "USD-FedFunds");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
