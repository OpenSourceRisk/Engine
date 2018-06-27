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

/*! \file qle/cashflows/equitycoupon.hpp
    \brief coupon paying the return on an equity
    \ingroup cashflows
*/

#ifndef quantext_equity_coupon_hpp
#define quantext_equity_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {

//! equity coupon pricer
/*! \ingroup cashflows
*/
class EquityCouponPricer;

//! equity coupon
/*! 
    \ingroup cashflows
*/
class EquityCoupon : public Coupon,
                     public Observer {
public:
    EquityCoupon(const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        const boost::shared_ptr<YieldTermStructure>& equityRefRateCurve,
        const boost::shared_ptr<YieldTermStructure>& divYieldCurve,
        const DayCounter& dayCounter,
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        const Date& exCouponDate = Date()
    );

    //! \name CashFlow interface
    //@{
    Real amount() const { return rate() * accrualPeriod() * nominal(); }
    //@}

    //! \name Coupon interface
    //@{
    //Real price(const Handle<YieldTermStructure>& discountingCurve) const;
    DayCounter dayCounter() const { return dayCounter_; }
    Real accruedAmount(const Date&) const;
    Rate rate() const;
    //@}

    //! \name Inspectors
    //@{
    //! equity reference rate curve
    const boost::shared_ptr<YieldTermStructure>& equityRefRateCurve() const { return equityRefRateCurve_; }
    //! equity dividend curve
    const boost::shared_ptr<YieldTermStructure>& divYieldCurve() const { return divYieldCurve_; }
    //@}

    //! \name Observer interface
    //@{
    void update() { notifyObservers(); }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
    void setPricer(const boost::shared_ptr<EquityCouponPricer>&);
    boost::shared_ptr<EquityCouponPricer> pricer() const;

    // use to calculate for fixing date, allows change of
    Rate equityForwardRate() const;

protected:
    boost::shared_ptr<EquityCouponPricer> pricer_;
    boost::shared_ptr<YieldTermStructure> equityRefRateCurve_;
    boost::shared_ptr<YieldTermStructure> divYieldCurve_;
    DayCounter dayCounter_;
    Natural fixingDays_;
    bool isTotalReturn_;

    //! makes sure you were given the correct type of pricer
    // this can also done in external pricer setter classes via
    // accept/visit mechanism
   // virtual bool checkPricerImpl(const
    //    boost::shared_ptr<EquityCouponPricer>&) const = 0;
};

// inline definitions


inline void EquityCoupon::accept(AcyclicVisitor& v) {
    Visitor<EquityCoupon>* v1 =
        dynamic_cast<Visitor<EquityCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

inline boost::shared_ptr<EquityCouponPricer> EquityCoupon::pricer() const {
    return pricer_;
}

//! helper class building a sequence of equity coupons
/*! \ingroup cashflows
*/
class EquityLeg {
public:
    EquityLeg(const Schedule& schedule,
        const boost::shared_ptr<YieldTermStructure>& equityRefRateCurve,
        const boost::shared_ptr<YieldTermStructure>& divYieldCurve);
    EquityLeg& withNotional(Real notional);
    EquityLeg& withNotionals(const std::vector<Real>& notionals);
    EquityLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    EquityLeg& withPaymentAdjustment(BusinessDayConvention convention);
    EquityLeg& withPaymentCalendar(const Calendar& calendar);
    EquityLeg& withEquityCouponPricer(const boost::shared_ptr<EquityCouponPricer>& couponPricer);
    operator Leg() const;

private:
    Schedule schedule_;
    boost::shared_ptr<YieldTermStructure> equityRefRateCurve_;
    boost::shared_ptr<YieldTermStructure> divYieldCurve_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    boost::shared_ptr<EquityCouponPricer> couponPricer_;
};

} // namespace QuantExt

#endif
