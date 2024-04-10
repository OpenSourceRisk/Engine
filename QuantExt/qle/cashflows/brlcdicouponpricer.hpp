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

/*! \file qle/cashflows/brlcdicouponpricer.hpp
    \brief Coupon pricer for a BRL CDI coupon
*/

#ifndef quantext_brl_cdi_coupon_pricer_hpp
#define quantext_brl_cdi_coupon_pricer_hpp

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>

namespace QuantExt {

//! BRL CDI coupon pricer

class BRLCdiCouponPricer : public QuantLib::FloatingRateCouponPricer {
public:
    //! \name FloatingRateCouponPricer interface
    //@{
    virtual QuantLib::Rate swapletRate() const override;
    virtual void initialize(const QuantLib::FloatingRateCoupon& coupon) override;

    virtual QuantLib::Real swapletPrice() const override;
    virtual QuantLib::Real capletPrice(QuantLib::Rate effectiveCap) const override;
    virtual QuantLib::Rate capletRate(QuantLib::Rate effectiveCap) const override;
    virtual QuantLib::Real floorletPrice(QuantLib::Rate effectiveFloor) const override;
    virtual QuantLib::Rate floorletRate(QuantLib::Rate effectiveFloor) const override;
    //@}

private:
    /*! The coupon to be priced, for now we support both QuantLib::OvernightIndexedCoupon
        and QuantExt::OvernightIndexedCoupon, see ORE ticket 1159 */
    const QuantLib::OvernightIndexedCoupon* couponQl_;
    const QuantExt::OvernightIndexedCoupon* couponQle_;

    //! The index underlying the coupon to be priced
    QuantLib::ext::shared_ptr<BRLCdi> index_;
};

} // namespace QuantExt

#endif
