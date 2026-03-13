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
    \brief paying the compounded interest due to daily overnight fixings.
*/
#pragma once
#include <qle/cashflows/overnightindexedcouponbase.hpp>
#include <ql/cashflows/couponpricer.hpp>

namespace QuantExt {

using namespace QuantLib;
class QuantLib::OptionletVolatilityStructure;
class OvernightIndexedCouponPricer;

//! Overnight (compounding) coupon
/** %Coupon paying the compounded interest due to daily overnight fixings.
 *  \ingroup cashflows
 */
class OvernightIndexedCoupon : public OvernightIndexedCouponBase {
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
    /** If `true`, the spread is included in the daily compounding. If `false` the compounding is performed without the 
     *   spread and the spread is added to the final rate.
     */
    bool includeSpread() const { return includeSpread_; }
    /** The `effectiveSpread` \f$s^*\f$ and `effectiveIndexFixing` \f$f^*\f$ are set such that the coupon amount is 
     *  \f$\Pi\f$ is given by:
     *  \f[
     *      N \tau(t_s, t_e) ( g f^* + s^* )
     *  \f]
     * 
     *   \note
     *   - a `gearing` value of 1 is required if `includeSpread` is set to `true`.
     *   - the `effectiveSpread` is equal to the input `spread` if `includeSpread` is set to `false`.
     */
    Real effectiveSpread() const;
    Real effectiveIndexFixing() const;
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
private:
    bool includeSpread_;

    // Can apply telescopic formula, without approximation, if either of the following hold:
    // 1. no lookback and fixingDays_ align with overnight index fixing days.
    // 2. have lookback, obs shift is true and fixingDays_ align with overnight index fixing days.
    void setTelescopicDates() override;

    // Calculate the effective rate up to a given date.
    QuantLib::Rate effectiveRate(const QuantLib::Date& date) const override;

    // Check for overnight index coupon pricer, throw if not and return shared pointer to it if valid.
    QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer> oicPricer() const;
};

//! OvernightIndexedCoupon pricer
class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    void initialize(const FloatingRateCoupon& coupon) override;
    void compute() const;
    Rate swapletRate() const override;
    Rate effectiveSpread() const;
    Rate effectiveIndexFixing() const;
    Rate effectiveRate(const Date& date) const;
    Real swapletPrice() const override { QL_FAIL("swapletPrice not available"); }
    Real capletPrice(Rate) const override { QL_FAIL("capletPrice not available"); }
    Rate capletRate(Rate) const override { QL_FAIL("capletRate not available"); }
    Real floorletPrice(Rate) const override { QL_FAIL("floorletPrice not available"); }
    Rate floorletRate(Rate) const override { QL_FAIL("floorletRate not available"); }

protected:
    std::tuple<Rate, Spread, Rate> compute(const QuantLib::Date& date) const;
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
    OvernightLeg& withOvernightIndexedCouponPricer(
        const QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer);
    OvernightLeg& withPaymentDates(const std::vector<Date>& paymentDates);
    OvernightLeg& withCapFlooredOvernightIndexedCouponPricer(
        const QuantLib::ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& couponPricer);
    OvernightLeg& withObservationShift(bool observationShift);
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
    bool observationShift_;
};

} // namespace QuantExt
