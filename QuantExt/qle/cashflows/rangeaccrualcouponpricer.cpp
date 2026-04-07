/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <qle/cashflows/rangeaccrualcouponpricer.hpp>

#include <ql/pricingengines/blackformula.hpp>

#include <cmath>

namespace QuantExt {

using namespace QuantLib;

RangeAccrualPricerByCallSpread::RangeAccrualPricerByCallSpread(
    Handle<OptionletVolatilityStructure> ovs, Real eps)
    : ovs_(std::move(ovs)), eps_(eps) {
    registerWith(ovs_);
}

Real RangeAccrualPricerByCallSpread::digitalPrice(Real strike, Real forward,
                                                   Real obsTime, Real deflator) const {
    // Deep ITM / OTM shortcuts
    if (strike <= 0.0)
        return deflator;

    Real ttExpiry = obsTime;
    if (ttExpiry <= 0.0)
        return (forward > strike) ? deflator : 0.0;

    Real K_lo = strike - eps_ / 2.0;
    Real K_hi = strike + eps_ / 2.0;

    Real price;
    if (ovs_->volatilityType() == Normal) {
        Real volLo = ovs_->volatility(ttExpiry, K_lo);
        Real volHi = ovs_->volatility(ttExpiry, K_hi);
        Real stdLo = volLo * std::sqrt(ttExpiry);
        Real stdHi = volHi * std::sqrt(ttExpiry);
        Real cLo = bachelierBlackFormula(Option::Call, K_lo, forward, stdLo, deflator);
        Real cHi = bachelierBlackFormula(Option::Call, K_hi, forward, stdHi, deflator);
        price = (cLo - cHi) / eps_;
    } else {
        Real displacement = ovs_->displacement();
        Real volLo = ovs_->volatility(ttExpiry, K_lo);
        Real volHi = ovs_->volatility(ttExpiry, K_hi);
        Real stdLo = volLo * std::sqrt(ttExpiry);
        Real stdHi = volHi * std::sqrt(ttExpiry);
        Real cLo = blackFormula(Option::Call, K_lo, forward, stdLo, deflator, displacement);
        Real cHi = blackFormula(Option::Call, K_hi, forward, stdHi, deflator, displacement);
        price = (cLo - cHi) / eps_;
    }

    return std::max(price, 0.0);
}

Real RangeAccrualPricerByCallSpread::digitalRangePrice(
    Real lower, Real upper, Real forward, Real obsTime, Real deflator) const {
    Real lowerPrice = digitalPrice(lower, forward, obsTime, deflator);
    Real upperPrice = digitalPrice(upper, forward, obsTime, deflator);
    return std::max(lowerPrice - upperPrice, 0.0);
}

Real RangeAccrualPricerByCallSpread::swapletPrice() const {
    Real result = 0.0;
    const bool isFixedRate = (fixedRate_ != Null<Real>());
    // In fixed-rate mode: deflator = discount (price plain digital indicators)
    // In floating mode:   deflator = discount * L_0 (digital-floater pricing)
    const Real deflator = isFixedRate ? discount_ : discount_ * initialValues_[0];

    for (Size i = 0; i < observationsNo_; ++i) {
        Real digital = digitalRangePrice(lowerTrigger_, upperTrigger_,
                                         initialValues_[i + 1],
                                         observationTimes_[i], deflator);
        result += digital;
    }

    if (isFixedRate) {
        return fixedRate_ * result * accrualFactor_ / observationsNo_;
    } else {
        return gearing_ * (result * accrualFactor_ / observationsNo_) + spreadLegValue_;
    }
}

} // namespace QuantExt
