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

#include <qle/math/blockmatrixinverse.hpp>

#include <boost/numeric/ublas/lu.hpp>

using namespace boost::numeric::ublas;

namespace QuantExt {
using namespace QuantLib;
    
namespace {
bool isNull(const Matrix& A) {
    for (auto const& a : A) {
        if (std::abs(a) > QL_EPSILON)
            return false;
    }
    return true;
} // isNull

bool isNull(const QuantLib::SparseMatrix& A) {
    for (auto i1 = A.begin1(); i1 != A.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            if (std::abs(*i2) > QL_EPSILON)
                return false;
        }
    }
    return true;
} // isNull

} // namespace

QuantLib::SparseMatrix inverse(QuantLib::SparseMatrix m) {
    QL_REQUIRE(m.size1() == m.size2(), "matrix is not square");
    boost::numeric::ublas::permutation_matrix<Size> pert(m.size1());
    // lu decomposition
    const Size singular = lu_factorize(m, pert);
    QL_REQUIRE(singular == 0, "singular matrix given");
    QuantLib::SparseMatrix inverse = boost::numeric::ublas::identity_matrix<Real>(m.size1());
    // backsubstitution
    boost::numeric::ublas::lu_substitute(m, pert, inverse);
    return inverse;
}

Matrix blockMatrixInverse(const Matrix& A, const std::vector<Size>& blockIndices) {

    QL_REQUIRE(blockIndices.size() > 0, "blockMatrixInverse: at least one entry in blockIndices required");
    Size n = blockIndices.back();
    QL_REQUIRE(n > 0 && A.rows() == A.columns() && A.rows() == n,
               "blockMatrixInverse: matrix (" << A.rows() << "x" << A.columns() << ") must be square of size " << n
                                              << "x" << n << ", n>0");

    if (blockIndices.size() == 1) {
        Matrix res = inverse(A);
        return res;
    }

    Size mid = (blockIndices.size() - 1) / 2;
    Size m = blockIndices[mid];
    QL_REQUIRE(m > 0 && m < n,
               "blockMatrixInverse: expected m (" << m << ") to be positive and less than n (" << n << ")");
    std::vector<Size> leftIndices(blockIndices.begin(), blockIndices.begin() + (mid + 1));
    std::vector<Size> rightIndices(blockIndices.begin() + (mid + 1), blockIndices.end());
    for (auto& i : rightIndices) {
        i -= m;
    }
    QL_REQUIRE(leftIndices.size() > 0, "blockMatrixInverse: expected left indices to be non-empty");
    QL_REQUIRE(rightIndices.size() > 0, "blockMatrixInverse: expected right indices to be non-empty");

    // FIXME it would be nice if we could avoid building these submatrices, instead use a view
    // on parts of the original matrix
    Matrix a(m, m), b(m, n - m), c(n - m, m), d(n - m, n - m);

    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < n; ++j) {
            if (i < m) {
                if (j < m)
                    a[i][j] = A[i][j];
                else
                    b[i][j - m] = A[i][j];
            } else {
                if (j < m)
                    c[i - m][j] = A[i][j];
                else
                    d[i - m][j - m] = A[i][j];
            }
        }
    }

    Matrix aInv = blockMatrixInverse(a, leftIndices);
    Matrix tmp = c * aInv;
    Matrix schurCompInv;
    if (isNull(c) || isNull(b))
        schurCompInv = blockMatrixInverse(d, rightIndices);
    else
        schurCompInv = blockMatrixInverse(d - tmp * b, rightIndices);
    Matrix b2 = (-1) * (aInv * b * schurCompInv);
    Matrix a2 = aInv - b2 * tmp;
    Matrix c2 = (-1) * schurCompInv * tmp;

    Matrix res(n, n);
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < n; ++j) {
            if (i < m) {
                if (j < m)
                    res[i][j] = a2[i][j];
                else
                    res[i][j] = b2[i][j - m];
            } else {
                if (j < m)
                    res[i][j] = c2[i - m][j];
                else
                    res[i][j] = schurCompInv[i - m][j - m];
            }
        }
    }

    return res;
} // blockMatrixInverse(Matrix)

QuantLib::SparseMatrix blockMatrixInverse(const QuantLib::SparseMatrix& A, const std::vector<Size>& blockIndices) {

    QL_REQUIRE(blockIndices.size() > 0, "blockMatrixInverse: at least one entry in blockIndices required");
    Size n = blockIndices.back();
    QL_REQUIRE(n > 0 && A.size1() == A.size2() && A.size1() == n,
               "blockMatrixInverse: matrix (" << A.size1() << "x" << A.size2() << ") must be square of size " << n
                                              << "x" << n << ", n>0");

    if (blockIndices.size() == 1) {
        QuantLib::SparseMatrix res = inverse(A);
        return res;
    }

    Size mid = (blockIndices.size() - 1) / 2;
    Size m = blockIndices[mid];
    QL_REQUIRE(m > 0 && m < n,
               "blockMatrixInverse: expected m (" << m << ") to be positive and less than n (" << n << ")");
    std::vector<Size> leftIndices(blockIndices.begin(), blockIndices.begin() + (mid + 1));
    std::vector<Size> rightIndices(blockIndices.begin() + (mid + 1), blockIndices.end());
    for (auto& i : rightIndices) {
        i -= m;
    }
    QL_REQUIRE(leftIndices.size() > 0, "blockMatrixInverse: expected left indices to be non-empty");
    QL_REQUIRE(rightIndices.size() > 0, "blockMatrixInverse: expected right indices to be non-empty");

    QuantLib::SparseMatrix a = project(A, range(0, m), range(0, m));
    QuantLib::SparseMatrix b = project(A, range(0, m), range(m, n));
    QuantLib::SparseMatrix c = project(A, range(m, n), range(0, m));
    QuantLib::SparseMatrix d = project(A, range(m, n), range(m, n));

    QuantLib::SparseMatrix aInv = blockMatrixInverse(a, leftIndices);
    QuantLib::SparseMatrix tmp(n - m, m), schurCompInv, p(m, n - m), p1(n - m, n - m), p2(m, m), b2(m, n - m), c2(n - m, m);
    axpy_prod(c, aInv, tmp, true);
    if (isNull(c) || isNull(b))
        schurCompInv = blockMatrixInverse(d, rightIndices);
    else {
        axpy_prod(tmp, b, p1, true);
        schurCompInv = blockMatrixInverse(d - p1, rightIndices);
    }
    axpy_prod(aInv, b, p, true);
    axpy_prod(-p, schurCompInv, b2);
    axpy_prod(b2, tmp, p2);
    QuantLib::SparseMatrix a2 = aInv - p2;
    axpy_prod(-schurCompInv, tmp, c2);

    QuantLib::SparseMatrix res(n, n);

    for (auto i1 = a2.begin1(); i1 != a2.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            res(i2.index1(), i2.index2()) = *i2;
        }
    }
    for (auto i1 = b2.begin1(); i1 != b2.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            res(i2.index1(), i2.index2() + m) = *i2;
        }
    }
    for (auto i1 = c2.begin1(); i1 != c2.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            res(i2.index1() + m, i2.index2()) = *i2;
        }
    }
    for (auto i1 = schurCompInv.begin1(); i1 != schurCompInv.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            res(i2.index1() + m, i2.index2() + m) = *i2;
        }
    }

    return res;
} // blockMatrixInverse(SparseMatrix)

Real modifiedMaxNorm(const QuantLib::SparseMatrix& A) {
    Real r = 0.0;
    for (auto i1 = A.begin1(); i1 != A.end1(); ++i1) {
        for (auto i2 = i1.begin(); i2 != i1.end(); ++i2) {
            r = std::max(r, std::abs(*i2));
        }
    }
    return std::sqrt(static_cast<Real>(A.size1()) * static_cast<Real>(A.size2())) * r;
}

} // namespace QuantExt
