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
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <qle/math/fillemptymatrix.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <qle/termstructures/fxvannavolgasmilesection.hpp>
#include <iostream>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {

struct CommonVars {

    /* ------ GLOBAL VARIABLES ------ */
    Date today;
    DayCounter dc;
    vector<Date> dates;
    vector<Real> strikes;
    Matrix vols;
    vector<Real> atmVols;

    vector<Volatility> rrs;
    vector<Volatility> bfs;

    Handle<Quote> baseSpot;
    Handle<YieldTermStructure> baseDomesticYield;
    Handle<YieldTermStructure> baseForeignYield;

    CommonVars() {

        today = Date(1, Jan, 2014);
        dc = ActualActual();

        Settings::instance().evaluationDate() = today;

        dates.push_back(Date(1, Feb, 2014));
        dates.push_back(Date(1, Mar, 2014));
        dates.push_back(Date(1, Apr, 2014));
        dates.push_back(Date(1, Jan, 2015));

        strikes.push_back(90);
        strikes.push_back(100);
        strikes.push_back(110);

        vols = Matrix(3, 4);
        vols[0][0] = 0.12;
        vols[1][0] = 0.10;
        vols[2][0] = 0.13;
        vols[0][1] = 0.22;
        vols[1][1] = 0.20;
        vols[2][1] = 0.23;
        vols[0][2] = 0.32;
        vols[1][2] = 0.30;
        vols[2][2] = 0.33;
        vols[0][3] = 0.42;
        vols[1][3] = 0.40;
        vols[2][3] = 0.43;

        atmVols.push_back(0.1);
        atmVols.push_back(0.2);
        atmVols.push_back(0.3);
        atmVols.push_back(0.4);

        rrs = vector<Volatility>(atmVols.size(), 0.01);
        bfs = vector<Volatility>(atmVols.size(), 0.001);

        baseSpot = Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(100)));

        baseDomesticYield = Handle<YieldTermStructure>(
            boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), dc));
        baseForeignYield = Handle<YieldTermStructure>(
            boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.01)), dc));
    }
};

struct VolData {
    const char* tenor;
    Volatility atm;
    Volatility rr;
    Volatility bf;
    Time time;
    Real df_d;
    Real df_f;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FillIncompleteMatrixTest)

BOOST_AUTO_TEST_CASE(testBlankLineFill) {

    BOOST_TEST_MESSAGE("Testing filling matrices with blank lines");

    Real non_val = -1;

    // empty row matrix & col matrix
    Matrix empty_row, empty_col;
    empty_row = Matrix(5, 5, Real(33));
    empty_col = Matrix(5, 5, Real(33));

    for (int i = 0; i < 5; i++) {
        empty_row[2][i] = non_val;
        empty_col[i][2] = non_val;
    }

    // check fail
    try {
        Matrix tmp_mat = empty_row;
        FillIncompleteMatrix(tmp_mat, true, non_val);
        BOOST_FAIL("FillIncompleteMatrix succeded with Matrix with empty row while interpolating within rows.");
    } catch (QuantLib::Error) {
    } catch (...) {
        BOOST_FAIL("FillIncompleteMatrix failed where is should, but unknown error. Blank row.");
    }

    try {
        Matrix tmp_mat = empty_col;
        FillIncompleteMatrix(tmp_mat, false, non_val);
        BOOST_FAIL("FillIncompleteMatrix succeded with Matrix with empty column");
    } catch (QuantLib::Error) {
    } catch (...) {
        BOOST_FAIL("FillIncompleteMatrix failed where is should, but unknown error. Blank column.");
    }

    try {
        Matrix tmp_mat = empty_row;
        FillIncompleteMatrix(tmp_mat, false, non_val);
    } catch (...) {
        BOOST_FAIL("Matrix fill failed with empty row when interpolating within columns.");
    }

    try {
        Matrix tmp_mat = empty_col;
        FillIncompleteMatrix(tmp_mat, true, non_val);
    } catch (...) {
        BOOST_FAIL("Matrix fill failed with empty col when interpolating within rows.");
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
            if (i == 0 || j == 0 || i == 4 || j== 4) {
                incomplete_m[i][j] = j + i + 1;
            }
        }
    }

    // fill matrix
    Matrix to_fill_row = incomplete_m;
    Matrix to_fill_col = incomplete_m;
    FillIncompleteMatrix(to_fill_row, true, non_val);
    FillIncompleteMatrix(to_fill_col, false, non_val);

    // check results
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (to_fill_row[i][j] != to_fill_col[i][j] || to_fill_row[i][j] != j + i + 1) {
                BOOST_FAIL("FillIncompleteMatrix incorrectly interpolated internal entries");
            }
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
        FillIncompleteMatrix(to_fill_rows, false, non_val);
        FillIncompleteMatrix(to_fill_cols, true, non_val);

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
        for (int j = 0; j < 5; j++) {
            if (i == j) {
                incomplete_m[i][j] = i;
            }
        }
    }

    // fill matrix
    Matrix to_fill_rows = incomplete_m;
    Matrix to_fill_cols = incomplete_m;
    FillIncompleteMatrix(to_fill_rows, true, non_val);
    FillIncompleteMatrix(to_fill_cols, false, non_val);

    // check results
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            bool check_row = to_fill_rows[i][j] == incomplete_m[i][i];
            bool check_col = to_fill_cols[i][j] == incomplete_m[j][j];
            if (!check_row || !check_col) {
                BOOST_FAIL("FillIncompleteMatrix failed to correclty fill a diagonal matrix.");
            }
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
