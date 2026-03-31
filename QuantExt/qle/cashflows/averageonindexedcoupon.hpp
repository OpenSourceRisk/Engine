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
    \brief coupon paying the interest due to the weighted average of daily overnight fixings.
*/
#pragma once
#include <qle/cashflows/overnightindexedcouponbase.hpp>
#include <ql/cashflows/couponpricer.hpp>

namespace QuantExt {

using namespace QuantLib;
class AverageONIndexedCouponPricer;

/** Overnight (averaging) coupon.
 *  %Coupon paying the interest due to the weighted, by overnight day count fraction, average of a series of overnight 
 *  fixings over the coupon period.
 *
 *  For more details about the coupon structure, see OvernightIndexedCouponBase.
 *
 *  \see OvernightIndexedCouponBase
 *  \ingroup cashflows
 */
class AverageONIndexedCoupon : public OvernightIndexedCouponBase {
public:
    AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                           Spread spread = 0.0, Natural rateCutoff = 0, const DayCounter& dayCounter = DayCounter(),
                           const Period& lookback = 0 * Days, const Size fixingDays = Null<Size>(),
                           const Date& rateComputationStartDate = Date(),
                           const Date& rateComputationEndDate = Date(), const bool telescopicValueDates = false,
                           bool observationShift = true, bool staleDatesCheck = true);
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

private:
    // Calculate the effective rate up to a given date.
    std::pair<QuantLib::Rate, QuantLib::Date> effectiveRate(const QuantLib::Date& date) const override;

    // Check for average overnight index coupon pricer, throw if not and return shared pointer to it if valid.
    QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer> oicPricer() const;
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
    //! stripped caplet volatility
    Real strippedCapletVolatility() const;
    //! stripped floorlet volatility
    Real strippedFloorletVolatility() const;
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

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
    mutable Real strippedCapletVolatility_;
    mutable Real strippedFloorletVolatility_;
};

//! capped floored averaged indexed coupon pricer base class
class CapFlooredAverageONIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    CapFlooredAverageONIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    Real effectiveCapletVolatility() const;   // only available after capletRate() was called
    Real effectiveFloorletVolatility() const; // only available after floorletRate() was called
    Real strippedCapletVolatility() const;    // only available after capletRate() was called
    Real strippedFloorletVolatility() const;  // only available after floorletRate() was called

protected:
    Handle<OptionletVolatilityStructure> capletVol_;
    mutable Real effectiveCapletVolatility_ = Null<Real>();
    mutable Real effectiveFloorletVolatility_ = Null<Real>();
    mutable Real strippedCapletVolatility_ = Null<Real>();
    mutable Real strippedFloorletVolatility_ = Null<Real>();
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
    AverageONLeg& withLastRecentPeriod(const QuantLib::ext::optional<Period>& lastRecentPeriod);
    AverageONLeg& withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar);
    AverageONLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);
    AverageONLeg& withAverageONIndexedCouponPricer(const QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withCapFlooredAverageONIndexedCouponPricer(
        const QuantLib::ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withObservationShift(bool observationShift);
    AverageONLeg& withStaleDatesCheck(bool staleDatesCheck);
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
    QuantLib::ext::optional<Period> lastRecentPeriod_;
    Calendar lastRecentPeriodCalendar_;
    std::vector<QuantLib::Date> paymentDates_;
    QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer> couponPricer_;
    QuantLib::ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer> capFlooredCouponPricer_;
    bool observationShift_;
    bool staleDatesCheck_;
};

} // namespace QuantExt
