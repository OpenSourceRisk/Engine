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

#include <qle/math/deltagammavar.hpp>
#include <qle/math/trace.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/solvers1d/brent.hpp>

namespace QuantExt {

namespace detail {
void check(const Real p) { QL_REQUIRE(p >= 0.0 && p <= 1.0, "p (" << p << ") must be in [0,1] in VaR calculation"); }

void check(const Matrix& omega, const Array& delta) {
    QL_REQUIRE(omega.rows() == omega.columns(),
               "omega (" << omega.rows() << "x" << omega.columns() << ") must be square in VaR calculation");
    QL_REQUIRE(delta.size() == omega.rows(), "delta vector size (" << delta.size() << ") must match omega ("
                                                                   << omega.rows() << "x" << omega.columns() << ")");
}

void check(const Matrix& omega, const Array& delta, const Matrix& gamma) {
    QL_REQUIRE(gamma.rows() == omega.rows() && gamma.columns() == omega.columns(),
               "gamma (" << gamma.rows() << "x" << gamma.columns() << ") must have same dimensions as omega ("
                         << omega.rows() << "x" << omega.columns() << ")");
}
} // namespace detail

namespace {
void moments(const Matrix& omega, const Array& delta, const Matrix& gamma, Real& num, Real& mu, Real& variance) {
    detail::check(omega, delta, gamma);

    // see Carol Alexander, Market Risk, Vol IV
    // Formulas IV5.30 and IV5.31 are buggy though:
    // IV.5.30 should have ... + 3 \delta' \Omega \Gamma \Omega \delta in the numerator
    // IV.5.31 should have ... + 12 \delta’ \Omega (\Gamma \Omega)^2 \delta + 3\sigma^4 in the numerator

    num = std::max(QuantExt::detail::absMax(delta), QuantExt::detail::absMax(gamma));
    if (close_enough(num, 0.0))
        return;

    Array tmpDelta = 1.0 / num * delta;
    Matrix tmpGamma = 1.0 / num * gamma;

    Real dOd = DotProduct(tmpDelta, omega * tmpDelta);
    Matrix go = tmpGamma * omega;
    Matrix go2 = go * go;
    Real trGo2 = Trace(go2);

    mu = 0.5 * Trace(go);
    variance = dOd + 0.5 * trGo2;
}
} // namespace

Real deltaVar(const Matrix& omega, const Array& delta, const Real p) {
    detail::check(p);
    detail::check(omega, delta);
    Real num = detail::absMax(delta);
    if (close_enough(num, 0.0))
        return 0.0;
    Array tmpDelta = delta / num;
    return std::sqrt(DotProduct(tmpDelta, omega * tmpDelta)) * QuantLib::InverseCumulativeNormal()(p) * num;
} // deltaVar

Real deltaGammaVarNormal(const Matrix& omega, const Array& delta, const Matrix& gamma, const Real p) {
    detail::check(p);
    Real s = QuantLib::InverseCumulativeNormal()(p);
    Real num = 0.0, mu = 0.0, variance = 0.0;
    moments(omega, delta, gamma, num, mu, variance);
    if (close_enough(num, 0.0) || close_enough(variance, 0.0))
        return 0.0;
    return (std::sqrt(variance) * s + mu) * num;

} // deltaGammaVarNormal

} // namespace QuantExt
