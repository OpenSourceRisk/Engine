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
    Real start = equityCurve_->fixing(coupon_->accrualStartDate(), false, isTotalReturn_);
    Real end = equityCurve_->fixing(coupon_->accrualEndDate(), false, isTotalReturn_);

    Real dividends = equityCurve_->dividendsBetweenDates(coupon_->accrualStartDate(), coupon_->accrualEndDate());
    return (end + dividends - start) / start;
}

void EquityCouponPricer::initialize(const EquityCoupon& coupon) {

    coupon_ = &coupon;

    equityCurve_ = boost::dynamic_pointer_cast<EquityIndex>(coupon.equityCurve());
    isTotalReturn_ = coupon.isTotalReturn();
}

} // QuantExt