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
    vector<Real> x_axis;
    for (int i = 0; i < mat.columns(); i++) {
        x_axis.push_back(i);
    }

    // loop over rows and interpolate
    for (int i = 0; i < mat.rows(); i++) {
        vector<Real> y;                        // values for the expiries we have for this strike
        vector<Real> x;                        // the 'tics' expiries.
        vector<Real> y_desired(x_axis.size()); // interpolated y over defined x_axis

        // flat extrapolate short end
        if (mat[i][0] == blank) {
            int pos1 = mat.columns();
            for (int j = 1; j < mat.columns(); j++) {
                if (mat[i][j] != blank) {
                    pos1 = j;
                    break;
                }
            }
            QL_REQUIRE(pos1 < mat.columns(), "Matrix has empty row.");
            for (int j = 0; j < pos1; j++) {
                mat[i][j] = mat[i][pos1];
            }
        }

        // flat extrapolate far end
        if (mat[i][mat.columns() - 1] == blank) {
            int pos1 = -1;
            for (int j = mat.columns() - 2; j >= 0; j--) {
                if (mat[i][j] != blank) {
                    pos1 = j;
                    break;
                }
            }
            for (int j = pos1 + 1; j < mat.columns(); j++) {
                mat[i][j] = mat[i][pos1];
            }
        }

        // build x and y for row
        for (int j = 0; j < mat.columns(); j++) {
            if (mat[i][j] != blank) {
                x.push_back(j);
                y.push_back(mat[i][j]);
            }
        }

        // interpolate over entire x_axis
        Interpolation f = LinearInterpolation(x.begin(), x.end(), y.begin());
        std::transform(x_axis.begin(), x_axis.end(), y_desired.begin(), f);

        // set row in mat
        for (int j = 0; j < mat.columns(); j++) {
            mat[i][j] = y_desired[j];
        }
    }
}

void fillIncompleteMatrix(Matrix& mat, bool interpRows, Real blank) {
    if (interpRows)
        fillMatrixImpl(mat, blank);
    else {
        Matrix m2 = transpose(mat);
        fillMatrixImpl(m2, blank);
        Matrix m3 = transpose(m2);
        mat.swap(m3);
    }
}
} // namespace QuantExt