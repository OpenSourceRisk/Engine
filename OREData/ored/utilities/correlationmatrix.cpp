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

#include <boost/algorithm/string.hpp>
#include <ored/utilities/correlationmatrix.hpp>

using namespace std;

namespace ore {
namespace data {

// typedef std::pair<std::string, std::string> key;

void CorrelationMatrixBuilder::addCorrelation(const std::string& factor1, const std::string& factor2,
                                              Real correlation) {
    checkFactor(factor1);
    checkFactor(factor2);
    // we store the correlations in a map, but we sort the key (pair of strings)
    // first for quicker lookup.
    key k = buildkey(factor1, factor2);
    QL_REQUIRE(data_.find(k) == data_.end(), "Correlation for key " << k.first << "," << k.second << " already set");
    QL_REQUIRE(correlation >= -1.0 && correlation <= 1.0, "Invalid correlation " << correlation);
    data_[k] = correlation;
}

// -- public interface, wrapping the Impl methods
// we don't need the factors vector
Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, factors);
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys,
                                                               const std::vector<std::string>& infIndices) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, infIndices, factors);
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys,
                                                               const std::vector<std::string>& infIndices,
                                                               const std::vector<std::string>& names) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, infIndices, names, factors);
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys,
                                                               const std::vector<std::string>& infIndices,
                                                               const std::vector<std::string>& names,
                                                               const std::vector<std::string>& equities) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, infIndices, names, equities, factors);
}

// -- Impl methods, used for building ontop of each other.

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   vector<string>& factors) {
    QL_REQUIRE(factors.size() == 0, "Factors Input vector must be empty");

    QL_REQUIRE(ccys.size() >= 1, "At least one currency required to build correlation matrix");
    for (Size i = 0; i < ccys.size(); i++)
        QL_REQUIRE(ccys[i].size() == 3, "Invalid ccy code " << ccys[i]);

    // Build (2n-1)*(2n-1) matrix
    // IR/IR first, than IR/FX
    Size n = ccys.size();
    Size len = 2 * n - 1;

    // start with an empty matrix
    Matrix mat(len, len, 0);
    for (Size i = 0; i < len; i++)
        mat[i][i] = 1.0;

    // build factors
    // { "EUR", "USD", "GBP" } -> { "IR:EUR", "IR:USD", "IR:GBP", "FX:EURUSD", "FX:EURGBP" }
    factors.resize(len);
    for (Size i = 0; i < n; i++)
        factors[i] = "IR:" + ccys[i];
    for (Size i = 1; i < n; i++)
        factors[n + i - 1] = "FX:" + ccys[i] + ccys[0];

    // now populate matrix
    // the lookup function takes care of FX:EURUSD / FX:USDEUR conversion
    for (Size i = 0; i < len; i++) {
        for (Size j = 0; j < i; j++)
            mat[i][j] = mat[j][i] = lookup(factors[i], factors[j]);
    }

    // check it
    checkMatrix(mat);

    return mat;
}

// increases a matrix by n rows + n columns, values are 0, diagionals 1.
Disposable<Matrix> CorrelationMatrixBuilder::extendMatrix(const Matrix& mat, Size n) {
    Matrix newMat(mat.rows() + n, mat.columns() + n, 0);
    // copy old into new
    for (Size i = 0; i < mat.rows(); ++i) {
        for (Size j = 0; j < mat.columns(); ++j)
            newMat[i][j] = mat[i][j];
    }
    // set diagional
    for (Size i = 0; i < n; ++i)
        newMat[mat.rows() + i][mat.columns() + i] = 1.0;
    return newMat;
}

Disposable<Matrix> CorrelationMatrixBuilder::extendCorrelationMatrix(const Matrix& mat, const vector<string>& names,
                                                                     vector<string>& factors, const string& prefix) {
    Size m = names.size();

    // build extra factors
    // { "UKRPI", "EUHICP" } -> { "INF:UKRPI", "INF:EUHICP" }
    for (Size i = 0; i < m; i++)
        factors.push_back(prefix + names[i]);

    // Build extended matrix. 2n-1 => 2n-1+m
    Matrix mat1 = extendMatrix(mat, m);

    // now populate additional terms
    Size len = mat1.rows();
    QL_REQUIRE(len == mat1.columns(), "Matrix not square");
    for (Size i = 0; i < len; i++) {
        for (Size j = max(i + 1, mat.rows()); j < len; j++)
            mat1[i][j] = mat1[j][i] = lookup(factors[i], factors[j]);
    }

    // check it
    checkMatrix(mat1);

    return mat1;
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& infIndices,
                                                                   vector<string>& factors) {

    // First, build the IR/FX matrix
    Matrix mat1 = correlationMatrixImpl(ccys, factors);

    if (infIndices.size() == 0)
        return mat1;

    Matrix mat2 = extendCorrelationMatrix(mat1, infIndices, factors, "INF:");
    return mat2;
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& infIndices,
                                                                   const vector<string>& names,
                                                                   vector<string>& factors) {

    // First, build the IR/FX/INF matrix
    Matrix mat1 = correlationMatrixImpl(ccys, infIndices, factors);

    if (names.size() == 0)
        return mat1;

    Matrix mat2 = extendCorrelationMatrix(mat1, names, factors, "CR:");
    return mat2;
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& infIndices,
                                                                   const vector<string>& names,
                                                                   const vector<string>& equities,
                                                                   vector<string>& factors) {

    // First, build the IR/FX/INF/CR matrix
    Matrix mat1 = correlationMatrixImpl(ccys, infIndices, names, factors);

    if (equities.size() == 0)
        return mat1;

    Matrix mat2 = extendCorrelationMatrix(mat1, equities, factors, "EQ:");
    return mat2;
}

// static
void CorrelationMatrixBuilder::checkMatrix(const Matrix& m) {
    QL_REQUIRE(m.rows() == m.columns(), "Error matrix is not square");
    for (Size i = 0; i < m.rows(); i++)
        QL_REQUIRE(m[i][i] == 1.0, "Error, diagonal is not equal to 1 at point " << i);
    for (Size i = 0; i < m.rows(); i++) {
        for (Size j = 0; j < i; j++) {
            QL_REQUIRE(m[i][j] == m[j][i], "Error, matrix is not symmetric at [" << i << "," << j << "]");
            QL_REQUIRE(m[i][j] >= -1.0 && m[i][j] <= 1.0,
                       "Error, invalid correlation value" << m[i][j] << " at [" << i << "," << j << "]");
        }
    }
}

// static
Size CorrelationMatrixBuilder::countNonZero(const Matrix& m) {
    checkMatrix(m);
    Size c = 0;
    for (Size i = 0; i < m.rows(); i++) {
        for (Size j = 0; j < i; j++) {
            if (m[i][j] != 0)
                c++;
        }
    }
    return c;
}

void CorrelationMatrixBuilder::checkFactor(const string& f) {
    if (f.substr(0, 3) == "IR:") {
        QL_REQUIRE(f.size() == 6, "Invalid factor name " << f);
    } else if (f.substr(0, 3) == "FX:") {
        QL_REQUIRE(f.size() == 9, "Invalid factor name " << f);
    } else if (f.substr(0, 4) == "INF:") {
        // Inflation index, check it's at least 1 character
        QL_REQUIRE(f.size() > 4, "Invalid factor name " << f);
    } else if (f.substr(0, 3) == "CR:") {
        // credit name, just check it's at least 1 character
        QL_REQUIRE(f.size() > 3, "Invalid factor name " << f);
    } else if (f.substr(0, 3) == "EQ:") {
        // equity name, just check it's at least 1 character
        QL_REQUIRE(f.size() > 3, "Invalid factor name " << f);
    } else {
        QL_FAIL("Invalid factor name " << f);
    }
}

CorrelationMatrixBuilder::key CorrelationMatrixBuilder::buildkey(const string& f1In, const string& f2In) {
    string f1(f1In), f2(f2In); // we need local copies
    QL_REQUIRE(f1 != f2, "Correlation factors must be unique (" << f1 << ")");
    if (f1 < f2)
        return make_pair(f1, f2);
    else
        return make_pair(f2, f1);
}

static bool isFX(const string& s) { return s.size() == 9 && s.substr(0, 3) == "FX:"; }

static string invertFX(const string& s) { return "FX:" + s.substr(6, 3) + s.substr(3, 3); }

Real CorrelationMatrixBuilder::lookup(const string& f1, const string& f2) {
    key k = buildkey(f1, f2);
    if (data_.find(k) != data_.end())
        return data_[k];

    // There are up to 3 possible alternatives.
    bool isfx1 = isFX(f1);
    bool isfx2 = isFX(f2);
    if (isfx1) {
        k = buildkey(invertFX(f1), f2);
        if (data_.find(k) != data_.end())
            return -data_[k]; // invert
    }
    if (isfx2) {
        k = buildkey(f1, invertFX(f2));
        if (data_.find(k) != data_.end())
            return -data_[k]; // invert
    }
    if (isfx1 && isfx2) {
        k = buildkey(invertFX(f1), invertFX(f2));
        if (data_.find(k) != data_.end())
            return data_[k]; // don't invert
    }

    // default.
    return 0.0;
}
} // namespace data
} // namespace ore
