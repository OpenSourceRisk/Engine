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

#include <qle/cashflows/durationadjustedcmscoupon.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/cmscoupon.hpp>

namespace QuantExt {

DurationAdjustedCmsCoupon::DurationAdjustedCmsCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                                     const Date& endDate, Natural fixingDays,
                                                     const QuantLib::ext::shared_ptr<SwapIndex>& swapIndex, Size duration,
                                                     Real gearing, Spread spread, const Date& refPeriodStart,
                                                     const Date& refPeriodEnd, const DayCounter& dayCounter,
                                                     bool isInArrears, const Date& exCouponDate)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, swapIndex, gearing, spread,
                         refPeriodStart, refPeriodEnd, dayCounter, isInArrears, exCouponDate),
      swapIndex_(swapIndex), duration_(duration) {}

Size DurationAdjustedCmsCoupon::duration() const { return duration_; }
Real DurationAdjustedCmsCoupon::durationAdjustment() const {
    if (duration_ == 0) {
        return 1.0;
    } else {
        Real swapRate = swapIndex_->fixing(fixingDate());
        Real tmp = 0.0;
        for (Size i = 0; i < duration_; ++i) {
            tmp += 1.0 / std::pow(1.0 + swapRate, static_cast<Real>(i + 1));
        }
        return tmp;
    }
}

Rate DurationAdjustedCmsCoupon::indexFixing() const { return FloatingRateCoupon::indexFixing() * durationAdjustment(); }

void DurationAdjustedCmsCoupon::accept(AcyclicVisitor& v) {
    Visitor<DurationAdjustedCmsCoupon>* v1 = dynamic_cast<Visitor<DurationAdjustedCmsCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

DurationAdjustedCmsLeg::DurationAdjustedCmsLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<SwapIndex>& swapIndex,
                                               const Size duration)
    : schedule_(schedule), swapIndex_(swapIndex), paymentLag_(0), paymentCalendar_(Calendar()),
      paymentAdjustment_(Following), inArrears_(false), zeroPayments_(false), exCouponPeriod_(Period()),
      exCouponCalendar_(Calendar()), exCouponAdjustment_(Unadjusted), exCouponEndOfMonth_(false), duration_(duration) {}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withPaymentLag(Natural lag) {
    paymentLag_ = lag;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withPaymentCalendar(const Calendar& cal) {
    paymentCalendar_ = cal;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withGearings(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withSpreads(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::inArrears(bool flag) {
    inArrears_ = flag;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withZeroPayments(bool flag) {
    zeroPayments_ = flag;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withDuration(Size duration) {
    duration_ = duration;
    return *this;
}

DurationAdjustedCmsLeg& DurationAdjustedCmsLeg::withExCouponPeriod(const Period& period, const Calendar& cal,
                                                                   BusinessDayConvention convention, bool endOfMonth) {
    exCouponPeriod_ = period;
    exCouponCalendar_ = cal;
    exCouponAdjustment_ = convention;
    exCouponEndOfMonth_ = endOfMonth;
    return *this;
}

DurationAdjustedCmsLeg::operator Leg() const {
    Size n = schedule_.size() - 1;

    QL_REQUIRE(!notionals_.empty(), "no notional given");
    QL_REQUIRE(notionals_.size() <= n, "too many notionals (" << notionals_.size() << "), only " << n << " required");
    QL_REQUIRE(gearings_.size() <= n, "too many gearings (" << gearings_.size() << "), only " << n << " required");
    QL_REQUIRE(spreads_.size() <= n, "too many spreads (" << spreads_.size() << "), only " << n << " required");
    QL_REQUIRE(caps_.size() <= n, "too many caps (" << caps_.size() << "), only " << n << " required");
    QL_REQUIRE(floors_.size() <= n, "too many floors (" << floors_.size() << "), only " << n << " required");
    QL_REQUIRE(!zeroPayments_ || !inArrears_, "in-arrears and zero features are not compatible");

    Leg leg;
    leg.reserve(n);

    Calendar calendar = schedule_.calendar();
    Calendar paymentCalendar = schedule_.calendar();

    if (!paymentCalendar_.empty()) {
        paymentCalendar = paymentCalendar_;
    }

    Date refStart, start, refEnd, end;
    Date exCouponDate;
    Date lastPaymentDate = paymentCalendar.advance(schedule_.date(n), paymentLag_, Days, paymentAdjustment_);

    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);
        Date paymentDate =
            zeroPayments_ ? lastPaymentDate : paymentCalendar.advance(end, paymentLag_, Days, paymentAdjustment_);
        if (i == 0 && (schedule_.hasIsRegular() && schedule_.hasTenor() && !schedule_.isRegular(i + 1))) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refStart = calendar.adjust(end - schedule_.tenor(), bdc);
        }
        if (i == n - 1 && (schedule_.hasIsRegular() && schedule_.hasTenor() && !schedule_.isRegular(i + 1))) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refEnd = calendar.adjust(start + schedule_.tenor(), bdc);
        }
        if (exCouponPeriod_ != Period()) {
            Calendar exCouponCalendar = calendar;
            if (!exCouponCalendar_.empty()) {
                exCouponCalendar = exCouponCalendar_;
            }
            exCouponDate =
                exCouponCalendar_.advance(paymentDate, -exCouponPeriod_, exCouponAdjustment_, exCouponEndOfMonth_);
        }
        auto cpn = QuantLib::ext::make_shared<DurationAdjustedCmsCoupon>(
            paymentDate, detail::get(notionals_, i, 1.0), start, end,
            detail::get(fixingDays_, i, swapIndex_->fixingDays()), swapIndex_, duration_,
            detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), refStart, refEnd, paymentDayCounter_,
            inArrears_, exCouponDate);
        if (detail::noOption(caps_, floors_, i)) {
            leg.push_back(cpn);
        } else {
            leg.push_back(QuantLib::ext::make_shared<CappedFlooredCoupon>(cpn, detail::get(caps_, i, Null<Rate>()),
                                                                  detail::get(floors_, i, Null<Rate>())));
        }
    }
    return leg;
}

} // namespace QuantExt
