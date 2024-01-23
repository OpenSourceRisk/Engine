/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <boost/timer/timer.hpp>

#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/asttoscriptconverter.hpp>
#include <ored/scripting/randomastgenerator.hpp>
#include <oret/toplevelfixture.hpp>

#include <iostream>
#include <iomanip>

using namespace ore::data;

namespace {
struct TestDatum {
    std::string label, script, expectedAST;
};

std::ostream& operator<<(std::ostream& os, const TestDatum& testDatum) { return os << "[" << testDatum.label << "]"; }

TestDatum scriptData[] = {
    {"assignment number", "x=1;",
     "Sequence\n"
     "  Assignment\n"
     "    Variable(x)\n"
     "      -\n"
     "    ConstantNumber(1.000000)\n"},

    {"assignment number", "x=1.2;",
     "Sequence\n"
     "  Assignment\n"
     "    Variable(x)\n"
     "      -\n"
     "    ConstantNumber(1.200000)\n"},

    {"european option",
     "Option = Quantity * PAY(max( PutCall * (Underlying(Expiry) - Strike), 0 ),\n"
     "                        Expiry, Settlement, PayCcy);",
     "Sequence\n"
     "  Assignment\n"
     "    Variable(Option)\n"
     "      -\n"
     "    OperatorMultiply\n"
     "      Variable(Quantity)\n"
     "        -\n"
     "      FunctionPay\n"
     "        FunctionMax\n"
     "          OperatorMultiply\n"
     "            Variable(PutCall)\n"
     "              -\n"
     "            OperatorMinus\n"
     "              VarEvaluation\n"
     "                Variable(Underlying)\n"
     "                  -\n"
     "                Variable(Expiry)\n"
     "                  -\n"
     "                -\n"
     "              Variable(Strike)\n"
     "                -\n"
     "          ConstantNumber(0.000000)\n"
     "        Variable(Expiry)\n"
     "          -\n"
     "        Variable(Settlement)\n"
     "          -\n"
     "        Variable(PayCcy)\n"
     "          -\n"},

    {"american window option",
     "NUMBER Exercise, Continuation;\n"
     "NUMBER ExerciseIndex;\n"
     "FOR i IN (SIZE(Expiry), 1, -1) DO\n"
     "    Exercise = PAY( max(PutCall * (Underlying(Expiry[i]) - Strike), 0 ),\n"
     "                    Expiry[i], Settlement[i], PayCcy );\n"
     "    IF exercise > NPV( continuation, Expiry[i] ) THEN\n"
     "        Continuation = Exercise;\n"
     "        ExerciseIndex = i;\n"
     "    END;\n"
     "END;\n"
     "Option = Quantity * PAY(Continuation,\n"
     "                        Expiry[exerciseIndex], Settlement[exericseIndex], PayCcy);\n",
     "Sequence\n"
     "  DeclarationNumber\n"
     "    Variable(Exercise)\n"
     "      -\n"
     "    Variable(Continuation)\n"
     "      -\n"
     "  DeclarationNumber\n"
     "    Variable(ExerciseIndex)\n"
     "      -\n"
     "  Loop(i)\n"
     "    Size(Expiry)\n"
     "    ConstantNumber(1.000000)\n"
     "    ConstantNumber(-1.000000)\n"
     "    Sequence\n"
     "      Assignment\n"
     "        Variable(Exercise)\n"
     "          -\n"
     "        FunctionPay\n"
     "          FunctionMax\n"
     "            OperatorMultiply\n"
     "              Variable(PutCall)\n"
     "                -\n"
     "              OperatorMinus\n"
     "                VarEvaluation\n"
     "                  Variable(Underlying)\n"
     "                    -\n"
     "                  Variable(Expiry)\n"
     "                    Variable(i)\n"
     "                      -\n"
     "                  -\n"
     "                Variable(Strike)\n"
     "                  -\n"
     "            ConstantNumber(0.000000)\n"
     "          Variable(Expiry)\n"
     "            Variable(i)\n"
     "              -\n"
     "          Variable(Settlement)\n"
     "            Variable(i)\n"
     "              -\n"
     "          Variable(PayCcy)\n"
     "            -\n"
     "      IfThenElse\n"
     "        ConditionGt\n"
     "          Variable(exercise)\n"
     "            -\n"
     "          FunctionNpv\n"
     "            Variable(continuation)\n"
     "              -\n"
     "            Variable(Expiry)\n"
     "              Variable(i)\n"
     "                -\n"
     "            -\n"
     "            -\n"
     "            -\n"
     "        Sequence\n"
     "          Assignment\n"
     "            Variable(Continuation)\n"
     "              -\n"
     "            Variable(Exercise)\n"
     "              -\n"
     "          Assignment\n"
     "            Variable(ExerciseIndex)\n"
     "              -\n"
     "            Variable(i)\n"
     "              -\n"
     "        -\n"
     "  Assignment\n"
     "    Variable(Option)\n"
     "      -\n"
     "    OperatorMultiply\n"
     "      Variable(Quantity)\n"
     "        -\n"
     "      FunctionPay\n"
     "        Variable(Continuation)\n"
     "          -\n"
     "        Variable(Expiry)\n"
     "          Variable(exerciseIndex)\n"
     "            -\n"
     "        Variable(Settlement)\n"
     "          Variable(exericseIndex)\n"
     "            -\n"
     "        Variable(PayCcy)\n"
     "          -\n"},

    {"volatility swap",
     "NUMBER variance;\n"
     "FOR i IN (2, SIZE(Observation), 1) DO\n"
     "variance = variance + pow(\n"
     "              ln( Underlying(Observation[i]) / Underlying(Observation[i-1]) ), 2);\n"
     "END;\n"
     "Option = Notional * PAY( pow(variance / SIZE(Observation), 0.5) - Strike,\n"
     "                         Observation[SIZE(Observation)], Payment, PayCcy);\n",
     "Sequence\n"
     "  DeclarationNumber\n"
     "    Variable(variance)\n"
     "      -\n"
     "  Loop(i)\n"
     "    ConstantNumber(2.000000)\n"
     "    Size(Observation)\n"
     "    ConstantNumber(1.000000)\n"
     "    Sequence\n"
     "      Assignment\n"
     "        Variable(variance)\n"
     "          -\n"
     "        OperatorPlus\n"
     "          Variable(variance)\n"
     "            -\n"
     "          FunctionPow\n"
     "            FunctionLog\n"
     "              OperatorDivide\n"
     "                VarEvaluation\n"
     "                  Variable(Underlying)\n"
     "                    -\n"
     "                  Variable(Observation)\n"
     "                    Variable(i)\n"
     "                      -\n"
     "                  -\n"
     "                VarEvaluation\n"
     "                  Variable(Underlying)\n"
     "                    -\n"
     "                  Variable(Observation)\n"
     "                    OperatorMinus\n"
     "                      Variable(i)\n"
     "                        -\n"
     "                      ConstantNumber(1.000000)\n"
     "                  -\n"
     "            ConstantNumber(2.000000)\n"
     "  Assignment\n"
     "    Variable(Option)\n"
     "      -\n"
     "    OperatorMultiply\n"
     "      Variable(Notional)\n"
     "        -\n"
     "      FunctionPay\n"
     "        OperatorMinus\n"
     "          FunctionPow\n"
     "            OperatorDivide\n"
     "              Variable(variance)\n"
     "                -\n"
     "              Size(Observation)\n"
     "            ConstantNumber(0.500000)\n"
     "          Variable(Strike)\n"
     "            -\n"
     "        Variable(Observation)\n"
     "          Size(Observation)\n"
     "        Variable(Payment)\n"
     "          -\n"
     "        Variable(PayCcy)\n"
     "          -\n"},

    {"cliquet option",
     "NUMBER i;\n"
     "FOR i IN (2, SIZE(Valuation), 1) DO\n"
     "P = P + max( min( Underlying(Valuation[i]) / Underlying(Valuation[i-1]) - 1,\n"
     "                  localCap),\n"
     "             localFloor);\n"
     "END;\n"
     "Option = PAY(Notional * max( min(P, globalCap) , globalFloor),\n"
     "             Valuation[SIZE(Valuation)], Settlement, PayCcy);\n",
     "Sequence\n"
     "  DeclarationNumber\n"
     "    Variable(i)\n"
     "      -\n"
     "  Loop(i)\n"
     "    ConstantNumber(2.000000)\n"
     "    Size(Valuation)\n"
     "    ConstantNumber(1.000000)\n"
     "    Sequence\n"
     "      Assignment\n"
     "        Variable(P)\n"
     "          -\n"
     "        OperatorPlus\n"
     "          Variable(P)\n"
     "            -\n"
     "          FunctionMax\n"
     "            FunctionMin\n"
     "              OperatorMinus\n"
     "                OperatorDivide\n"
     "                  VarEvaluation\n"
     "                    Variable(Underlying)\n"
     "                      -\n"
     "                    Variable(Valuation)\n"
     "                      Variable(i)\n"
     "                        -\n"
     "                    -\n"
     "                  VarEvaluation\n"
     "                    Variable(Underlying)\n"
     "                      -\n"
     "                    Variable(Valuation)\n"
     "                      OperatorMinus\n"
     "                        Variable(i)\n"
     "                          -\n"
     "                        ConstantNumber(1.000000)\n"
     "                    -\n"
     "                ConstantNumber(1.000000)\n"
     "              Variable(localCap)\n"
     "                -\n"
     "            Variable(localFloor)\n"
     "              -\n"
     "  Assignment\n"
     "    Variable(Option)\n"
     "      -\n"
     "    FunctionPay\n"
     "      OperatorMultiply\n"
     "        Variable(Notional)\n"
     "          -\n"
     "        FunctionMax\n"
     "          FunctionMin\n"
     "            Variable(P)\n"
     "              -\n"
     "            Variable(globalCap)\n"
     "              -\n"
     "          Variable(globalFloor)\n"
     "            -\n"
     "      Variable(Valuation)\n"
     "        Size(Valuation)\n"
     "      Variable(Settlement)\n"
     "        -\n"
     "      Variable(PayCcy)\n"
     "        -\n"},

    {"autocallable",
     "NUMBER StrikePrice, KnockInPrice, Value;\n"
     "NUMBER terminated, knockedIn, u, v;\n"
     "FOR u IN (1, SIZE(Underlying), 1) DO\n"
     "    StrikePrice = StrikePrice + Underlying[u](StrikeDate);\n"
     "END;\n"
     "StrikePrice = StrikePrice / SIZE(Underlying);\n"
     "KnockInPrice = KnockInRatio * StrikePrice;\n"
     "FOR v IN (1, SIZE(Valuation), 1) DO\n"
     "    IF v == SIZE(Valuation) AND knockedIn == 1 AND terminated == 0 THEN\n"
     "        Option = PAY(Notional * ( 1 - Value / StrikePrice), Valuation[v], Settlement[v], PayCcy);\n"
     "    ELSE \n"
     "        IF v > 2 AND terminated == 0 THEN\n"
     "            Value = 0;\n"
     "            FOR u IN (1, SIZE(Underlying), 1) DO\n"
     "                Value = Value + Underlying[u](Valuation[v]);\n"
     "            END;\n"
     "            Value = Value / SIZE(Underlying);\n"
     "            IF Value > StrikePrice THEN\n"
     "                Option = PAY (Notional * v * 0.06, Valuation[v], Settlement[v], PayCcy);\n"
     "                terminated = 1;\n"
     "            ELSE\n"
     "                IF Value < KnockInPrice THEN\n"
     "                   knockedIn = 1;\n"
     "                END;\n"
     "            END;\n"
     "        END;\n"
     "    END;\n"
     "END;\n",
     "Sequence\n"
     "  DeclarationNumber\n"
     "    Variable(StrikePrice)\n"
     "      -\n"
     "    Variable(KnockInPrice)\n"
     "      -\n"
     "    Variable(Value)\n"
     "      -\n"
     "  DeclarationNumber\n"
     "    Variable(terminated)\n"
     "      -\n"
     "    Variable(knockedIn)\n"
     "      -\n"
     "    Variable(u)\n"
     "      -\n"
     "    Variable(v)\n"
     "      -\n"
     "  Loop(u)\n"
     "    ConstantNumber(1.000000)\n"
     "    Size(Underlying)\n"
     "    ConstantNumber(1.000000)\n"
     "    Sequence\n"
     "      Assignment\n"
     "        Variable(StrikePrice)\n"
     "          -\n"
     "        OperatorPlus\n"
     "          Variable(StrikePrice)\n"
     "            -\n"
     "          VarEvaluation\n"
     "            Variable(Underlying)\n"
     "              Variable(u)\n"
     "                -\n"
     "            Variable(StrikeDate)\n"
     "              -\n"
     "            -\n"
     "  Assignment\n"
     "    Variable(StrikePrice)\n"
     "      -\n"
     "    OperatorDivide\n"
     "      Variable(StrikePrice)\n"
     "        -\n"
     "      Size(Underlying)\n"
     "  Assignment\n"
     "    Variable(KnockInPrice)\n"
     "      -\n"
     "    OperatorMultiply\n"
     "      Variable(KnockInRatio)\n"
     "        -\n"
     "      Variable(StrikePrice)\n"
     "        -\n"
     "  Loop(v)\n"
     "    ConstantNumber(1.000000)\n"
     "    Size(Valuation)\n"
     "    ConstantNumber(1.000000)\n"
     "    Sequence\n"
     "      IfThenElse\n"
     "        ConditionAnd\n"
     "          ConditionAnd\n"
     "            ConditionEq\n"
     "              Variable(v)\n"
     "                -\n"
     "              Size(Valuation)\n"
     "            ConditionEq\n"
     "              Variable(knockedIn)\n"
     "                -\n"
     "              ConstantNumber(1.000000)\n"
     "          ConditionEq\n"
     "            Variable(terminated)\n"
     "              -\n"
     "            ConstantNumber(0.000000)\n"
     "        Sequence\n"
     "          Assignment\n"
     "            Variable(Option)\n"
     "              -\n"
     "            FunctionPay\n"
     "              OperatorMultiply\n"
     "                Variable(Notional)\n"
     "                  -\n"
     "                OperatorMinus\n"
     "                  ConstantNumber(1.000000)\n"
     "                  OperatorDivide\n"
     "                    Variable(Value)\n"
     "                      -\n"
     "                    Variable(StrikePrice)\n"
     "                      -\n"
     "              Variable(Valuation)\n"
     "                Variable(v)\n"
     "                  -\n"
     "              Variable(Settlement)\n"
     "                Variable(v)\n"
     "                  -\n"
     "              Variable(PayCcy)\n"
     "                -\n"
     "        Sequence\n"
     "          IfThenElse\n"
     "            ConditionAnd\n"
     "              ConditionGt\n"
     "                Variable(v)\n"
     "                  -\n"
     "                ConstantNumber(2.000000)\n"
     "              ConditionEq\n"
     "                Variable(terminated)\n"
     "                  -\n"
     "                ConstantNumber(0.000000)\n"
     "            Sequence\n"
     "              Assignment\n"
     "                Variable(Value)\n"
     "                  -\n"
     "                ConstantNumber(0.000000)\n"
     "              Loop(u)\n"
     "                ConstantNumber(1.000000)\n"
     "                Size(Underlying)\n"
     "                ConstantNumber(1.000000)\n"
     "                Sequence\n"
     "                  Assignment\n"
     "                    Variable(Value)\n"
     "                      -\n"
     "                    OperatorPlus\n"
     "                      Variable(Value)\n"
     "                        -\n"
     "                      VarEvaluation\n"
     "                        Variable(Underlying)\n"
     "                          Variable(u)\n"
     "                            -\n"
     "                        Variable(Valuation)\n"
     "                          Variable(v)\n"
     "                            -\n"
     "                        -\n"
     "              Assignment\n"
     "                Variable(Value)\n"
     "                  -\n"
     "                OperatorDivide\n"
     "                  Variable(Value)\n"
     "                    -\n"
     "                  Size(Underlying)\n"
     "              IfThenElse\n"
     "                ConditionGt\n"
     "                  Variable(Value)\n"
     "                    -\n"
     "                  Variable(StrikePrice)\n"
     "                    -\n"
     "                Sequence\n"
     "                  Assignment\n"
     "                    Variable(Option)\n"
     "                      -\n"
     "                    FunctionPay\n"
     "                      OperatorMultiply\n"
     "                        OperatorMultiply\n"
     "                          Variable(Notional)\n"
     "                            -\n"
     "                          Variable(v)\n"
     "                            -\n"
     "                        ConstantNumber(0.060000)\n"
     "                      Variable(Valuation)\n"
     "                        Variable(v)\n"
     "                          -\n"
     "                      Variable(Settlement)\n"
     "                        Variable(v)\n"
     "                          -\n"
     "                      Variable(PayCcy)\n"
     "                        -\n"
     "                  Assignment\n"
     "                    Variable(terminated)\n"
     "                      -\n"
     "                    ConstantNumber(1.000000)\n"
     "                Sequence\n"
     "                  IfThenElse\n"
     "                    ConditionLt\n"
     "                      Variable(Value)\n"
     "                        -\n"
     "                      Variable(KnockInPrice)\n"
     "                        -\n"
     "                    Sequence\n"
     "                      Assignment\n"
     "                        Variable(knockedIn)\n"
     "                          -\n"
     "                        ConstantNumber(1.000000)\n"
     "                    -\n"
     "            -\n"}};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ScriptParserTest)

BOOST_DATA_TEST_CASE(testScriptParsing, boost::unit_test::data::make(scriptData), testScript) {

    BOOST_TEST_MESSAGE("Testing Script Parser...");

    ScriptParser parser(testScript.script);
    if (parser.success()) {
        BOOST_TEST_MESSAGE("Parsing succeeded\n" << to_string(parser.ast()));
    } else {
        BOOST_TEST_MESSAGE("Parsing failed\n" << parser.error());
    }
    BOOST_REQUIRE(parser.success());
    BOOST_CHECK_EQUAL(to_string(parser.ast(), false), testScript.expectedAST);
}

BOOST_DATA_TEST_CASE(testRoundTrip, boost::unit_test::data::make(scriptData), testScript) {

    BOOST_TEST_MESSAGE("Testing Script Parser AST->Script->AST Roundtrip...");

    ScriptParser parser(testScript.script);
    BOOST_REQUIRE(parser.success());
    std::string script = to_script(parser.ast());
    BOOST_TEST_MESSAGE("Generated script:\n<<<<<<<<<<\n" + script + "\n>>>>>>>>>>");
    ScriptParser parser2(script);
    BOOST_REQUIRE(parser2.success());
    BOOST_CHECK_EQUAL(to_string(parser.ast(), false), to_string(parser2.ast(), false));
}

BOOST_AUTO_TEST_CASE(testRandomRoundTrip) {

    BOOST_TEST_MESSAGE("Testing Script Parser Random AST->Script->AST Roundtrip...");

    std::vector<std::tuple<Size, Size, Size>> testSizes;
    testSizes.push_back(std::make_tuple(1, 5, 1000));
    testSizes.push_back(std::make_tuple(5, 5, 1000));
    testSizes.push_back(std::make_tuple(10, 5, 1000));
    testSizes.push_back(std::make_tuple(1, 10, 1000));
    testSizes.push_back(std::make_tuple(5, 10, 1000));
    testSizes.push_back(std::make_tuple(10, 10, 100));

    for (auto const& t : testSizes) {
        BOOST_TEST_MESSAGE("Testing Script Parser Random AST->Script->AST RoundTrip (len="
                           << std::get<0>(t) << ", "
                           << "dep=" << std::get<1>(t) << ", n=" << std::get<2>(t) << ")");
        Size maxLen = 0;
        double avgLen = 0.0, avgTiming = 0.0, maxTiming = 0.0;
        for (Size i = 0; i < std::get<2>(t); ++i) {
            auto ast = generateRandomAST(std::get<0>(t), std::get<1>(t), 42 + i);
            std::string script = to_script(ast);
            maxLen = std::max(script.length(), maxLen);
            avgLen += static_cast<double>(script.length()) / static_cast<double>(std::get<2>(t));
            boost::timer::cpu_timer timer;
            ScriptParser parser(script);
            if (!parser.success()) {
                BOOST_TEST_MESSAGE("Error while parsing script: " << parser.error());
            }
            BOOST_REQUIRE(parser.success());
            double timing = timer.elapsed().wall * 1e-9;
            maxTiming = std::max(timing, maxTiming);
            avgTiming += timing / static_cast<double>(std::get<2>(t));
            BOOST_CHECK_EQUAL(to_string(ast, false), to_string(parser.ast(), false));
        }
        BOOST_TEST_MESSAGE("Finished, script size avg = " << avgLen << ", max = " << maxLen
                                                          << ", timing avg = " << avgTiming * 1E3
                                                          << " ms, max = " << maxTiming * 1E3 << " ms");
    }
}

BOOST_AUTO_TEST_CASE(generateRandomScript, *boost::unit_test::disabled()) {

    // not a test, just for convenience, to be removed at some stage...

    BOOST_TEST_MESSAGE("Creating randomg script based on LEN, DEP and SEED env variables");

    if (getenv("LEN") && getenv("DEP") && getenv("SEED")) {
        auto ast = generateRandomAST(atoi(getenv("LEN")), atoi(getenv("DEP")), atoi(getenv("SEED")));
        std::string script = to_script(ast);
        BOOST_TEST_MESSAGE("Generated script:\n<<<<<<<<<<\n" + script + "\n>>>>>>>>>>");
    }
}

BOOST_AUTO_TEST_CASE(testInteractive, *boost::unit_test::disabled()) {

    // not a test, just for convenience, to be removed at some stage...

    BOOST_TEST_MESSAGE("Running Script Parser on INPUT env variable...");

    std::string script = "IF x==2 THEN y=1; ELSE z=2; END;";
    if (auto c = getenv("INPUT"))
        script = std::string(c);

    ScriptParser parser(script);
    if (parser.success()) {
        std::cout << "Parsing succeeded\n" << to_string(parser.ast()) << std::endl;
    } else {
        std::cout << "Parsing failed\n" << parser.error();
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
