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

/*! \file strippedcapflooredyoyinflationcoupon.hpp
\brief strips the embedded option from cap floored yoy inflation coupons
*/

#ifndef quantext_stripped_capfloored_yoyinflation_coupon_hpp
#define quantext_stripped_capfloored_yoyinflation_coupon_hpp

#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>

namespace QuantExt {
using namespace QuantLib;

class StrippedCappedFlooredYoYInflationCoupon : public YoYInflationCoupon {

public:
    explicit StrippedCappedFlooredYoYInflationCoupon(
        const ext::shared_ptr<CappedFlooredYoYInflationCoupon>& underlying);

    //! Coupon interface
    Rate rate() const override;
    //! cap
    Rate cap() const;
    //! floor
    Rate floor() const;
    //! effective cap
    Rate effectiveCap() const;
    //! effective floor
    Rate effectiveFloor() const;

    //! Observer interface
    void update() override;

    //! Visitability
    virtual void accept(AcyclicVisitor&) override;

    bool isCap() const;
    bool isFloor() const;
    bool isCollar() const;

    void setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer);

    const ext::shared_ptr<CappedFlooredYoYInflationCoupon> underlying() { return underlying_; }

protected:
    ext::shared_ptr<CappedFlooredYoYInflationCoupon> underlying_;
};

class StrippedCappedFlooredYoYInflationCouponLeg {
public:
    explicit StrippedCappedFlooredYoYInflationCouponLeg(const Leg& underlyingLeg);
    operator Leg() const;

private:
    Leg underlyingLeg_;
};

} // namespace QuantExt

#endif
