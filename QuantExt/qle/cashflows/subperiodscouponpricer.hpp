/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file subperiodscouponpricer.hpp
    \brief Pricer for sub-period coupons
*/

#ifndef quantext_sub_periods_coupon_pricer_hpp
#define quantext_sub_periods_coupon_pricer_hpp

#include <ql/cashflows/couponpricer.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>

using namespace QuantLib;

namespace QuantExt {
    //! Pricer for sub-period coupons
    class SubPeriodsCouponPricer : public FloatingRateCouponPricer {
      public:
        SubPeriodsCouponPricer() {}
        
        void initialize(const FloatingRateCoupon& coupon);
        Rate swapletRate() const;
        
        Real swapletPrice() const {
            QL_FAIL("swapletPrice not available"); }
        Real capletPrice(Rate) const {
            QL_FAIL("capletPrice not available"); }
        Rate capletRate(Rate) const {
            QL_FAIL("capletRate not available"); }
        Real floorletPrice(Rate) const {
            QL_FAIL("floorletPrice not available"); }
        Rate floorletRate(Rate) const {
            QL_FAIL("floorletRate not available"); }
      
      protected:
        Real gearing_;
        Spread spread_;
        Time accrualPeriod_;
        boost::shared_ptr<InterestRateIndex> index_;
        SubPeriodsCoupon::Type type_;
        bool includeSpread_;

        const SubPeriodsCoupon* coupon_;
    };
}

#endif
