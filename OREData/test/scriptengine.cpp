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

#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/dummymodel.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/staticanalyser.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/model/blackscholesmodelbuilder.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

#include <ql/indexes/ibor/eonia.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <iomanip>
#include <iostream>

using namespace ore::data;
using namespace QuantExt;
using boost::timer::cpu_timer;
using boost::timer::default_places;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ScriptEngineTest)

BOOST_AUTO_TEST_CASE(testSimpleScript) {
    BOOST_TEST_MESSAGE("Testing simple script...");
    std::string script = "NUMBER x,i; FOR i IN (1,100,1) DO x = x + i; END;";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    auto context = QuantLib::ext::make_shared<Context>();
    ScriptEngine engine(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_TEST_MESSAGE("Script Engine successfully run, context is:\n" << *context);

    BOOST_CHECK(deterministic(context->scalars["i"]));
    BOOST_CHECK(deterministic(context->scalars["x"]));

    BOOST_CHECK(equal(context->scalars["i"], ValueType(RandomVariable(1, 100.0))).at(0));
    BOOST_CHECK(equal(context->scalars["x"], ValueType(RandomVariable(1, 100.0 / 2.0 * 101.0l))).at(0));
}

namespace {
// helper for testFunctions
RandomVariable executeScript(const std::string& script, const QuantLib::ext::shared_ptr<Context> initialContext) {
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    auto context = QuantLib::ext::make_shared<Context>(*initialContext);
    ScriptEngine engine(parser.ast(), context, QuantLib::ext::make_shared<DummyModel>(1));
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars.find("result") != context->scalars.end());
    BOOST_REQUIRE(context->scalars.at("result").which() == ValueTypeWhich::Number);
    return QuantLib::ext::get<RandomVariable>(context->scalars.at("result"));
}
} // namespace

BOOST_AUTO_TEST_CASE(testFunctions) {
    BOOST_TEST_MESSAGE("Testing functions...");

    auto c = QuantLib::ext::make_shared<Context>();
    RandomVariable x(1, 2.0);
    RandomVariable y(1, -2.0);
    RandomVariable result(1, 0.0);
    c->scalars["x"] = x;
    c->scalars["y"] = y;
    c->scalars["omega"] = RandomVariable(1, -1.0);
    c->scalars["ref"] = EventVec{1, Date(6, Jun, 2019)};
    c->scalars["expiry"] = EventVec{1, Date(6, Jun, 2022)};
    c->scalars["strike"] = RandomVariable(1, 98.0);
    c->scalars["forward"] = RandomVariable(1, 100.0);
    c->scalars["vol"] = RandomVariable(1, 0.2);
    c->scalars["result"] = result;

    BOOST_CHECK(close_enough_all(executeScript("result=x+y;", c), x + y));
    BOOST_CHECK(close_enough_all(executeScript("result=x-y;", c), x - y));
    BOOST_CHECK(close_enough_all(executeScript("result=x*y;", c), x * y));
    BOOST_CHECK(close_enough_all(executeScript("result=x/y;", c), x / y));

    BOOST_CHECK(close_enough_all(executeScript("result=-x;", c), -x));
    BOOST_CHECK(close_enough_all(executeScript("result=abs(y);", c), abs(y)));
    BOOST_CHECK(close_enough_all(executeScript("result=exp(x);", c), exp(x)));
    BOOST_CHECK(close_enough_all(executeScript("result=ln(x);", c), log(x)));
    BOOST_CHECK(close_enough_all(executeScript("result=sqrt(x);", c), sqrt(x)));
    BOOST_CHECK(close_enough_all(executeScript("result=normalCdf(x);", c), normalCdf(x)));
    BOOST_CHECK(close_enough_all(executeScript("result=normalPdf(x);", c), normalPdf(x)));

    BOOST_CHECK(close_enough_all(executeScript("result=max(x,y);", c), max(x, y)));
    BOOST_CHECK(close_enough_all(executeScript("result=min(x,y);", c), min(x, y)));
    BOOST_CHECK(close_enough_all(executeScript("result=pow(x,y);", c), pow(x, y)));

    BOOST_CHECK(close_enough_all(executeScript("result=x+y-y+x/y*y-x;", c), x + y - y + x / y * y - x));

    BOOST_CHECK_CLOSE(
        executeScript("result=black(omega,ref,expiry,strike,forward,vol);", c).at(0),
        blackFormula(
            Option::Put, 98.0, 100.0,
            0.2 * std::sqrt(ActualActual(ActualActual::ISDA).yearFraction(Date(6, June, 2019), Date(6, June, 2022)))),
        1E-10);
}

BOOST_AUTO_TEST_CASE(testDaycounterFunctions) {
    BOOST_TEST_MESSAGE("Testing daycounter functions...");

    Date d1(15, Sep, 2019), d2(8, Jan, 2033);
    Actual365Fixed dc;

    auto c = QuantLib::ext::make_shared<Context>();
    EventVec date1{1, d1};
    EventVec date2{1, d2};
    DaycounterVec daycounter{1, "A365F"};
    RandomVariable result(1, 0.0);
    c->scalars["date1"] = date1;
    c->scalars["date2"] = date2;
    c->scalars["daycounter"] = daycounter;
    c->scalars["result"] = result;

    BOOST_CHECK_CLOSE(executeScript("result=dcf(daycounter, date1, date2);", c).at(0), dc.yearFraction(d1, d2), 1E-10);
    BOOST_CHECK_CLOSE(executeScript("result=days(daycounter, date1, date2);", c).at(0), dc.dayCount(d1, d2), 1E-12);
}

BOOST_AUTO_TEST_CASE(testSortFunction) {
    BOOST_TEST_MESSAGE("Testing sort function...");

    constexpr Real tol = 1E-14;

    RandomVariable x1(2), x2(2), x3(2), x4(2);
    x1.set(0,3.0);
    x1.set(1,1.0);
    x2.set(0,4.0);
    x2.set(1,2.0);
    x3.set(0,2.0);
    x3.set(1,4.0);
    x4.set(0,1.0);
    x4.set(1,3.0);

    std::vector<RandomVariable> x{x1, x2, x3, x4};
    std::vector<ValueType> xv;
    for (auto const& c : x)
        xv.push_back(c);

    std::vector<ValueType> y(4, RandomVariable(2)), i(4, RandomVariable(2));

    auto c0 = QuantLib::ext::make_shared<Context>();
    c0->arrays["x"] = xv;
    c0->arrays["y"] = y;
    c0->arrays["i"] = i;

    // sort x, write result back to x
    auto c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine(ScriptParser("SORT (x);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine.run());
    std::vector<ValueType> result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 1.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);

    // sort x, but store result in y
    c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine2(ScriptParser("SORT ( x, y );").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine2.run());
    result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), x.at(0).at(0), tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), x.at(1).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), x.at(2).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), x.at(3).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), x.at(0).at(1), tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), x.at(1).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), x.at(2).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), x.at(3).at(1), tol);
    result = c->arrays.at("y");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 1.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);

    // sort x, store result in y and index permutation in i
    c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine3(ScriptParser("SORT ( x, y, i );").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine3.run());
    result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), x.at(0).at(0), tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), x.at(1).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), x.at(2).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), x.at(3).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), x.at(0).at(1), tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), x.at(1).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), x.at(2).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), x.at(3).at(1), tol);
    result = c->arrays.at("y");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 1.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);
    result = c->arrays.at("i");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 4.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 1.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 3.0, tol);

    // check illegal call with y having a different size from x
    c = QuantLib::ext::make_shared<Context>(*c0);
    c->arrays["y"] = std::vector<ValueType>(3);
    ScriptEngine engine4(ScriptParser("SORT(x,y);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_THROW(engine4.run(), std::exception);

    // check illegal call with i having a different size from x (but y has the correct size)
    c = QuantLib::ext::make_shared<Context>(*c0);
    c->arrays["i"] = std::vector<ValueType>(3);
    ScriptEngine engine5(ScriptParser("SORT(x,y,i);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_THROW(engine5.run(), std::exception);
}

BOOST_AUTO_TEST_CASE(testPermuteFunction) {
    BOOST_TEST_MESSAGE("Testing permute function...");

    constexpr Real tol = 1E-14;

    RandomVariable x1(2), x2(2), x3(2), x4(2);
    x1.set(0,3.0);
    x1.set(1,1.0);
    x2.set(0,4.0);
    x2.set(1,2.0);
    x3.set(0,2.0);
    x3.set(1,4.0);
    x4.set(0,1.0);
    x4.set(1,3.0);

    RandomVariable p1(2), p2(2), p3(2), p4(2);
    p1.set(0,4.0);
    p1.set(1,1.0);
    p2.set(0,3.0);
    p2.set(1,2.0);
    p3.set(0,1.0);
    p3.set(1,4.0);
    p4.set(0,2.0);
    p4.set(1,3.0);

    std::vector<RandomVariable> x{x1, x2, x3, x4};
    std::vector<RandomVariable> p{p1, p2, p3, p4};
    std::vector<ValueType> xv, pv;
    for (auto const& c : x)
        xv.push_back(c);
    for (auto const& c : p)
        pv.push_back(c);

    std::vector<ValueType> yv(4, RandomVariable(2));

    auto c0 = QuantLib::ext::make_shared<Context>();
    c0->arrays["x"] = xv;
    c0->arrays["y"] = yv;
    c0->arrays["p"] = pv;

    // permute x, write result back to x
    auto c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine(ScriptParser("PERMUTE (x,p);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine.run());
    std::vector<ValueType> result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 1.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);

    // permute x, but store result in y
    c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine2(ScriptParser("PERMUTE ( x, y, p);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine2.run());
    result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), x.at(0).at(0), tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), x.at(1).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), x.at(2).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), x.at(3).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), x.at(0).at(1), tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), x.at(1).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), x.at(2).at(1), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), x.at(3).at(1), tol);
    result = c->arrays.at("y");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), 1.0, tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), 4.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);

    // check illegal call with p having a different size from x
    c = QuantLib::ext::make_shared<Context>(*c0);
    c->arrays["p"] = std::vector<ValueType>(5);
    ScriptEngine engine3(ScriptParser("PERMUTE(x,p);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_THROW(engine3.run(), std::exception);

    // check illegal call with y having a different size from x, but p having the correct size
    c = QuantLib::ext::make_shared<Context>(*c0);
    c->arrays["y"] = std::vector<ValueType>(5);
    ScriptEngine engine4(ScriptParser("PERMUTE(x,y,p);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_THROW(engine4.run(), std::exception);

    // check illegal call with p having the correct size, but containing an illegal permutation index
    c = QuantLib::ext::make_shared<Context>(*c0);
    std::vector<ValueType> pv2 = pv;
    QuantLib::ext::get<RandomVariable>(pv2[2]).set(1, 5.0);
    c->arrays["p"] = pv2;
    ScriptEngine engine5(ScriptParser("PERMUTE(x,p);").ast(), c, QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_THROW(engine5.run(), std::exception);
}

BOOST_AUTO_TEST_CASE(testSortPermuteFunctionsWithFilter) {
    BOOST_TEST_MESSAGE("Testing sort and permute functions with filter...");

    constexpr Real tol = 1E-14;

    RandomVariable x1(2), x2(2), x3(2), x4(2);
    x1.set(0,3.0);
    x1.set(1,1.0);
    x2.set(0,4.0);
    x2.set(1,2.0);
    x3.set(0,2.0);
    x3.set(1,4.0);
    x4.set(0,1.0);
    x4.set(1,3.0);

    RandomVariable p1(2), p2(2), p3(2), p4(2);
    p1.set(0,4.0);
    p1.set(1,1.0);
    p2.set(0,3.0);
    p2.set(1,2.0);
    p3.set(0,1.0);
    p3.set(1,4.0);
    p4.set(0,2.0);
    p4.set(1,3.0);

    std::vector<RandomVariable> x{x1, x2, x3, x4};
    std::vector<RandomVariable> p{p1, p2, p3, p4};
    std::vector<ValueType> xv, pv;
    for (auto const& c : x)
        xv.push_back(c);
    for (auto const& c : p)
        pv.push_back(c);

    RandomVariable indicator(2);
    indicator.set(0, 0.0);
    indicator.set(1, 1.0);

    auto c0 = QuantLib::ext::make_shared<Context>();
    c0->arrays["x"] = xv;
    c0->arrays["p"] = xv;
    c0->scalars["indicator"] = indicator;

    // sort x if y is positive, i.e. on path #1, but not on path #0
    auto c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine(ScriptParser("IF indicator > 0 THEN SORT (x); END;").ast(), c,
                        QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine.run());
    std::vector<ValueType> result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), x.at(0).at(0), tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), x.at(1).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), x.at(2).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), x.at(3).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);

    // permute x if y is positive, i.e. again on path #1, but not on path#0
    c = QuantLib::ext::make_shared<Context>(*c0);
    ScriptEngine engine2(ScriptParser("IF indicator > 0 THEN PERMUTE (x,p); END;").ast(), c,
                         QuantLib::ext::make_shared<DummyModel>(2));
    BOOST_REQUIRE_NO_THROW(engine2.run());
    result = c->arrays.at("x");
    BOOST_REQUIRE(result.size() == x.size());                                         // check array size
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(0)).size() == x.at(0).size()); // check number of paths
    BOOST_REQUIRE(QuantLib::ext::get<RandomVariable>(result.at(1)).size() == x.at(1).size());
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(0), x.at(0).at(0), tol); // check path #0
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(0), x.at(1).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(0), x.at(2).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(0), x.at(3).at(0), tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(0)).at(1), 1.0, tol); // check path #1
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(1)).at(1), 2.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(2)).at(1), 3.0, tol);
    BOOST_CHECK_CLOSE(QuantLib::ext::get<RandomVariable>(result.at(3)).at(1), 4.0, tol);
}

BOOST_AUTO_TEST_CASE(testHistoricFixingsFunction) {
    BOOST_TEST_MESSAGE("Testing HISTFIXING() function...");

    std::string script = "NUMBER hasFixing1, hasFixing2, hasFixing3;\n"
                         "hasFixing1 = HISTFIXING(Underlying, date1);\n"
                         "hasFixing2 = HISTFIXING(Underlying, date2);\n"
                         "hasFixing3 = HISTFIXING(Underlying, date3);\n";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    constexpr Size nPaths = 50000;

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Underlying"] = IndexVec{nPaths, "EQ-SP5"};
    context->scalars["date1"] = EventVec{nPaths, Date(7, May, 2019)};
    context->scalars["date2"] = EventVec{nPaths, Date(8, May, 2019)};
    context->scalars["date3"] = EventVec{nPaths, Date(9, May, 2019)};

    class MyModel : public DummyModel {
        using DummyModel::DummyModel;
        const Date& referenceDate() const override {
            static Date date(8, May, 2019);
            return date;
        }
    };

    auto model = QuantLib::ext::make_shared<MyModel>(nPaths);

    EquityIndex2 ind("SP5", NullCalendar(), Currency());
    ind.addFixing(Date(8, May, 2019), 100.0);
    ind.addFixing(Date(9, May, 2019), 100.0);

    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["hasFixing1"].which() == ValueTypeWhich::Number);
    BOOST_REQUIRE(context->scalars["hasFixing2"].which() == ValueTypeWhich::Number);
    BOOST_REQUIRE(context->scalars["hasFixing3"].which() == ValueTypeWhich::Number);

    RandomVariable rv1 = QuantLib::ext::get<RandomVariable>(context->scalars["hasFixing1"]);
    RandomVariable rv2 = QuantLib::ext::get<RandomVariable>(context->scalars["hasFixing2"]);
    RandomVariable rv3 = QuantLib::ext::get<RandomVariable>(context->scalars["hasFixing3"]);

    BOOST_CHECK(rv1.deterministic());
    BOOST_CHECK(rv2.deterministic());
    BOOST_CHECK(rv3.deterministic());

    constexpr Real tol = 1E-10;
    BOOST_CHECK_CLOSE(rv1.at(0), 0.0, tol); // no historic fixing set
    BOOST_CHECK_CLOSE(rv2.at(0), 1.0, tol); // have historic fixing on today
    BOOST_CHECK_CLOSE(rv3.at(0), 0.0, tol); // have historic fixing, but date is > today
}

BOOST_AUTO_TEST_CASE(testDateIndexFunctionEq) {
    BOOST_TEST_MESSAGE("Testing DATEINDEX(...,...,EQ) function");

    std::string script = "NUMBER i;\n"
                         "i = DATEINDEX(d, a, EQ);";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    constexpr Size nPaths = 10;
    constexpr Real tol = 1E-10;

    auto model = QuantLib::ext::make_shared<DummyModel>(nPaths);

    std::vector<ValueType> dates;
    dates.push_back(EventVec{nPaths, Date(7, May, 2019)});
    dates.push_back(EventVec{nPaths, Date(10, June, 2020)});

    // find date at index 1
    auto context1 = QuantLib::ext::make_shared<Context>();
    context1->arrays["a"] = dates;
    context1->scalars["d"] = EventVec{nPaths, Date(7, May, 2019)};
    ScriptEngine engine1(parser.ast(), context1, model);
    BOOST_REQUIRE_NO_THROW(engine1.run());
    BOOST_REQUIRE(context1->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv1 = QuantLib::ext::get<RandomVariable>(context1->scalars["i"]);
    BOOST_CHECK(rv1.deterministic());
    BOOST_CHECK_CLOSE(rv1.at(0), 1.0, tol);

    // find date at index 2
    auto context2 = QuantLib::ext::make_shared<Context>();
    context2->arrays["a"] = dates;
    context2->scalars["d"] = EventVec{nPaths, Date(10, June, 2020)};
    ScriptEngine engine2(parser.ast(), context2, model);
    BOOST_REQUIRE_NO_THROW(engine2.run());
    BOOST_REQUIRE(context2->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv2 = QuantLib::ext::get<RandomVariable>(context2->scalars["i"]);
    BOOST_CHECK(rv2.deterministic());
    BOOST_CHECK_CLOSE(rv2.at(0), 2.0, tol);

    // do not find date
    auto context3 = QuantLib::ext::make_shared<Context>();
    context3->arrays["a"] = dates;
    context3->scalars["d"] = EventVec{nPaths, Date(15, June, 2020)};
    ScriptEngine engine3(parser.ast(), context3, model);
    BOOST_REQUIRE_NO_THROW(engine3.run());
    BOOST_REQUIRE(context3->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv3 = QuantLib::ext::get<RandomVariable>(context3->scalars["i"]);
    BOOST_CHECK(rv3.deterministic());
    BOOST_CHECK_CLOSE(rv3.at(0), 0.0, tol);

    // search in a number array => expect to not find date (but no error)
    auto context4 = QuantLib::ext::make_shared<Context>();
    std::vector<ValueType> numbers(5, RandomVariable(nPaths));
    context4->arrays["a"] = numbers;
    context4->scalars["d"] = EventVec{nPaths, Date(15, June, 2020)};
    ScriptEngine engine4(parser.ast(), context4, model);
    BOOST_REQUIRE_NO_THROW(engine4.run());
    BOOST_REQUIRE(context4->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv4 = QuantLib::ext::get<RandomVariable>(context4->scalars["i"]);
    BOOST_CHECK(rv4.deterministic());
    BOOST_CHECK_CLOSE(rv4.at(0), 0.0, tol);

    // value to find is not a date => error
    auto context5 = QuantLib::ext::make_shared<Context>();
    context5->arrays["a"] = dates;
    context5->scalars["d"] = RandomVariable(nPaths);
    ScriptEngine engine5(parser.ast(), context5, model);
    BOOST_CHECK_THROW(engine5.run(), QuantLib::Error);

    // search array is actually a scalar => error
    auto context6 = QuantLib::ext::make_shared<Context>();
    context6->scalars["a"] = EventVec{nPaths, Date(15, June, 2020)};
    context6->scalars["d"] = EventVec{nPaths, Date(15, June, 2020)};
    ScriptEngine engine6(parser.ast(), context6, model);
    BOOST_CHECK_THROW(engine6.run(), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testDateIndexFunctionGeq) {
    BOOST_TEST_MESSAGE("Testing DATEINDEX(...,...,GEQ) function");

    std::string script = "NUMBER i;\n"
                         "i = DATEINDEX(d, a, GEQ);";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    constexpr Size nPaths = 10;
    constexpr Real tol = 1E-10;

    auto model = QuantLib::ext::make_shared<DummyModel>(nPaths);

    std::vector<ValueType> dates;
    dates.push_back(EventVec{nPaths, Date(7, May, 2019)});
    dates.push_back(EventVec{nPaths, Date(10, June, 2020)});

    // find date at index 1 (exact match)
    auto context1 = QuantLib::ext::make_shared<Context>();
    context1->arrays["a"] = dates;
    context1->scalars["d"] = EventVec{nPaths, Date(7, May, 2019)};
    ScriptEngine engine1(parser.ast(), context1, model);
    BOOST_REQUIRE_NO_THROW(engine1.run());
    BOOST_REQUIRE(context1->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv1 = QuantLib::ext::get<RandomVariable>(context1->scalars["i"]);
    BOOST_CHECK(rv1.deterministic());
    BOOST_CHECK_CLOSE(rv1.at(0), 1.0, tol);

    // find date at index 2 (exact match)
    auto context2 = QuantLib::ext::make_shared<Context>();
    context2->arrays["a"] = dates;
    context2->scalars["d"] = EventVec{nPaths, Date(10, June, 2020)};
    ScriptEngine engine2(parser.ast(), context2, model);
    BOOST_REQUIRE_NO_THROW(engine2.run());
    BOOST_REQUIRE(context2->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv2 = QuantLib::ext::get<RandomVariable>(context2->scalars["i"]);
    BOOST_CHECK(rv2.deterministic());
    BOOST_CHECK_CLOSE(rv2.at(0), 2.0, tol);

    // do not find date
    auto context3 = QuantLib::ext::make_shared<Context>();
    context3->arrays["a"] = dates;
    context3->scalars["d"] = EventVec{nPaths, Date(15, June, 2020)};
    ScriptEngine engine3(parser.ast(), context3, model);
    BOOST_REQUIRE_NO_THROW(engine3.run());
    BOOST_REQUIRE(context3->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv3 = QuantLib::ext::get<RandomVariable>(context3->scalars["i"]);
    BOOST_CHECK(rv3.deterministic());
    BOOST_CHECK_CLOSE(rv3.at(0), 3.0, tol);

    // find date at index1 (from earlier date)
    auto context4 = QuantLib::ext::make_shared<Context>();
    context4->arrays["a"] = dates;
    context4->scalars["d"] = EventVec{nPaths, Date(2, May, 2019)};
    ScriptEngine engine4(parser.ast(), context4, model);
    BOOST_REQUIRE_NO_THROW(engine4.run());
    BOOST_REQUIRE(context4->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv4 = QuantLib::ext::get<RandomVariable>(context4->scalars["i"]);
    BOOST_CHECK(rv4.deterministic());
    BOOST_CHECK_CLOSE(rv4.at(0), 1.0, tol);

    // find date at index2 (from earlier date)
    auto context5 = QuantLib::ext::make_shared<Context>();
    context5->arrays["a"] = dates;
    context5->scalars["d"] = EventVec{nPaths, Date(2, June, 2020)};
    ScriptEngine engine5(parser.ast(), context5, model);
    BOOST_REQUIRE_NO_THROW(engine5.run());
    BOOST_REQUIRE(context5->scalars["i"].which() == ValueTypeWhich::Number);
    RandomVariable rv5 = QuantLib::ext::get<RandomVariable>(context5->scalars["i"]);
    BOOST_CHECK(rv5.deterministic());
    BOOST_CHECK_CLOSE(rv5.at(0), 2.0, tol);
}

BOOST_AUTO_TEST_CASE(testFwdCompFunction) {
    BOOST_TEST_MESSAGE("Testing FWDCOMP() function");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script = "NUMBER rate;\n"
                         "rate = FWDCOMP(underlying, obs, start, end, spread, gearing);\n";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    constexpr Size nPaths = 10; // does not matter, we use a model with deterministic rates below

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(ref, 0.02, ActualActual(ActualActual::ISDA)));
    auto on = QuantLib::ext::make_shared<Eonia>(yts);

    Date start = Date(10, October, 2018);
    Date end = Date(10, October, 2019);
    std::string indexName = "EUR-EONIA";
    Real spread = 0.0;
    Real gearing = 1.0;

    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>> irIndices;
    irIndices.push_back(std::make_pair(indexName, on));
    Model::McParams mcParams;
    mcParams.regressionOrder = 1;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, std::vector<std::string>{"EUR"}, std::vector<Handle<YieldTermStructure>>{yts},
        std::vector<Handle<Quote>>(), irIndices,
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>(), std::vector<std::string>(),
        std::vector<std::string>(), Handle<BlackScholesModelWrapper>(QuantLib::ext::make_shared<BlackScholesModelWrapper>()),
        std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>(), mcParams,
        std::set<Date>{});

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["underlying"] = IndexVec{nPaths, indexName};
    context->scalars["obs"] = EventVec{nPaths, start};
    context->scalars["start"] = EventVec{nPaths, start};
    context->scalars["end"] = EventVec{nPaths, end};
    context->scalars["spread"] = RandomVariable(nPaths, spread);
    context->scalars["gearing"] = RandomVariable(nPaths, gearing);

    QuantExt::OvernightIndexedCoupon coupon(end, 1.0, start, end, on, gearing, spread);

    auto indexInfo = QuantLib::ext::make_shared<StaticAnalyser>(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(indexInfo->run(););
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgFixingDates().size(), 1);
    BOOST_REQUIRE(indexInfo->fwdCompAvgFixingDates().at(indexName).size() == coupon.fixingDates().size());
    Size i = 0;
    for (auto const& f : indexInfo->fwdCompAvgFixingDates().at(indexName)) {
        BOOST_CHECK_EQUAL(f, coupon.fixingDates()[i++]);
    }
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgEvalDates().size(), 1);
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgStartEndDates().size(), 1);
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgEvalDates().at(indexName).size(), 1);
    BOOST_CHECK_EQUAL(*indexInfo->fwdCompAvgEvalDates().at(indexName).begin(), start);
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgStartEndDates().size(), 1);
    BOOST_REQUIRE_EQUAL(indexInfo->fwdCompAvgStartEndDates().at(indexName).size(), 2);
    auto it = indexInfo->fwdCompAvgStartEndDates().at(indexName).begin();
    BOOST_CHECK_EQUAL(*it++, start);
    BOOST_CHECK_EQUAL(*it, end);

    for (auto const& d : indexInfo->fwdCompAvgFixingDates().at(indexName)) {
        on->addFixing(d, 0.01);
    }

    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["rate"].which() == ValueTypeWhich::Number);
    RandomVariable rv = QuantLib::ext::get<RandomVariable>(context->scalars["rate"]);
    BOOST_TEST_MESSAGE("rate from engine = " << rv.at(0) << " rate from coupon = " << coupon.rate());
    BOOST_CHECK_CLOSE(rv.at(0), coupon.rate(), 1E-10);
}

BOOST_AUTO_TEST_CASE(testProbFunctions) {
    BOOST_TEST_MESSAGE("Testing ABOVEPROB(), BELOWPROB() functions");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script = "AboveProb = ABOVEPROB(Underlying, Date1, Date2, BarrierUp);\n"
                         "BelowProb = BELOWPROB(Underlying, Date1, Date2, BarrierDown);\n";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    Size nPaths = 10000;

    Real s0 = 100.0;
    Real vol = 0.10;
    Date date1(7, May, 2020);
    Date date2(7, December, 2020);
    Real barrierUp = 110.0;
    Real barrierDown = 80.0;

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Underlying"] = IndexVec{nPaths, "EQ-Dummy"};
    context->scalars["Date1"] = EventVec{nPaths, date1};
    context->scalars["Date2"] = EventVec{nPaths, date2};
    context->scalars["BarrierUp"] = RandomVariable(nPaths, barrierUp);
    context->scalars["BarrierDown"] = RandomVariable(nPaths, barrierDown);
    context->scalars["AboveProb"] = RandomVariable(nPaths, barrierUp);
    context->scalars["BelowProb"] = RandomVariable(nPaths, barrierDown);

    Handle<YieldTermStructure> yts0(QuantLib::ext::make_shared<FlatForward>(ref, 0.0, ActualActual(ActualActual::ISDA)));
    Handle<BlackVolTermStructure> volts(
        QuantLib::ext::make_shared<BlackConstantVol>(ref, NullCalendar(), vol, ActualActual(ActualActual::ISDA)));
    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts0, volts);
    std::set<Date> simulationDates = {date1, date2};
    Model::McParams mcParams;
    mcParams.regressionOrder = 1;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, "USD", yts0, "EQ-Dummy", "USD",
        BlackScholesModelBuilder(yts0, process, simulationDates, std::set<Date>(), 1).model(), mcParams, simulationDates);

    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["AboveProb"].which() == ValueTypeWhich::Number);
    BOOST_REQUIRE(context->scalars["BelowProb"].which() == ValueTypeWhich::Number);
    RandomVariable rvAbove = QuantLib::ext::get<RandomVariable>(context->scalars["AboveProb"]);
    RandomVariable rvBelow = QuantLib::ext::get<RandomVariable>(context->scalars["BelowProb"]);
    BOOST_REQUIRE(rvAbove.size() == nPaths);
    BOOST_REQUIRE(rvBelow.size() == nPaths);

    Real avgAbove = expectation(rvAbove).at(0);
    Real avgBelow = expectation(rvBelow).at(0);
    BOOST_TEST_MESSAGE("prob estimation using ABOVEPROB(), BELOWPROB(): " << avgAbove << " (above), " << avgBelow
                                                                          << " (below)");

    // compute the probs using MC + a brute force check on a "fine" time grid
    Size timeSteps = 500;
    std::vector<Real> times;
    Real t0 = process->riskFreeRate()->timeFromReference(date1);
    Real t1 = process->riskFreeRate()->timeFromReference(date2);
    times.push_back(t0);
    for (Size i = 0; i < timeSteps; ++i) {
        times.push_back(t0 + (t1 - t0) * static_cast<Real>(i + 1) / static_cast<Real>(timeSteps));
    }
    TimeGrid timeGrid(times.begin(), times.end());
    auto pg = makeMultiPathGenerator(SobolBrownianBridge, process, timeGrid, SobolBrownianGenerator::Steps);
    double avgAbove2 = 0.0, avgBelow2 = 0.0;
    for (Size path = 0; path < nPaths; ++path) {
        MultiPath p = pg->next().value;
        // brute force check for barrier hit on time grid
        bool hitAbove = false, hitBelow = false;
        for (Size i = 1; i < timeGrid.size(); ++i) {
            if (p[0][i] > barrierUp)
                hitAbove = true;
            else if (p[0][i] < barrierDown)
                hitBelow = true;
        }
        if (hitAbove)
            avgAbove2 += 1.0 / static_cast<Real>(nPaths);
        if (hitBelow)
            avgBelow2 += 1.0 / static_cast<Real>(nPaths);
    }
    BOOST_TEST_MESSAGE("prob estimation using MC (timeSteps=" << timeSteps << "): " << avgAbove2 << " (above), "
                                                              << avgBelow2 << " (below)");
    BOOST_CHECK_CLOSE(avgAbove, avgAbove2, 5.0);
    BOOST_CHECK_CLOSE(avgBelow, avgBelow2, 5.0);

    // compute the probs using an analytical formula on the start and end point
    std::vector<Real> times2 = {t0, t1};
    TimeGrid timeGrid2(times2.begin(), times2.end());
    auto pg2 = makeMultiPathGenerator(SobolBrownianBridge, process, timeGrid2, SobolBrownianGenerator::Steps);
    double checkAvgAbove = 0.0, checkAvgBelow = 0.0;
    for (Size path = 0; path < nPaths; ++path) {
        MultiPath p = pg2->next().value;
        Real v1 = p[0][1], v2 = p[0][2];
        Real pAbove, pBelow;
        if (v1 > barrierUp || v2 > barrierUp)
            pAbove = 1.0;
        else
            pAbove = std::exp(-2.0 / (vol * vol * (t1 - t0)) * std::log(v1 / barrierUp) * std::log(v2 / barrierUp));
        if (v1 < barrierDown || v2 < barrierDown)
            pBelow = 1.0;
        else
            pBelow = std::exp(-2.0 / (vol * vol * (t1 - t0)) * std::log(v1 / barrierDown) * std::log(v2 / barrierDown));
        checkAvgAbove += pAbove / static_cast<Real>(nPaths);
        checkAvgBelow += pBelow / static_cast<Real>(nPaths);
    }
    BOOST_TEST_MESSAGE("prob estimation using MC + analytical formula for endpoints: " << checkAvgAbove << " (above), "
                                                                                       << checkAvgBelow << " (below)");
    BOOST_CHECK_CLOSE(avgAbove, checkAvgAbove, 1.0E-4);
    BOOST_CHECK_CLOSE(avgBelow, checkAvgBelow, 1.0E-4);
}

BOOST_AUTO_TEST_CASE(testEuropeanOption) {
    BOOST_TEST_MESSAGE("Testing european option...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script = "Option = Quantity * PAY(max( PutCall * (Underlying(Expiry) - Strike), 0 ),\n"
                         "                        Expiry, Settlement, PayCcy);";
    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    Real s0 = 100.0;
    Real vol = 0.18;
    Real rate = 0.02;
    Real quantity = 10.0;
    Real putcall = 1.0;
    Real strike = 100.0;
    Date expiry(7, May, 2020);
    Date settlement(9, May, 2020);

    constexpr Size nPaths = 50000;

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Quantity"] = RandomVariable(nPaths, quantity);
    context->scalars["PutCall"] = RandomVariable(nPaths, putcall);
    context->scalars["Strike"] = RandomVariable(nPaths, strike);
    context->scalars["Underlying"] = IndexVec{nPaths, "EQ-SP5"};
    context->scalars["Expiry"] = EventVec{nPaths, expiry};
    context->scalars["Settlement"] = EventVec{nPaths, settlement};
    context->scalars["PayCcy"] = CurrencyVec{nPaths, "USD"};
    context->scalars["Option"] = RandomVariable(nPaths, 0.0);

    auto indexInfo = QuantLib::ext::make_shared<StaticAnalyser>(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(indexInfo->run());
    BOOST_REQUIRE_EQUAL(indexInfo->indexEvalDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->indexEvalDates().begin()->first, "EQ-SP5");
    BOOST_CHECK_EQUAL(indexInfo->indexEvalDates().begin()->second.size(), 1);
    BOOST_CHECK_EQUAL(*indexInfo->indexEvalDates().begin()->second.begin(), expiry);
    BOOST_REQUIRE_EQUAL(indexInfo->payObsDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->payObsDates().begin()->first, "USD");
    BOOST_CHECK_EQUAL(indexInfo->payObsDates().begin()->second.size(), 1);
    BOOST_CHECK_EQUAL(*indexInfo->payObsDates().begin()->second.begin(), expiry);
    BOOST_REQUIRE_EQUAL(indexInfo->payPayDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->payPayDates().begin()->first, "USD");
    BOOST_CHECK_EQUAL(indexInfo->payPayDates().begin()->second.size(), 1);
    BOOST_CHECK_EQUAL(*indexInfo->payPayDates().begin()->second.begin(), settlement);
    BOOST_CHECK(indexInfo->regressionDates().empty());

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(ref, rate, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> yts0(QuantLib::ext::make_shared<FlatForward>(ref, 0.0, ActualActual(ActualActual::ISDA)));
    Handle<BlackVolTermStructure> volts(
        QuantLib::ext::make_shared<BlackConstantVol>(ref, NullCalendar(), vol, ActualActual(ActualActual::ISDA)));
    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);

    // compile the sim and pay dates required by the BS model ctor below
    std::set<Date> simulationDates, payDates;
    for (auto const& s : indexInfo->indexEvalDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    for (auto const& s : indexInfo->payObsDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    simulationDates.insert(indexInfo->regressionDates().begin(), indexInfo->regressionDates().end());
    for (auto const& s : indexInfo->payPayDates())
        payDates.insert(s.second.begin(), s.second.end());

    cpu_timer timer;
    Model::McParams mcParams;
    mcParams.regressionOrder = 6;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, "USD", yts, "EQ-SP5", "USD",
        BlackScholesModelBuilder(yts, process, simulationDates, payDates, 1).model(), mcParams, simulationDates);
    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["Option"].which() == ValueTypeWhich::Number);
    RandomVariable rv = QuantLib::ext::get<RandomVariable>(context->scalars["Option"]);
    BOOST_REQUIRE(rv.size() == nPaths);
    Real avg = expectation(rv).at(0);
    timer.stop();
    BOOST_TEST_MESSAGE("option value estimation " << avg << " (timing " << timer.format(default_places, "%w") << "s)");

    // hardcoded version of the script
    timer.start();
    std::vector<Real> times;
    times.push_back(process->riskFreeRate()->timeFromReference(expiry));
    auto pg = makeMultiPathGenerator(SobolBrownianBridge, process, TimeGrid(times.begin(), times.end()),
                                     SobolBrownianGenerator::Steps);
    double avg2 = 0.0;
    for (Size path = 0; path < nPaths; ++path) {
        MultiPath p = pg->next().value;
        Real v = quantity * std::max(putcall * (p[0][1] - strike), 0.0);
        avg2 += v;
    }
    avg2 *= process->riskFreeRate()->discount(settlement) / static_cast<double>(nPaths);
    timer.stop();
    BOOST_TEST_MESSAGE("result with hardcoded script " << avg2 << " (timing " << timer.format(default_places, "%w")
                                                       << "s)");
    BOOST_CHECK_CLOSE(avg, avg2, 1E-10);

    // analytical computation
    double expected =
        quantity * blackFormula(Option::Call, s0, s0 / yts->discount(expiry),
                                vol * std::sqrt(yts->timeFromReference(expiry)), yts->discount(settlement));
    BOOST_TEST_MESSAGE("option value expected " << expected);
    BOOST_CHECK_CLOSE(avg, expected, 0.1);
}

BOOST_AUTO_TEST_CASE(testAmericanOption) {
    BOOST_TEST_MESSAGE("Testing american option...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script = "NUMBER Exercise;\n"
                         "NUMBER i;\n"
                         "FOR i IN (SIZE(Expiry), 1, -11) DO\n"
                         "    Exercise = PAY( PutCall * (Underlying(Expiry[i]) - Strike),\n"
                         "                    Expiry[i], Settlement[i], PayCcy );\n"
                         "    IF Exercise > NPV( Option, Expiry[i], Exercise > 0 ) AND Exercise > 0 THEN\n"
                         "        Option = Exercise;\n"
                         "    END;\n"
                         "END;\n"
                         "Option = Quantity * Option;\n";

    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    Real s0 = 100.0;
    Real vol = 0.18;
    Real rate = 0.01;
    Real quantity = 10.0;
    Real putcall = -1.0;
    Real strike = 100.0;

    constexpr Size nPaths = 100000;

    Schedule expirySchedule(Date(8, May, 2019), Date(9, May, 2020), 1 * Weeks, NullCalendar(), Unadjusted, Unadjusted,
                            DateGeneration::Forward, false);
    std::vector<ValueType> expiryDates, settlDates;
    for (auto const& d : expirySchedule.dates()) {
        expiryDates.push_back(EventVec{nPaths, d});
        settlDates.push_back(EventVec{nPaths, d /* + 2*/}); // for comparison with fd engine set settlment = expiry
    }

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Quantity"] = RandomVariable(nPaths, quantity);
    context->scalars["PutCall"] = RandomVariable(nPaths, putcall);
    context->scalars["Strike"] = RandomVariable(nPaths, strike);
    context->scalars["Underlying"] = IndexVec{nPaths, "EQ-SP5"};
    context->arrays["Expiry"] = expiryDates;
    context->arrays["Settlement"] = settlDates;
    context->scalars["PayCcy"] = CurrencyVec{nPaths, "USD"};
    context->scalars["Option"] = RandomVariable(nPaths, 0.0);

    auto indexInfo = QuantLib::ext::make_shared<StaticAnalyser>(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(indexInfo->run());

    // compile the sim and pay dates required by the BS model ctor below
    std::set<Date> simulationDates, payDates;
    for (auto const& s : indexInfo->indexEvalDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    for (auto const& s : indexInfo->payObsDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    simulationDates.insert(indexInfo->regressionDates().begin(), indexInfo->regressionDates().end());
    for (auto const& s : indexInfo->payPayDates())
        payDates.insert(s.second.begin(), s.second.end());

    BOOST_REQUIRE_EQUAL(simulationDates.size(), expiryDates.size());

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(ref, rate, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> yts0(QuantLib::ext::make_shared<FlatForward>(ref, 0.0, ActualActual(ActualActual::ISDA)));
    Handle<BlackVolTermStructure> volts(
        QuantLib::ext::make_shared<BlackConstantVol>(ref, NullCalendar(), vol, ActualActual(ActualActual::ISDA)));
    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);

    cpu_timer timer;
    Model::McParams mcParams;
    mcParams.regressionOrder = 6;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, "USD", yts, "EQ-SP5", "USD",
        BlackScholesModelBuilder(yts, process, simulationDates, payDates, 1).model(), mcParams, simulationDates);
    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_TEST_MESSAGE(*context);
    BOOST_REQUIRE(context->scalars["Option"].which() == ValueTypeWhich::Number);
    RandomVariable rv = QuantLib::ext::get<RandomVariable>(context->scalars["Option"]);
    BOOST_REQUIRE(rv.size() == nPaths);
    Real avg = expectation(rv).at(0);
    timer.stop();
    BOOST_TEST_MESSAGE("option value estimation " << avg << " (timing " << timer.format(default_places, "%w") << "s)");

    // compare with result from fd engine
    auto fdEngine = QuantLib::ext::make_shared<FdBlackScholesVanillaEngine>(process, 100, 100);
    VanillaOption option(QuantLib::ext::make_shared<PlainVanillaPayoff>(putcall > 0.0 ? Option::Call : Option::Put, strike),
                         QuantLib::ext::make_shared<AmericanExercise>(ref, expirySchedule.dates().back()));
    option.setPricingEngine(fdEngine);
    timer.start();
    Real fdNpv = option.NPV() * quantity;
    timer.stop();
    BOOST_TEST_MESSAGE("fd engine result " << fdNpv << " (timing " << timer.format(default_places, "%w") << "s)");
    BOOST_CHECK_CLOSE(avg, fdNpv, 5.0);
}

BOOST_AUTO_TEST_CASE(testAsianOption) {
    BOOST_TEST_MESSAGE("Testing asian option...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script = "NUMBER avg; NUMBER i;"
                         "FOR i IN (1,SIZE(ObservationDates),1) DO"
                         "  avg = avg + Underlying(ObservationDates[i]);"
                         "END;"
                         "Option = Quantity * PAY( max( PutCall * (avg / SIZE(ObservationDates) - Strike), 0),"
                         "                         Settlement, Settlement, PayCcy);";

    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    Real s0 = 100.0;
    Real vol = 0.18;
    Real rate = 0.02;
    Real quantity = 10.0;
    Real putcall = 1.0;
    Real strike = 100.0;

    constexpr Size nPaths = 10000;

    Schedule observationSchedule(Date(9, May, 2019), Date(9, May, 2020), 1 * Weeks, NullCalendar(), Unadjusted,
                                 Unadjusted, DateGeneration::Forward, false);
    std::vector<ValueType> observationDates;
    for (auto const& d : observationSchedule.dates())
        observationDates.push_back(EventVec{nPaths, d});

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Quantity"] = RandomVariable(nPaths, quantity);
    context->scalars["PutCall"] = RandomVariable(nPaths, putcall);
    context->scalars["Strike"] = RandomVariable(nPaths, strike);
    context->scalars["Underlying"] = IndexVec{nPaths, "EQ-SP5"};
    context->arrays["ObservationDates"] = observationDates;
    context->scalars["Settlement"] = observationDates.back();
    context->scalars["PayCcy"] = CurrencyVec{nPaths, "USD"};
    context->scalars["Option"] = RandomVariable(nPaths, 0.0);

    auto indexInfo = QuantLib::ext::make_shared<StaticAnalyser>(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(indexInfo->run());

    // compile the sim and pay dates required by the BS model ctor below
    std::set<Date> simulationDates, payDates;
    for (auto const& s : indexInfo->indexEvalDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    for (auto const& s : indexInfo->payObsDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    simulationDates.insert(indexInfo->regressionDates().begin(), indexInfo->regressionDates().end());
    for (auto const& s : indexInfo->payPayDates())
        payDates.insert(s.second.begin(), s.second.end());

    BOOST_REQUIRE_EQUAL(indexInfo->indexEvalDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->indexEvalDates().begin()->first, "EQ-SP5");
    BOOST_REQUIRE_EQUAL(simulationDates.size(), observationDates.size());
    Size i = 0;
    for (auto const& d : simulationDates)
        BOOST_CHECK_EQUAL(d, QuantLib::ext::get<EventVec>(observationDates[i++]).value);

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(ref, rate, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> yts0(QuantLib::ext::make_shared<FlatForward>(ref, 0.0, ActualActual(ActualActual::ISDA)));
    Handle<BlackVolTermStructure> volts(
        QuantLib::ext::make_shared<BlackConstantVol>(ref, NullCalendar(), vol, ActualActual(ActualActual::ISDA)));
    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);

    cpu_timer timer;
    Model::McParams mcParams;
    mcParams.regressionOrder = 6;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, "USD", yts, "EQ-SP5", "USD",
        BlackScholesModelBuilder(yts, process, simulationDates, payDates, 1).model(), mcParams, simulationDates);
    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["Option"].which() == ValueTypeWhich::Number);
    RandomVariable rv = QuantLib::ext::get<RandomVariable>(context->scalars["Option"]);
    BOOST_REQUIRE(rv.size() == nPaths);
    Real avg = expectation(rv).at(0);
    timer.stop();
    BOOST_TEST_MESSAGE("option value estimation " << avg << " (timing " << timer.format(default_places, "%w") << "s)");

    // hardcoded version of the script
    std::vector<Real> times;
    for (auto const& d : observationDates)
        times.push_back(process->riskFreeRate()->timeFromReference(QuantLib::ext::get<EventVec>(d).value));
    auto pg = makeMultiPathGenerator(SobolBrownianBridge, process, TimeGrid(times.begin(), times.end()),
                                     SobolBrownianGenerator::Steps);
    double avg2 = 0.0;
    timer.start();
    for (Size path = 0; path < nPaths; ++path) {
        MultiPath p = pg->next().value;
        Real payoff = 0.0;
        for (Size i = 1; i < p[0].length(); ++i)
            payoff += p[0][i];
        Real v = std::max(payoff / static_cast<double>(observationDates.size()) - strike, 0.0);
        avg2 += v;
    }
    avg2 *= quantity * process->riskFreeRate()->discount(QuantLib::ext::get<EventVec>(observationDates.back()).value) /
            static_cast<double>(nPaths);
    timer.stop();
    BOOST_TEST_MESSAGE("result with hardcoded script " << avg2 << "(timing " << timer.format(default_places, "%w")
                                                       << "s)");
    BOOST_CHECK_CLOSE(avg, avg2, 1E-10);
}

BOOST_AUTO_TEST_CASE(testAutocallable) {
    BOOST_TEST_MESSAGE("Testing autocallable...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::string script =
        "NUMBER StrikePrice, KnockInPrice, Value;\n"
        "NUMBER terminated, knockedIn, u, v;\n"
        "FOR u IN (1, SIZE(Underlying), 1) DO\n"
        "    StrikePrice = StrikePrice + Underlying[u](StrikeDate);\n"
        "END;\n"
        "StrikePrice = StrikePrice / SIZE(Underlying);\n"
        "KnockInPrice = KnockInRatio * StrikePrice;\n"
        "FOR v IN (1, SIZE(Valuation), 1) DO\n"
        "    Value = 0;\n"
        "    FOR u IN (1, SIZE(Underlying), 1) DO\n"
        "        Value = Value + Underlying[u](Valuation[v]);\n"
        "    END;\n"
        "    Value = Value / SIZE(Underlying);\n"
        "    IF Value < KnockInPrice THEN\n"
        "        knockedIn = 1;\n"
        "    END;\n"
        "    IF v == SIZE(Valuation) THEN\n"
        "        IF knockedIn == 1 AND terminated == 0 THEN\n"
        "            Option = PAY(Notional * ( 1 - Value / StrikePrice), Valuation[v], Settlement[v], PayCcy);\n"
        "        END;\n"
        "    ELSE\n"
        "        IF v > 1 AND terminated == 0 THEN\n"
        "            IF Value > StrikePrice THEN\n"
        "                Option = PAY(Notional * v * 0.06, Valuation[v], Settlement[v], PayCcy);\n"
        "                terminated = 1;\n"
        "            END;\n"
        "        END;\n"
        "    END;\n"
        "END;\n";

    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    Real s0 = 100.0;
    Real vol = 0.18;
    Real rate = 0.02;
    Real notional = 1000.0;
    Real strike = 100.0;
    Real knockInRatio = 0.7;

    constexpr Size nPaths = 10000;

    Schedule observationSchedule(Date(9, May, 2019), Date(9, May, 2020), 1 * Months, NullCalendar(), Unadjusted,
                                 Unadjusted, DateGeneration::Forward, false);
    std::vector<ValueType> observationDates, settlementDates;
    for (Size i = 1; i < observationSchedule.dates().size(); ++i) {
        Date d = observationSchedule.date(i);
        observationDates.push_back(EventVec{nPaths, d});
        settlementDates.push_back(EventVec{nPaths, d + 5});
    }
    std::vector<Date> expectedSimDates = observationSchedule.dates(); // inclusive the strike date
    std::vector<std::string> indicesStr = {"EQ-1", "EQ-2", "EQ-3"};
    std::vector<ValueType> indices;
    for (auto const& i : indicesStr)
        indices.push_back(IndexVec{nPaths, i});

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Notional"] = RandomVariable(nPaths, notional);
    context->scalars["Strike"] = RandomVariable(nPaths, strike);
    context->scalars["StrikeDate"] = EventVec{nPaths, observationSchedule.dates().front()};
    context->scalars["KnockInRatio"] = RandomVariable(nPaths, knockInRatio);
    context->arrays["Underlying"] = indices;
    context->arrays["Valuation"] = observationDates;
    context->arrays["Settlement"] = settlementDates;
    context->scalars["PayCcy"] = CurrencyVec{nPaths, "USD"};
    context->scalars["Option"] = RandomVariable(nPaths, 0.0);

    auto indexInfo = QuantLib::ext::make_shared<StaticAnalyser>(parser.ast(), context);
    BOOST_REQUIRE_NO_THROW(indexInfo->run());
    BOOST_REQUIRE_EQUAL(indexInfo->indexEvalDates().size(), 3);
    Size i = 0;
    for (auto const& k : indexInfo->indexEvalDates())
        BOOST_CHECK_EQUAL(k.first, indicesStr[i++]);
    for (auto const& ind : indicesStr) {
        Size i = 0;
        BOOST_CHECK_EQUAL(indexInfo->indexEvalDates().at(ind).size(), expectedSimDates.size());
        for (auto const& d : indexInfo->indexEvalDates().at(ind))
            BOOST_CHECK_EQUAL(d, expectedSimDates[i++]);
    }
    BOOST_REQUIRE_EQUAL(indexInfo->payObsDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->payObsDates().begin()->first, "USD");
    i = 0;
    BOOST_REQUIRE_EQUAL(indexInfo->payObsDates().begin()->second.size(), observationDates.size());
    for (auto const& d : indexInfo->payObsDates().begin()->second) {
        BOOST_CHECK_EQUAL(d, QuantLib::ext::get<EventVec>(observationDates[i++]).value);
    }
    BOOST_REQUIRE_EQUAL(indexInfo->payPayDates().size(), 1);
    BOOST_CHECK_EQUAL(indexInfo->payPayDates().begin()->first, "USD");
    i = 0;
    BOOST_REQUIRE_EQUAL(indexInfo->payPayDates().begin()->second.size(), settlementDates.size());
    for (auto const& d : indexInfo->payPayDates().begin()->second) {
        BOOST_CHECK_EQUAL(d, QuantLib::ext::get<EventVec>(settlementDates[i++]).value);
    }

    // compile the sim and pay dates required by the BS model ctor below
    std::set<Date> simulationDates, payDates;
    for (auto const& s : indexInfo->indexEvalDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    for (auto const& s : indexInfo->payObsDates())
        simulationDates.insert(s.second.begin(), s.second.end());
    simulationDates.insert(indexInfo->regressionDates().begin(), indexInfo->regressionDates().end());
    for (auto const& s : indexInfo->payPayDates())
        payDates.insert(s.second.begin(), s.second.end());

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(ref, rate, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> yts0(QuantLib::ext::make_shared<FlatForward>(ref, 0.0, ActualActual(ActualActual::ISDA)));
    Handle<BlackVolTermStructure> volts(
        QuantLib::ext::make_shared<BlackConstantVol>(ref, NullCalendar(), vol, ActualActual(ActualActual::ISDA)));
    auto process1 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);
    auto process2 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);
    auto process3 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), yts0, yts, volts);
    std::vector<QuantLib::ext::shared_ptr<QuantLib::StochasticProcess1D>> processes = {process1, process2, process3};
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processesBs = {process1, process2, process3};
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations;
    correlations[std::make_pair("EQ-1", "EQ-2")] = Handle<QuantExt::CorrelationTermStructure>(
        QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.5, ActualActual(ActualActual::ISDA)));
    correlations[std::make_pair("EQ-1", "EQ-3")] = Handle<QuantExt::CorrelationTermStructure>(
        QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.4, ActualActual(ActualActual::ISDA)));
    correlations[std::make_pair("EQ-2", "EQ-3")] = Handle<QuantExt::CorrelationTermStructure>(
        QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.6, ActualActual(ActualActual::ISDA)));
    cpu_timer timer;
    Model::McParams mcParams;
    mcParams.regressionOrder = 6;
    auto model = QuantLib::ext::make_shared<BlackScholes>(
        nPaths, std::vector<std::string>(1, "USD"), std::vector<Handle<YieldTermStructure>>(1, yts),
        std::vector<Handle<Quote>>(), std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>(),
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>(), indicesStr,
        std::vector<std::string>(3, "USD"),
        BlackScholesModelBuilder({yts}, processesBs, simulationDates, payDates, 24).model(), correlations, mcParams,
        simulationDates);
    ScriptEngine engine(parser.ast(), context, model);
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_REQUIRE(context->scalars["Option"].which() == ValueTypeWhich::Number);
    RandomVariable rv = QuantLib::ext::get<RandomVariable>(context->scalars["Option"]);
    BOOST_REQUIRE(rv.size() == nPaths);
    Real avg = expectation(rv).at(0);
    timer.stop();
    BOOST_TEST_MESSAGE("option value estimation " << avg << " (timing " << timer.format(default_places, "%w") << "s)");
    BOOST_TEST_MESSAGE(*context);

    // hardcoded version of the script
    std::vector<Real> times;
    for (auto const& d : expectedSimDates)
        times.push_back(yts->timeFromReference(d));
    TimeGrid grid(times.begin(), times.end(), 1);
    std::vector<Size> positionInTimeGrid(times.size());
    for (Size i = 0; i < times.size(); ++i)
        positionInTimeGrid[i] = grid.index(times[i]);

    for (Size i = 0; i < times.size(); ++i)
        BOOST_TEST_MESSAGE("time point #" << i << ": " << times[i] << ", position in grid " << positionInTimeGrid[i]);
    for (Size i = 0; i < grid.size(); ++i)
        BOOST_TEST_MESSAGE("grid point #" << i << ": " << grid[i]);

    Matrix correlation{{1.0, 0.5, 0.4}, {0.5, 1.0, 0.6}, {0.4, 0.6, 1.0}};
    auto process = QuantLib::ext::make_shared<StochasticProcessArray>(processes, correlation);
    auto pg = makeMultiPathGenerator(SobolBrownianBridge, process, grid, SobolBrownianGenerator::Steps);
    double avg2 = 0.0;
    timer.start();
    constexpr Size nUnd = 3;
    const Size nObs = observationDates.size();
    for (Size path = 0; path < nPaths; ++path) {
        MultiPath p = pg->next().value;
        // script translated to c++
        Real Option = 0.0;
        Real StrikePrice = 0.0, KnockInPrice = 0.0, Value = 0.0;
        Size terminated = 0, knockedIn = 0;
        for (Size u = 0; u < nUnd; ++u)
            StrikePrice += p[u][positionInTimeGrid[1 - 1]];
        StrikePrice /= nUnd;
        KnockInPrice = knockInRatio * StrikePrice;
        for (Size v = 0; v < nObs; ++v) {
            Value = 0;
            for (Size u = 0; u < nUnd; ++u) {
                Value += p[u][positionInTimeGrid[v + 2 - 1]];
            }
            Value /= nUnd;
            if (Value < KnockInPrice && !close_enough(Value, KnockInPrice))
                knockedIn = 1;
            if (v == nObs - 1) {
                if (knockedIn == 1 && terminated == 0) {
                    Option = notional * (1 - Value / StrikePrice) *
                             yts->discount(QuantLib::ext::get<EventVec>(settlementDates[v]).value);
                }
            } else {
                if (v > 0 && terminated == 0) {
                    if (Value > StrikePrice && !close_enough(Value, StrikePrice)) {
                        Option =
                            notional * (v + 1) * 0.06 * yts->discount(QuantLib::ext::get<EventVec>(settlementDates[v]).value);
                        terminated = 1;
                    }
                }
            }
        }
        // script translation end
        avg2 += Option;
    }
    avg2 /= nPaths;
    timer.stop();
    BOOST_TEST_MESSAGE("result with hardcoded script " << avg2 << "(timing " << timer.format(default_places, "%w")
                                                       << "s)");
    // 1% tolerance, since hardcoded impl uses pseudoSqrt() while BlackScholes uses CholeskyDecomposition
    BOOST_CHECK_CLOSE(avg, avg2, 1.0);
}

BOOST_AUTO_TEST_CASE(testNestedIfThenElse) {
    BOOST_TEST_MESSAGE("Testing nested if-then-else statements...");

    std::string script = "IF x < 8 THEN\n"
                         "  IF x < 4 THEN\n"
                         "    IF x < 2 THEN\n"
                         "      IF x < 1 THEN\n"
                         "        y = 0;\n"
                         "      ELSE\n"
                         "        y = 1;\n"
                         "      END;\n"
                         "    ELSE\n"
                         "      IF x < 3 THEN\n"
                         "        y = 2;\n"
                         "      ELSE\n"
                         "        y = 3;\n"
                         "      END;\n"
                         "    END;\n"
                         "  ELSE\n"
                         "    IF x < 6 THEN\n"
                         "      IF x < 5 THEN\n"
                         "        y = 4;\n"
                         "      ELSE\n"
                         "        y = 5;\n"
                         "      END;\n"
                         "    ELSE\n"
                         "      IF x < 7 THEN\n"
                         "        y = 6;\n"
                         "      ELSE\n"
                         "        y = 7;\n"
                         "      END;\n"
                         "    END;\n"
                         "  END;\n"
                         "ELSE\n"
                         "  IF x < 12 THEN\n"
                         "    IF x < 10 THEN\n"
                         "      IF x < 9 THEN\n"
                         "        y = 8;\n"
                         "      ELSE\n"
                         "        y = 9;\n"
                         "      END;\n"
                         "    ELSE\n"
                         "      IF x < 11 THEN\n"
                         "        y = 10;\n"
                         "      ELSE\n"
                         "        y = 11;\n"
                         "      END;\n"
                         "    END;\n"
                         "  ELSE\n"
                         "    IF x < 14 THEN\n"
                         "      IF x < 13 THEN\n"
                         "        y = 12;\n"
                         "      ELSE\n"
                         "        y = 13;\n"
                         "      END;\n"
                         "    ELSE\n"
                         "      IF x < 15 THEN\n"
                         "        y = 14;\n"
                         "      ELSE\n"
                         "        y = 15;\n"
                         "      END;\n"
                         "    END;\n"
                         "  END;\n"
                         "END;\n";

    ScriptParser parser(script);
    BOOST_REQUIRE(parser.success());
    BOOST_TEST_MESSAGE("Parsing successful, AST:\n" << to_string(parser.ast()));

    auto context = QuantLib::ext::make_shared<Context>();

    RandomVariable x(16), y(16);
    for (Size i = 0; i < 16; ++i)
        x.set(i, static_cast<Real>(i));

    context->scalars["x"] = x;
    context->scalars["y"] = y;

    ScriptEngine engine(parser.ast(), context, QuantLib::ext::make_shared<DummyModel>(16));
    BOOST_REQUIRE_NO_THROW(engine.run());
    BOOST_TEST_MESSAGE("Script Engine successfully run, context is:\n" << *context);

    BOOST_REQUIRE(y.size() == 16);
    for (Size i = 0; i < 16; ++i) {
        BOOST_CHECK_EQUAL(x.at(i), i);
    }
}

BOOST_AUTO_TEST_CASE(testInteractive, *boost::unit_test::disabled()) {

    // not a test, just for convenience, to be removed at some stage...

    BOOST_TEST_MESSAGE("Running Script Engine on INPUT env variable...");

    std::string script = "NUMBER i,x;"
                         "FOR i IN (1,10,1) DO x=x+i; END;";
    if (auto c = getenv("INPUT"))
        script = std::string(c);

    ScriptParser parser(script);
    if (parser.success()) {
        std::cerr << "Parsing succeeded\n" << to_string(parser.ast()) << std::endl;
    } else {
        std::cerr << "Parsing failed\n" << parser.error();
    }

    auto context = QuantLib::ext::make_shared<Context>();
    ScriptEngine engine(parser.ast(), context, nullptr);
    try {
        engine.run();
        std::cerr << "Script successfully executed, context is:\n" << *context << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR during script execution: " << e.what() << std::endl;
        std::cerr << *context << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
