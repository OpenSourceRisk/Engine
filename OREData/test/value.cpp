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

#include <ored/scripting/value.hpp>

#include <oret/toplevelfixture.hpp>

#include <ql/time/date.hpp>
#include <ql/tuple.hpp>

#include <iostream>
#include <iomanip>

using namespace ore::data;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ValueTypeTest)

BOOST_AUTO_TEST_CASE(testValueRandomVariableBinaryOps) {
    BOOST_TEST_MESSAGE("Testing Value RandomVariable binary ops...");
    constexpr Size n = 1;
    RandomVariable a(n, 23), b(n, -10);
    ValueType x(a), y(b);
    BOOST_REQUIRE_EQUAL(x.which(), 0);
    BOOST_REQUIRE_EQUAL(y.which(), 0);
    ValueType z = x + y;
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), a + b);
    z = x - y;
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), a - b);
    z = x * y;
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), a * b);
    z = x / y;
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), a / b);
    z = min(x, y);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), min(a, b));
    z = max(x, y);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), max(a, b));
    z = pow(x, y);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), pow(a, b));
}

BOOST_AUTO_TEST_CASE(testValueRandomVariableUnaryOps) {
    BOOST_TEST_MESSAGE("Testing Value RandomVariable unary ops...");
    constexpr Size n = 1;
    RandomVariable a(n, 3.0);
    ValueType x(a);
    BOOST_REQUIRE_EQUAL(x.which(), 0);
    ValueType z = -x;
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), -a);
    z = abs(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), abs(a));
    z = exp(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), exp(a));
    z = log(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), log(a));
    z = sqrt(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), sqrt(a));
    z = normalCdf(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), normalCdf(a));
    z = normalPdf(x);
    BOOST_REQUIRE_EQUAL(z.which(), 0);
    BOOST_CHECK_EQUAL(QuantLib::ext::get<RandomVariable>(z), normalPdf(a));
}

BOOST_AUTO_TEST_CASE(testValueIllegalOps) {
    BOOST_TEST_MESSAGE("Testing Value illegal ops...");
    constexpr Size n = 1;
    ValueType a = RandomVariable(n, 23.0);
    ValueType c = EventVec{n, Date()};
    ValueType d = CurrencyVec{n, ""};
    ValueType e = IndexVec{n, ""};
    BOOST_CHECK_THROW(a + c, QuantLib::Error);
    BOOST_CHECK_THROW(a + d, QuantLib::Error);
    BOOST_CHECK_THROW(a + e, QuantLib::Error);
    BOOST_CHECK_THROW(a - c, QuantLib::Error);
    BOOST_CHECK_THROW(a - d, QuantLib::Error);
    BOOST_CHECK_THROW(a - e, QuantLib::Error);
    BOOST_CHECK_THROW(a * c, QuantLib::Error);
    BOOST_CHECK_THROW(a * d, QuantLib::Error);
    BOOST_CHECK_THROW(a * e, QuantLib::Error);
    BOOST_CHECK_THROW(a / c, QuantLib::Error);
    BOOST_CHECK_THROW(a / d, QuantLib::Error);
    BOOST_CHECK_THROW(a / e, QuantLib::Error);
    BOOST_CHECK_THROW(min(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(min(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(min(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(max(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(max(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(max(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(pow(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(pow(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(pow(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(-c, QuantLib::Error);
    BOOST_CHECK_THROW(-d, QuantLib::Error);
    BOOST_CHECK_THROW(-e, QuantLib::Error);
    BOOST_CHECK_THROW(abs(c), QuantLib::Error);
    BOOST_CHECK_THROW(abs(d), QuantLib::Error);
    BOOST_CHECK_THROW(abs(e), QuantLib::Error);
    BOOST_CHECK_THROW(exp(c), QuantLib::Error);
    BOOST_CHECK_THROW(exp(d), QuantLib::Error);
    BOOST_CHECK_THROW(exp(e), QuantLib::Error);
    BOOST_CHECK_THROW(log(c), QuantLib::Error);
    BOOST_CHECK_THROW(log(d), QuantLib::Error);
    BOOST_CHECK_THROW(log(e), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testAssignments) {
    BOOST_TEST_MESSAGE("Testing Value assignments...");
    constexpr Size n = 1;
    RandomVariable a(n, 10);
    EventVec c{n, Date(2, Jan, 2017)};
    CurrencyVec d{n, "EUR"};
    IndexVec e{n, "EUR-EURIBOR-6M"};
    ValueType a2(RandomVariable(n, 0.0));
    ValueType c2(EventVec{n, Date()});
    ValueType d2(CurrencyVec{n, ""});
    ValueType e2(IndexVec{n, ""});
    // type = type
    BOOST_REQUIRE_NO_THROW(typeSafeAssign(a2, a););
    BOOST_REQUIRE_EQUAL(a2, ValueType(a));
    try {
        typeSafeAssign(c2, c);
    } catch (const std::exception& e) {
        BOOST_FAIL(e.what());
    }
    BOOST_REQUIRE_NO_THROW(typeSafeAssign(c2, c););
    BOOST_REQUIRE_EQUAL(c2, ValueType(c));
    BOOST_REQUIRE_NO_THROW(typeSafeAssign(d2, d););
    BOOST_REQUIRE_EQUAL(d2, ValueType(d));
    BOOST_REQUIRE_NO_THROW(typeSafeAssign(e2, e););
    BOOST_REQUIRE_EQUAL(e2, ValueType(e));
}

BOOST_AUTO_TEST_CASE(testIllegalAssignments) {
    BOOST_TEST_MESSAGE("Testing Value illegal assignments...");
    constexpr Size n = 1;
    RandomVariable a(n, 10);
    EventVec c{n, Date(2, Jan, 2017)};
    CurrencyVec d{n, "EUR"};
    IndexVec e{n, "EUR-EURIBOR-6M"};
    ValueType a2(RandomVariable(n, 0.0));
    ValueType c2(EventVec{n, Date()});
    ValueType d2(CurrencyVec{n, ""});
    ValueType e2(IndexVec{n, ""});
    // Illegal assignments
    BOOST_CHECK_THROW(typeSafeAssign(a2, c);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(a2, d);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(a2, e);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(c2, a);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(c2, d);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(c2, e);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(d2, a);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(d2, c);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(d2, e);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(e2, a);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(e2, c);, QuantLib::Error);
    BOOST_CHECK_THROW(typeSafeAssign(e2, d);, QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testComparisons) {
    BOOST_TEST_MESSAGE("Testing Value comparisons...");
    constexpr Size n = 1;
    ValueType a = RandomVariable(n, 10.0), a2 = RandomVariable(n, 11.0);
    ValueType c = EventVec{n, Date(2, Jan, 2017)}, c2 = EventVec{n, Date(3, Jan, 2017)};
    ValueType d = CurrencyVec{n, "EUR"}, d2 = CurrencyVec{n, "USD"};
    ValueType e = IndexVec{n, "EUR-EURIBOR-6M"}, e2 = IndexVec{n, "USD-LIBOR-3M"};
    // RandomVariable
    BOOST_CHECK(equal(a, a).at(0));
    BOOST_CHECK(notequal(a, a2).at(0));
    BOOST_CHECK(lt(a, a2).at(0));
    BOOST_CHECK(leq(a, a).at(0));
    BOOST_CHECK(gt(a2, a).at(0));
    BOOST_CHECK(geq(a, a).at(0));
    // EventVec
    BOOST_CHECK(equal(c, c).at(0));
    BOOST_CHECK(notequal(c, c2).at(0));
    BOOST_CHECK(lt(c, c2).at(0));
    BOOST_CHECK(leq(c, c).at(0));
    BOOST_CHECK(gt(c2, c).at(0));
    BOOST_CHECK(geq(c, c).at(0));
    // CurrencyVec
    BOOST_CHECK(equal(d, d).at(0));
    BOOST_CHECK(notequal(d, d2).at(0));
    BOOST_CHECK_THROW(lt(d, d2), QuantLib::Error);
    BOOST_CHECK_THROW(leq(d, d2), QuantLib::Error);
    BOOST_CHECK_THROW(gt(d, d2), QuantLib::Error);
    BOOST_CHECK_THROW(geq(d, d2), QuantLib::Error);
    // IndexVec
    BOOST_CHECK(equal(e, e).at(0));
    BOOST_CHECK(notequal(e, e2).at(0));
    BOOST_CHECK_THROW(lt(e, e2), QuantLib::Error);
    BOOST_CHECK_THROW(leq(e, e2), QuantLib::Error);
    BOOST_CHECK_THROW(gt(e, e2), QuantLib::Error);
    BOOST_CHECK_THROW(geq(e, e2), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testIllegalComparisons) {
    BOOST_TEST_MESSAGE("Testing Value illegal comparisons...");
    constexpr Size n = 1;
    ValueType a = RandomVariable(n, 23.0);
    ValueType c = EventVec{n, Date()};
    ValueType d = CurrencyVec{n, ""};
    ValueType e = IndexVec{n, ""};
    BOOST_CHECK_THROW(equal(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(equal(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(equal(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(equal(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(equal(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(equal(d, e), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(notequal(d, e), QuantLib::Error);
    BOOST_CHECK_THROW(lt(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(lt(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(lt(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(lt(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(lt(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(lt(d, e), QuantLib::Error);
    BOOST_CHECK_THROW(leq(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(leq(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(leq(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(leq(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(leq(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(leq(d, e), QuantLib::Error);
    BOOST_CHECK_THROW(gt(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(gt(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(gt(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(gt(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(gt(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(gt(d, e), QuantLib::Error);
    BOOST_CHECK_THROW(geq(a, c), QuantLib::Error);
    BOOST_CHECK_THROW(geq(a, d), QuantLib::Error);
    BOOST_CHECK_THROW(geq(a, e), QuantLib::Error);
    BOOST_CHECK_THROW(geq(c, d), QuantLib::Error);
    BOOST_CHECK_THROW(geq(c, e), QuantLib::Error);
    BOOST_CHECK_THROW(geq(d, e), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
