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

/*! \file interpolatediborcouponpricer.hpp
    \brief black pricer for interpolated ibor coupon
    \ingroup cashflows
*/

#pragma once

#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/indexes/interpolatediborindex.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

using QuantLib::Handle;
using QuantLib::Option;
using QuantLib::Quote;
using QuantLib::Null;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Spread;
using QuantLib::Time;

namespace QuantExt {

// this is largely a copy of QuantLb::IborCouponPricer
class InterpolatedIborCouponPricer : public QuantLib::FloatingRateCouponPricer {
    public:
    explicit InterpolatedIborCouponPricer(
        Handle<QuantLib::OptionletVolatilityStructure> v = Handle<QuantLib::OptionletVolatilityStructure>(),
        QuantLib::ext::optional<bool> useIndexedCoupon = QuantLib::ext::nullopt);

    bool useIndexedCoupon() const { return useIndexedCoupon_; }

    Handle<QuantLib::OptionletVolatilityStructure> capletVolatility() const {
        return capletVol_;
    }
    void setCapletVolatility(
                        const Handle<QuantLib::OptionletVolatilityStructure>& v =
                                Handle<QuantLib::OptionletVolatilityStructure>()) {
        unregisterWith(capletVol_);
        capletVol_ = v;
        registerWith(capletVol_);
        update();
    }
    void initialize(const QuantLib::FloatingRateCoupon& coupon) override;

    void initializeCachedData(const InterpolatedIborCoupon& coupon) const;

    protected:

    const InterpolatedIborCoupon* coupon_;

    QuantLib::ext::shared_ptr<InterpolatedIborIndex> index_;
    Date fixingDate_;
    Real gearing_;
    Spread spread_;
    Time accrualPeriod_;

    Date fixingValueDate_, fixingEndDate_, fixingMaturityDate_;
    Time spanningTime_, spanningTimeIndexMaturity_;

    Handle<QuantLib::OptionletVolatilityStructure> capletVol_;
    bool useIndexedCoupon_;
};

// this is largely a copy of QuantLb::BlackIborCouponPricer
class BlackInterpolatedIborCouponPricer : public InterpolatedIborCouponPricer {
public:
    enum TimingAdjustment { Black76, BivariateLognormal };
    BlackInterpolatedIborCouponPricer(
        const Handle<QuantLib::OptionletVolatilityStructure>& v = Handle<QuantLib::OptionletVolatilityStructure>(),
        const TimingAdjustment timingAdjustment = Black76,
        const Handle<Quote> correlation = Handle<Quote>(QuantLib::ext::shared_ptr<Quote>(new QuantLib::SimpleQuote(1.0))),
        const QuantLib::ext::optional<bool> useIndexedCoupon = QuantLib::ext::nullopt)
        : InterpolatedIborCouponPricer(v, useIndexedCoupon), timingAdjustment_(timingAdjustment), correlation_(correlation) {
        QL_REQUIRE(timingAdjustment_ == Black76 || timingAdjustment_ == BivariateLognormal,
                   "unknown timing adjustment (code " << timingAdjustment_ << ")");
        registerWith(correlation_);
    };
    void initialize(const QuantLib::FloatingRateCoupon& coupon) override;
    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

protected:
    Real optionletPrice(QuantLib::Option::Type optionType, Real effStrike) const;
    Real optionletRate(QuantLib::Option::Type optionType, Real effStrike) const;

    virtual Rate adjustedFixing(Rate fixing = QuantLib::Null<Rate>()) const;

    Real discount_;

private:
    Real computeFixingAdjustment(const QuantLib::ext::shared_ptr<QuantLib::IborIndex>&,
                                 const QuantLib::Handle<QuantLib::YieldTermStructure>&) const;
    const TimingAdjustment timingAdjustment_;
    const Handle<Quote> correlation_;
};

// inline

inline Real BlackInterpolatedIborCouponPricer::swapletPrice() const {
    // past or future fixing is managed in InterestRateIndex::fixing()
    QL_REQUIRE(discount_ != Null<Rate>(), "no forecast curve provided");
    return swapletRate() * accrualPeriod_ * discount_;
}

inline Rate BlackInterpolatedIborCouponPricer::swapletRate() const {
    return gearing_ * adjustedFixing() + spread_;
}

inline Real BlackInterpolatedIborCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_REQUIRE(discount_ != Null<Rate>(), "no forecast curve provided");
    return capletRate(effectiveCap) * accrualPeriod_ * discount_;
}

inline Rate BlackInterpolatedIborCouponPricer::capletRate(Rate effectiveCap) const {
    return gearing_ * optionletRate(Option::Call, effectiveCap);
}

inline Real BlackInterpolatedIborCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_REQUIRE(discount_ != Null<Rate>(), "no forecast curve provided");
    return floorletRate(effectiveFloor) * accrualPeriod_ * discount_;
}

inline Rate BlackInterpolatedIborCouponPricer::floorletRate(Rate effectiveFloor) const {
    return gearing_ * optionletRate(Option::Put, effectiveFloor);
}

} // namespace QuantExt
