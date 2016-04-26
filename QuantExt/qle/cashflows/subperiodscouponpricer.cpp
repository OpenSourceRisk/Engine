/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/cashflows/subperiodscouponpricer.hpp>

namespace QuantExt {

    void SubPeriodsCouponPricer::
        initialize(const FloatingRateCoupon& coupon) {

        coupon_ = dynamic_cast<const SubPeriodsCoupon*>(&coupon);
        QL_REQUIRE(coupon_, "SubPeriodsCoupon required");

        index_ = boost::dynamic_pointer_cast
            <InterestRateIndex>(coupon_->index());
        QL_REQUIRE(index_, "InterestRateIndex required");

        gearing_ = coupon_->gearing();
        spread_ = coupon_->spread();
        accrualPeriod_ = coupon_->accrualPeriod();
        type_ = coupon_->type();
        includeSpread_ = coupon_->includeSpread();
    }

    Rate SubPeriodsCouponPricer::swapletRate() const {

        std::vector<Date> fixingDates = coupon_->fixingDates();
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
                accumulatedRate +=
                    (fixings[i] + incSpread) * accrualFractions[i];
            }
            rate = gearing_ * accumulatedRate / accrualPeriod_ + excSpread;
        } else if (type_ == SubPeriodsCoupon::Compounding) {
            accumulatedRate = 1.0;
            for (Size i = 0; i < numPeriods; ++i) {
                accumulatedRate *=
                    (1.0 + (fixings[i] + incSpread) * accrualFractions[i]);
            }
            rate = gearing_ * (accumulatedRate - 1.0) / accrualPeriod_ +
                excSpread;
        } else {
            QL_FAIL("Invalid sub-period coupon type");
        }

        return rate;
    }
}
