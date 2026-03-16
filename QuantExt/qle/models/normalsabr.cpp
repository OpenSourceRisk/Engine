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

#include <qle/models/exactbachelierimpliedvolatility.hpp>
#include <qle/models/normalsabr.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>

namespace QuantExt {

Real normalSabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho) {

    // update extreme parameters

    alpha = std::max(alpha, 1E-5);
    if (rho < -1 + 1E-5)
        rho = -1 + 1E-5;
    else if (rho > 1 - 1E-5)
        rho = 1 - 1E-5;

    // calculate result

    Real zeta = nu / alpha * (forward - strike);
    Real x = std::log((std::sqrt(1.0 - 2.0 * rho * zeta + zeta * zeta) - rho + zeta) / (1.0 - rho));
    Real f = close_enough(x, 0.0) ? 1.0 : zeta / x;
    Real vol = alpha * f * (1.0 + expiryTime * (2.0 - 3.0 * rho * rho) * nu * nu / 24.0);
    QL_REQUIRE(std::isfinite(vol), "normalSabrVolatility: computed invalid vol for strike="
                                       << strike << ", forward=" << forward << ", expiryTime=" << expiryTime
                                       << ", alpha=" << alpha << ", nu=" << nu << ", rho=" << rho);
    return std::max(vol, 0.00001);
}

Real normalSabrAlphaFromAtmVol(Rate forward, Time expiryTime, Real atmVol, Real nu, Real rho) {
    return std::max(atmVol / (1.0 + expiryTime * (2.0 - 3.0 * rho * rho) * nu * nu / 24.0), 0.00001);
}

namespace {

Real deltaR(const Real t) { return std::exp(t / 8.0) - (3072.0 + t * (384.0 + t * (24.0 + t))) / 3072.0; }

Real g(const Real s) { return s / std::tanh(s) - 1.0; }

Real R(const Real t, const Real s) {
    Real s2 = s * s;
    Real s4 = s2 * s2;
    if (s < 0.03) {
        return (3072.0 + t * (384.0 + t * (24.0 + t))) / 3072.0 - t * (2688.0 + t * (80.0 + 21.0 * t)) / 322560.0 * s2 +
               t * (2816.0 - t * (88.0 + 63.0 * t)) / 3548160.0 * s4;
    }
    Real s6 = s2 * s4;
    Real t2 = t * t;
    Real t3 = t2 * t;
    Real gval = g(s);
    return 1.0 + 3.0 * t * gval / (8.0 * s2) - (5.0 * t2 * (-8.0 * s2 + gval * (24.0 + 3.0 * gval))) / (128.0 * s4) +
           (35.0 * t3 * (-40.0 * s2 + gval * (120 + gval * (24.0 + 3.0 * gval)))) / (1024.0 * s6);
}

Real G(const Real t, const Real s) {
    return std::sqrt(std::sinh(s) / s) * std::exp(-s * s / (2.0 * t) - t / 8.0) * (R(t, s) + deltaR(t));
}

} // namespace

Real normalFreeBoundarySabrPrice(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho) {
    // update extreme parameters

    nu = std::max(nu, 1E-6);
    if (rho < -1 + 1E-5)
        rho = -1 + 1E-5;
    else if (rho > 1 - 1E-5)
        rho = 1 - 1E-5;

    // compute option price

    Real V0 = alpha / nu;
    Real k = (strike - forward) / V0 + rho;
    Real rhobar = std::sqrt(1.0 - rho * rho);
    Real arg = (-rho * k + std::sqrt(k * k + rhobar * rhobar)) / (rhobar * rhobar);
    QL_REQUIRE(arg > 1.0 - 1E-12, "invalid arg (" << arg << "), must be >= 1");
    Real s0 = std::acosh(std::max(1.0, arg));

    auto integrand = [k, rho, nu, expiryTime](const Real s) {
        Real tmp = (k - rho * std::cosh(s));
        Real arg = std::sinh(s) * std::sinh(s) - tmp * tmp;
        QL_REQUIRE(arg > -1E-12, "invalid arg (" << arg << "), must be >= 0 (tmp=" << tmp << ")");
        return G(nu * nu * expiryTime, s) / std::sinh(s) * std::sqrt(std::max(0.0, arg));
    };

    Real lowerBound = std::max(s0, 1E-12);
    Real upperBound = std::max(1.5 * s0, 1.0);
    while (integrand(upperBound) > 1E-12)
        upperBound *= 1.5;

    GaussLobattoIntegral gl(10000, 1E-8);
    Real price = V0 / M_PI * gl(integrand, lowerBound, upperBound);
    price += std::max(forward - strike, 0.0);

    return price;
}

Real normalFreeBoundarySabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho) {
    return exactBachelierImpliedVolatility(Option::Call, strike, forward, expiryTime,
                                           normalFreeBoundarySabrPrice(strike, forward, expiryTime, alpha, nu, rho));
}

} // namespace QuantExt
