/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <qle/math/stoplightbounds.hpp>

// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/math/distributions/binomial.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <algorithm>

#ifdef ORE_USE_EIGEN
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#include <Eigen/Sparse>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#else
#include <ql/math/matrixutilities/qrdecomposition.hpp>
#endif

using namespace QuantLib;
using namespace boost::accumulators;

namespace QuantExt {
namespace {
void checkMatrix(const Matrix& m) {
    Size n = m.rows();
    QL_REQUIRE(n > 0, "matrix is null");
    for (Size i = 0; i < n; ++i) {
        QL_REQUIRE(QuantLib::close_enough(m[i][i], 1.0),
                   "correlation matrix has non unit diagonal element at (" << i << "," << i << ")");
        for (Size j = 0; j < n; ++j) {
            QL_REQUIRE(QuantLib::close_enough(m[i][j], m[j][i]), "correlation matrix is not symmetric, for (i,j)=("
                                                           << i << "," << j << "), values are " << m[i][j] << " and "
                                                           << m[j][i]);
            QL_REQUIRE(m[i][j] >= -1.0 && m[i][j] <= 1.0,
                       "correlation matrix entry out of bounds at (" << i << "," << j << "), value is " << m[i][j]);
        }
    }
} // checkMatrix

} // anonymous namespace

std::vector<Size> stopLightBoundsTabulated(const std::vector<Real>& stopLightP, const Size observations,
                                           const Size numberOfDays, const Real p, const std::vector<int>& obsNb,
                                           const std::vector<int>& amberLimit, const std::vector<int>& redLimit) {

    if (stopLightP.size() == 2 && QuantLib::close_enough(stopLightP[0], 0.95) &&
        QuantLib::close_enough(stopLightP[1], 0.9999) && numberOfDays == 10 && observations >= 1 &&
        observations <= (obsNb.back() + 9) && QuantLib::close_enough(p, 0.99)) {
        Size idx = std::upper_bound(obsNb.begin(), obsNb.end(), observations) - obsNb.begin();
        QL_REQUIRE(idx > 0 && idx <= amberLimit.size() && idx <= redLimit.size(),
                   "stopLightBoundsTabulated: unexpected index 0");
        return {static_cast<Size>(amberLimit[idx - 1]), static_cast<Size>(redLimit[idx - 1])};
    } else {
        QL_FAIL(
            "stopLightBoundsTabulated: no tabulated values found for sl-p = "
            << boost::join(stopLightP | boost::adaptors::transformed([](double x) { return std::to_string(x); }), ",")
            << ", obs = " << observations << ", numberOfDays = " << numberOfDays << ", p = " << p);
    }
}

std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations, const Size numberOfDays,
                                  const Real p, const Size numberOfPortfolios, const Matrix& correlation,
                                  const Size samples, const Size seed, const SalvagingAlgorithm::Type salvaging,
                                  const Size exceptions, Real* cumProb) {
    checkMatrix(correlation);
    Size r = correlation.rows();

    QL_REQUIRE(stopLightP.size() > 0, "stopLightBounds: stopLightP is empty");
    QL_REQUIRE(numberOfDays > 0, "stopLightBounds: numberOfDays must be greater than zero");
    QL_REQUIRE(numberOfPortfolios > 0, "stopLightBounds: numberOfPortfolios must be greater than zero");
    QL_REQUIRE(numberOfPortfolios == r, "stopLightBounds: numberOfPortfolios ("
                                            << numberOfPortfolios << ") must match correlation matrix dimension (" << r
                                            << "x" << r);
    QL_REQUIRE(samples > 0, "stopLightBounds: samples must be greater than zero");
    QL_REQUIRE(p > 0.0, "stopLightBounds: p must be greater than zero");
    if (exceptions != Null<Size>()) {
        QL_REQUIRE(cumProb != nullptr, "stopLightBounds: cumProb is a null pointer");
        *cumProb = 0.0;
    }

    Matrix pseudoRoot = pseudoSqrt(correlation, salvaging);
    Size len = observations + (numberOfDays - 1);
    auto sgen = PseudoRandom::make_sequence_generator(len * r, seed);
    // auto sgen = LowDiscrepancy::make_sequence_generator(len * r, seed);
    Real h = InverseCumulativeNormal()(p) * std::sqrt(static_cast<Real>(numberOfDays));
    Real minP = *std::min_element(stopLightP.begin(), stopLightP.end());
    Size cache = Size(std::floor(static_cast<Real>(samples) * (1.0 - minP) + 0.5)) + 2;
    accumulator_set<Real, stats<tag::tail_quantile<right>>> acc(tag::tail<right>::cache_size = cache);
    for (Size i = 0; i < samples; ++i) {
        auto seq = sgen.nextSequence().value;
        Size exCount = 0;
        for (Size rr = 0; rr < r; ++rr) {
            Array oneDayPls(len, 0.0);
            for (Size l = 0; l < len; ++l) {
                for (Size kk = 0; kk < r; ++kk) {
                    oneDayPls[l] += pseudoRoot[rr][kk] * seq[len * kk + l];
                }
            }
            // compute the 10d PL only once ...
            Real pl = 0.0;
            for (Size dd = 0; dd < numberOfDays; ++dd) {
                pl += oneDayPls[dd];
            }
            if (pl > h)
                ++exCount;
            for (Size l = 0; l < observations - 1; ++l) {
                // and only correct for the tail and head
                pl += oneDayPls[l + numberOfDays] - oneDayPls[l];
                if (pl > h)
                    ++exCount;
            }
        } // for rr
        acc(static_cast<Real>(exCount));
        if (exceptions != Null<Size>() && exCount <= exceptions) {
            *cumProb += 1.0 / static_cast<Real>(samples);
        }
    } // for samples
    std::vector<Size> res;
    for (auto const s : stopLightP) {
        Size tmp = static_cast<Size>(quantile(acc, quantile_probability = s));
        res.push_back(tmp > 0 ? tmp - 1 : 0);
    }
    return res;
} // stopLightBounds (overlapping, correlated)

std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations, const Real p,
                                  const Size exceptions, Real* cumProb) {

    QL_REQUIRE(stopLightP.size() > 0, "stopLightBounds: stopLightP is empty");
    QL_REQUIRE(p > 0.0, "stopLightBounds: p must be greater than zero");
    if (exceptions != Null<Size>()) {
        QL_REQUIRE(cumProb != nullptr, "stopLightBounds: cumProb is a null pointer");
        *cumProb = 0.0;
    }
    boost::math::binomial_distribution<Real> b((Real)observations, (Real)(1.0) - p);
    std::vector<Size> res;
    for (auto const s : stopLightP) {

        QL_REQUIRE(s > 0.5, "stopLightBounds: stopLightP (" << s << ") must be greater than 0.5");

        /* According to https://www.boost.org/doc/libs/1_86_0/libs/math/doc/html/math_toolkit/dist_ref/dists/binomial_dist.html

           "the quantile function will by default return an integer result that has been rounded outwards. That is to
           say lower quantiles (where the probability is less than 0.5) are rounded downward, and upper quantiles (where
           the probability is greater than 0.5) are rounded upwards. This behaviour ensures that if an X% quantile is
           requested, then at least the requested coverage will be present in the central region, and no more than the
           requested coverage will be present in the tails."

           This means that the quantile K we compute for p fulfills P( X <= K ) >= p.

           Note: If the computed K is zero, the end result has to be zero too. */

        res.push_back(std::max(static_cast<Size>(boost::math::quantile(b, s)), (Size)(1)) - 1);
    }
    if (exceptions != Null<Size>()) {
        *cumProb = boost::math::cdf(b, exceptions);
    }
    return res;
} // stopLightBounds (iid)

std::vector<std::pair<Size, std::vector<Size>>> generateStopLightBoundTable(const std::vector<Size>& observations,
                                                                            const std::vector<Real>& stopLightP,
                                                                            const Size samples, const Size seed,
                                                                            const Size numberOfDays, const Real p) {

    QL_REQUIRE(!observations.empty(), "generateStopLightBoundTable(): no observations given");
    QL_REQUIRE(!stopLightP.empty(), "generateStopLightBoundTable(): stopLightP is empty");
    QL_REQUIRE(numberOfDays > 0, "generateStopLightBoundTable(): numberOfDays must be greater than zero");
    QL_REQUIRE(samples > 0, "generateStopLightBoundTable(): samples must be greater than zero");
    QL_REQUIRE(p > 0.0, "generateStopLightBoundTable():: p must be greater than zero");

    for (Size i = 0; i < observations.size() - 1; ++i) {
        QL_REQUIRE(observations[i] > 0, "generateStopLightBoundTable(): observations must be positive, got 0 at " << i);
        QL_REQUIRE(observations[i] < observations[i + 1],
                   "generateStopLightBoundTable(): observations must be increasing, got "
                       << observations[i] << " at " << i << " and " << observations[i + 1] << " at " << i + 1);
    }

    Size len = observations.back() + (numberOfDays - 1);
    auto sgen = PseudoRandom::make_sequence_generator(len, seed);
    Real h = InverseCumulativeNormal()(p) * std::sqrt(static_cast<Real>(numberOfDays));

    /*! populate matrix where the rows correspond to the observations and column j
       contain P(exceptions == j) */
    // heuristic... is this ok, too low (will throw an excetpion below), too high (wastes memory)?
    Size cols;
    if (observations.back() <= 100)
        cols = observations.back() + 1;
    else if (observations.back() <= 500)
        cols = observations.back() / 5;
    else
        cols = observations.back() / 10;

    Matrix cumProb(observations.size(), cols, 0.0);

    for (Size i = 0; i < samples; ++i) {
        auto seq = sgen.nextSequence().value;
        Size exCount = 0, obsIdx = 0;
        Real pl = 0.0;
        for (Size l = 0; l < observations.back(); ++l) {
            if (l == 0) {
                // compute the 10d PL only once ...
                for (Size dd = 0; dd < numberOfDays; ++dd) {
                    pl += seq[dd];
                }
            } else {
                // and only correct for the tail and head
                pl += seq[l - 1 + numberOfDays] - seq[l - 1];
            }
            if (pl > h)
                ++exCount;
            if (l == observations[obsIdx] - 1) {
                if (exCount < cols)
                    cumProb(obsIdx, exCount) += 1.0 / static_cast<Real>(samples);
                obsIdx++;
            }
        }
    }

    // compute result table
    std::vector<std::pair<Size, std::vector<Size>>> result;
    for (Size i = 0; i < observations.size(); ++i) {
        Real P = 0.0;
        Size idx = 0;
        std::vector<Size> b;
        for (Size j = 0; j < cols && idx < stopLightP.size(); ++j) {
            P += cumProb(i, j);
            while (idx < stopLightP.size() && (P >= stopLightP[idx] || QuantLib::close_enough(P, stopLightP[idx]))) {
                b.push_back(std::max<Size>(j, 1) - 1);
                idx++;
            }
        }
        QL_REQUIRE(b.size() == stopLightP.size(),
                   "generateStopLightBoundTable(): could not determine bound for observations = "
                       << observations[i] << " and stopLightP = " << stopLightP[b.size()]
                       << " - try to increase number of cols in matrix cumProb");
        result.push_back(std::make_pair(observations[i], b));
    }

    return result;
}

std::vector<double> decorrelateOverlappingPnls(const std::vector<double>& pnl, const Size numberOfDays) {

    if (numberOfDays == 1)
        return pnl;

#ifdef ORE_USE_EIGEN

    Eigen::SparseMatrix<double> correlation(pnl.size(), pnl.size());

    for (Size i = 0; i < pnl.size(); ++i) {
        for (Size j = i - std::min<Size>(i, numberOfDays - 1);
             j <= i + std::min<Size>(numberOfDays - 1, pnl.size() - i - 1); ++j) {
            correlation.insert(i, j) =
                1.0 - static_cast<double>(std::abs((int)i - (int)j)) / static_cast<double>(numberOfDays);
        }
    }

    Eigen::VectorXd b(pnl.size());

    for (Size i = 0; i < pnl.size(); ++i)
        b[i] = pnl[i];

    Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> cholesky(correlation);
    Eigen::SparseMatrix<double> L = cholesky.matrixL();
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.analyzePattern(L);
    solver.factorize(L);
    Eigen::VectorXd x = cholesky.permutationP().inverse() * solver.solve(cholesky.permutationP() * b);

    std::vector<double> res(pnl.size());
    for (Size i = 0; i < pnl.size(); ++i)
        res[i] = x[i];

    return res;

#else

    Matrix correlation(pnl.size(), pnl.size(), 0.0);

    for (Size i = 0; i < pnl.size(); ++i) {
        for (Size j = i - std::min<Size>(i, numberOfDays - 1);
             j <= i + std::min<Size>(numberOfDays - 1, pnl.size() - i - 1); ++j) {
            correlation(i, j) =
                1.0 - static_cast<double>(std::abs((int)i - (int)j)) / static_cast<double>(numberOfDays);
        }
    }

    Array b(pnl.begin(), pnl.end());
    Matrix L = CholeskyDecomposition(correlation);
    Array x = QuantLib::qrSolve(L, b);
    return std::vector<double>(x.begin(), x.end());

#endif
}

} // namespace QuantExt
