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
using QuantExt::AverageONIndexedCoupon;
using QuantExt::CappedFlooredAverageONIndexedCoupon;
using QuantExt::CapFlooredAverageONIndexedCouponPricer;
using QuantExt::AverageONLeg;
using namespace std;
%}

%shared_ptr(AverageONIndexedCoupon)
class AverageONIndexedCoupon : public FloatingRateCoupon {
    public:
        AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                               const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                               Spread spread = 0.0, Natural rateCutoff = 0, const DayCounter& dayCounter = DayCounter(),
                               const Period& lookback = 0 * Days, const Size fixingDays = Null<Size>(),
                               const Date& rateComputationStartDate = Null<Date>(),
                               const Date& rateComputationEndDate = Null<Date>(), const bool telescopicValueDates = false);

        const std::vector<QuantLib::Date>& fixingDates() const;
        const std::vector<Time>& dt() const;
        const std::vector<Rate>& indexFixings() const;
        const std::vector<Date>& valueDates() const;
        Natural rateCutoff() const;
        const Period& lookback() const;
        const Date& rateComputationStartDate() const;
        const Date& rateComputationEndDate() const;
        const ext::shared_ptr<OvernightIndex>& overnightIndex() const;
        Date fixingDate() const override;
};


%shared_ptr(CappedFlooredAverageONIndexedCoupon)
class CappedFlooredAverageONIndexedCoupon : public FloatingRateCoupon {
    public:
        CappedFlooredAverageONIndexedCoupon(const ext::shared_ptr<AverageONIndexedCoupon>& underlying,
                                            Real cap = Null<Real>(), Real floor = Null<Real>(), bool nakedOption = false,
                                            bool localCapFloor = false, bool includeSpread = false);
        bool isCapped() const;
        bool isFloored() const;

        ext::shared_ptr<AverageONIndexedCoupon> underlying() const;
        bool nakedOption() const;
        bool localCapFloor() const;
        bool includeSpread() const;
    
};

%shared_ptr(CapFlooredAverageONIndexedCouponPricer)
class CapFlooredAverageONIndexedCouponPricer : public Floating RateCoupon {
    public:
        CapFlooredAverageONIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
        Handle<OptionletVolatilityStructure> capletVolatility() const;
};

%shared_ptr(AverageONLeg)
class AverageONLeg {
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
    AverageONLeg& withLastRecentPeriod(const boost::optional<Period>& lastRecentPeriod);
    AverageONLeg& withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar);
    AverageONLeg& withAverageONIndexedCouponPricer(const ext::shared_ptr<AverageONIndexedCouponPricer>& couponPricer);
    AverageONLeg& withCapFlooredAverageONIndexedCouponPricer(
        const ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer>& couponPricer);
    operator Leg() const;
};