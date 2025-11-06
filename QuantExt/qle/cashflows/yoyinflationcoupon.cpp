/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/yoyinflationcoupon.hpp>

namespace QuantExt {

YoYInflationCoupon::YoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                       const Date& endDate, Natural fixingDays,
                                       const ext::shared_ptr<YoYInflationIndex>& index, const Period& observationLag,
                                       CPI::InterpolationType interpolation, const DayCounter& dayCounter,
                                       Real gearing, Spread spread, const Date& refPeriodStart,
                                       const Date& refPeriodEnd, bool addInflationNotional)
    : QuantLib::YoYInflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, observationLag,
                                   interpolation, dayCounter, gearing, spread, refPeriodStart, refPeriodEnd),
      addInflationNotional_(addInflationNotional) {}

YoYInflationCoupon::YoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                       const Date& endDate, Natural fixingDays,
                                       const ext::shared_ptr<YoYInflationIndex>& index, const Period& observationLag,
                                       const DayCounter& dayCounter, Real gearing, Spread spread,
                                       const Date& refPeriodStart, const Date& refPeriodEnd, bool addInflationNotional)
    : YoYInflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, observationLag, CPI::AsIndex,
                                   dayCounter, gearing, spread, refPeriodStart, refPeriodEnd, addInflationNotional) {}

void YoYInflationCoupon::accept(AcyclicVisitor& v) {
    Visitor<YoYInflationCoupon>* v1 = dynamic_cast<Visitor<YoYInflationCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        QuantLib::YoYInflationCoupon::accept(v);
}

Rate YoYInflationCoupon::rate() const {
    Real RateYoY = QuantLib::YoYInflationCoupon::rate();
    if (addInflationNotional_) {
        RateYoY = gearing_ * ((RateYoY - spread_) / gearing_ + 1) + spread_;
    }
    return RateYoY;
}

CappedFlooredYoYInflationCoupon::CappedFlooredYoYInflationCoupon(
    const ext::shared_ptr<QuantLib::YoYInflationCoupon>& underlying, Rate cap, Rate floor, bool addInflationNotional)
    : QuantLib::CappedFlooredYoYInflationCoupon(underlying, cap, floor), addInflationNotional_(addInflationNotional) {
    if (addInflationNotional_) {
        if (isCapped_) {
            cap_ = cap_ - 1;
        }
        if (isFloored_) {
            floor_ = floor_ - 1;
        }
    }
}

CappedFlooredYoYInflationCoupon::CappedFlooredYoYInflationCoupon(
    const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
    const ext::shared_ptr<YoYInflationIndex>& index, const Period& observationLag, CPI::InterpolationType interpolation,
    const DayCounter& dayCounter, Real gearing, Spread spread, const Rate cap, const Rate floor,
    const Date& refPeriodStart, const Date& refPeriodEnd, bool addInflationNotional)
    : QuantLib::CappedFlooredYoYInflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index,
                                                observationLag, interpolation, dayCounter, gearing, spread, cap, floor,
                                                refPeriodStart, refPeriodEnd),
      addInflationNotional_(addInflationNotional) {
    if (addInflationNotional_) {
        if (isCapped_) {
            cap_ = cap_ - 1;
        }
        if (isFloored_) {
            floor_ = floor_ - 1;
        }
    }
}

CappedFlooredYoYInflationCoupon::CappedFlooredYoYInflationCoupon(
    const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
    const ext::shared_ptr<YoYInflationIndex>& index, const Period& observationLag,
    const DayCounter& dayCounter, Real gearing, Spread spread, const Rate cap, const Rate floor,
    const Date& refPeriodStart, const Date& refPeriodEnd, bool addInflationNotional)
    : CappedFlooredYoYInflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, observationLag,
                                      CPI::AsIndex, dayCounter, gearing, spread, cap, floor, refPeriodStart,
                                      refPeriodEnd) {}

void CappedFlooredYoYInflationCoupon::accept(AcyclicVisitor& v) {
    Visitor<CappedFlooredYoYInflationCoupon>* v1 = dynamic_cast<Visitor<CappedFlooredYoYInflationCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        QuantLib::CappedFlooredYoYInflationCoupon::accept(v);
}

Rate CappedFlooredYoYInflationCoupon::rate() const {
    Real RateYoY = QuantLib::CappedFlooredYoYInflationCoupon::rate();
    if (addInflationNotional_) {
        RateYoY = gearing_ * ((RateYoY - spread_) / gearing_ + 1) + spread_;
    }
    return RateYoY;
}

yoyInflationLeg::yoyInflationLeg(Schedule schedule, Calendar paymentCalendar, ext::shared_ptr<YoYInflationIndex> index,
                                 const Period& observationLag, CPI::InterpolationType interpolation)
    : schedule_(std::move(schedule)), index_(std::move(index)), observationLag_(observationLag),
      interpolation_(interpolation), paymentAdjustment_(ModifiedFollowing),
      paymentCalendar_(std::move(paymentCalendar)), addInflationNotional_(false) {}

yoyInflationLeg::yoyInflationLeg(Schedule schedule, Calendar paymentCalendar, ext::shared_ptr<YoYInflationIndex> index,
                                 const Period& observationLag)
    : yoyInflationLeg(schedule, paymentCalendar, index, observationLag, CPI::AsIndex) {}

yoyInflationLeg& yoyInflationLeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withGearings(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withSpreads(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withRateCurve(const Handle<YieldTermStructure>& rateCurve) {
    rateCurve_ = rateCurve;
    return *this;
}

yoyInflationLeg& yoyInflationLeg::withInflationNotional(bool addInflationNotional) {
    addInflationNotional_ = addInflationNotional;
    return *this;
}

yoyInflationLeg::operator Leg() const {

    Size n = schedule_.size() - 1;
    QL_REQUIRE(!notionals_.empty(), "no notional given");
    QL_REQUIRE(notionals_.size() <= n, "too many nominals (" << notionals_.size() << "), only " << n << " required");
    QL_REQUIRE(gearings_.size() <= n, "too many gearings (" << gearings_.size() << "), only " << n << " required");
    QL_REQUIRE(spreads_.size() <= n, "too many spreads (" << spreads_.size() << "), only " << n << " required");
    QL_REQUIRE(caps_.size() <= n, "too many caps (" << caps_.size() << "), only " << n << " required");
    QL_REQUIRE(floors_.size() <= n, "too many floors (" << floors_.size() << "), only " << n << " required");

    Leg leg;
    leg.reserve(n);

    Calendar calendar = paymentCalendar_;

    Date refStart, start, refEnd, end;

    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);
        Date paymentDate = calendar.adjust(end, paymentAdjustment_);
        if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refStart = schedule_.calendar().adjust(end - schedule_.tenor(), bdc);
        }
        if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refEnd = schedule_.calendar().adjust(start + schedule_.tenor(), bdc);
        }
        if (detail::get(gearings_, i, 1.0) == 0.0) { // fixed coupon
            leg.push_back(ext::shared_ptr<CashFlow>(new FixedRateCoupon(
                paymentDate, detail::get(notionals_, i, 1.0), detail::effectiveFixedRate(spreads_, caps_, floors_, i),
                paymentDayCounter_, start, end, refStart, refEnd)));
        } else {                                       // yoy inflation coupon
            if (detail::noOption(caps_, floors_, i)) { // just swaplet
                ext::shared_ptr<YoYInflationCoupon> coup(new YoYInflationCoupon(
                    paymentDate, detail::get(notionals_, i, 1.0), start, end, detail::get(fixingDays_, i, 0), index_,
                    observationLag_, interpolation_, paymentDayCounter_, detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0),
                    refStart, refEnd, addInflationNotional_));

                // in this case you can set a pricer
                // straight away because it only provides computation - not data
                ext::shared_ptr<YoYInflationCouponPricer> pricer =
                    ext::make_shared<YoYInflationCouponPricer>(rateCurve_);
                coup->setPricer(pricer);
                leg.push_back(ext::dynamic_pointer_cast<CashFlow>(coup));

            } else { // cap/floorlet
                leg.push_back(ext::shared_ptr<CashFlow>(new CappedFlooredYoYInflationCoupon(
                    paymentDate, detail::get(notionals_, i, 1.0), start, end, detail::get(fixingDays_, i, 0), index_,
                    observationLag_, interpolation_, paymentDayCounter_, detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0),
                    detail::get(caps_, i, Null<Rate>()), detail::get(floors_, i, Null<Rate>()), refStart, refEnd,
                    addInflationNotional_)));
            }
        }
    }

    return leg;
}

} // namespace QuantExt
