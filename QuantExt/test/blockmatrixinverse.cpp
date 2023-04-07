/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/math/blockmatrixinverse.hpp>

#include "toplevelfixture.hpp"

#include <ql/math/comparison.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlockMatrixInverseTest)

namespace {
void check(const Matrix res, const Matrix ex) {
    QL_REQUIRE(res.rows() == ex.rows(), "different number of rows (" << res.rows() << ", expected " << ex.rows());
    QL_REQUIRE(res.columns() == ex.columns(),
               "different number of columns (" << res.columns() << ", expected " << ex.columns());
    // BOOST_TEST_MESSAGE("check matrices: \n" << res << "\n" << ex);
    for (Size i = 0; i < res.rows(); ++i) {
        for (Size j = 0; j < res.columns(); ++j) {
            if (std::abs(ex[i][j]) < 1E-5)
                BOOST_CHECK_SMALL(std::abs(res[i][j] - ex[i][j]), 1E-10);
            else
                BOOST_CHECK_CLOSE(res[i][j], ex[i][j], 1E-10);
        }
    }
} // check
} // namespace

BOOST_AUTO_TEST_CASE(testSingleBlock) {
    BOOST_TEST_MESSAGE("Test block matrix inversion with single block matrix");

    // clang-format off
    std::vector<Real> data = { 1.0, 2.0, 2.0,
                               1.0, 1.0, 5.0,
                               -2.0, 0.5, 4.0 };
    // clang-format on

    Matrix m(3, 3);
    std::copy(data.begin(), data.end(), m.begin());

    std::vector<Size> indices = {3};

    Matrix res = blockMatrixInverse(m, indices);
    Matrix ex = inverse(m);
    check(res, ex);
} // testSingleBlock

BOOST_AUTO_TEST_CASE(testTwoBlocks) {
    BOOST_TEST_MESSAGE("Test block matrix inversion with two blocks");

    // clang-format off
    std::vector<Real> data = { 1.0, 2.0, 2.0, 3.0,
                               1.0, 1.0, 5.0, 1.0,
                               -2.0, 0.5, 4.0, -2.0,
                               3.0, 1.0, -1.0, -1.0};
    // clang-format on

    Matrix m(4, 4);
    std::copy(data.begin(), data.end(), m.begin());

    std::vector<Size> indices = {2, 4};

    Matrix res = blockMatrixInverse(m, indices);
    Matrix ex = inverse(m);
    check(res, ex);
} // testTwoBlocks

BOOST_AUTO_TEST_CASE(testThreeBlocks) {
    BOOST_TEST_MESSAGE("Test block matrix inversion with three blocks");

    // clang-format off
    std::vector<Real> data = { 1.0, 2.0, 2.0, 3.0,
                               1.0, 1.0, 5.0, 1.0,
                               -2.0, 0.5, 4.0, -2.0,
                               3.0, 1.0, -1.0, -1.0};
    // clang-format on

    Matrix m(4, 4);
    std::copy(data.begin(), data.end(), m.begin());

    std::vector<Size> indices = {1, 2, 4};

    Matrix res = blockMatrixInverse(m, indices);
    Matrix ex = inverse(m);
    check(res, ex);
} // testThreeBlocks

BOOST_AUTO_TEST_CASE(testFourBlocksBigMatrix) {
    BOOST_TEST_MESSAGE("Test block matrix inversion with four blocks big matrix");

    Matrix m(300, 300, 0.0);

    std::vector<Size> indices = {50, 100, 280, 300};

    MersenneTwisterUniformRng mt(42);
    for (Size i = 0; i < indices.size(); ++i) {
        Size a0 = (i == 0 ? 0 : indices[i - 1]);
        Size a1 = indices[i];
        for (Size ii = a0; ii < a1; ++ii) {
            for (Size jj = a0; jj < a1; ++jj) {
                m[ii][jj] = mt.nextReal();
            }
        }
    }

    boost::timer::cpu_timer timer;
    Matrix res = blockMatrixInverse(m, indices);
    timer.stop();
    BOOST_TEST_MESSAGE("block matrix inversion: " << timer.elapsed().wall * 1e-6 << " ms");
    timer.start();
    Matrix ex = inverse(m);
    timer.stop();
    BOOST_TEST_MESSAGE("plain matrix inversion: " << timer.elapsed().wall * 1e-6 << " ms");
    check(res, ex);
} // testFourBlocksBigMatrix

BOOST_AUTO_TEST_CASE(testTenBlocksBigMatrix) {
    BOOST_TEST_MESSAGE("Test block matrix inversion with ten blocks big matrix");

    Matrix m(500, 500, 0.0);

    std::vector<Size> indices = {30, 80, 130, 150, 200, 280, 370, 420, 430, 500};

    MersenneTwisterUniformRng mt(42);
    for (Size i = 0; i < indices.size(); ++i) {
        Size a0 = (i == 0 ? 0 : indices[i - 1]);
        Size a1 = indices[i];
        for (Size ii = a0; ii < a1; ++ii) {
            for (Size jj = a0; jj < a1; ++jj) {
                m[ii][jj] = mt.nextReal();
            }
        }
    }

    boost::timer::cpu_timer timer;
    Matrix res = blockMatrixInverse(m, indices);
    timer.stop();
    BOOST_TEST_MESSAGE("block matrix inversion: " << timer.elapsed().wall * 1e-6 << " ms");
    timer.start();
    Matrix ex = inverse(m);
    timer.stop();
    BOOST_TEST_MESSAGE("plain matrix inversion: " << timer.elapsed().wall * 1e-6 << " ms");
    check(res, ex);
} // testFourBlocksBigMatrix

BOOST_AUTO_TEST_CASE(testSparseMatrix) {
    BOOST_TEST_MESSAGE("Test sparse matrix with two blocks");

    // clang-format off
    std::vector<Real> data = { 1.0, 2.0, 0.0, 3.0,
                               0.0, 1.0, 5.0, 0.0,
                               -2.0, 0.0, 4.0, -2.0,
                               3.0, 1.0, -1.0, -1.0};
    // clang-format on

    Matrix m(4, 4);
    std::copy(data.begin(), data.end(), m.begin());

    SparseMatrix sm(4, 4);
    for (Size i = 0; i < 4; ++i) {
        for (Size j = 0; j < 4; ++j) {
            Real tmp = data[i * 4 + j];
            if (!close_enough(tmp, 0.0))
                sm(i, j) = tmp;
        }
    }

    std::vector<Size> indices = {2, 4};

    SparseMatrix res = blockMatrixInverse(sm, indices);
    Matrix ex = inverse(m);

    Matrix res2(4, 4);
    for (Size i = 0; i < 4; ++i) {
        for (Size j = 0; j < 4; ++j) {
            res2[i][j] = res(i, j);
        }
    }

    check(res2, ex);
} // testSingleBlock


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
