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

#include <boost/timer/timer.hpp>
#include <ored/utilities/formulaparser.hpp>
#include <qle/math/compiledformula.hpp>
#include <oret/toplevelfixture.hpp>

using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FormulaParserTest)

BOOST_AUTO_TEST_CASE(testDouble) {
    BOOST_TEST_MESSAGE("Testing Formula Parser with double...");

    double tol = 1E-12;

    // variable mapping
    double x = 42.0, y = -2.3;
    auto variables = [&x, &y](const std::string& s) {
        if (s == "x")
            return x;
        else if (s == "y")
            return y;
        else
            QL_FAIL("variable " << s << " not known.");
    };
    //

    BOOST_CHECK_CLOSE(parseFormula<double>("3"), 3.0, tol);

    BOOST_CHECK_THROW(parseFormula<double>("x"), QuantLib::Error);
    BOOST_CHECK_THROW(parseFormula<double>("{x}"), QuantLib::Error);
    BOOST_CHECK_THROW(parseFormula<double>("{z}", variables), QuantLib::Error);

    BOOST_CHECK_CLOSE(parseFormula<double>("{x}", variables), 42.0, tol);

    BOOST_CHECK_CLOSE(parseFormula<double>("3+4"), 7.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("3*4"), 12.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("3/4"), 0.75, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("3-4"), -1.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("-4"), -4.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("3+(-4)"), -1.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("-{x}", variables), -x, tol);

    BOOST_CHECK_CLOSE(parseFormula<double>("abs({x})", variables), std::abs(x), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("abs({y})", variables), std::abs(y), tol);

    BOOST_CHECK_SMALL(parseFormula<double>("gtZero({y})", variables), tol);
    BOOST_CHECK_SMALL(parseFormula<double>("geqZero({y})", variables), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("gtZero({x})", variables), 1.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("geqZero({x})", variables), 1.0, tol);
    BOOST_CHECK_SMALL(parseFormula<double>("gtZero(0.0)", variables), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("geqZero(0.0)", variables), 1.0, tol);

    BOOST_CHECK_CLOSE(parseFormula<double>("exp({y})", variables), std::exp(y), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("log({x})", variables), std::log(x), tol);

    BOOST_CHECK_CLOSE(parseFormula<double>("max({x},{y})", variables), std::max(x, y), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("min({x},{y})", variables), std::min(x, y), tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("pow({x},{y})", variables), std::pow(x, y), tol);

    BOOST_CHECK_CLOSE(parseFormula<double>("max((2.3*({x}-{y}))-4.3,0.0)", variables),
                      std::max(2.3 * (x - y) - 4.3, 0.0), tol);

    //
    BOOST_CHECK_CLOSE(parseFormula<double>("1+2-3-4+5"), 1.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("1+2-3*2*(-4)-4*2*3"), 3.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("1+(2-3)*5+2*3"), 2.0, tol);
    BOOST_CHECK_CLOSE(parseFormula<double>("(1+(2-3)*(5+2))*3"), -18.0, tol);

    // performance test

    boost::timer::cpu_timer timer;
    for (Size i = 0; i < 100000; ++i) {
        parseFormula<double>("max((2.3*({x}-{y}))-4.3,0.0)", variables);
    }
    timer.stop();
    BOOST_TEST_MESSAGE("timing full parsing (100k evaluations) = " << timer.format(boost::timer::default_places, "%w")
                                                                   << " secs.");
}

BOOST_AUTO_TEST_CASE(testCompiledFormula) {
    BOOST_TEST_MESSAGE("Testing Formula Parser with CompiledFormula...");

    double tol = 1E-12;

    double x = 42.0, y = -2.3;
    std::vector<std::string> variables;

    QuantExt::CompiledFormula f = parseFormula("((2*{x})+3)/{y}", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), (2.0 * x + 3.0) / y, tol);

    f = parseFormula("{x}-{y}", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), x - y, tol);

    f = parseFormula("-{y}", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "y");
    BOOST_CHECK_CLOSE(f({y}), -y, tol);

    f = parseFormula("{x}/{y}", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), x / y, tol);

    f = parseFormula("max({x},{y})", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), std::max(x, y), tol);

    f = parseFormula("min({x},{y})", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), std::min(x, y), tol);

    f = parseFormula("pow({x},{y})", variables);
    BOOST_REQUIRE(variables.size() == 2);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_EQUAL(variables[1], "y");
    BOOST_CHECK_CLOSE(f({x, y}), std::pow(x, y), tol);

    f = parseFormula("abs({y})", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "y");
    BOOST_CHECK_CLOSE(f({y}), std::abs(y), tol);

    f = parseFormula("gtZero({x})", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_CLOSE(f({x}), 1.0, tol);
    BOOST_CHECK_SMALL(f({y}), tol);
    BOOST_CHECK_SMALL(f({0.0}), tol);

    f = parseFormula("geqZero({x})", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_CLOSE(f({x}), 1.0, tol);
    BOOST_CHECK_SMALL(f({y}), tol);
    BOOST_CHECK_CLOSE(f({0.0}), 1.0, tol);

    f = parseFormula("exp({y})", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "y");
    BOOST_CHECK_CLOSE(f({y}), std::exp(y), tol);

    f = parseFormula("log({x})", variables);
    BOOST_REQUIRE(variables.size() == 1);
    BOOST_CHECK_EQUAL(variables[0], "x");
    BOOST_CHECK_CLOSE(f({x}), std::log(x), tol);

    // performance test

    f = parseFormula("max((2.3*({x}-{y}))-4.3,0.0)", variables);
    boost::timer::cpu_timer timer;
    double dummy = 0.0;
    std::vector<Real> v(2);
    for (Size i = 0; i < 100000; ++i) {
        v[0] = static_cast<double>(i) / static_cast<double>(10000);
        v[1] = static_cast<double>(100000 - i) / static_cast<double>(10000);
        dummy += f(v);
    }
    BOOST_TEST_MESSAGE("dummy = " << dummy);
    timer.stop();
    BOOST_TEST_MESSAGE("timing precompiled formula (100k evalulations) = "
                       << timer.format(boost::timer::default_places, "%w") << " secs.");

    timer.start();
    dummy = 0.0;
    for (Size i = 0; i < 100000; ++i) {
        Real x = static_cast<double>(i) / static_cast<double>(10000);
        Real y = static_cast<double>(100000 - i) / static_cast<double>(10000);
        dummy += std::max((2.3 * (x - y)) - 4.3, 0.0);
    }
    BOOST_TEST_MESSAGE("dummy = " << dummy);
    timer.stop();
    BOOST_TEST_MESSAGE("timing native (100k evaluations) = " << timer.format(boost::timer::default_places, "%w")
                                                             << " secs.");
    // muparser benchmark, if installed; to make this run you need to add
    // #include <muParser.h>
    // at the beginning of this file and alos link against the muparser library
    // sample results (mac book pro 2015 i7)
    // timing precompiled formula (100k evalulations) = 0.00332
    // timing muparser = 0.002974

    // double val_x = 0.0, val_y = 0.0;
    // mu::Parser p;
    // p.DefineVar("x", &val_x);
    // p.DefineVar("y", &val_y);
    // p.SetExpr("max((2.3*(x-y))-4.3,0.0)");

    // timer.start();
    // dummy = 0.0;
    // for (Size i = 0; i < 100000; ++i) {
    //     val_x = static_cast<double>(i) / static_cast<double>(10000);
    //     val_y = static_cast<double>(100000 - i) / static_cast<double>(10000);
    //     dummy += p.Eval();
    // }
    // timer.stop();

    // BOOST_TEST_MESSAGE("timing muparser = " << timer.format(boost::timer::default_places, "%w") << " secs.");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
