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

/*! \file qle/math/deltagammavar.hpp
    \brief functions to compute delta or delta-gamma VaR numbers
    \ingroup math
*/

#ifndef quantext_deltagammavar_hpp
#define quantext_deltagammavar_hpp

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/utilities/disposable.hpp>

// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/foreach.hpp>

using namespace QuantLib;

namespace QuantExt {

//! function that computes a delta VaR
/*! For a given covariance matrix and a delta vector this function computes a parametric var w.r.t. a given
 * confidence level for multivariate normal risk factors. */
Real deltaVar(const Matrix& omega, const Array& delta, const Real p);

//! function that computes a delta-gamma normal VaR
/*! For a given a covariance matrix, a delta vector and a gamma matrix this function computes a parametric var
 * w.r.t. a given confidence level. The gamma matrix is taken into account when computing the variance of the PL
 * distirbution, but the PL distribution is still assumed to be normal. */
Real deltaGammaVarNormal(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p);

//! function that computes a delta-gamma VaR using Monte Carlo (single quantile)
/*! For a given a covariance matrix, a delta vector and a gamma matrix this function computes a parametric var
 * w.r.t. a given confidence level. The var quantile is estimated from Monte-Carlo realisations of a second order
 * sensitivity based PL. */
template <class RNG>
Real deltaGammaVarMc(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p, const Size paths,
                     const Size seed);

//! function that computes a delta-gamma VaR using Monte Carlo (multiple quantiles)
/*! For a given a covariance matrix, a delta vector and a gamma matrix this function computes a parametric var
 * w.r.t. a vector of given confidence levels. The var quantile is estimated from Monte-Carlo realisations of a second
 * order sensitivity based PL. */
template <class RNG>
Disposable<std::vector<Real> > deltaGammaVarMc(const Matrix& omega, const Array& delta, const Matrix& gamma,
                                               const std::vector<Real>& p, const Size paths, const Size seed);

namespace detail {
void check(const Real p);
void check(const Matrix& omega, const Array& delta);
void check(const Matrix& omega, const Array& delta, const Matrix& gamma);
template <typename A> Real absMax(const A& a) {
    Real tmp = 0.0;
    BOOST_FOREACH (Real x, a) {
        if (std::abs(x) > tmp)
            tmp = std::abs(x);
    }
    return tmp;
}
} // namespace detail

// implementation

template <class RNG>
Disposable<std::vector<Real> > deltaGammaVarMc(const Matrix& omega, const Array& delta, const Matrix& gamma,
                                               const std::vector<Real>& p, const Size paths, const Size seed) {
    BOOST_FOREACH (Real q, p) { detail::check(q); }
    detail::check(omega, delta, gamma);

    Real num = std::max(detail::absMax(delta), detail::absMax(gamma));
    if (close_enough(num, 0.0)) {
        std::vector<Real> res(p.size(), 0.0);
        return res;
    }

    Matrix L = CholeskyDecomposition(omega, true);

    Real pmin = QL_MAX_REAL;
    BOOST_FOREACH (Real q, p) { pmin = std::min(pmin, q); }

    Size cache = Size(std::floor(static_cast<double>(paths) * (1.0 - pmin) + 0.5)) + 2;
    boost::accumulators::accumulator_set<
        double, boost::accumulators::stats<boost::accumulators::tag::tail_quantile<boost::accumulators::right> > >
        acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = cache);

    typename RNG::rsg_type rng = RNG::make_sequence_generator(delta.size(), seed);

    for (Size i = 0; i < paths; ++i) {
        std::vector<Real> seq = rng.nextSequence().value;
        Array z(seq.begin(), seq.end());
        Array u = L * z;
        acc(DotProduct(u, delta) + 0.5 * DotProduct(u, gamma * u));
    }

    std::vector<Real> res;
    BOOST_FOREACH (Real q, p) {
        res.push_back(boost::accumulators::quantile(acc, boost::accumulators::quantile_probability = q));
    }

    return res;
}

template <class RNG>
Real deltaGammaVarMc(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p, const Size paths,
                     const Size seed) {

    std::vector<Real> pv(1, p);
    return deltaGammaVarMc<RNG>(omega, delta, gamma, pv, paths, seed).front();
}

} // namespace QuantExt

#endif
