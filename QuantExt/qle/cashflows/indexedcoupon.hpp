/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/indexedcoupon.hpp
    \brief coupon with an indexed notional
    \ingroup cashflows
*/

#ifndef quantext_indexed_coupon_hpp
#define quantext_indexed_coupon_hpp

#include <ql/cashflows/coupon.hpp>
#include <ql/index.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! indexed coupon
/*!
    \ingroup cashflows
*/
class IndexedCoupon : public Coupon, public Observer {
public:
    /*! pays c->amount() / c->nominal() * qty * index(fixingDate), i.e. the original nominal of the coupon
      is replaced by qty times the index fixing */
    IndexedCoupon(const boost::shared_ptr<Coupon>& c, const Real qty, const boost::shared_ptr<Index>& index,
                  const Date& fixingDate);
    /*! pays c->amount() / c->nominal() * qty * initialFixing */
    IndexedCoupon(const boost::shared_ptr<Coupon>& c, const Real qty, const Real initialFixing);

    //! \name Coupon interface
    //@{
    Real nominal() const override;
    Real rate() const override;
    DayCounter dayCounter() const override;
    //@}

private:
    const boost::shared_ptr<Coupon> c_;
    const Real qty_;
    const boost::shared_ptr<Index> index_;
    const Date fixingDate_;
    const Real initialFixing_;
};

//! indexed coupon leg
/*!
    \ingroup cashflows
*/
class IndexedCouponLeg {
public:
    IndexedCouponLeg(const Leg& underlyingLeg, const Real qty, const boost::shared_ptr<Index>& index);
    IndexedCouponLeg& withInitialFixing(const Real initialFixing);
    IndexedCouponLeg& withValuationSchedule(const Schedule& valuationSchedule);
    IndexedCouponLeg& withFixingDays(const Size fixingDays);
    IndexedCouponLeg& withFixingCalendar(const Calendar& fixingCalendar);
    IndexedCouponLeg& withFixingConvention(const BusinessDayConvention& fixingConvention);
    IndexedCouponLeg& inArrearsFixing(const bool inArrearsFixing = true);

    operator Leg() const;

private:
    const Leg underlyingLeg_;
    const Real qty_;
    const boost::shared_ptr<Index> index_;
    Real initialFixing_;
    Schedule valuationSchedule_;
    Size fixingDays_;
    Calendar fixingCalendar_;
    BusinessDayConvention fixingConvention_;
    bool inArrearsFixing_;
};

} // namespace QuantExt

#endif
