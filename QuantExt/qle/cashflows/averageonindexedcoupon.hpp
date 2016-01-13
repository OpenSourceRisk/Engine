/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file averageonindexedcoupon.hpp
    \brief coupon paying the weighted average of the daily overnight rate
*/

#ifndef quantlib_average_on_indexed_coupon_hpp
#define quantlib_average_on_indexed_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {
    
    class AverageONIndexedCouponPricer;

    //! average overnight coupon
    /*! %Coupon paying the interest due to the weighted average of daily 
         overnight fixings. */
    class AverageONIndexedCoupon : public FloatingRateCoupon {
      public:
        AverageONIndexedCoupon(const Date& paymentDate,
            Real nominal, const Date& startDate, const Date& endDate,
            const boost::shared_ptr<OvernightIndex>& overnightIndex,
            Real gearing = 1.0, Spread spread = 0.0, Natural rateCutoff = 0, 
            const DayCounter& dayCounter = DayCounter());
        //! \name Inspectors
        //@{
        //! fixing dates for the rates to be averaged
        const std::vector<Date>& fixingDates() const { return fixingDates_; }
        //! accrual periods for the averaging
        const std::vector<Time>& dt() const { return dt_; }
        //! fixings to be averaged
        const std::vector<Rate>& indexFixings() const;
        //! value dates for the rates to be averaged
        const std::vector<Date>& valueDates() const { return valueDates_; }
        //! rate cutoff associated with the coupon
        Natural rateCutoff() const { return rateCutoff_; }
        //@}
        //! \name FloatingRateCoupon interface
        //@{
        //! the date when the coupon is fully determined
        Date fixingDate() const;
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
      private:
        std::vector<Date> valueDates_, fixingDates_;
        mutable std::vector<Rate> fixings_;
        Size numPeriods_;
        std::vector<Time> dt_;
        Natural rateCutoff_;
    };

    //! helper class building a sequence of overnight coupons
    class AverageONLeg {
      public:
        AverageONLeg(const Schedule& schedule,
            const boost::shared_ptr<OvernightIndex>& overnightIndex);
        AverageONLeg& withNotional(Real notional);
        AverageONLeg& withNotionals(const std::vector<Real>& notionals);
        AverageONLeg& withPaymentDayCounter(const DayCounter& dayCounter);
        AverageONLeg& withPaymentAdjustment(BusinessDayConvention convention);
        AverageONLeg& withGearing(Real gearing);
        AverageONLeg& withGearings(const std::vector<Real>& gearings);
        AverageONLeg& withSpread(Spread spread);
        AverageONLeg& withSpreads(const std::vector<Spread>& spreads);
        AverageONLeg& withRateCutoff(Natural rateCutoff);
        AverageONLeg& withPaymentCalendar(const Calendar& calendar);
        AverageONLeg& withAverageONIndexedCouponPricer(const boost::shared_ptr
            <AverageONIndexedCouponPricer>& couponPricer);
        operator Leg() const;
      private:
        Schedule schedule_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        std::vector<Real> notionals_;
        DayCounter paymentDayCounter_;
        BusinessDayConvention paymentAdjustment_;
        std::vector<Real> gearings_;
        std::vector<Spread> spreads_;
        Calendar paymentCalendar_;
        Natural rateCutoff_;
        boost::shared_ptr<AverageONIndexedCouponPricer> couponPricer_;
    };

}

#endif
