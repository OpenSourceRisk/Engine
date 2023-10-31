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

#include "toplevelfixture.hpp"

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <qle/math/randomvariable.hpp>

#include <ql/time/date.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/math/distributions/normal.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantExt;
using namespace QuantLib;

BOOST_AUTO_TEST_SUITE(QuantExtTestSuite)

BOOST_AUTO_TEST_SUITE(RandomVariableTest)

BOOST_AUTO_TEST_CASE(testFilter) {
    BOOST_TEST_MESSAGE("Testing filter...");

    Filter f(100, false);
    BOOST_CHECK(f.deterministic());

    f.set(5, true);
    BOOST_CHECK(!f.deterministic());
    BOOST_CHECK_EQUAL(f[0], false);
    BOOST_CHECK_EQUAL(f[5], true);
    BOOST_CHECK_EQUAL(f[10], false);

    Filter g(100, true);
    Filter r = f && g;
    ;
    BOOST_CHECK_EQUAL(r[0], false);
    BOOST_CHECK_EQUAL(r[5], true);

    Filter h(100, false);
    r = f && h;
    BOOST_CHECK_EQUAL(r[0], false);
    BOOST_CHECK_EQUAL(r[5], false);
    BOOST_CHECK(r.deterministic());

    g.set(0, false);
    r = f && g;
    BOOST_CHECK_EQUAL(r[0], false);
    BOOST_CHECK_EQUAL(r[5], true);

    g.set(5, false);
    r = f && g;
    BOOST_CHECK_EQUAL(r[0], false);
    BOOST_CHECK_EQUAL(r[5], false);

    g.set(10, true);
    r = f || g;
    BOOST_CHECK_EQUAL(r[0], false);
    BOOST_CHECK_EQUAL(r[5], true);
    BOOST_CHECK_EQUAL(r[10], true);

    r = !r;
    BOOST_CHECK_EQUAL(r[0], true);
    BOOST_CHECK_EQUAL(r[5], false);
    BOOST_CHECK_EQUAL(r[10], false);

    Filter x(100, false), y(100, true);
    BOOST_CHECK((x && y).deterministic());
    BOOST_CHECK((x || y).deterministic());
    BOOST_CHECK((!x).deterministic());

    Filter z(200, false);
    BOOST_CHECK_THROW(x && z, QuantLib::Error);
    BOOST_CHECK_THROW(x || z, QuantLib::Error);

    BOOST_CHECK_THROW(r.at(100), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testRandomVariable) {
    BOOST_TEST_MESSAGE("Testing random variable...");

    double tol = 1E-10;

    RandomVariable r(100, 1.0);
    BOOST_CHECK(r.deterministic());

    r.set(5, 2.0);
    BOOST_CHECK(!r.deterministic());
    BOOST_CHECK_CLOSE(r[0], 1.0, tol);
    BOOST_CHECK_CLOSE(r[5], 2.0, tol);
    BOOST_CHECK_CLOSE(r[10], 1.0, tol);

    RandomVariable s = r + r;
    BOOST_CHECK_CLOSE(s[0], 2.0, tol);
    BOOST_CHECK_CLOSE(s[5], 4.0, tol);
    BOOST_CHECK_CLOSE(s[10], 2.0, tol);

    RandomVariable x(100), y(100);
    BOOST_CHECK((x + y).deterministic());
    BOOST_CHECK((x - y).deterministic());
    BOOST_CHECK((x * y).deterministic());
    BOOST_CHECK((x / y).deterministic());

    x.set(5, 2.0);
    y.set(5, 2.0);
    Filter c = close_enough(x, y);
    for (Size i = 0; i < c.size(); ++i)
        BOOST_CHECK_EQUAL(c[i], true);
    BOOST_CHECK(close_enough_all(x, y));

    y.set(5, 3.0);
    c = close_enough(x, y);
    BOOST_CHECK(!c.deterministic());
    BOOST_CHECK_EQUAL(c[0], true);
    BOOST_CHECK_EQUAL(c[5], false);

    BOOST_CHECK_THROW(r.at(100), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testFunctions) {
    BOOST_TEST_MESSAGE("Testing functions...");

    double tol = 1E-12;
    double x = 2.0, y = -2.0;
    RandomVariable X(1, x), Y(1, y);
    boost::math::normal_distribution<double> n;

    BOOST_CHECK_CLOSE((X + Y).at(0), x + y, tol);
    BOOST_CHECK_CLOSE((X - Y).at(0), x - y, tol);
    BOOST_CHECK_CLOSE((X * Y).at(0), x * y, tol);
    BOOST_CHECK_CLOSE((X / Y).at(0), x / y, tol);

    BOOST_CHECK_CLOSE((-X).at(0), -x, tol);
    BOOST_CHECK_CLOSE((QuantExt::abs(Y)).at(0), std::abs(y), tol);
    BOOST_CHECK_CLOSE((QuantExt::exp(X)).at(0), std::exp(x), tol);
    BOOST_CHECK_CLOSE((QuantExt::log(X)).at(0), std::log(x), tol);
    BOOST_CHECK_CLOSE((QuantExt::sqrt(X)).at(0), std::sqrt(x), tol);

    BOOST_CHECK_CLOSE((normalCdf(X)).at(0), boost::math::cdf(n, x), tol);
    BOOST_CHECK_CLOSE((normalPdf(X)).at(0), boost::math::pdf(n, x), tol);
}

BOOST_AUTO_TEST_CASE(testBlack) {
    BOOST_TEST_MESSAGE("Testing black formula...");

    Option::Type type[4] = {Option::Call, Option::Call, Option::Put, Option::Put};

    RandomVariable omega(4), t(4), strike(4), forward(4), impliedVol(4);

    omega.set(0, 1.0);
    omega.set(1, 1.0);
    omega.set(2, -1.0);
    omega.set(3, -1.0);

    t.setAll(10.0);

    strike.set(0, 98.0);
    strike.set(1, 0.0);
    strike.set(2, 98.0);
    strike.set(3, 0.0);

    forward.setAll(100.0);
    impliedVol.setAll(0.2);

    RandomVariable res = black(omega, t, strike, forward, impliedVol);

    for (Size i = 0; i < 4; ++i) {
        BOOST_CHECK_CLOSE(res.at(i),
                          blackFormula(type[i], strike.at(i), forward.at(i), impliedVol.at(i) * std::sqrt(t.at(i))),
                          1E-12);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
