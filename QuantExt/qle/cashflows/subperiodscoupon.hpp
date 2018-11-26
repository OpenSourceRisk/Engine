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

/*! \file subperiodscoupon.hpp
    \brief Coupon with a number of sub-periods

        \ingroup cashflows
*/

#ifndef quantext_sub_periods_coupon_hpp
#define quantext_sub_periods_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {
//! Sub-periods pricer
class SubPeriodsCouponPricer;

//! Sub-periods coupon
/*! The coupon period tenor is a multiple of the tenor associated with
*   the index. The index tenor divides the coupon period into sub-periods.
*   The index fixing for each sub-period is compounded or averaged over
*   the full coupon period.

        \ingroup cashflows
*/
class SubPeriodsCoupon : public FloatingRateCoupon {
public:
    enum Type { Averaging, Compounding };
    SubPeriodsCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                     const boost::shared_ptr<InterestRateIndex>& index, Type type, BusinessDayConvention convention,
                     Spread spread = 0.0, const DayCounter& dayCounter = DayCounter(), bool includeSpread = false,
                     Real gearing = 1.0);
    //! \name Inspectors
    //@{
    //! fixing dates for the sub-periods
    const std::vector<Date>& fixingDates() const { return fixingDates_; }
    //! accrual periods for the sub-periods
    const std::vector<Time>& accrualFractions() const { return accrualFractions_; }
    //! fixings for the sub-periods
    const std::vector<Rate>& indexFixings() const;
    //! value dates for the sub-periods
    const std::vector<Date>& valueDates() const { return valueDates_; }
    //! whether sub-period fixings are averaged or compounded
    Type type() const { return type_; }
    //! whether to include/exclude spread in compounding/averaging
    bool includeSpread() const { return includeSpread_; }
    //! Need to be able to change spread to solve for fair spread
    Spread spread() const { return spread_; }
    Spread& spread() { return spread_; }
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! the date when the coupon is fully determined
    Date fixingDate() const { return fixingDates_.back(); }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}
private:
    Type type_;
    bool includeSpread_;
    std::vector<Date> valueDates_, fixingDates_;
    mutable std::vector<Rate> fixings_;
    Size numPeriods_;
    std::vector<Time> accrualFractions_;
};

//! helper class building a sequence of sub-period coupons
/*! \ingroup cashflows
 */
class SubPeriodsLeg {
public:
    SubPeriodsLeg(const Schedule& schedule, const boost::shared_ptr<InterestRateIndex>& index);
    SubPeriodsLeg& withNotional(Real notional);
    SubPeriodsLeg& withNotionals(const std::vector<Real>& notionals);
    SubPeriodsLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    SubPeriodsLeg& withPaymentAdjustment(BusinessDayConvention convention);
    SubPeriodsLeg& withGearing(Real gearing);
    SubPeriodsLeg& withGearings(const std::vector<Real>& gearings);
    SubPeriodsLeg& withSpread(Spread spread);
    SubPeriodsLeg& withSpreads(const std::vector<Spread>& spreads);
    SubPeriodsLeg& withPaymentCalendar(const Calendar& calendar);
    SubPeriodsLeg& withType(SubPeriodsCoupon::Type type);
    SubPeriodsLeg& includeSpread(bool includeSpread);
    operator Leg() const;

private:
    Schedule schedule_;
    boost::shared_ptr<InterestRateIndex> index_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    Calendar paymentCalendar_;
    SubPeriodsCoupon::Type type_;
    bool includeSpread_;
};
} // namespace QuantExt

#endif
