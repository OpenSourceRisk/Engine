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

/*! \file strippedcapflooredcpicoupon.hpp
\brief strips the embedded option from cap floored cpi coupons
*/

#ifndef quantext_stripped_capfloored_cpicoupon_hpp
#define quantext_stripped_capfloored_cpicoupon_hpp

#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/cpicoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

class StrippedCappedFlooredCPICoupon : public CPICoupon {

public:
    explicit StrippedCappedFlooredCPICoupon(const ext::shared_ptr<CappedFlooredCPICoupon>& underlying);
    //! Coupon interface
    Rate rate() const override;
    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;

    //! Observer interface
    void update() override;
    //! Visitability
    virtual void accept(AcyclicVisitor&) override;

    //! Underlying coupon
    const ext::shared_ptr<CappedFlooredCPICoupon> underlying() { return underlying_; }

    bool isCap() const;
    bool isFloor() const;
    bool isCollar() const;

protected:
    ext::shared_ptr<CappedFlooredCPICoupon> underlying_;
};

//! Stripped capped or floored CPI cashflow.
/*! Extended QuantLib::CPICashFlow
 */
class StrippedCappedFlooredCPICashFlow : public CPICashFlow {
public:
    StrippedCappedFlooredCPICashFlow(const ext::shared_ptr<CappedFlooredCPICashFlow>& underlying);
    //! Cashflow interface
    Real amount() const override;
    //! Underlying cash flow
    ext::shared_ptr<CappedFlooredCPICashFlow> underlying() const { return underlying_; }

private:
    ext::shared_ptr<CappedFlooredCPICashFlow> underlying_;
};

class StrippedCappedFlooredCPICouponLeg {
public:
    explicit StrippedCappedFlooredCPICouponLeg(const Leg& underlyingLeg);
    operator Leg() const;

private:
    Leg underlyingLeg_;
};

} // namespace QuantExt

#endif
