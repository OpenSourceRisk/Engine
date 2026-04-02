/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_coupons_i
#define qle_coupons_i

%include indexes.i
%include cashflows.i
%include scheduler.i

%{
using QuantExt::OvernightIndexedCouponBase;
using QuantExt::AverageONIndexedCoupon;
using QuantExt::AverageONIndexedCouponPricer;
using QuantExt::CappedFlooredAverageONIndexedCoupon;
using QuantExt::CappedFlooredOvernightIndexedCouponPricer;
using QuantExt::BlackOvernightIndexedCouponPricer;
using QuantExt::CapFlooredAverageONIndexedCouponPricer;
using QuantExt::BlackAverageONIndexedCouponPricer;
using QuantExt::AverageONLeg;
using namespace std;
%}

%shared_ptr(OvernightIndexedCouponBase)
class OvernightIndexedCouponBase : public FloatingRateCoupon {
  private:
    OvernightIndexedCouponBase();
  public:
    enum class Type { Compounding, Averaging };

    const std::vector<QuantLib::Date>& fixingDates() const;
    const std::vector<QuantLib::Time>& dt() const;
    const std::vector<QuantLib::Rate>& indexFixings() const;
    const std::vector<QuantLib::Date>& valueDates() const;
    const std::vector<QuantLib::Date>& interestDates() const;
    const QuantLib::Period& lookback() const;
    QuantLib::Natural rateCutoff() const;
    const QuantLib::Date& rateComputationStartDate() const;
    const QuantLib::Date& rateComputationEndDate() const;
    bool observationShift() const;
    bool hasLookback() const;
    const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& overnightIndex() const;
    bool canApplyTelescopic() const;
    bool telescopicDates() const;
    bool separateRateCompPeriod() const;
    bool includeSpread() const;
    Type rateType() const;
};

%shared_ptr(AverageONIndexedCoupon)
class AverageONIndexedCoupon : public OvernightIndexedCouponBase {
  public:
    AverageONIndexedCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        const ext::shared_ptr<OvernightIndex>& overnightIndex,
        Real gearing = 1.0,
        Spread spread = 0.0,
        Natural rateCutoff = 0,
        const DayCounter& dayCounter = DayCounter(),
        const Period& lookback = 0 * Days,
        const Size fixingDays = Null<Size>(),
        const Date& rateComputationStartDate = Date(),
        const Date& rateComputationEndDate = Date(),
        const bool telescopicValueDates = false,
        bool observationShift = true);
};

%shared_ptr(CappedFlooredAverageONIndexedCoupon)
class CappedFlooredAverageONIndexedCoupon : public FloatingRateCoupon {
  public:
    CappedFlooredAverageONIndexedCoupon(
        const ext::shared_ptr<AverageONIndexedCoupon>& underlying,
        Real cap = Null<Real>(),
        Real floor = Null<Real>(),
        bool nakedOption = false,
        bool localCapFloor = false,
        bool includeSpread = false);

    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
    bool isCapped() const;
    bool isFloored() const;
    ext::shared_ptr<AverageONIndexedCoupon> underlying();
    bool nakedOption() const;
    bool localCapFloor();
    bool includeSpread();
};

%shared_ptr(CappedFlooredOvernightIndexedCouponPricer)
class CappedFlooredOvernightIndexedCouponPricer : public FloatingRateCouponPricer {
  private:
    CappedFlooredOvernightIndexedCouponPricer();
  public:
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
};

%shared_ptr(BlackOvernightIndexedCouponPricer)
class BlackOvernightIndexedCouponPricer : public CappedFlooredOvernightIndexedCouponPricer {
  public:
    BlackOvernightIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
};

%shared_ptr(CapFlooredAverageONIndexedCouponPricer)
class CapFlooredAverageONIndexedCouponPricer : public FloatingRateCouponPricer {
  private:
    CapFlooredAverageONIndexedCouponPricer();
  public:
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
};

%shared_ptr(BlackAverageONIndexedCouponPricer)
class BlackAverageONIndexedCouponPricer : public CapFlooredAverageONIndexedCouponPricer {
  public:
    BlackAverageONIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
};

%shared_ptr(QuantExt::OvernightIndexedCoupon)
%shared_ptr(QuantExt::OvernightIndexedCouponPricer)
%shared_ptr(QuantExt::CappedFlooredOvernightIndexedCoupon)
%shared_ptr(QuantExt::OvernightLeg)

namespace QuantExt {

%rename(QLEOvernightIndexedCoupon) OvernightIndexedCoupon;
class OvernightIndexedCoupon : public OvernightIndexedCouponBase {
  public:
    OvernightIndexedCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        const ext::shared_ptr<OvernightIndex>& overnightIndex,
        Real gearing = 1.0,
        Spread spread = 0.0,
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        const DayCounter& dayCounter = DayCounter(),
        bool telescopicValueDates = false,
        bool includeSpread = false,
        const Period& lookback = 0 * Days,
        const Natural rateCutoff = 0,
        const Natural fixingDays = Null<Size>(),
        const Date& rateComputationStartDate = Date(),
        const Date& rateComputationEndDate = Date(),
        bool observationShift = true);

    bool includeSpread() const;
    Real effectiveSpread() const;
    Real effectiveIndexFixing() const;
};

%rename(QLEOvernightIndexedCouponPricer) OvernightIndexedCouponPricer;
class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
  public:
    Rate effectiveSpread() const;
    Rate effectiveIndexFixing() const;
    std::pair<Rate, Date> effectiveRate(const Date& date) const;
    std::tuple<Rate, Spread, Rate> rateSpreadFixing() const;
};

%rename(QLECappedFlooredOvernightIndexedCoupon) CappedFlooredOvernightIndexedCoupon;
class CappedFlooredOvernightIndexedCoupon : public FloatingRateCoupon {
  public:
    CappedFlooredOvernightIndexedCoupon(
        const ext::shared_ptr<OvernightIndexedCoupon>& underlying,
        Real cap = Null<Real>(),
        Real floor = Null<Real>(),
        bool nakedOption = false,
        bool localCapFloor = false);

    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
    bool isCapped() const;
    bool isFloored() const;

    ext::shared_ptr<OvernightIndexedCoupon> underlying();
    bool nakedOption() const;
    bool localCapFloor() const;
};

} // namespace QuantExt

// Add the Legs also similar to QuantLib.

// QuantExt::AverageONLeg
%{
Leg _AverageONLeg(
    const Schedule& schedule,
    const ext::shared_ptr<OvernightIndex>& index,
    const std::vector<Real>& nominals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    bool telescopicValueDates = false,
    Natural rateCutoff = 0,
    const Calendar& paymentCalendar = Calendar(),
    const Integer paymentLag = 0,
    const Period& lookback = 0 * Days,
    Natural fixingDays = Null<Natural>(),
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool includeSpread = false,
    bool nakedOption = false,
    bool localCapFloor = false,
    bool inArrears = true,
    const ext::optional<Period>& lastRecentPeriod = ext::nullopt,
    const Calendar& lastRecentPeriodCalendar = Calendar(),
    const std::vector<Date>& paymentDates = {},
    bool observationShift = true)
{
    return QuantExt::AverageONLeg(schedule, index)
        .withNotionals(nominals)
        .withPaymentDayCounter(paymentDayCounter)
        .withPaymentAdjustment(paymentConvention)
        .withGearings(gearings)
        .withSpreads(spreads)
        .withTelescopicValueDates(telescopicValueDates)
        .withRateCutoff(rateCutoff)
        .withPaymentCalendar(paymentCalendar.empty() ? schedule.calendar() : paymentCalendar)
        .withPaymentLag(paymentLag)
        .withLookback(lookback)
        .withFixingDays(fixingDays)
        .withCaps(caps)
        .withFloors(floors)
        .includeSpreadInCapFloors(includeSpread)
        .withNakedOption(nakedOption)
        .withLocalCapFloor(localCapFloor)
        .withInArrears(inArrears)
        .withLastRecentPeriod(lastRecentPeriod)
        .withLastRecentPeriodCalendar(lastRecentPeriodCalendar)
        .withPaymentDates(paymentDates)
        .withObservationShift(observationShift);
}
%}

#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _AverageONLeg;
#endif
%rename(AverageONLeg) _AverageONLeg;
Leg _AverageONLeg(
    const Schedule& schedule,
    const ext::shared_ptr<OvernightIndex>& index,
    const std::vector<Real>& nominals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    bool telescopicValueDates = false,
    Natural rateCutoff = 0,
    const Calendar& paymentCalendar = Calendar(),
    const Integer paymentLag = 0,
    const Period& lookback = 0 * Days,
    Natural fixingDays = Null<Natural>(),
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool includeSpread = false,
    bool nakedOption = false,
    bool localCapFloor = false,
    bool inArrears = true,
    const ext::optional<Period>& lastRecentPeriod = ext::nullopt,
    const Calendar& lastRecentPeriodCalendar = Calendar(),
    const std::vector<Date>& paymentDates = {},
    bool observationShift = true);

// QuantExt::OvernightLeg
%{
Leg _QLEOvernightLeg(
    const Schedule& schedule,
    const ext::shared_ptr<OvernightIndex>& index,
    const std::vector<Real>& nominals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    const Integer paymentLag = 0,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    bool telescopicValueDates = false,
    bool includeSpread = false,
    const Period& lookback = 0 * Days,
    Natural rateCutoff = 0,
    Natural fixingDays = Null<Natural>(),
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool nakedOption = false,
    bool localCapFloor = false,
    bool inArrears = true,
    const ext::optional<Period>& lastRecentPeriod = ext::nullopt,
    const Calendar& lastRecentPeriodCalendar = Calendar(),
    bool observationShift = true,
    const std::vector<Date>& paymentDates = {})
{
    return QuantExt::OvernightLeg(schedule, index)
        .withNotionals(nominals)
        .withPaymentDayCounter(paymentDayCounter)
        .withPaymentAdjustment(paymentConvention)
        .withPaymentCalendar(paymentCalendar.empty() ? schedule.calendar() : paymentCalendar)
        .withPaymentLag(paymentLag)
        .withGearings(gearings)
        .withSpreads(spreads)
        .withTelescopicValueDates(telescopicValueDates)
        .includeSpread(includeSpread)
        .withLookback(lookback)
        .withRateCutoff(rateCutoff)
        .withFixingDays(fixingDays)
        .withCaps(caps)
        .withFloors(floors)
        .withNakedOption(nakedOption)
        .withLocalCapFloor(localCapFloor)
        .withInArrears(inArrears)
        .withLastRecentPeriod(lastRecentPeriod)
        .withLastRecentPeriodCalendar(lastRecentPeriodCalendar)
        .withPaymentDates(paymentDates)
        .withObservationShift(observationShift);
}
%}

#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _QLEOvernightLeg;
#endif
%rename(QLEOvernightLeg) _QLEOvernightLeg;
Leg _QLEOvernightLeg(
    const Schedule& schedule,
    const ext::shared_ptr<OvernightIndex>& index,
    const std::vector<Real>& nominals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    const Integer paymentLag = 0,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    bool telescopicValueDates = false,
    bool includeSpread = false,
    const Period& lookback = 0 * Days,
    Natural rateCutoff = 0,
    Natural fixingDays = Null<Natural>(),
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool nakedOption = false,
    bool localCapFloor = false,
    bool inArrears = true,
    const ext::optional<Period>& lastRecentPeriod = ext::nullopt,
    const Calendar& lastRecentPeriodCalendar = Calendar(),
    bool observationShift = true,
    const std::vector<Date>& paymentDates = {});

#endif
