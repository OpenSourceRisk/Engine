/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/cashflows/equitycouponpricer.hpp>

namespace QuantExt {

void EquityCouponPricer::AdditionalResultCache::clear() {
    initialPrice = Null<Real>();
    startFixingTotal = Null<Real>();
    startFixing = Null<Real>();
    startFxFixing = Null<Real>();
    endFixingTotal = Null<Real>();
    endFixing = Null<Real>();
    endFxFixing = Null<Real>();
    pastDividends = Null<Real>();
    forecastDividends = Null<Real>();
}

Rate EquityCouponPricer::swapletRate() {
    additionalResultCache_.clear();

    // Start fixing shouldn't include dividends as the assumption of continuous dividends means they will have been paid
    // as they accrued in the previous period (or at least at the end when performance is measured).
    additionalResultCache_.initialPrice = coupon_->initialPrice();
    additionalResultCache_.endFixing = equityCurve_->fixing(coupon_->fixingEndDate(), false, false);

    // fx rates at start and end, at start we only convert if the initial price is not already in target ccy
    additionalResultCache_.startFxFixing =
        fxIndex_ && !coupon_->initialPriceIsInTargetCcy() ? fxIndex_->fixing(coupon_->fixingStartDate()) : 1.0;
    additionalResultCache_.endFxFixing = fxIndex_ ? fxIndex_->fixing(coupon_->fixingEndDate()) : 1.0;

    Real dividends = 0.0;

    // Dividends that are already fixed dividends + yield accrued over remaining period.
    // yield accrued = Forward without dividend yield - Forward with dividend yield
    if (returnType_ == EquityReturnType::Total || returnType_ == EquityReturnType::Dividend) {
        // projected dividends from today until the fixing end date
        additionalResultCache_.endFixingTotal = equityCurve_->fixing(coupon_->fixingEndDate(), false, true);
        dividends = additionalResultCache_.endFixingTotal - additionalResultCache_.endFixing;
        // subtract projected dividends from today until the fixing start date
        if (coupon_->fixingStartDate() > Settings::instance().evaluationDate()) {
            additionalResultCache_.startFixingTotal = equityCurve_->fixing(coupon_->fixingStartDate(), false, true);
            additionalResultCache_.startFixing = equityCurve_->fixing(coupon_->fixingStartDate(), false, false);
            dividends -= (additionalResultCache_.startFixingTotal - additionalResultCache_.startFixing);
        }
        additionalResultCache_.forecastDividends = dividends;
        // add historical dividends
        additionalResultCache_.pastDividends =
            equityCurve_->dividendsBetweenDates(coupon_->fixingStartDate(), coupon_->fixingEndDate());
        dividends += additionalResultCache_.pastDividends;
    }

    if (returnType_ == EquityReturnType::Dividend)
        return dividends; 
    else if (additionalResultCache_.initialPrice == 0)
        return (additionalResultCache_.endFixing + dividends * dividendFactor_) * additionalResultCache_.endFxFixing;
    else if (returnType_ == EquityReturnType::Absolute)
        return ((additionalResultCache_.endFixing + dividends * dividendFactor_) * additionalResultCache_.endFxFixing -
                additionalResultCache_.initialPrice * additionalResultCache_.startFxFixing);
    else
        return ((additionalResultCache_.endFixing + dividends * dividendFactor_) * additionalResultCache_.endFxFixing -
                additionalResultCache_.initialPrice * additionalResultCache_.startFxFixing) /
               (additionalResultCache_.initialPrice * additionalResultCache_.startFxFixing);
}

void EquityCouponPricer::initialize(const EquityCoupon& coupon) {

    coupon_ = &coupon;

    equityCurve_ = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityIndex2>(coupon.equityCurve());
    fxIndex_ = QuantLib::ext::dynamic_pointer_cast<FxIndex>(coupon.fxIndex());
    returnType_ = coupon.returnType();
    dividendFactor_ = coupon.dividendFactor();
}

} // namespace QuantExt
