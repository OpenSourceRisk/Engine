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

#include <ored/utilities/correlationmatrix.hpp>
#include <boost/algorithm/string.hpp>

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
                                                               const std::vector<std::string>& regions) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, regions, factors);
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys,
                                                               const std::vector<std::string>& regions,
                                                               const std::vector<std::string>& names) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, regions, names, factors);
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrix(const std::vector<std::string>& ccys,
                                                               const std::vector<std::string>& regions,
                                                               const std::vector<std::string>& names,
                                                               const std::vector<std::string>& equities) {
    vector<string> factors;
    return correlationMatrixImpl(ccys, regions, names, equities, factors);
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
        factors[n + i - 1] = "FX:" + ccys[0] + ccys[i];

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

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& regions,
                                                                   vector<string>& factors) {

    // First, build the IR/FX matrix
    Matrix mat1 = correlationMatrixImpl(ccys, factors);

    Size m = regions.size();
    if (m == 0)
        return mat1;

    // build extra factors, use dummy index name
    // { "EU", "US" } -> { "INF:EU/Index/CPI", "INF:EU/Index/RR", "INF:US/Index/CPI", "INF:US/Index/RR" }
    for (Size i = 0; i < m; i++) {
        factors.push_back("INF:" + regions[i] + "/Index/CPI");
        factors.push_back("INF:" + regions[i] + "/Index/RR");
    }

    // Build extended matrix. 2n-1 => 2n-1+2m
    Matrix mat = extendMatrix(mat1, 2 * m);

    // now populate additional terms
    Size len = mat.rows();
    QL_REQUIRE(len == mat.columns(), "Matrix not square");
    for (Size i = 0; i < len; i++) {
        for (Size j = max(i + 1, mat1.rows()); j < len; j++)
            mat[i][j] = mat[j][i] = lookup(factors[i], factors[j]);
    }

    // check it
    checkMatrix(mat);

    return mat;
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& regions,
                                                                   const vector<string>& names,
                                                                   vector<string>& factors) {

    // First, build the IR/FX/INF matrix
    Matrix mat2 = correlationMatrixImpl(ccys, regions, factors);

    Size k = names.size();
    if (k == 0)
        return mat2;

    // build extra factors
    // { "APPLE", "HP" } -> { "CR:APPLE", "CR:HP" }
    for (Size i = 0; i < k; i++)
        factors.push_back("CR:" + names[i]);

    // Build extended matrix. 2n-1+2m => 2n-1+2m+k
    Matrix mat = extendMatrix(mat2, k);

    // now populate additional terms
    Size len = mat.rows();
    QL_REQUIRE(len == mat.columns(), "Matrix not square");
    for (Size i = 0; i < len; i++) {
        for (Size j = max(i + 1, mat2.rows()); j < len; j++)
            mat[i][j] = mat[j][i] = lookup(factors[i], factors[j]);
    }

    // check it
    checkMatrix(mat);

    return mat;
}

Disposable<Matrix> CorrelationMatrixBuilder::correlationMatrixImpl(const vector<string>& ccys,
                                                                   const vector<string>& regions,
                                                                   const vector<string>& names,
                                                                   const vector<string>& equities,
                                                                   vector<string>& factors) {

    // First, build the IR/FX/INF matrix
    Matrix mat3 = correlationMatrixImpl(ccys, regions, names, factors);

    Size q = equities.size();
    if (q == 0)
        return mat3;

    // build extra factors
    // { "GOOGLE", "AMAZON" } -> { "EQ:GOOGLE", "EQ:AMAZON" }
    for (Size i = 0; i < q; i++)
        factors.push_back("EQ:" + names[i]);

    // Build extended matrix. 2n-1+2m+p => 2n-1+2m+p+q
    Matrix mat = extendMatrix(mat3, q);

    // now populate additional terms
    Size len = mat.rows();
    QL_REQUIRE(len == mat.columns(), "Matrix not square");
    for (Size i = 0; i < len; i++) {
        for (Size j = max(i + 1, mat3.rows()); j < len; j++)
            mat[i][j] = mat[j][i] = lookup(factors[i], factors[j]);
    }

    // check it
    checkMatrix(mat);

    return mat;
}

// static
void CorrelationMatrixBuilder::checkMatrix(const Matrix& m) {
    QL_REQUIRE(m.rows() == m.columns(), "Error matrix is not square");
    for (Size i = 0; i < m.rows(); i++)
        QL_REQUIRE(m[i][i] == 1.0, "Error, diagonal is not equal to 1 at point " << i);
    for (Size i = 0; i < m.rows(); i++) {
        for (Size j = 0; j < i; j++) {
            QL_REQUIRE(m[i][j] == m[j][i], "Error, matrix is not symmetric at [" << i << "," << j << "]");
            QL_REQUIRE(m[i][j] >= -1.0 && m[i][j] <= 1.0, "Error, invalid correlation value" << m[i][j] << " at [" << i
                                                                                             << "," << j << "]");
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

// splits up INF:Region/Index/[CPI|RR], throws if invalid
static void parseInflationName(const string& s, string& regionOut, string& indexOut, bool& isCpi) {
    QL_REQUIRE(s.size() > 4 && s.substr(0, 4) == "INF:", "Invalid Inflation factor " << s);
    vector<string> tmp;
    boost::split(tmp, s, boost::is_any_of(":/"));
    QL_REQUIRE(tmp.size() == 4 && tmp[0] == "INF" && tmp[1].size() == 2 && // valid region code
                   tmp[2].size() > 0,                                      // valid index
               "Invalid factor name " << s);
    regionOut = tmp[1];
    indexOut = tmp[2];
    boost::to_upper(tmp[3]);
    QL_REQUIRE(tmp[3] == "CPI" || tmp[3] == "RR", "Invalid factor name " << s);
    isCpi = (tmp[3] == "CPI");
}

void CorrelationMatrixBuilder::checkFactor(const string& f) {
    if (f.substr(0, 3) == "IR:") {
        QL_REQUIRE(f.size() == 6, "Invalid factor name " << f);
    } else if (f.substr(0, 3) == "FX:") {
        QL_REQUIRE(f.size() == 9, "Invalid factor name " << f);
    } else if (f.substr(0, 4) == "INF:") {
        // format is INF:<Region>/<index>/[CPI|RR]
        string s1, s2;
        bool b;
        parseInflationName(f, s1, s2, b); // this will throw if bad
    } else if (f.substr(0, 3) == "CR:") {
        // credit name, just check it's at leasr 1 character
        QL_REQUIRE(f.size() > 3, "Invalid factor name " << f);
    } else if (f.substr(0, 3) == "EQ:") {
        // equity name, just check it's at leasr 1 character
        QL_REQUIRE(f.size() > 3, "Invalid factor name " << f);
    } else {
        QL_FAIL("Invalid factor name " << f);
    }
}

static bool isInf(const string& s) { return s.size() > 4 && s.substr(0, 4) == "INF:"; }
// strip out Index from inflation name
static string convertInf(const string& s) {
    string r, i;
    bool isCpi;
    parseInflationName(s, r, i, isCpi);
    return "INF:" + r + (isCpi ? "/CPI" : "/RR");
}

CorrelationMatrixBuilder::key CorrelationMatrixBuilder::buildkey(const string& f1In, const string& f2In) {
    string f1(f1In), f2(f2In); // we need local copies
    QL_REQUIRE(f1 != f2, "Correlation factors must be unique (" << f1 << ")");
    // for inflation - we strip out the index!
    if (isInf(f1))
        f1 = convertInf(f1);
    if (isInf(f2))
        f2 = convertInf(f2);

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
}
}
