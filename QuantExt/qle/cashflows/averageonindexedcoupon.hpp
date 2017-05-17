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

/*! \file averageonindexedcoupon.hpp
    \brief coupon paying the weighted average of the daily overnight rate

        \ingroup cashflows
*/

#ifndef quantext_average_on_indexed_coupon_hpp
#define quantext_average_on_indexed_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {

//! average overnight coupon pricer
/*! \ingroup cashflows
 */
class AverageONIndexedCouponPricer;

//! average overnight coupon
/*! %Coupon paying the interest due to the weighted average of daily
     overnight fixings. The rateCutoff counts the number of fixing
     dates starting at the end date whose fixings are not taken into
     account, but rather replaced by the last known fixing before.

             \ingroup cashflows
*/
class AverageONIndexedCoupon : public FloatingRateCoupon {
public:
    AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           const boost::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                           Spread spread = 0.0, Natural rateCutoff = 0, const DayCounter& dayCounter = DayCounter());
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
/*! \ingroup cashflows
 */
class AverageONLeg {
public:
    AverageONLeg(const Schedule& schedule, const boost::shared_ptr<OvernightIndex>& overnightIndex);
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
    AverageONLeg& withAverageONIndexedCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& couponPricer);
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

} // namespace QuantExt

#endif
