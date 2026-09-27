/*
 Copyright (C) 2026 Hei-MaoM
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License. You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <string>

#include <boost/test/unit_test.hpp>
#include <orea/app/inputparameters.hpp>
#include <test/oreatoplevelfixture.hpp>

using namespace ore::analytics;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(OutputParametersTest)

BOOST_AUTO_TEST_CASE(testHistoricalVarOutputFileWhenOnlyHistoricalVarActive) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "active", std::string("N"));
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "active", std::string("Y"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "histvar.csv");
}

BOOST_AUTO_TEST_CASE(testParametricVarOutputFileWhenOnlyParametricVarActive) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "active", std::string("Y"));
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "active", std::string("N"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "paramvar.csv");
}

BOOST_AUTO_TEST_CASE(testVarOutputFileKeepsParametricFallbackWithoutActiveFlags) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "paramvar.csv");
}

BOOST_AUTO_TEST_CASE(testVarOutputFileKeepsParametricFallbackWhenParametricActiveFlagIsMissing) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "active", std::string("Y"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "paramvar.csv");
}

BOOST_AUTO_TEST_CASE(testVarOutputFileKeepsHistoricalFallbackWhenHistoricalActiveFlagIsMissing) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "active", std::string("Y"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "histvar.csv");
}

BOOST_AUTO_TEST_CASE(testVarOutputFileKeepsParametricFallbackWhenBothVarActive) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "active", std::string("Y"));
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "active", std::string("Y"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "paramvar.csv");
}

BOOST_AUTO_TEST_CASE(testVarOutputFileKeepsParametricFallbackWhenBothVarInactive) {
    auto params = QuantLib::ext::make_shared<Parameters>();
    params->set("parametricVar", "active", std::string("N"));
    params->set("parametricVar", "outputFile", std::string("paramvar.csv"));
    params->set("historicalSimulationVar", "active", std::string("N"));
    params->set("historicalSimulationVar", "outputFile", std::string("histvar.csv"));

    OutputParameters output(params);

    BOOST_CHECK_EQUAL(output.outputFileName("var", "csv"), "paramvar.csv");
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
