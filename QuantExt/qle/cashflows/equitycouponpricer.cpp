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
    Real start = equityCurve_->fixing(coupon_->accrualStartDate(), false, false);
    Real end = equityCurve_->fixing(coupon_->accrualEndDate(), false, false);

    Real dividends = 0.0;
    
    // Dividends that are already fixed dividends + yield accrued over remaining period.
    // yield accrued = Forward without dividend yield - Forward with dividend yield
    if (isTotalReturn_)
        dividends = (equityCurve_->fixing(coupon_->accrualEndDate(), false, true) - end) +
                    equityCurve_->dividendsBetweenDates(coupon_->accrualStartDate(), coupon_->accrualEndDate());

    return (end + dividends * dividendFactor_ - start) / start;
}

void EquityCouponPricer::initialize(const EquityCoupon& coupon) {

    coupon_ = &coupon;

    equityCurve_ = boost::dynamic_pointer_cast<EquityIndex>(coupon.equityCurve());
    isTotalReturn_ = coupon.isTotalReturn();
    dividendFactor_ = coupon.dividendFactor();
}

} // QuantExt