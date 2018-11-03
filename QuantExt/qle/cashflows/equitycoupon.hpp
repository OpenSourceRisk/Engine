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

#include <ql/cashflows/coupon.hpp>
#include <ql/handle.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/equityindex.hpp>

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
class EquityCoupon : public Coupon, public Observer {
public:
    EquityCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
                 const boost::shared_ptr<EquityIndex>& equityCurve, const DayCounter& dayCounter,
                 bool isTotalReturn = false, Real dividendFactor = 1.0, const Date& refPeriodStart = Date(),
                 const Date& refPeriodEnd = Date(), const Date& exCouponDate = Date());

    //! \name CashFlow interface
    //@{
    Real amount() const { return rate() * nominal(); }
    //@}

    //! \name Coupon interface
    //@{
    // Real price(const Handle<YieldTermStructure>& discountingCurve) const;
    DayCounter dayCounter() const { return dayCounter_; }
    Real accruedAmount(const Date&) const;
    // calculates the rate for the period, not yearly i.e. (S(t+1)-S(t))/S(t)
    Rate rate() const;
    //@}

    //! \name Inspectors
    //@{
    //! equity reference rate curve
    const boost::shared_ptr<EquityIndex>& equityCurve() const { return equityCurve_; }
    //! total return or price return?
    bool isTotalReturn() const { return isTotalReturn_; }
    //! are dividends scaled (e.g. to account for tax)
    Real dividendFactor() const { return dividendFactor_; }
    //! The date at which the starting equity price is fixed
    Date fixingStartDate() const { return fixingStartDate_; }
    //! The date at which performance is measured
    Date fixingEndDate() const { return fixingEndDate_; }
	//! return both fixing dates
    std::vector<Date> fixingDates() const;
    //! This function is called for other coupon types
    Date fixingDate() const {
        QL_FAIL("Equity Coupons have 2 fixings, not 1.");
        return Date();
    }
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

protected:
    boost::shared_ptr<EquityCouponPricer> pricer_;
    boost::shared_ptr<EquityIndex> equityCurve_;
    DayCounter dayCounter_;
    Natural fixingDays_;
    Date fixingStartDate_;
    Date fixingEndDate_;
    bool isTotalReturn_;
    Real dividendFactor_;
};

// inline definitions

inline void EquityCoupon::accept(AcyclicVisitor& v) {
    Visitor<EquityCoupon>* v1 = dynamic_cast<Visitor<EquityCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

inline boost::shared_ptr<EquityCouponPricer> EquityCoupon::pricer() const { return pricer_; }

//! helper class building a sequence of equity coupons
/*! \ingroup cashflows
 */
class EquityLeg {
public:
    EquityLeg(const Schedule& schedule, const boost::shared_ptr<EquityIndex>& equityCurve);
    EquityLeg& withNotional(Real notional);
    EquityLeg& withNotionals(const std::vector<Real>& notionals);
    EquityLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    EquityLeg& withPaymentAdjustment(BusinessDayConvention convention);
    EquityLeg& withPaymentCalendar(const Calendar& calendar);
    EquityLeg& withTotalReturn(bool);
    EquityLeg& withDividendFactor(Real);
    EquityLeg& withFixingDays(Natural);
    operator Leg() const;

private:
    Schedule schedule_;
    boost::shared_ptr<EquityIndex> equityCurve_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    bool isTotalReturn_;
    Real dividendFactor_ = 1.0;
    Natural fixingDays_ = 0;
};

} // namespace QuantExt

#endif
