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
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>

using namespace boost::unit_test_framework;
using namespace ore::data;
using namespace ore::analytics;

// Improve readability in this test code
typedef ShiftScenarioGenerator::ScenarioDescription SSDes;
typedef SSDes::Type SSType;
typedef RiskFactorKey RFKey;
typedef RFKey::KeyType RFType;

namespace {

// Generate ScenarioDescription objects for testing
vector<SSDes> generateDescriptions() {
    vector<SSDes> result;

    result.push_back(SSDes(SSType::Base));
    result.push_back(SSDes(SSType::Up, RFKey(RFType::DiscountCurve, "EUR", 0), "2W"));
    result.push_back(SSDes(SSType::Up, RFKey(RFType::IndexCurve, "EUR-EURIBOR-6M", 11), "30Y"));
    result.push_back(SSDes(SSType::Down, RFKey(RFType::FXSpot, "JPYUSD", 0), "spot"));
    result.push_back(SSDes(SSType::Up, RFKey(RFType::SwaptionVolatility, "USD", 22), "3M/3Y/ATM"));
    result.push_back(SSDes(SSDes(SSType::Up, RFKey(RFType::DiscountCurve, "EUR", 0), "2W"),
                           SSDes(SSType::Up, RFKey(RFType::DiscountCurve, "EUR", 1), "1M")));
    result.push_back(SSDes(SSDes(SSType::Up, RFKey(RFType::FXSpot, "JPYUSD", 0), "spot"),
                           SSDes(SSType::Up, RFKey(RFType::DiscountCurve, "USD", 1), "1M")));

    return result;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(ShiftScenarioGeneratorTest)

BOOST_AUTO_TEST_CASE(testShiftScenarioStringConstruction) {
    string strDes;
    for (const auto& des : generateDescriptions()) {
        strDes = to_string(des);
        SSDes desFromString(strDes);
        BOOST_CHECK_EQUAL(des, desFromString);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
