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

#include <qle/cashflows/averageonindexedcouponpricer.hpp>

namespace QuantExt {

void AverageONIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {

    coupon_ = dynamic_cast<const AverageONIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "AverageONIndexedCoupon required");

    overnightIndex_ = boost::dynamic_pointer_cast<OvernightIndex>(coupon_->index());
    QL_REQUIRE(overnightIndex_, "OvernightIndex required");

    gearing_ = coupon_->gearing();
    spread_ = coupon_->spread();
    accrualPeriod_ = coupon_->accrualPeriod();
}

Rate AverageONIndexedCouponPricer::swapletRate() const {

    std::vector<Date> fixingDates = coupon_->fixingDates();
    std::vector<Time> accrualFractions = coupon_->dt();
    Size numPeriods = accrualFractions.size();
    Real accumulatedRate = 0;

    if (approximationType_ == Takada) {
        Size i = 0;
        Date valuationDate = Settings::instance().evaluationDate();
        // Deal with past fixings.
        while (i < numPeriods && fixingDates[i] < valuationDate) {
            Rate pastFixing = overnightIndex_->fixing(fixingDates[i]);
            accumulatedRate += pastFixing * accrualFractions[i];
            ++i;
        }
        // Use valuation date's fixing also if available.
        if (i < numPeriods && fixingDates[i] == valuationDate) {
            Rate valuationDateFixing = IndexManager::instance().getHistory(overnightIndex_->name())[valuationDate];
            if (valuationDateFixing != Null<Real>()) {
                accumulatedRate += valuationDateFixing * accrualFractions[i];
                ++i;
            }
        }
        // Use Takada approximation (2011) for forecasting.
        if (i < numPeriods) {
            Handle<YieldTermStructure> projectionCurve = overnightIndex_->forwardingTermStructure();
            QL_REQUIRE(!projectionCurve.empty(),
                       "Null term structure set to this instance of " << overnightIndex_->name());

            Date startForecast = coupon_->valueDates()[i];
            Date endForecast = coupon_->valueDates()[numPeriods];
            DiscountFactor startDiscount = projectionCurve->discount(startForecast);
            DiscountFactor endDiscount = projectionCurve->discount(endForecast);
            accumulatedRate += log(startDiscount / endDiscount);
        }
    } else if (approximationType_ == None) {
        std::vector<Rate> fixings = coupon_->indexFixings();
        for (Size i = 0; i < numPeriods; ++i) {
            accumulatedRate += fixings[i] * accrualFractions[i];
        }
    } else {
        QL_FAIL("Invalid Approximation for AverageONIndexedCouponPricer");
    }
    // Return factor * rate + spread
    Rate rate = gearing_ * accumulatedRate / accrualPeriod_ + spread_;
    return rate;
}

} // namespace QuantExt
