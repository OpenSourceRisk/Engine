/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <qle/math/deltagammavar.hpp>

namespace QuantExt {
using namespace std;

static void fillMatrixImpl(Matrix& mat, Real blank) {

    // define entire axis
    vector<Real> xAxis;
    for (unsigned int i = 0; i < mat.columns(); i++) {
        xAxis.push_back(i);
    }

    // loop over rows and interpolate
    for (Size i = 0; i < mat.rows(); i++) {
        vector<Real> y;                      // values for the expiries we have
        vector<Real> x;                      // the required 'tics' on axis to interpolate
        vector<Real> yDesired(xAxis.size()); // interpolated y over defined x_axis

        // flat extrapolate short end
        if (mat[i][0] == blank) {
            Size pos1 = mat.columns();
            for (Size j = 1; j < mat.columns(); j++) {
                if (mat[i][j] != blank) {
                    pos1 = j;
                    break;
                }
            }
            QL_REQUIRE(pos1 < mat.columns(), "Matrix has empty line.");
            for (Size j = 0; j < pos1; j++) {
                mat[i][j] = mat[i][pos1];
            }
        }

        // flat extrapolate far end
        if (mat[i][mat.columns() - 1] == blank) {
            int pos1 = -1;
            for (Size j = mat.columns() - 2; j >= 0; j--) {
                if (mat[i][j] != blank) {
                    pos1 = j;
                    break;
                }
            }
            for (Size j = pos1 + 1; j < mat.columns(); j++) {
                mat[i][j] = mat[i][pos1];
            }
        }

        // build x and y for row
        for (unsigned int j = 0; j < mat.columns(); j++) {
            if (mat[i][j] != blank) {
                x.push_back(j);
                y.push_back(mat[i][j]);
            }
        }

        // interpolate over entire x_axis
        Interpolation f = LinearInterpolation(x.begin(), x.end(), y.begin());
        std::transform(xAxis.begin(), xAxis.end(), yDesired.begin(), f);

        // set row in mat
        for (Size j = 0; j < mat.columns(); j++) {
            mat[i][j] = yDesired[j];
        }
    }
}

void fillIncompleteMatrix(Matrix& mat, bool interpRows = true, Real blank = QuantLib::Null<Real>()) {
    QL_REQUIRE(mat.columns() > 0 && mat.rows() > 0, "Matrix has no elements.");

    // check if already complete
    bool isFull = true;
    for (Size i = 0; i < mat.rows(); i++) {
        for (Size j = 0; j < mat.columns(); j++) {
            if (mat[i][j] == blank) {
                isFull = false;
            }
            if (!isFull) {
                break;
            }
        }
        if (!isFull) {
            break;
        }
    }

    if (!isFull) {
        if (mat.columns() == 1 && mat.rows() == 1) {
            QL_FAIL("1 X 1 empty matrix given to fill."); // !is_full and 1 X 1 matrix.
        }
        if (interpRows) {
            QL_REQUIRE(mat.columns() > 1, "Too few columns in matrix to interpolate within rows.");
            fillMatrixImpl(mat, blank);
        } else {
            QL_REQUIRE(mat.rows() > 1, "Too few rows in matrix to interpolate within columns.");
            Matrix m2 = transpose(mat);
            fillMatrixImpl(m2, blank);
            Matrix m3 = transpose(m2);
            mat.swap(m3);
        }
    }
}
} // namespace QuantExt
