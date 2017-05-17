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

#include <qle/cashflows/subperiodscouponpricer.hpp>

namespace QuantExt {

void SubPeriodsCouponPricer::initialize(const FloatingRateCoupon& coupon) {

    coupon_ = dynamic_cast<const SubPeriodsCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "SubPeriodsCoupon required");

    index_ = boost::dynamic_pointer_cast<InterestRateIndex>(coupon_->index());
    QL_REQUIRE(index_, "InterestRateIndex required");

    gearing_ = coupon_->gearing();
    spread_ = coupon_->spread();
    accrualPeriod_ = coupon_->accrualPeriod();
    type_ = coupon_->type();
    includeSpread_ = coupon_->includeSpread();
}

Rate SubPeriodsCouponPricer::swapletRate() const {

    std::vector<Time> accrualFractions = coupon_->accrualFractions();
    Size numPeriods = accrualFractions.size();
    Spread incSpread = includeSpread_ ? spread_ : 0.0;
    Spread excSpread = includeSpread_ ? 0.0 : spread_;
    Real accumulatedRate;
    Rate rate;

    std::vector<Rate> fixings = coupon_->indexFixings();
    if (type_ == SubPeriodsCoupon::Averaging) {
        accumulatedRate = 0.0;
        for (Size i = 0; i < numPeriods; ++i) {
            accumulatedRate += (fixings[i] + incSpread) * accrualFractions[i];
        }
        rate = gearing_ * accumulatedRate / accrualPeriod_ + excSpread;
    } else if (type_ == SubPeriodsCoupon::Compounding) {
        accumulatedRate = 1.0;
        for (Size i = 0; i < numPeriods; ++i) {
            accumulatedRate *= (1.0 + (fixings[i] + incSpread) * accrualFractions[i]);
        }
        rate = gearing_ * (accumulatedRate - 1.0) / accrualPeriod_ + excSpread;
    } else {
        QL_FAIL("Invalid sub-period coupon type");
    }

    return rate;
}
} // namespace QuantExt
