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

#include <qle/models/normalsabr.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

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

} // namespace QuantExt
