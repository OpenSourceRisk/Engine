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

#pragma once

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

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
                           const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                           Spread spread = 0.0, Natural rateCutoff = 0, const DayCounter& dayCounter = DayCounter(),
                           const Period& lookback = 0 * Days, const Size fixingDays = Null<Size>(),
                           const Date& rateComputationStartDate = Null<Date>(),
                           const Date& rateComputationEndDate = Null<Date>(), const bool telescopicValueDates = false);
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
    //! lookback period
    const Period& lookback() const { return lookback_; }
    //! rate computation start date
    const Date& rateComputationStartDate() const { return rateComputationStartDate_; }
    //! rate computation end date
    const Date& rateComputationEndDate() const { return rateComputationEndDate_; }
    //! the underlying index
    const ext::shared_ptr<OvernightIndex>& overnightIndex() const { return overnightIndex_; }
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! the date when the coupon is fully determined
    Date fixingDate() const override;
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
private:
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex_;
    std::vector<Date> valueDates_, fixingDates_;
    mutable std::vector<Rate> fixings_;
    Size numPeriods_;
    std::vector<Time> dt_;
    Natural rateCutoff_;
    Period lookback_;
    Date rateComputationStartDate_, rateComputationEndDate_;
};

//! capped floored overnight indexed coupon
class CappedFlooredAverageONIndexedCoupon : public FloatingRateCoupon {
public:
    /*! capped / floored averaged, backward-looking on coupon, local means that the daily rates are capped / floored
      while a global cap / floor is applied to the effective period rate */
    CappedFlooredAverageONIndexedCoupon(const ext::shared_ptr<AverageONIndexedCoupon>& underlying,
                                        Real cap = Null<Real>(), Real floor = Null<Real>(), bool nakedOption = false,
                                        bool localCapFloor = false, bool includeSpread = false);

    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    void alwaysForwardNotifications() override;
    //@}
    //! \name Coupon interface
    //@{
    Rate rate() const override;
    Rate convexityAdjustment() const override;
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    Date fixingDate() const override { return underlying_->fixingDate(); }
    //@}
    //! cap
    Rate cap() const;
    //! floor
    Rate floor() const;
    //! effective cap of fixing
    Rate effectiveCap() const;
    //! effective floor of fixing
    Rate effectiveFloor() const;
    //! effective caplet volatility
    Real effectiveCapletVolatility() const;
    //! effective floorlet volatility
    Real effectiveFloorletVolatility() const;
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;

    bool isCapped() const { return cap_ != Null<Real>(); }
    bool isFloored() const { return floor_ != Null<Real>(); }

    ext::shared_ptr<AverageONIndexedCoupon> underlying() const { return underlying_; }
    bool nakedOption() const { return nakedOption_; }
    bool localCapFloor() const { return localCapFloor_; }
    bool includeSpread() const { return includeSpread_; }

protected:
    ext::shared_ptr<AverageONIndexedCoupon> underlying_;
    Rate cap_, floor_;
    bool nakedOption_;
    bool localCapFloor_;
    bool includeSpread_;
    mutable Real effectiveCapletVolatility_;
    mutable Real effectiveFloorletVolatility_;
};

//! capped floored averaged indexed coupon pricer base class
class CapFlooredAverageONIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    CapFlooredAverageONIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v,
                                           const bool effectiveVolatilityInput = false);
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    bool effectiveVolatilityInput() const;
    Real effectiveCapletVolatility() const;   // only available after capletRate() was called
    Real effectiveFloorletVolatility() const; // only available after floorletRate() was called

protected:
    Handle<OptionletVolatilityStructure> capletVol_;
    bool effectiveVolatilityInput_;
    mutable Real effectiveCapletVolatility_ = Null<Real>();
    mutable Real effectiveFloorletVolatility_ = Null<Real>();
};

//! helper class building a sequence of overnight coupons
/*! \ingroup cashflows
 */
class AverageONLeg {
public:
    AverageONLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex);
    AverageONLeg& withNotional(Real notional);
    AverageONLeg& withNotionals(const std::vector<Real>& notionals);
    AverageONLeg& withPaymentDayCounter(const DayCounter& dayCounter);
    AverageONLeg& withPaymentAdjustment(BusinessDayConvention convention);
    AverageONLeg& withGearing(Real gearing);
    AverageONLeg& withGearings(const std::vector<Real>& gearings);
    AverageONLeg& withSpread(Spread spread);
    AverageONLeg& withSpreads(const std::vector<Spread>& spreads);
    AverageONLeg& withTelescopicValueDates(bool telescopicValueDates);
    AverageONLeg& withRateCutoff(Natural rateCutoff);
    AverageONLeg& withPaymentCalendar(const Calendar& calendar);
    AverageONLeg& withPaymentLag(Natural lag);
    AverageONLeg& withLookback(const Period& lookback);
    AverageONLeg& withFixingDays(const Size fixingDays);
    AverageONLeg& withCaps(Rate cap);
    AverageONLeg& withCaps(const std::vector<Rate>& caps);
    AverageONLeg& withFloors(Rate floor);
    AverageONLeg& withFloors(const std::vector<Rate>& floors);
    AverageONLeg& includeSpreadInCapFloors(bool includeSpread);
    AverageONLeg& withNakedOption(const bool nakedOption);
    AverageONLeg& withLocalCapFloor(const bool localCapFloor);
    AverageONLeg& withInArrears(const bool inArrears);
    AverageONLeg& withLastRecentPeriod(const boost::optional<Period>& lastRecentPeriod);
    AverageONLeg& withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar);
    AverageONLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);
    AverageONLeg& withAverageONIndexedCouponPricer(const QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withCapFlooredAverageONIndexedCouponPricer(
        const QuantLib::ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer>& couponPricer);
    operator Leg() const;

private:
    Schedule schedule_;
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Natural paymentLag_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    bool telescopicValueDates_;
    Calendar paymentCalendar_;
    Natural rateCutoff_;
    Period lookback_;
    Natural fixingDays_;
    std::vector<Rate> caps_, floors_;
    bool includeSpread_;
    bool nakedOption_;
    bool localCapFloor_;
    bool inArrears_;
    boost::optional<Period> lastRecentPeriod_;
    Calendar lastRecentPeriodCalendar_;
    std::vector<QuantLib::Date> paymentDates_;
    QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer> couponPricer_;
    QuantLib::ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer> capFlooredCouponPricer_;
};

} // namespace QuantExt
