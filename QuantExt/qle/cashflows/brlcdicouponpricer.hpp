/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/cashflows/brlcdicouponpricer.hpp
    \brief Coupon pricer for a BRL CDI coupon
*/

#ifndef quantext_brl_cdi_coupon_pricer_hpp
#define quantext_brl_cdi_coupon_pricer_hpp

#include <qle/indexes/ibor/brlcdi.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>

namespace QuantExt {

//! BRL CDI coupon pricer

class BRLCdiCouponPricer : public QuantLib::FloatingRateCouponPricer {
public:
    //! \name FloatingRateCouponPricer interface
    //@{
    virtual QuantLib::Rate swapletRate() const;
    virtual void initialize(const QuantLib::FloatingRateCoupon& coupon);

    virtual QuantLib::Real swapletPrice() const;
    virtual QuantLib::Real capletPrice(QuantLib::Rate effectiveCap) const;
    virtual QuantLib::Rate capletRate(QuantLib::Rate effectiveCap) const;
    virtual QuantLib::Real floorletPrice(QuantLib::Rate effectiveFloor) const;
    virtual QuantLib::Rate floorletRate(QuantLib::Rate effectiveFloor) const;
    //@}

private:
    //! The coupon to be priced
    const QuantLib::OvernightIndexedCoupon* coupon_;

    //! The index underlying the coupon to be priced
    boost::shared_ptr<BRLCdi> index_;
};

}

#endif
