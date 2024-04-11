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
    /*! pays c->amount() * qty * index(fixingDate) */
    IndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c, const Real qty, const QuantLib::ext::shared_ptr<Index>& index,
                  const Date& fixingDate);
    /*! pays c->amount() * qty * initialFixing */
    IndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c, const Real qty, const Real initialFixing);

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name Coupon interface
    //@{
    Real amount() const override;
    Real nominal() const override;
    Real rate() const override;
    DayCounter dayCounter() const override;
    Real accruedAmount(const Date& d) const override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<Coupon> underlying() const;
    Real quantity() const;
    QuantLib::ext::shared_ptr<Index> index() const; // might be null
    const Date& fixingDate() const;         // might be null
    Real initialFixing() const;             // might be null
    Real multiplier() const;
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

private:
    QuantLib::ext::shared_ptr<Coupon> c_;
    Real qty_;
    QuantLib::ext::shared_ptr<Index> index_;
    Date fixingDate_;
    Real initialFixing_;
};

//! indexed cashflow
/*!
    \ingroup cashflows
*/
class IndexWrappedCashFlow : public CashFlow, public Observer {
public:
    /*! pays c->amount() * qty * index(fixingDate) */
    IndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c, const Real qty, const QuantLib::ext::shared_ptr<Index>& index,
                  const Date& fixingDate);
    /*! pays c->amount() * qty * initialFixing */
    IndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c, const Real qty, const Real initialFixing);

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name Cashflow interface
    //@{
    Date date() const override;
    Real amount() const override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<CashFlow> underlying() const;
    Real quantity() const;
    QuantLib::ext::shared_ptr<Index> index() const; // might be null
    const Date& fixingDate() const;         // might be null
    Real initialFixing() const;             // might be null
    Real multiplier() const;
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

private:
    QuantLib::ext::shared_ptr<CashFlow> c_;
    Real qty_;
    QuantLib::ext::shared_ptr<Index> index_;
    Date fixingDate_;
    Real initialFixing_;

};

// if c casts to Coupon, unpack an indexed coupon, otherwise an index-wrapped cashflow
QuantLib::ext::shared_ptr<CashFlow> unpackIndexedCouponOrCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c);

// remove all index wrappers around a coupon
QuantLib::ext::shared_ptr<Coupon> unpackIndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c);

// remove all index wrappers around a cashflow
QuantLib::ext::shared_ptr<CashFlow> unpackIndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c);

// get cumulated multiplier for indexed coupon or cashflow
Real getIndexedCouponOrCashFlowMultiplier(const QuantLib::ext::shared_ptr<CashFlow>& c);

// get all fixingDates / indices / multipliers for indexed coupon or index-wrapped cashflow
std::vector<std::tuple<Date, QuantLib::ext::shared_ptr<Index>, Real>>
getIndexedCouponOrCashFlowFixingDetails(const QuantLib::ext::shared_ptr<CashFlow>& c);

//! indexed coupon leg
/*!
    \ingroup cashflows
*/
class IndexedCouponLeg {
public:
    IndexedCouponLeg(const Leg& underlyingLeg, const Real qty, const QuantLib::ext::shared_ptr<Index>& index);
    IndexedCouponLeg& withInitialFixing(const Real initialFixing);
    IndexedCouponLeg& withInitialNotionalFixing(const Real initialNotionalFixing);
    IndexedCouponLeg& withValuationSchedule(const Schedule& valuationSchedule);
    IndexedCouponLeg& withFixingDays(const Size fixingDays);
    IndexedCouponLeg& withFixingCalendar(const Calendar& fixingCalendar);
    IndexedCouponLeg& withFixingConvention(const BusinessDayConvention& fixingConvention);
    IndexedCouponLeg& inArrearsFixing(const bool inArrearsFixing = true);

    operator Leg() const;

private:
    const Leg underlyingLeg_;
    const Real qty_;
    const QuantLib::ext::shared_ptr<Index> index_;
    Real initialFixing_;
    Real initialNotionalFixing_;
    Schedule valuationSchedule_;
    Size fixingDays_;
    Calendar fixingCalendar_;
    BusinessDayConvention fixingConvention_;
    bool inArrearsFixing_;
};

} // namespace QuantExt

#endif
