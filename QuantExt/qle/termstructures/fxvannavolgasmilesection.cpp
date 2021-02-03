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

#include <ql/math/distributions/normaldistribution.hpp>
#include <qle/termstructures/fxvannavolgasmilesection.hpp>

using namespace QuantLib;

namespace QuantExt {

VannaVolgaSmileSection::VannaVolgaSmileSection(Real spot, Real rd, Real rf, Time t, Volatility atmVol, Volatility rr,
                                               Volatility bf, bool firstApprox, const DeltaVolQuote::AtmType& atmType,
                                               const DeltaVolQuote::DeltaType& deltaType, const Real delta)
    : FxSmileSection(spot, rd, rf, t), atmVol_(atmVol), rr_(rr), bf_(bf), firstApprox_(firstApprox) {

    // Consistent Pricing of FX Options
    // Castagna & Mercurio (2006)
    // eq(4) + (5).
    vol_c_ = atmVol + bf_ + 0.5 * rr_;
    vol_p_ = atmVol + bf_ - 0.5 * rr_;

    // infer strikes from delta and vol quote
    try {
        BlackDeltaCalculator a(Option::Type::Call, deltaType, spot, domesticDiscount(), foreignDiscount(),
                               sqrt(t) * atmVol);
        k_atm_ = a.atmStrike(atmType);
    } catch (const std::exception& e) {
        QL_FAIL("VannaVolgaSmileSection: Error during calculating atm strike: "
                << e.what() << " (t=" << t << ", atmVol=" << atmVol << ", bf=" << bf_ << ", rr=" << rr << ", vol_c="
                << vol_c_ << ", vol_p=" << vol_p_ << ", atmType=" << atmType << ", deltaType=" << deltaType
                << ", spot=" << spot << ", domDsc=" << domesticDiscount() << ", forDsc=" << foreignDiscount() << ")");
    }

    try {
        BlackDeltaCalculator c(Option::Type::Call, deltaType, spot, domesticDiscount(), foreignDiscount(),
                               sqrt(t) * vol_c_);
        k_c_ = c.strikeFromDelta(delta);
    } catch (const std::exception& e) {
        QL_FAIL("VannaVolgaSmileSection: Error during calculating call strike at delta "
                << delta << ": " << e.what() << " (t=" << t << ", atmVol=" << atmVol << ", bf=" << bf_ << ", rr=" << rr
                << ", vol_c=" << vol_c_ << ", vol_p=" << vol_p_ << ", deltaType=" << deltaType << ", spot=" << spot
                << ", domDsc=" << domesticDiscount() << ", forDsc=" << foreignDiscount() << ")");
    }

    try {
        BlackDeltaCalculator p(Option::Type::Put, deltaType, spot, domesticDiscount(), foreignDiscount(),
                               sqrt(t) * vol_p_);
        k_p_ = p.strikeFromDelta(-delta);
    } catch (const std::exception& e) {
        QL_FAIL("VannaVolgaSmileSection: Error during calculating put strike at delta "
                << delta << ": " << e.what() << " (t=" << t << ", atmVol=" << atmVol << ", bf=" << bf_ << ", rr=" << rr
                << ", vol_c=" << vol_c_ << ", vol_p=" << vol_p_ << ", deltaType=" << deltaType << ", spot=" << spot
                << ", domDsc=" << domesticDiscount() << ", forDsc=" << foreignDiscount() << ")");
    }
}

Real VannaVolgaSmileSection::d1(Real x) const {
    return (log(spot_ / x) + (rd_ - rf_ + 0.5 * atmVol_ * atmVol_) * t_) / (atmVol_ * sqrt(t_));
}

Real VannaVolgaSmileSection::d2(Real x) const {
    return (log(spot_ / x) + (rd_ - rf_ - 0.5 * atmVol_ * atmVol_) * t_) / (atmVol_ * sqrt(t_));
}

Volatility VannaVolgaSmileSection::volatility(Real k) const {
    QL_REQUIRE(k >= 0, "Non-positive strike (" << k << ")");

    // eq(14). Note sigma = sigma_ATM here.
    Real k1 = k_p_;
    Real k2 = k_atm_;
    Real k3 = k_c_;

    // TODO: Cache the (constant) denominator
    Real r1 = log(k2 / k) * log(k3 / k) / (log(k2 / k1) * log(k3 / k1));
    Real r2 = log(k / k1) * log(k3 / k) / (log(k2 / k1) * log(k3 / k2));
    Real r3 = log(k / k1) * log(k / k2) / (log(k3 / k1) * log(k3 / k2));

    Real sigma1_k = r1 * vol_p_ + r2 * atmVol_ + r3 * vol_c_;
    if (firstApprox_) {
        return std::max(sigma1_k, Real(0.0001)); // for extreme ends: cannot return negative impl vols
    }

    Real D1 = sigma1_k - atmVol_;

    // No middle term as sigma = sigma_atm
    Real D2 = r1 * d1(k1) * d2(k1) * (vol_p_ - atmVol_) * (vol_p_ - atmVol_) +
              r3 * d1(k3) * d2(k3) * (vol_c_ - atmVol_) * (vol_c_ - atmVol_);

    Real d1d2k = d1(k) * d2(k);

    Real tmp = atmVol_ * atmVol_ + d1d2k * (2 * atmVol_ * D1 + D2);
    QL_REQUIRE(tmp >= 0, "VannaVolga attempting to take square root of negative number in second approximation. "
                         "Consider using first approximation in fxvol config.");

    return atmVol_ + (-atmVol_ + sqrt(tmp)) / d1d2k;
}

} // namespace QuantExt
