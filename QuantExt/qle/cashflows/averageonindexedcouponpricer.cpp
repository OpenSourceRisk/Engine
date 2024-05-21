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

    overnightIndex_ = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(coupon_->index());
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
    QL_REQUIRE(coupon_->rateCutoff() < numPeriods,
               "rate cutoff (" << coupon_->rateCutoff() << ") must be less than number of fixings in period ("
                               << numPeriods << ")");
    Size nCutoff = numPeriods - coupon_->rateCutoff();

    if (approximationType_ == Takada) {
        Size i = 0;
        Date valuationDate = Settings::instance().evaluationDate();
        // Deal with past fixings.
        while (i < numPeriods && fixingDates[std::min(i, nCutoff)] < valuationDate) {
            Rate pastFixing = overnightIndex_->pastFixing(fixingDates[std::min(i, nCutoff)]);
            QL_REQUIRE(pastFixing != Null<Real>(),
                       "Missing " << overnightIndex_->name() << " fixing for " << fixingDates[std::min(i, nCutoff)]);
            accumulatedRate += pastFixing * accrualFractions[i];
            ++i;
        }
        // Use valuation date's fixing also if available.
        if (i < numPeriods && fixingDates[std::min(i, nCutoff)] == valuationDate) {
            Rate valuationDateFixing = overnightIndex_->pastFixing(valuationDate);
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

            // handle the part until the rate cutoff (might be empty, i.e. startForecast = endForecast)
            Date startForecast = coupon_->valueDates()[i];
            Date endForecast = coupon_->valueDates()[std::max(nCutoff, i)];
            DiscountFactor startDiscount = projectionCurve->discount(startForecast);
            DiscountFactor endDiscount = projectionCurve->discount(endForecast);

            // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
            if (nCutoff < numPeriods) {
                // forward discount factor for one calendar day on the cutoff date
                DiscountFactor discountCutoffDate = projectionCurve->discount(coupon_->valueDates()[nCutoff] + 1) /
                                                    projectionCurve->discount(coupon_->valueDates()[nCutoff]);
                // keep the above forward discount factor constant during the cutoff period
                endDiscount *=
                    std::pow(discountCutoffDate, coupon_->valueDates()[numPeriods] - coupon_->valueDates()[nCutoff]);
            }

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
    Rate tau = overnightIndex_->dayCounter().yearFraction(coupon_->valueDates().front(), coupon_->valueDates().back());
    Rate rate = gearing_ * accumulatedRate / tau + spread_;
    return rate;
}

} // namespace QuantExt
