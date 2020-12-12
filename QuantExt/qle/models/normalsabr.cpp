/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qlep/models/normalsabr.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

Real normalSabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho) {
    // B59.A in Hagan, Managing Smile Risk
    if (alpha < 1E-6)
        return 0.0;
    Real zeta = nu / alpha * (forward - strike);
    Real x = close_enough(rho, 1.0)
                 ? 0.0
                 : std::log((std::sqrt(1.0 - 2.0 * rho * zeta + zeta * zeta) - rho + zeta) / (1.0 - rho));
    Real f = close_enough(x, 0.0) ? 1.0 : zeta / x;
    Real vol = alpha * f * (1.0 + expiryTime * (2.0 - 3.0 * rho * rho) * nu * nu / 24.0);
    QL_REQUIRE(std::isfinite(vol), "normalSabrVolatility: computed invalid vol for strike="
                                       << strike << ", forward=" << forward << ", expiryTime=" << expiryTime
                                       << ", alpha=" << alpha << ", nu=" << nu << ", rho=" << rho);
    return std::max(vol, 0.00001);
}

Real normalSabrAlphaFromAtm(Rate forward, Time expiryTime, Real atmVol, Real nu, Real rho) {
    return std::max(atmVol / (1.0 + expiryTime * (2.0 - 3.0 * rho * rho) * nu * nu / 24.0), 0.00001);
}

} // namespace QuantExt
