/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/cashflows/inflationcouponpricer.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace QuantExt {
    using namespace QuantLib;

    CappedFlooredYoYCouponPricer::
        CappedFlooredYoYCouponPricer(const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol)
        : vol_(vol) {
        if (!vol_.empty()) registerWith(vol_);
    }

    void CappedFlooredYoYCouponPricer::setVolatility(
        const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol) {
        QL_REQUIRE(!vol.empty(), "empty optionlet vol handle")
            capletVol_ = vol;
        registerWith(capletVol_);
    }

    Real CappedFlooredYoYCouponPricer::floorletPrice(Rate effectiveFloor) const {
        Real floorletPrice = optionletPrice(Option::Put, effectiveFloor);
        return gearing_ * floorletPrice;
    }

    Real CappedFlooredYoYCouponPricer::capletPrice(Rate effectiveCap) const {
        Real capletPrice = optionletPrice(Option::Call, effectiveCap);
        return gearing_ * capletPrice;
    }

    Rate CappedFlooredYoYCouponPricer::floorletRate(Rate effectiveFloor) const {
        return floorletPrice(effectiveFloor) /
            (coupon_->accrualPeriod()*discount_);
    }

    Rate CappedFlooredYoYCouponPricer::capletRate(Rate effectiveCap) const {
        return capletPrice(effectiveCap) / (coupon_->accrualPeriod()*discount_);
    }

    Real CappedFlooredYoYCouponPricer::optionletPrice(Option::Type optionType,
        Real effStrike) const {
        Date fixingDate = coupon_->fixingDate();
        if (fixingDate <= Settings::instance().evaluationDate()) {
            Real a, b;
            if (optionType == Option::Call) {
                a = coupon_->indexFixing();
                b = effStrike;
            }
            else {
                a = effStrike;
                b = coupon_->indexFixing();
            }
            return std::max(a - b, 0.0)* coupon_->accrualPeriod()*discount_;
        }
        else {
            QL_REQUIRE(!volatility().empty(),
                "missing optionlet volatility");
            Real stdDev =
                std::sqrt(volatility()->totalVariance(fixingDate,
                    effStrike));
            Rate fixing = optionType*effStrike*adjustedFixing()*stdDev;
            return fixing * coupon_->accrualPeriod() * discount_;
        }
    }

    Rate CappedFlooredYoYCouponPricer::adjustedFixing(Rate fixing) const {
        if (fixing == Null<Rate>())
            fixing = coupon_->indexFixing();
        return fixing;
    }

    void CappedFlooredYoYCouponPricer::initialize(const InflationCoupon& coupon) {
        coupon_ = dynamic_cast<const QuantLib::YoYInflationCoupon*>(&coupon);
        QL_REQUIRE(coupon_, "year-on-year inflation coupon needed");
        gearing_ = coupon_->gearing();
        spread_ = coupon_->spread();
        paymentDate_ = coupon_->date();
        rateCurve_ = ext::dynamic_pointer_cast<QuantLib::YoYInflationIndex>(coupon.index())
            ->yoyInflationTermStructure()
            ->nominalTermStructure();
        discount_ = 1.0;
        if (paymentDate_ > rateCurve_->referenceDate())
            discount_ = rateCurve_->discount(paymentDate_);
        spreadLegValue_ = spread_ * coupon_->accrualPeriod()* discount_;
    }


    Real CappedFlooredYoYCouponPricer::swapletPrice() const {
        Real swapletPrice = adjustedFixing() * coupon_->accrualPeriod() * discount_;
        return gearing_ * swapletPrice + spreadLegValue_;
    }

    Rate CappedFlooredYoYCouponPricer::swapletRate() const {
        return gearing_ * adjustedFixing() + spread_;
    }

} // namespace QuantExt
