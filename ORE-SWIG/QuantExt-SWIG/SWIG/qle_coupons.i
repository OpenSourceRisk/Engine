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
using QuantExt::CappedFlooredAverageONIndexedCoupon;
using QuantExt::CapFlooredAverageONIndexedCouponPricer;
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
    bool telescopicDates() const;
    bool separateRateCompPeriod() const;
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
        const Date& rateComputationStartDate = Null<Date>(),
        const Date& rateComputationEndDate = Null<Date>(),
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

%shared_ptr(CapFlooredAverageONIndexedCouponPricer)
class CapFlooredAverageONIndexedCouponPricer : public FloatingRateCouponPricer {
  public:
    CapFlooredAverageONIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
};

%shared_ptr(AverageONLeg)
class AverageONLeg {
  public:
    AverageONLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& overnightIndex);
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
    AverageONLeg& withAverageONIndexedCouponPricer(const ext::shared_ptr<AverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withCapFlooredAverageONIndexedCouponPricer(
        const ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withObservationShift(bool observationShift);
    operator Leg() const;
};


namespace QuantExt {

%rename(QleOvernightIndexedCoupon) OvernightIndexedCoupon;
%shared_ptr(OvernightIndexedCoupon)
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
        const Date& rateComputationStartDate = Null<Date>(),
        const Date& rateComputationEndDate = Null<Date>(),
        bool observationShift = true);

    bool includeSpread() const;
    Real effectiveSpread() const;
    Real effectiveIndexFixing() const;
};

%rename(QleOvernightIndexedCouponPricer) OvernightIndexedCouponPricer;
%shared_ptr(OvernightIndexedCouponPricer)
class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
  public:
    Rate effectiveSpread() const;
    Rate effectiveIndexFixing() const;
    Rate effectiveRate(const Date& date) const;
    std::tuple<Rate, Spread, Rate> rateSpreadFixing() const;
};

%rename(QleCappedFlooredOvernightIndexedCoupon) CappedFlooredOvernightIndexedCoupon;
%shared_ptr(CappedFlooredOvernightIndexedCoupon)
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

%rename(QleCappedFlooredOvernightIndexedCouponPricer) CappedFlooredOvernightIndexedCouponPricer;
%shared_ptr(CappedFlooredOvernightIndexedCouponPricer)
class CappedFlooredOvernightIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    CappedFlooredOvernightIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
    Handle<OptionletVolatilityStructure> capletVolatility() const;
    Real effectiveCapletVolatility() const;
    Real effectiveFloorletVolatility() const;
    Real strippedCapletVolatility() const;
    Real strippedFloorletVolatility() const;
};

%rename(QleOvernightLeg) OvernightLeg;
%shared_ptr(OvernightLeg)
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
        const ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer);
    OvernightLeg& withPaymentDates(const std::vector<Date>& paymentDates);
    OvernightLeg& withCapFlooredOvernightIndexedCouponPricer(
        const ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& couponPricer);
    OvernightLeg& withObservationShift(bool observationShift);
    operator Leg() const;
};

}
