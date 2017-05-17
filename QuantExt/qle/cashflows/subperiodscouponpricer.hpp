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

/*! \file subperiodscouponpricer.hpp
    \brief Pricer for sub-period coupons

        \ingroup cashflows
*/

#ifndef quantext_sub_periods_coupon_pricer_hpp
#define quantext_sub_periods_coupon_pricer_hpp

#include <ql/cashflows/couponpricer.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>

using namespace QuantLib;

namespace QuantExt {
//! Pricer for sub-period coupons
/*! \ingroup cashflows
 */
class SubPeriodsCouponPricer : public FloatingRateCouponPricer {
public:
    SubPeriodsCouponPricer() {}

    void initialize(const FloatingRateCoupon& coupon);
    Rate swapletRate() const;

    Real swapletPrice() const { QL_FAIL("swapletPrice not available"); }
    Real capletPrice(Rate) const { QL_FAIL("capletPrice not available"); }
    Rate capletRate(Rate) const { QL_FAIL("capletRate not available"); }
    Real floorletPrice(Rate) const { QL_FAIL("floorletPrice not available"); }
    Rate floorletRate(Rate) const { QL_FAIL("floorletRate not available"); }

protected:
    Real gearing_;
    Spread spread_;
    Time accrualPeriod_;
    boost::shared_ptr<InterestRateIndex> index_;
    SubPeriodsCoupon::Type type_;
    bool includeSpread_;

    const SubPeriodsCoupon* coupon_;
};
} // namespace QuantExt

#endif
