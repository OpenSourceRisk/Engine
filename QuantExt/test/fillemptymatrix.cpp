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

/*! \file fxvolsmile.cpp
    \brief filling non-complete matrix
*/

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <qle/math/fillemptymatrix.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(fillIncompleteMatrixTest)

BOOST_AUTO_TEST_CASE(testBlankLineFill) {
    BOOST_TEST_MESSAGE("Testing filling matrices with blank lines");

    Real non_val = -1;

    // empty row matrix & empty col matrix
    Matrix empty_row, empty_col;
    empty_row = Matrix(5, 5, Real(22.5));
    empty_col = Matrix(5, 5, Real(22.5));

    for (int i = 0; i < 5; i++) {
        empty_row[2][i] = non_val;
        empty_col[i][2] = non_val;
    }

    // check fails + successes
    Matrix tmp_mat_r = empty_row;
    Matrix tmp_mat_c = empty_col;
    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp_mat_r, true, non_val), QuantLib::Error);
    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp_mat_c, false, non_val), QuantLib::Error);

    tmp_mat_r = empty_row;
    tmp_mat_c = empty_col;
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_mat_r, false, non_val));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_mat_c, true, non_val));

    // check vals
    Real tol = 1.0E-8;
    for (int i = 0; i < 5; i++) {
        BOOST_CHECK_CLOSE(tmp_mat_r[2][i], Real(22.5), tol);
        BOOST_CHECK_CLOSE(tmp_mat_c[i][2], Real(22.5), tol);
    }
}

BOOST_AUTO_TEST_CASE(testInterpolateOnly) {
    BOOST_TEST_MESSAGE("Testing interpolation only");

    Real non_val = -1;
    Matrix incomplete_m;
    // incomplete matrix of the form:
    /*
    1   2   3   4   5
    2   3   4   5   6
    3   4   5   6   7
    4   5   6   7   8
    5   6   7   8   9
    */
    // But center block is non_values.

    incomplete_m = Matrix(5, 5, non_val);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (i == 0 || j == 0 || i == 4 || j == 4) {
                incomplete_m[i][j] = j + i + 1;
            }
        }
    }

    // fill matrix
    Matrix to_fill_row = incomplete_m;
    Matrix to_fill_col = incomplete_m;
    fillIncompleteMatrix(to_fill_row, true, non_val);
    fillIncompleteMatrix(to_fill_col, false, non_val);

    // check results
    Real tol = 1.0E-8;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            BOOST_CHECK_CLOSE(to_fill_row[i][j], j + i + 1, tol);
            BOOST_CHECK_CLOSE(to_fill_col[i][j], j + i + 1, tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testExtrapolateOnly) {
    BOOST_TEST_MESSAGE("Testing extrapolation of edges in filling the matrix");

    Real non_val = -1;
    Matrix missing_rows, missing_cols;
    vector<vector<Real> > test_cases; // for different test cases, different missing lines
    for (int i = 0; i < 4; i++) {
        vector<Real> tmp;
        for (int j = 0; j <= i; j++) {
            tmp.push_back(j);
        }
        test_cases.push_back(tmp);
    }

    // incomplete matricies of the form:
    /*
    '   2   3   4   5       '   '   '   '   '
    '   3   4   5   6       2   3   4   5   6
    '   4   5   6   7       3   4   5   6   7
    '   5   6   7   8       4   5   6   7   8
    '   6   7   8   9       5   6   7   8   9
    */
    // incomplete_rows: some rows at leading edge are missing.
    // incomplete_cols: some cols at leading edge are missing.

    // loop over cases with different missing rows/cols
    vector<vector<Real> >::iterator cs;
    for (cs = test_cases.begin(); cs != test_cases.end(); cs++) {

        // set up empty matrices
        missing_rows = Matrix(5, 5, non_val);
        missing_cols = Matrix(5, 5, non_val);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {

                // ignore empty lines
                bool set_row = find(cs->begin(), cs->end(), i) == cs->end();
                bool set_col = find(cs->begin(), cs->end(), j) == cs->end();
                if (set_row) {
                    missing_rows[i][j] = j + i + 1;
                }
                if (set_col) {
                    missing_cols[i][j] = j + i + 1;
                }
            }
        }

        // fill matrices
        Matrix to_fill_rows = missing_rows;
        Matrix to_fill_cols = missing_cols;
        fillIncompleteMatrix(to_fill_rows, false, non_val);
        fillIncompleteMatrix(to_fill_cols, true, non_val);

        // check results
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                int last_val = cs->size();
                Real expectedVal_row;
                Real expectedVal_col;
                if (j < last_val) {
                    expectedVal_col = missing_cols[i][last_val];
                } else {
                    expectedVal_col = missing_cols[i][j];
                }
                if (i < last_val) {
                    expectedVal_row = missing_rows[last_val][j];
                } else {
                    expectedVal_row = missing_rows[i][j];
                }
                bool check_row = to_fill_rows[i][j] == expectedVal_row;
                bool check_col = to_fill_cols[i][j] == expectedVal_col;

                if (!check_row || !check_col) {
                    BOOST_FAIL("FillIncomplete matrix failed on extrapolation only tests.");
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testInterpExtrap) {
    BOOST_TEST_MESSAGE("Testing interpolation and extrapolation");

    Real non_val = -1;
    Matrix incomplete_m;
    // incomplete matrix of the form:
    /*
    1   '   '   '   '
    '   2   '   '   '
    '   '   3   '   '
    '   '   '   4   '
    '   '   '   '   5
    */

    incomplete_m = Matrix(5, 5, non_val);
    for (int i = 0; i < 5; i++) {
        incomplete_m[i][i] = i;
    }

    // fill matrix
    Matrix to_fill_rows = incomplete_m;
    Matrix to_fill_cols = incomplete_m;
    fillIncompleteMatrix(to_fill_rows, true, non_val);
    fillIncompleteMatrix(to_fill_cols, false, non_val);

    // check results
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            BOOST_CHECK_EQUAL(to_fill_rows[i][j], incomplete_m[i][i]);
            BOOST_CHECK_EQUAL(to_fill_cols[i][j], incomplete_m[j][j]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSingleEntry) {

    Matrix inc = Matrix(1, 1, 22.5);
    Matrix tmp1 = inc, tmp2 = inc, tmp3 = inc, tmp4 = inc;

    // non-blank value.
    BOOST_TEST_MESSAGE("Testing single non-blank entry");
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp1, true, -1));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp2, false, -1));
    BOOST_CHECK_EQUAL(tmp1[0][0], inc[0][0]);
    BOOST_CHECK_EQUAL(tmp2[0][0], inc[0][0]);

    // blank value.
    BOOST_TEST_MESSAGE("Testing single blank entry");
    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp3, true, 22.5), QuantLib::Error);
    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp4, false, 22.5), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testEmptyMatrix) {
    BOOST_TEST_MESSAGE("testing empty matrices");

    Matrix m;
    BOOST_CHECK_THROW(fillIncompleteMatrix(m, true, -1), QuantLib::Error);
    BOOST_CHECK_THROW(fillIncompleteMatrix(m, false, -1), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testFullMatrix) {
    BOOST_TEST_MESSAGE("tesing full matrices");

    // set up matrices
    Matrix full_single = Matrix(1, 1, 22.5);
    Matrix full = Matrix(5, 5, 22.5);
    Matrix tmp_single_r = full_single;
    Matrix tmp_single_c = full_single;
    Matrix tmp_r = full;
    Matrix tmp_c = full;

    // "fill"
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_single_r, true, -1));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_single_c, false, -1));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_r, true, -1));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_c, false, -1));

    // check results
    BOOST_CHECK_EQUAL(tmp_single_r[0][0], full_single[0][0]);
    BOOST_CHECK_EQUAL(tmp_single_c[0][0], full_single[0][0]);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            BOOST_CHECK_EQUAL(tmp_r[i][j], full[i][j]);
            BOOST_CHECK_EQUAL(tmp_c[i][j], full[i][j]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSingleRowCol) {
    Matrix single_row = Matrix(1, 5, 22.5);
    Matrix single_col = Matrix(5, 1, 22.5);
    single_row[0][3] = -1; // single blank entry
    single_col[3][0] = -1; // single blank entry
    Matrix tmp_row = single_row;
    Matrix tmp_col = single_col;

    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp_row, false, -1), QuantLib::Error);
    BOOST_CHECK_THROW(fillIncompleteMatrix(tmp_col, true, -1), QuantLib::Error);

    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_row, true, -1));
    BOOST_CHECK_NO_THROW(fillIncompleteMatrix(tmp_col, false, -1));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
