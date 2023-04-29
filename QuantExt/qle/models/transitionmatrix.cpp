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

/*! \file models/transitionmatrix.hpp
    \brief utility functions for transition matrices and generators
    \ingroup models
*/

#include <qle/math/matrixfunctions.hpp>
#include <qle/models/transitionmatrix.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>

namespace QuantExt {

void sanitiseTransitionMatrix(Matrix& m) {
    for (Size i = 0; i < m.rows(); ++i) {
        Real sum = 0.0;
        for (Size j = 0; j < m.columns(); ++j) {
            m[i][j] = std::max(std::min(m[i][j], 1.0), 0.0);
            if (i != j)
                sum += m[i][j];
        }
        if (sum <= 1.0) {
            m(i, i) = 1.0 - sum;
        } else {
            sum += m[i][i];
            for (Size j = 0; j < m.columns(); ++j) {
                m[i][j] = m[i][j] / sum;
            }
        }
    }
}

void checkTransitionMatrix(const Matrix& t) {
    QL_REQUIRE(t.rows() == t.columns(), "transition matrix must be quadratic");
    for (Size i = 0; i < t.rows(); ++i) {
        Real sum = 0.0;
        for (Size j = 0; j < t.columns(); ++j) {
            sum += t[i][j];
            QL_REQUIRE(t[i][j] > 0.0 || close_enough(t[i][j], 0.0),
                       "transition matrix entry (" << i << "," << j << ") is negative: " << t[i][j]);
        }
        QL_REQUIRE(close_enough(sum, 1.0), "row " << i << " sum (" << sum << ") not equal to 1");
    }
    return;
}

void checkGeneratorMatrix(const Matrix& g) {
    QL_REQUIRE(g.rows() == g.columns(), "generator matrix must be quadratic");
    for (Size i = 0; i < g.rows(); ++i) {
        Real sum = 0.0;
        for (Size j = 0; j < g.columns(); ++j) {
            sum += g[i][j];
            if (i != j) {
                QL_REQUIRE(g[i][j] > 0.0 || close_enough(g[i][j], 0.0),
                           "generator matrix entry (" << i << "," << j << ") is negative: " << g[i][j]);
            }
        }
        QL_REQUIRE(std::fabs(sum) < QL_EPSILON, "row " << i << " sum (" << sum << ") not equal to 0");
    }
    return;
}

namespace {
struct PiCompare {
    PiCompare(const std::vector<Real> a) : a_(a) {}
    bool operator()(const Size i, const Size j) { return a_[i] < a_[j]; }
    std::vector<Real> a_;
};
} // anonymous namespace

Matrix generator(const Matrix& t, const Real horizon) {
    // naked log
    Matrix A = QuantExt::Logm(t) / horizon;
    // regularisation
    Size n = A.columns();
    for (Size row = 0; row < A.rows(); ++row) {
        // Step 1
        Real lambda = 0.0;
        for (Size i = 0; i < n; ++i) {
            lambda += A(row, i);
        }
        lambda /= static_cast<Real>(n);
        std::vector<Real> b(n);
        for (Size i = 0; i < n; ++i) {
            b[i] = A(row, i) - lambda;
        }
        // Step 2
        std::vector<Size> pi(n);
        for (Size i = 0; i < n; ++i)
            pi[i] = i;
        // ascending order, in the paper it says descending order...
        std::sort(pi.begin(), pi.end(), PiCompare(b));
        std::vector<Real> ahat(n);
        for (Size i = 0; i < n; ++i)
            ahat[i] = b[pi[i]];
        // Step 3
        // start with l=1, the paper says l=2....
        Size l = 1;
        for (; l <= n - 1; ++l) {
            Real sum = 0.0;
            for (Size i = 0; i <= n - (l + 1); ++i)
                sum += ahat[n - i - 1];
            if (static_cast<Real>(n - l + 1) * ahat[l + 1 - 1] >= ahat[0] + sum)
                break;
        }
        QL_REQUIRE(l <= n - 1, "regularisedGenerator: expected 2 <= l <= n-1");
        // Step 4
        std::vector<Real> ghat(n, 0.0);
        Real sum = 0.0;
        for (Size j = 0; j < n; ++j) {
            if (!(j + 1 >= 2 && j + 1 <= l))
                sum += ahat[j];
        }
        sum /= static_cast<Real>(n - l + 1);
        for (Size i = 0; i < n; ++i) {
            if (!(i + 1 >= 2 && i + 1 <= l))
                ghat[i] = ahat[i] - sum;
        }
        // Step 5
        for (Size i = 0; i < n; ++i) {
            A(row, pi[i]) = ghat[i];
        }
    } // for row
    return A;
}

template <class I> std::vector<Real> creditStateBoundaries(const I& begin, const I& end) {
    InverseCumulativeNormal icn;
    std::vector<Real> bounds(end - begin);
    Real sum = 0.0;
    for (Size i = 0; i < bounds.size(); ++i) {
        Real p = *(begin + i);
        QL_REQUIRE(p >= 0.0, "transistion probability " << i << " is negative: " << p);
        sum += p;
        QL_REQUIRE(sum < 1.0 || close_enough(sum, 1.0), "sum of transition probabilities is greater than 1: " << sum);
        bounds[i] = icn(sum);
    }
    QL_REQUIRE(close_enough(sum, 1.0), "sum of transition probabilities is not 1: " << sum);
    return bounds;
}

} // namespace QuantExt
