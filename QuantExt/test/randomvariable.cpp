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
#include <qle/math/randomvariable_regressioncache.hpp>

#include <cmath>

#include <ql/time/date.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/math/distributions/normal.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantExt;
using namespace QuantLib;

BOOST_AUTO_TEST_SUITE(QuantExtTestSuite)

BOOST_AUTO_TEST_SUITE(RandomVariableTest)

BOOST_AUTO_TEST_CASE(testSvdRegressionExcludedRows) {
    BOOST_TEST_MESSAGE("Testing SVD regression rank tolerance with excluded paths...");

    const Real small = std::ldexp(1.0, -49);
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>> basis = {
        [](const std::vector<const RandomVariable*>& x) { return *x[0]; },
        [](const std::vector<const RandomVariable*>& x) { return *x[1]; }};

    // The two active rows form diag(1, 2^-49). Padding with excluded paths
    // must not change which singular values are retained.
    for (Size n : {Size(2), Size(16), Size(64)}) {
        RandomVariable x(n, 17.0), z(n, -23.0), y(n, 101.0);
        Filter active(n, false);
        // Interleave padding as well as append it: active-row positions are irrelevant.
        const Size first = n == 2 ? 0 : n / 2;
        const Size second = n - 1;
        x.set(first, 1.0);
        z.set(first, 0.0);
        y.set(first, 1.0);
        x.set(second, 0.0);
        z.set(second, small);
        y.set(second, small);
        active.set(first, true);
        active.set(second, true);
        const std::vector<const RandomVariable*> regressors = {&x, &z};
        RandomVariableRegressionCache cache;
        for (Size repeat = 0; repeat < 2; ++repeat) {
            const Array c = regressionCoefficients(y, regressors, basis, active,
                RandomVariableRegressionMethod::SVD, "", &cache);
            BOOST_CHECK_SMALL(c[0] - 1.0, 1e-12);
            BOOST_CHECK_SMALL(c[1] - 1.0, 1e-12);
        }
        BOOST_CHECK_EQUAL(cache.miss(), 1);
        BOOST_CHECK_EQUAL(cache.hit(), 1);
        const Array qr = regressionCoefficients(y, regressors, basis, active,
                                                RandomVariableRegressionMethod::QR);
        BOOST_CHECK_SMALL(qr[0] - 1.0, 1e-12);
        BOOST_CHECK_SMALL(qr[1] - 1.0, 1e-12);
    }

    // Compare with physically compacted active observations on a non-diagonal,
    // overdetermined design, rather than relying only on the diagonal witness.
    RandomVariable compactX(4, 0.0), compactZ(4, 0.0), compactY(4, 0.0);
    RandomVariable paddedX(20, 37.0), paddedZ(20, -19.0), paddedY(20, 73.0);
    Filter included(20, false);
    const Real xs[] = {1.0, 2.0, -1.0, 3.0};
    const Real zs[] = {2.0, -1.0, 4.0, 1.0};
    const Real ys[] = {3.0, 7.0, -2.0, 11.0};
    const Size positions[] = {2, 8, 13, 19};
    for (Size i = 0; i < 4; ++i) {
        compactX.set(i, xs[i]);
        compactZ.set(i, zs[i]);
        compactY.set(i, ys[i]);
        paddedX.set(positions[i], xs[i]);
        paddedZ.set(positions[i], zs[i]);
        paddedY.set(positions[i], ys[i]);
        included.set(positions[i], true);
    }
    const Array compact = regressionCoefficients(compactY, {&compactX, &compactZ}, basis,
                                                 Filter(), RandomVariableRegressionMethod::SVD);
    const Array padded = regressionCoefficients(paddedY, {&paddedX, &paddedZ}, basis,
                                                included, RandomVariableRegressionMethod::SVD);
    for (Size i = 0; i < compact.size(); ++i)
        BOOST_CHECK_SMALL(padded[i] - compact[i], 1e-12);

    // Unfiltered zero-valued observations still count. This patch does not
    // redefine the existing rank policy for an unfiltered design matrix.
    RandomVariable x(16, 0.0), z(16, 0.0), y(16, 0.0);
    x.set(0, 1.0);
    z.set(1, small);
    y.set(0, 1.0);
    y.set(1, small);
    const std::vector<const RandomVariable*> regressors = {&x, &z};
    for (const Filter& f : {Filter(), Filter(16, true)}) {
        const Array c = regressionCoefficients(y, regressors, basis, f,
                                                RandomVariableRegressionMethod::SVD);
        BOOST_CHECK_SMALL(c[0] - 1.0, 1e-12);
        BOOST_CHECK_SMALL(c[1], 1e-12);
    }

    const Array none = regressionCoefficients(y, regressors, basis, Filter(16, false),
                                              RandomVariableRegressionMethod::SVD);
    BOOST_CHECK_SMALL(none[0], 1e-12);
    BOOST_CHECK_SMALL(none[1], 1e-12);

    // Fewer active observations than columns: keep the column-count floor
    // in max(active rows, columns); the pseudoinverse is still well-defined.
    Filter one(16, false);
    one.set(0, true);
    const Array single = regressionCoefficients(y, regressors, basis, one,
                                                RandomVariableRegressionMethod::SVD);
    BOOST_CHECK_SMALL(single[0] - 1.0, 1e-12);
    BOOST_CHECK_SMALL(single[1], 1e-12);
}

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
    BOOST_CHECK_CLOSE((x * Y).at(0), x * y, tol);
    BOOST_CHECK_CLOSE((X / Y).at(0), x / y, tol);
    BOOST_CHECK_CLOSE((x / Y).at(0), x / y, tol);
    BOOST_CHECK_CLOSE((X / y).at(0), x / y, tol);

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
