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

#include <qle/cashflows/interpolatediborcouponpricer.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/pricingengines/blackformula.hpp>

using namespace QuantLib;

namespace QuantExt {

//===========================================================================//
//                              InterpolatedIborCouponPricer                 //
//===========================================================================//

    InterpolatedIborCouponPricer::InterpolatedIborCouponPricer(
            Handle<OptionletVolatilityStructure> v,
            ext::optional<bool> useIndexedCoupon)
        : capletVol_(std::move(v)),
            useIndexedCoupon_(useIndexedCoupon ?
                              *useIndexedCoupon :
                              // !IborCoupon::Settings::instance().usingAtParCoupons()) {
                              false) {
        registerWith(capletVol_);
    }

    void InterpolatedIborCouponPricer::initializeCachedData(const InterpolatedIborCoupon& coupon) const {

        if(coupon.cachedDataIsInitialized_)
            return;

        coupon.fixingValueDate_ = coupon.iborIndex()->fixingCalendar().advance(
            coupon.fixingDate_, coupon.iborIndex()->fixingDays(), Days);
        coupon.fixingMaturityDate_ = coupon.iborIndex()->maturityDate(coupon.fixingValueDate_);

        if (useIndexedCoupon_) {
            coupon.fixingEndDate_ = coupon.fixingMaturityDate_;
        } else {
            if (coupon.isInArrears_ || coupon.fixingDays_ == Null<Size>())
                coupon.fixingEndDate_ = coupon.fixingMaturityDate_;
            else { // par coupon approximation
                Date nextFixingDate = coupon.iborIndex()->fixingCalendar().advance(
                    coupon.accrualEndDate(), -static_cast<Integer>(coupon.fixingDays_), Days);
                coupon.fixingEndDate_ = coupon.iborIndex()->fixingCalendar().advance(
                    nextFixingDate, coupon.iborIndex()->fixingDays(), Days);
                // make sure the estimation period contains at least one day
                coupon.fixingEndDate_ =
                    std::max(coupon.fixingEndDate_, coupon.fixingValueDate_ + 1);
            }
        }

        coupon.spanningTime_ = coupon.iborIndex()->dayCounter().yearFraction(
            coupon.fixingValueDate_, coupon.fixingEndDate_);

        QL_REQUIRE(coupon.spanningTime_ > 0.0,
                    "\n cannot calculate forward rate between "
                        << coupon.fixingValueDate_ << " and " << coupon.fixingEndDate_
                        << ":\n non positive time (" << coupon.spanningTime_ << ") using "
                        << coupon.iborIndex()->dayCounter().name() << " daycounter");

        coupon.spanningTimeIndexMaturity_ = coupon.iborIndex()->dayCounter().yearFraction(
            coupon.fixingValueDate_, coupon.fixingMaturityDate_);

        coupon.cachedDataIsInitialized_ = true;
    }

    void InterpolatedIborCouponPricer::initialize(const FloatingRateCoupon& coupon) {
        coupon_ = dynamic_cast<const InterpolatedIborCoupon *>(&coupon);
        QL_REQUIRE(coupon_, "InterpolatedIborCouponPricer: expected InterpolatedIborCoupon");

        initializeCachedData(*coupon_);

        index_ = coupon_->interpolatedIborIndex();
        gearing_ = coupon_->gearing();
        spread_ = coupon_->spread();
        accrualPeriod_ = coupon_->accrualPeriod();
        QL_REQUIRE(accrualPeriod_ != 0.0, "null accrual period");

        fixingDate_ = coupon_->fixingDate_;
        fixingValueDate_ = coupon_->fixingValueDate_;
        fixingMaturityDate_ = coupon_->fixingMaturityDate_;
        spanningTime_ = coupon_->spanningTime_;
        spanningTimeIndexMaturity_ = coupon_->spanningTimeIndexMaturity_;
    }

void BlackInterpolatedIborCouponPricer::initialize(const FloatingRateCoupon& coupon) {

    InterpolatedIborCouponPricer::initialize(coupon);

    // it's not a discount curve anyhow, this is not really used anywhere
    const Handle<YieldTermStructure>& rateCurve = index_->shortIndex()->forwardingTermStructure();

    if (rateCurve.empty()) {
        discount_ = Null<Real>(); // might not be needed, will be checked later
    } else {
        Date paymentDate = coupon_->date();
        if (paymentDate > rateCurve->referenceDate())
            discount_ = rateCurve->discount(paymentDate);
        else
            discount_ = 1.0;
    }

    auto index = QuantLib::ext::dynamic_pointer_cast<InterpolatedIborIndex>(index_);
    if (!index) {
        QL_FAIL("Interpolated ibor index required.");
    }

}

Real BlackInterpolatedIborCouponPricer::optionletRate(Option::Type optionType, Real effStrike) const {
    if (fixingDate_ <= Settings::instance().evaluationDate()) {
        // the amount is determined
        Real a, b;
        if (optionType==Option::Call) {
            a = coupon_->indexFixing();
            b = effStrike;
        } else {
            a = effStrike;
            b = coupon_->indexFixing();
        }
        return std::max(a - b, 0.0);
    } else {
        // not yet determined, use Black model
        QL_REQUIRE(!capletVolatility().empty(),
                    "missing optionlet volatility");
        Real stdDev =
            std::sqrt(capletVolatility()->blackVariance(fixingDate_,
                                                        effStrike));
        Real shift = capletVolatility()->displacement();
        bool shiftedLn =
            capletVolatility()->volatilityType() == ShiftedLognormal;
        Rate fixing =
            shiftedLn
                ? blackFormula(optionType, effStrike, adjustedFixing(),
                                stdDev, 1.0, shift)
                : bachelierBlackFormula(optionType, effStrike,
                                        adjustedFixing(), stdDev, 1.0);
        return fixing;
    }
}

Real BlackInterpolatedIborCouponPricer::optionletPrice(Option::Type optionType,
                                            Real effStrike) const {
    QL_REQUIRE(discount_ != Null<Rate>(), "no forecast curve provided");
    return optionletRate(optionType, effStrike) * accrualPeriod_ * discount_;
}

// this is almost identical to QuantLib::BlackIborCouponPricer::adjustedFixing()
Rate BlackInterpolatedIborCouponPricer::computeFixingAdjustment(
    const QuantLib::ext::shared_ptr<IborIndex>& index0, const Handle<YieldTermStructure>& overwriteEstimationCurve) const {

    QuantLib::ext::shared_ptr<IborIndex> index =
        overwriteEstimationCurve.empty() ? index0 : index0->clone(overwriteEstimationCurve);

    QL_REQUIRE(index, "BlackInterpolatedIborCouponPricer::computeAdjustedFixing(): index is null");

    Real fixing = index->fixing(coupon_->fixingDate()); // different from ql!

    // if the pay date is equal to the index estimation end date
    // there is no convexity; in all other cases in principle an
    // adjustment has to be applied, but the Black76 method only
    // applies the standard in arrears adjustment; the bivariate
    // lognormal method is more accurate in this regard.
    if ((!coupon_->isInArrears() && timingAdjustment_ == Black76))
        return 0.0;
    Date d1 = fixingDate_;
    Date d2 = index->valueDate(d1);
    Date d3 = index->maturityDate(d2);
    if (coupon_->date() == d3)
        return 0.0;

    QL_REQUIRE(!capletVolatility().empty(), "missing optionlet volatility");
    Date referenceDate = capletVolatility()->referenceDate();
    // no variance has accumulated, so the convexity is zero
    if (d1 <= referenceDate)
        return 0.0;
    const Time& tau = index->dayCounter().yearFraction(d2, d3);
    Real variance = capletVolatility()->blackVariance(d1, fixing);

    Real shift = capletVolatility()->displacement();
    bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;

    Spread adjustment = shiftedLn
                            ? Real((fixing + shift) * (fixing + shift) *
                                  variance * tau / (1.0 + fixing * tau))
                            : Real(variance * tau / (1.0 + fixing * tau));

    if (timingAdjustment_ == BivariateLognormal) {
        QL_REQUIRE(!correlation_.empty(), "no correlation given");
        const Date& d4 = coupon_->date();
        const Date& d5 = d4 >= d3 ? d3 : d2;
        Time tau2 = index->dayCounter().yearFraction(d5, d4);
        if (d4 >= d3)
            adjustment = 0.0;
        // if d4 < d2 (payment before index start) we just apply the
        // Black76 in arrears adjustment
        if (tau2 > 0.0) {
            Real fixing2 =
                (index_->shortIndex()->forwardingTermStructure()->discount(d5) /
                        index_->shortIndex()->forwardingTermStructure()->discount(d4) -
                    1.0) /
                tau2;
            adjustment -= shiftedLn
                              ? Real(correlation_->value() * tau2 * variance *
                                    (fixing + shift) * (fixing2 + shift) /
                                    (1.0 + fixing2 * tau2))
                              : Real(correlation_->value() * tau2 * variance /
                                    (1.0 + fixing2 * tau2));
        }
    }
    return adjustment;
}

Rate BlackInterpolatedIborCouponPricer::adjustedFixing(Rate fixing) const {

    QL_REQUIRE(fixing == Null<Rate>(),
               "BlackInterpolatedIborCouponPricer::adjustedFixing(): prescribed fixing not supported");

    fixing = index_->fixing(coupon_->fixingDate());

    // we overlay the convexity adjustments for the two indices;
    // notice that in the standard case, where the interpolation is done in a way
    // such that the accrual period length is matched, in effect the weighted timing
    // adjustments will add up to a "standard" adjustment for the interpolated index,
    // but we do not use in the calculation here
    auto index = QuantLib::ext::dynamic_pointer_cast<InterpolatedIborIndex>(index_);
    Real adjustment = index->shortWeight(coupon_->fixingDate()) *
                          computeFixingAdjustment(index->shortIndex(), index->overwriteEstimationCurve()) +
                      index->longWeight(coupon_->fixingDate()) *
                          computeFixingAdjustment(index->longIndex(), index->overwriteEstimationCurve());

    return fixing + adjustment;
}

} // namespace QuantExt
