/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file overnightindexedcoupon.hpp
    \brief coupon paying the compounded daily overnight rate,
           copy of QL class, added includeSpread flag
*/

#pragma once

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {
class OptionletVolatilityStructure;
}

namespace QuantExt {
using namespace QuantLib;

class OvernightIndexedCouponPricer;

//! overnight coupon
/*! %Coupon paying the compounded interest due to daily overnight fixings.

    if includeSpread = true, the spread is included in the daily compounding,
    otherwise it is added to the effective coupon rate after the compounding
*/
class OvernightIndexedCoupon : public FloatingRateCoupon {
public:
    OvernightIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                           Spread spread = 0.0, const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                           const DayCounter& dayCounter = DayCounter(), bool telescopicValueDates = false,
                           bool includeSpread = false, const Period& lookback = 0 * Days, const Natural rateCutoff = 0,
                           const Natural fixingDays = Null<Size>(), const Date& rateComputationStartDate = Null<Date>(),
                           const Date& rateComputationEndDate = Null<Date>(), bool applyObservationShift = false);
    //! \name Inspectors
    //@{
    //! fixing dates for the rates to be compounded
    const std::vector<Date>& fixingDates() const;
    //! accrual (compounding) periods
    const std::vector<Time>& dt() const;
    //! fixings to be compounded
    const std::vector<Rate>& indexFixings() const;
    //! value dates, associated with the fixing dates, for the rates to be compounded
    const std::vector<Date>& valueDates() const;
    //! interest dates against which the overnight rates are applied.
    const std::vector<Date>& interestDates() const;
    //! include spread in compounding?
    bool includeSpread() const { return includeSpread_; }
    /*! effectiveSpread and effectiveIndexFixing are set such that
        coupon amount = notional * accrualPeriod * ( gearing * effectiveIndexFixing + effectiveSpread )
        notice that
        - gearing = 1 is required if includeSpread = true
        - effectiveSpread = spread() if includeSpread = false */
    Real effectiveSpread() const;
    Real effectiveIndexFixing() const;
    //! lookback period
    const Period& lookback() const { return lookback_; }
    //! rate cutoff
    Natural rateCutoff() const { return rateCutoff_; }
    //! rate computation start date
    const Date& rateComputationStartDate() const { return rateComputationStartDate_; }
    //! rate computation end date
    const Date& rateComputationEndDate() const { return rateComputationEndDate_; }
    //! Is there an observation shift.
    bool applyObservationShift() const { return applyObservationShift_; }
    //! Is there a lookback.
    bool hasLookback() const { return lookback_.length() != 0; }
    //! the underlying index
    const ext::shared_ptr<OvernightIndex>& overnightIndex() const { return overnightIndex_; }
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! the date when the coupon is fully determined
    Date fixingDate() const override { return fixingDates_.back(); }
    Real accruedAmount(const Date&) const override;
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
private:
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex_;
    // The valueDates_ are the value dates associated with the corresponding fixing date.
    mutable std::vector<Date> valueDates_;
    // The fixingDates_ are the dates on which the overnight index fixings are observed.
    mutable std::vector<Date> fixingDates_;
    // The interestDates_ define the overnight periods against which the overnight rate is applied.
    mutable std::vector<Date> interestDates_;
    mutable std::vector<Rate> fixings_;
    Size n_;
    mutable std::vector<Time> dt_;
    bool includeSpread_;
    Period lookback_;
    Natural rateCutoff_;
    Date rateComputationStartDate_;
    Date rateComputationEndDate_;
    bool applyObservationShift_;

    // Record last possible fixing date.
    QuantLib::Date lastFixingDate_;
    // Index into fixing dates for current start of telescopic period. If not set, all dates are present.
    mutable QuantLib::ext::optional<std::size_t> tsStartIdx_;

    // True if telescopic dates requested and can be applied.
    bool telescopicDates_;

    // Cached adjusted evaluation date i.e. first business day preceding the evaluation date for which the 
    // current date schedules were calculated.
    mutable QuantLib::Date cachedEvalDate_;

    // Set value of telescopicDates_ according to whether it can be used or not.
    void setTelescopicDates();

    // Check if (telescopic) date schedules are stale.
    bool haveStaleDates() const;

    // Update (telescopic) date schedules that are stale.
    void updateSchedules() const;

    // Calculate the effective rate up to a given date.
    QuantLib::Rate effectiveRate(const QuantLib::Date& date) const;

    // Check for overnight index coupon pricer, throw if not and return shared pointer to it if valid.
    QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer> oicPricer() const;

    // Add schedule dates corresponding to all fixing dates from `fixStart` up to `fixEnd`.
    void addScheduleDates(QuantLib::Date fixEnd, QuantLib::Date fixStart, QuantLib::Date intStart,
        QuantLib::Date lbStart, bool exclEnd = false);

    // Add rate cut-off dates
    void addRateCutoffDates(QuantLib::Date fixEnd, QuantLib::Date rcoStart, QuantLib::Date rcoIntStart,
        QuantLib::Date rcoLbStart, bool exclEnd = false);

    // Add final dates in telescopic period.
    void addTelescopeBackStub(QuantLib::Date fixEnd, QuantLib::Date rcoStart, QuantLib::Date rcoIntStart,
                              QuantLib::Date rcoLbStart, const QuantLib::Date& intEnd, const QuantLib::Date& adjIntEnd,
                              const QuantLib::Date& lbEnd);

    // After adding dates in telescopic period, check if we have all dates and update tsStartIdx_ accordingly.
    void checkForAllDates() const;

    // Populate accrual values dt_.
    void populateAccruals() const;
};

//! OvernightIndexedCoupon pricer
class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    void initialize(const FloatingRateCoupon& coupon) override;
    void compute() const;
    Rate swapletRate() const override;
    Rate effectiveSpread() const;
    Rate effectiveIndexFixing() const;
    Real swapletPrice() const override { QL_FAIL("swapletPrice not available"); }
    Real capletPrice(Rate) const override { QL_FAIL("capletPrice not available"); }
    Rate capletRate(Rate) const override { QL_FAIL("capletRate not available"); }
    Real floorletPrice(Rate) const override { QL_FAIL("floorletPrice not available"); }
    Rate floorletRate(Rate) const override { QL_FAIL("floorletRate not available"); }

protected:
    const OvernightIndexedCoupon* coupon_;
    mutable Real swapletRate_, effectiveSpread_, effectiveIndexFixing_;
};

//! capped floored overnight indexed coupon
class CappedFlooredOvernightIndexedCoupon : public FloatingRateCoupon {
public:
    /*! capped / floored compounded, backward-looking on coupon, local means that the daily rates are capped / floored
      while a global cap / floor is applied to the effective period rate */
    CappedFlooredOvernightIndexedCoupon(const ext::shared_ptr<OvernightIndexedCoupon>& underlying,
                                        Real cap = Null<Real>(), Real floor = Null<Real>(), bool nakedOption = false,
                                        bool localCapFloor = false);

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

    ext::shared_ptr<OvernightIndexedCoupon> underlying() const { return underlying_; }
    bool nakedOption() const { return nakedOption_; }
    bool localCapFloor() const { return localCapFloor_; }

protected:
    ext::shared_ptr<OvernightIndexedCoupon> underlying_;
    Rate cap_, floor_;
    bool nakedOption_;
    bool localCapFloor_;
    mutable Real effectiveCapletVolatility_;
    mutable Real effectiveFloorletVolatility_;
};

//! capped floored overnight indexed coupon pricer base class
class CappedFlooredOvernightIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    CappedFlooredOvernightIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v,
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
class OvernightLeg {
public:
    OvernightLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& overnightIndex);
    OvernightLeg& withNotionals(Real notional);
    OvernightLeg& withNotionals(const std::vector<Real>& notionals);
    OvernightLeg& withPaymentDayCounter(const DayCounter&);
    OvernightLeg& withPaymentAdjustment(BusinessDayConvention);
    OvernightLeg& withPaymentCalendar(const Calendar&);
    OvernightLeg& withPaymentLag(Natural lag);
    OvernightLeg& withGearings(Real gearing);
    OvernightLeg& withGearings(const std::vector<Real>& gearings);
    OvernightLeg& withSpreads(Spread spread);
    OvernightLeg& withSpreads(const std::vector<Spread>& spreads);
    OvernightLeg& withTelescopicValueDates(bool telescopicValueDates);
    OvernightLeg& includeSpread(bool includeSpread);
    OvernightLeg& withLookback(const Period& lookback);
    OvernightLeg& withRateCutoff(const Natural rateCutoff);
    OvernightLeg& withFixingDays(const Natural fixingDays);
    OvernightLeg& withCaps(Rate cap);
    OvernightLeg& withCaps(const std::vector<Rate>& caps);
    OvernightLeg& withFloors(Rate floor);
    OvernightLeg& withFloors(const std::vector<Rate>& floors);
    OvernightLeg& withNakedOption(const bool nakedOption);
    OvernightLeg& withLocalCapFloor(const bool localCapFloor);
    OvernightLeg& withInArrears(const bool inArrears);
    OvernightLeg& withLastRecentPeriod(const QuantLib::ext::optional<Period>& lastRecentPeriod);
    OvernightLeg& withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar);
    OvernightLeg& withOvernightIndexedCouponPricer(const QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer);
    OvernightLeg& withPaymentDates(const std::vector<Date>& paymentDates);
    OvernightLeg& withCapFlooredOvernightIndexedCouponPricer(
        const QuantLib::ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& couponPricer);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<OvernightIndex> overnightIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    Calendar paymentCalendar_;
    BusinessDayConvention paymentAdjustment_;
    Natural paymentLag_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    bool telescopicValueDates_;
    bool includeSpread_;
    Period lookback_;
    Natural rateCutoff_;
    Natural fixingDays_;
    std::vector<Rate> caps_, floors_;
    bool nakedOption_;
    bool localCapFloor_;
    bool inArrears_;
    QuantLib::ext::optional<Period> lastRecentPeriod_;
    Calendar lastRecentPeriodCalendar_;
    std::vector<QuantLib::Date> paymentDates_;
    QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer> couponPricer_;
    QuantLib::ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer> capFlooredCouponPricer_;
};

} // namespace QuantExt
