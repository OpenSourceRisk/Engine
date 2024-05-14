/*
  Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/equitymargincoupon.hpp
    \brief coupon paying the return on an equity
    \ingroup cashflows
*/

#ifndef quantext_equity_margin_coupon_hpp
#define quantext_equity_margin_coupon_hpp

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

//! equity margin coupon pricer
/*! \ingroup cashflows
 */
class EquityMarginCouponPricer;

//! equity coupon
/*!
    \ingroup cashflows
*/
class EquityMarginCoupon : public Coupon, public Observer {
public:
    EquityMarginCoupon(const Date& paymentDate, Real nominal, Rate rate, Real marginFactor, const Date& startDate, const Date& endDate, Natural fixingDays,
                 const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve, const DayCounter& dayCounter,
                 bool isTotalReturn = false, Real dividendFactor = 1.0, bool notionalReset = false,
                 Real initialPrice = Null<Real>(), Real quantity = Null<Real>(), const Date& fixingStartDate = Date(),
                 const Date& fixingEndDate = Date(), const Date& refPeriodStart = Date(),
                 const Date& refPeriodEnd = Date(), const Date& exCouponDate = Date(), Real multiplier = Null<Real>(),
                 const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr, const bool initialPriceIsInTargetCcy = false);
    
    //! \name CashFlow interface
    //@{
    //@}

    //! \name Coupon interface
    //@{
    DayCounter dayCounter() const override { return dayCounter_; }
    Real accruedAmount(const Date&) const override;
    Real amount() const override;
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
    //! initial price
    Real initialPrice() const;
    //! initial price is in target ccy (if applicable, i.e. if fxIndex != null, otherwise ignored)
    bool initialPriceIsInTargetCcy() const;
    //! Number of equity shares held
    Real quantity() const { return quantity_; }
    //! FX conversion rate (or 1.0 if not applicable)
    Real fxRate() const;
    //! This function is called for other coupon types
    Date fixingDate() const {
        QL_FAIL("Equity Coupons have 2 fixings, not 1.");
        return Date();
    }
    Real marginFactor() const { return marginFactor_; }
    InterestRate fixedRate() const { return fixedRate_; }
    Real multiplier() const { return multiplier_; }
    //@}

    //! \name Observer interface
    //@{
    void update() override { notifyObservers(); }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

    void setPricer(const QuantLib::ext::shared_ptr<EquityMarginCouponPricer>&);
    QuantLib::ext::shared_ptr<EquityMarginCouponPricer> pricer() const;

protected:
    QuantLib::ext::shared_ptr<EquityMarginCouponPricer> pricer_;
    Natural fixingDays_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
    DayCounter dayCounter_;
    bool isTotalReturn_;
    Real dividendFactor_;
    bool notionalReset_;
    Real initialPrice_;
    bool initialPriceIsInTargetCcy_;
    Real quantity_;
    Date fixingStartDate_;
    Date fixingEndDate_;
    Natural paymentLag_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Real marginFactor_;
    InterestRate fixedRate_;
    Real multiplier_;
};

// inline definitions

inline void EquityMarginCoupon::accept(AcyclicVisitor& v) {
    Visitor<EquityMarginCoupon>* v1 = dynamic_cast<Visitor<EquityMarginCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

//! helper class building a sequence of equity margin coupons
/*! \ingroup cashflows
 */
class EquityMarginLeg {
public:
    EquityMarginLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
              const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    EquityMarginLeg& withCouponRates(Rate,
                                  const DayCounter& paymentDayCounter,
                                  Compounding comp = Simple,
                                  Frequency freq = Annual);
    EquityMarginLeg& withCouponRates(const std::vector<Rate>&,
                                  const DayCounter& paymentDayCounter,
                                  Compounding comp = Simple,
                                  Frequency freq = Annual);
    EquityMarginLeg& withCouponRates(const InterestRate&);
    EquityMarginLeg& withCouponRates(const std::vector<InterestRate>&);

    EquityMarginLeg& withInitialMarginFactor(const Real& marginFactor);

    EquityMarginLeg& withNotional(Real notional);
    EquityMarginLeg& withNotionals(const std::vector<Real>& notionals);
    EquityMarginLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    EquityMarginLeg& withPaymentAdjustment(BusinessDayConvention convention);
    EquityMarginLeg& withPaymentLag(Natural paymentLag);
    EquityMarginLeg& withPaymentCalendar(const Calendar& calendar);
    EquityMarginLeg& withTotalReturn(bool);
    EquityMarginLeg& withDividendFactor(Real);
    EquityMarginLeg& withInitialPrice(Real);
    EquityMarginLeg& withInitialPriceIsInTargetCcy(bool);
    EquityMarginLeg& withFixingDays(Natural);
    EquityMarginLeg& withValuationSchedule(const Schedule& valuationSchedule);
    EquityMarginLeg& withNotionalReset(bool);
    EquityMarginLeg& withQuantity(Real);
    EquityMarginLeg& withMultiplier(Real);

    operator Leg() const;

private:

    std::vector<InterestRate> couponRates_;
    Real marginFactor_;
    Schedule schedule_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    Natural paymentLag_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    bool isTotalReturn_;
    Real initialPrice_;
    bool initialPriceIsInTargetCcy_;
    Real dividendFactor_;
    Natural fixingDays_;
    Schedule valuationSchedule_;
    bool notionalReset_;
    Real quantity_;
    Real multiplier_;
};

} // namespace QuantExt

#endif
