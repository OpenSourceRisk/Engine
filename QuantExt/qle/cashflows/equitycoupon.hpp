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
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! equity coupon pricer
/*! \ingroup cashflows
 */
class EquityCouponPricer;

enum class EquityReturnType { Price, Total, Absolute, Dividend };

EquityReturnType parseEquityReturnType(const std::string& str);
std::ostream& operator<<(std::ostream& out, EquityReturnType t);

//! equity coupon
/*!
    \ingroup cashflows
*/
class EquityCoupon : public Coupon, public Observer {   
public:
    EquityCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
                 const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve, const DayCounter& dayCounter,
                 EquityReturnType returnType, Real dividendFactor = 1.0, bool notionalReset = false, 
                 Real initialPrice = Null<Real>(), Real quantity = Null<Real>(), const Date& fixingStartDate = Date(),
                 const Date& fixingEndDate = Date(), const Date& refPeriodStart = Date(),
                 const Date& refPeriodEnd = Date(), const Date& exCouponDate = Date(),
                 const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr, const bool initialPriceIsInTargetCcy = false,
		 Real legInitialNotional = Null<Real>(), const Date& legFixingDate = Date());

    //! \name CashFlow interface
    //@{
    Real amount() const override { return rate() * nominal(); }
    //@}

    //! \name Coupon interface
    //@{
    // Real price(const Handle<YieldTermStructure>& discountingCurve) const;
    DayCounter dayCounter() const override { return dayCounter_; }
    Real accruedAmount(const Date&) const override;
    // calculates the rate for the period, not yearly i.e. (S(t+1)-S(t))/S(t)
    Rate rate() const override;
    // nominal changes if notional is resettable
    Real nominal() const override;
    //@}

    //! \name Inspectors
    //@{
    //! equity reference rate curve
    const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve() const { return equityCurve_; }
    //! fx index curve
    const QuantLib::ext::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    //! the return type of the coupon
    EquityReturnType returnType() const { return returnType_; }
    //! are dividends scaled (e.g. to account for tax)
    Real dividendFactor() const { return dividendFactor_; }
    //! The date at which the starting equity price is fixed
    Date fixingStartDate() const { return fixingStartDate_; }
    //! The date at which performance is measured
    Date fixingEndDate() const { return fixingEndDate_; }
    //! return both fixing dates
    std::vector<Date> fixingDates() const;
    //! initial price
    Real initialPrice() const;
    //! initial price is in target ccy (if applicable, i.e. if fxIndex != null, otherwise ignored)
    bool initialPriceIsInTargetCcy() const;
    //! Number of equity shares held
    Real quantity() const;
    //! FX conversion rate (or 1.0 if not applicable)
    Real fxRate() const;
    //! This function is called for other coupon types
    Date fixingDate() const {
        QL_FAIL("Equity Coupons have 2 fixings, not 1.");
        return Date();
    }
    //! Initial notional of the equity leg, to compute quantity if not provided in the resetting case
    Real legInitialNotional() const { return legInitialNotional_; }
    //! Fixing date of the first equity coupon, to compute quantity if not provided in the resetting case
    Date legFixingDate() const { return legFixingDate_; }
    //@}

    //! \name Observer interface
    //@{
    void update() override { notifyObservers(); }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
    void setPricer(const QuantLib::ext::shared_ptr<EquityCouponPricer>&);
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer() const;

protected:
    QuantLib::ext::shared_ptr<EquityCouponPricer> pricer_;
    Natural fixingDays_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
    DayCounter dayCounter_;
    EquityReturnType returnType_;
    Real dividendFactor_;
    bool notionalReset_;
    Real initialPrice_;
    bool initialPriceIsInTargetCcy_;
    mutable Real quantity_;
    Date fixingStartDate_;
    Date fixingEndDate_;
    Natural paymentLag_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Real legInitialNotional_;
    Date legFixingDate_;
};

// inline definitions

inline void EquityCoupon::accept(AcyclicVisitor& v) {
    Visitor<EquityCoupon>* v1 = dynamic_cast<Visitor<EquityCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

inline QuantLib::ext::shared_ptr<EquityCouponPricer> EquityCoupon::pricer() const { return pricer_; }

//! helper class building a sequence of equity coupons
/*! \ingroup cashflows
 */
class EquityLeg {
public:
    EquityLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
              const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);
    EquityLeg& withNotional(Real notional);
    EquityLeg& withNotionals(const std::vector<Real>& notionals);
    EquityLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    EquityLeg& withPaymentAdjustment(BusinessDayConvention convention);
    EquityLeg& withPaymentLag(Natural paymentLag);
    EquityLeg& withPaymentCalendar(const Calendar& calendar);
    EquityLeg& withReturnType(EquityReturnType);
    EquityLeg& withDividendFactor(Real);
    EquityLeg& withInitialPrice(Real);
    EquityLeg& withInitialPriceIsInTargetCcy(bool);
    EquityLeg& withFixingDays(Natural);
    EquityLeg& withValuationSchedule(const Schedule& valuationSchedule);
    EquityLeg& withNotionalReset(bool);
    EquityLeg& withQuantity(Real);
    operator Leg() const;

private:
    Schedule schedule_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    Natural paymentLag_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    EquityReturnType returnType_;
    Real initialPrice_;
    bool initialPriceIsInTargetCcy_;
    Real dividendFactor_;
    Natural fixingDays_;
    Schedule valuationSchedule_;
    bool notionalReset_;
    Real quantity_;
};

} // namespace QuantExt

#endif
