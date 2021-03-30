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

Rate EquityCouponPricer::swapletRate() const {
    // Start fixing shouldn't include dividends as the assumption of continuous dividends means they will have been paid
    // as they accrued in the previous period (or at least at the end when performance is measured).
    Real start = coupon_->initialPrice();
    Real end = equityCurve_->fixing(coupon_->fixingEndDate(), false, false);

    // fx rates at start and end, at start we only convert if the initial price is not already in target ccy
    Real fxStart =
        fxIndex_ && !coupon_->initialPriceIsInTargetCcy() ? fxIndex_->fixing(coupon_->fixingStartDate()) : 1.0;
    Real fxEnd = fxIndex_ ? fxIndex_->fixing(coupon_->fixingEndDate()) : 1.0;

    Real dividends = 0.0;

    // Dividends that are already fixed dividends + yield accrued over remaining period.
    // yield accrued = Forward without dividend yield - Forward with dividend yield
    if (isTotalReturn_) {
        // projected dividends from today until the fixing end date
        dividends = equityCurve_->fixing(coupon_->fixingEndDate(), false, true) -
                    equityCurve_->fixing(coupon_->fixingEndDate(), false, false);
        // subtract projected dividends from today until the fixing start date
        if (coupon_->fixingStartDate() > Settings::instance().evaluationDate()) {
            dividends -= (equityCurve_->fixing(coupon_->fixingStartDate(), false, true) -
                          equityCurve_->fixing(coupon_->fixingStartDate(), false, false));
        }
        // add historical dividends
        dividends += equityCurve_->dividendsBetweenDates(coupon_->fixingStartDate(), coupon_->fixingEndDate());
    }

    if (start == 0) {
        return (end + dividends * dividendFactor_) * fxEnd;
    } else if (absoluteReturn_) {
        return ((end + dividends * dividendFactor_) * fxEnd - start * fxStart);
    } else {
        return ((end + dividends * dividendFactor_) * fxEnd - start * fxStart) / (start * fxStart);
    }
}

void EquityCouponPricer::initialize(const EquityCoupon& coupon) {

    coupon_ = &coupon;

    equityCurve_ = boost::dynamic_pointer_cast<EquityIndex>(coupon.equityCurve());
    fxIndex_ = boost::dynamic_pointer_cast<FxIndex>(coupon.fxIndex());
    isTotalReturn_ = coupon.isTotalReturn();
    absoluteReturn_ = coupon.absoluteReturn();
    dividendFactor_ = coupon.dividendFactor();
}

} // namespace QuantExt
