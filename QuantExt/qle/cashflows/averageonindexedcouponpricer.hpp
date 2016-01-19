/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file averageonindexedcouponpricer.hpp
    \brief Pricer for average overnight indexed coupons
*/

#ifndef quantext_average_on_indexed_coupon_pricer_hpp
#define quantext_average_on_indexed_coupon_pricer_hpp

#include <qle/cashflows/averageonindexedcoupon.hpp>

#include <ql/cashflows/couponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Pricer for average overnight indexed coupons
    class AverageONIndexedCouponPricer : public FloatingRateCouponPricer {
      public:
        enum Approximation {Takada, None};

        AverageONIndexedCouponPricer(Approximation approxType = Takada)
        : approximationType_(approxType) {}

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
        Approximation approximationType_;
        Real gearing_;
        Spread spread_;
        Time accrualPeriod_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;

        const AverageONIndexedCoupon* coupon_;
    };

} // namespace QuantExt

#endif
