/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/cashflows/formulabasedcoupon.hpp>

#include <ql/cashflows/cashflowvectors.hpp>

using namespace QuantLib;

namespace QuantExt {

FormulaBasedCoupon::FormulaBasedCoupon(const Currency& paymentCurrency, const Date& paymentDate, Real nominal,
                                       const Date& startDate, const Date& endDate, Natural fixingDays,
                                       const QuantLib::ext::shared_ptr<FormulaBasedIndex>& index, const Date& refPeriodStart,
                                       const Date& refPeriodEnd, const DayCounter& dayCounter, bool isInArrears)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, 1.0, 0.0, refPeriodStart,
                         refPeriodEnd, dayCounter, isInArrears),
      paymentCurrency_(paymentCurrency), index_(index) {}

void FormulaBasedCoupon::accept(AcyclicVisitor& v) {
    Visitor<FormulaBasedCoupon>* v1 = dynamic_cast<Visitor<FormulaBasedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

FormulaBasedLeg::FormulaBasedLeg(const Currency& paymentCurrency, const Schedule& schedule,
                                 const QuantLib::ext::shared_ptr<FormulaBasedIndex>& index)
    : paymentCurrency_(paymentCurrency), schedule_(schedule), index_(index), paymentAdjustment_(Following),
      paymentLag_(0), inArrears_(false), zeroPayments_(false) {}

FormulaBasedLeg& FormulaBasedLeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withPaymentLag(Natural lag) {
    paymentLag_ = lag;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withPaymentCalendar(const Calendar& cal) {
    paymentCalendar_ = cal;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::inArrears(bool flag) {
    inArrears_ = flag;
    return *this;
}

FormulaBasedLeg& FormulaBasedLeg::withZeroPayments(bool flag) {
    zeroPayments_ = flag;
    return *this;
}

FormulaBasedLeg::operator Leg() const {

    // unfortunately we have to copy this from ql/cashflows/cashflowvectors and modify it to
    // match the formula based coupon ctor, which is a bit different from other flr coupons

    Size n = schedule_.size() - 1;
    QL_REQUIRE(!notionals_.empty(), "no notional given");
    QL_REQUIRE(notionals_.size() <= n, "too many nominals (" << notionals_.size() << "), only " << n << " required");
    QL_REQUIRE(!zeroPayments_ || !inArrears_, "in-arrears and zero features are not compatible");

    Leg leg;
    leg.reserve(n);

    Calendar calendar = schedule_.calendar().empty() ? NullCalendar() : schedule_.calendar();
    Calendar paymentCalendar = paymentCalendar_.empty() ? calendar : paymentCalendar_;

    Date refStart, start, refEnd, end;
    Date lastPaymentDate = paymentCalendar.advance(schedule_.date(n), paymentLag_, Days, paymentAdjustment_);

    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);
        Date paymentDate =
            zeroPayments_ ? lastPaymentDate : paymentCalendar.advance(end, paymentLag_, Days, paymentAdjustment_);
        if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refStart = calendar.adjust(end - schedule_.tenor(), bdc);
        }
        if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refEnd = calendar.adjust(start + schedule_.tenor(), bdc);
        }
        leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(
            new FormulaBasedCoupon(paymentCurrency_, paymentDate, detail::get(notionals_, i, 1.0), start, end,
                                   detail::get(fixingDays_, i, index_->fixingDays()), index_, refStart, refEnd,
                                   paymentDayCounter_, inArrears_)));
    }
    return leg;
}

} // namespace QuantExt
