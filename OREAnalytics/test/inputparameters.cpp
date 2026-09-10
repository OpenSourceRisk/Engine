/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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
#include <test/oreatoplevelfixture.hpp>

#include <orea/app/inputparameters.hpp>
#include <ored/utilities/parsers.hpp>

using namespace ore::analytics;
using namespace ore::data;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(InputParametersTest)

BOOST_AUTO_TEST_CASE(testCreditMigrationAnalyticSetter) {
    BOOST_TEST_MESSAGE("Testing that setCreditMigrationAnalytic sets the creditMigration flag and nothing else");

    auto inputs = QuantLib::ext::make_shared<InputParameters>();
    inputs->setCreditMigrationAnalytic(true);

    bool creditMigration = false;
    BOOST_CHECK(inputs->loadParameter<bool>(creditMigration, "xva", "creditMigration", false, parseBool));
    BOOST_CHECK(creditMigration);

    QuantLib::Real riskWeight = 0.05;
    BOOST_CHECK_NO_THROW(
        inputs->loadParameter<QuantLib::Real>(riskWeight, "xva", "kvaTheirCvaRiskWeight", false, parseReal));
    BOOST_CHECK_EQUAL(riskWeight, 0.05);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
