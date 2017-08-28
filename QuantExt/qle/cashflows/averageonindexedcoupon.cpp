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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

AverageONIndexedCoupon::AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate,
                                               const boost::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
                                               Spread spread, Natural rateCutoff, const DayCounter& dayCounter)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, overnightIndex->fixingDays(), overnightIndex,
                         gearing, spread, Date(), Date(), dayCounter, false),
      rateCutoff_(rateCutoff) {

    // Populate the value dates.
    Schedule sch = MakeSchedule()
                       .from(startDate)
                       .to(endDate)
                       .withTenor(1 * Days)
                       .withCalendar(overnightIndex->fixingCalendar())
                       .withConvention(overnightIndex->businessDayConvention())
                       .backwards();
    valueDates_ = sch.dates();
    QL_ENSURE(valueDates_.size() - rateCutoff_ >= 2, "degenerate schedule");

    // Populate the fixing dates.
    numPeriods_ = valueDates_.size() - 1;
    if (overnightIndex->fixingDays() == 0) {
        fixingDates_ = std::vector<Date>(valueDates_.begin(), valueDates_.end() - 1);
    } else {
        fixingDates_.resize(numPeriods_);
        for (Size i = 0; i < numPeriods_; ++i)
            fixingDates_[i] = overnightIndex->fixingDate(valueDates_[i]);
    }

    // Populate the accrual periods.
    dt_.resize(numPeriods_);
    for (Size i = 0; i < numPeriods_; ++i)
        dt_[i] = dayCounter.yearFraction(valueDates_[i], valueDates_[i + 1]);
}

const std::vector<Rate>& AverageONIndexedCoupon::indexFixings() const {

    fixings_.resize(numPeriods_);
    Size i;

    for (i = 0; i < numPeriods_ - rateCutoff_; ++i) {
        fixings_[i] = index_->fixing(fixingDates_[i]);
    }

    Rate cutoffFixing = fixings_[i - 1];
    while (i < numPeriods_) {
        fixings_[i] = cutoffFixing;
        i++;
    }

    return fixings_;
}

Date AverageONIndexedCoupon::fixingDate() const { return fixingDates_[fixingDates_.size() - 1 - rateCutoff_]; }

void AverageONIndexedCoupon::accept(AcyclicVisitor& v) {
    Visitor<AverageONIndexedCoupon>* v1 = dynamic_cast<Visitor<AverageONIndexedCoupon>*>(&v);
    if (v1 != 0) {
        v1->visit(*this);
    } else {
        FloatingRateCoupon::accept(v);
    }
}

AverageONLeg::AverageONLeg(const Schedule& schedule, const boost::shared_ptr<OvernightIndex>& i)
    : schedule_(schedule), overnightIndex_(i), paymentAdjustment_(Following), paymentCalendar_(Calendar()),
      rateCutoff_(0) {}

AverageONLeg& AverageONLeg::withNotional(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

AverageONLeg& AverageONLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

AverageONLeg& AverageONLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

AverageONLeg& AverageONLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

AverageONLeg& AverageONLeg::withGearing(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

AverageONLeg& AverageONLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

AverageONLeg& AverageONLeg::withSpread(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

AverageONLeg& AverageONLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

AverageONLeg& AverageONLeg::withRateCutoff(Natural rateCutoff) {
    rateCutoff_ = rateCutoff;
    return *this;
}

AverageONLeg& AverageONLeg::withPaymentCalendar(const Calendar& calendar) {
    paymentCalendar_ = calendar;
    return *this;
}

AverageONLeg&
AverageONLeg::withAverageONIndexedCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& couponPricer) {
    couponPricer_ = couponPricer;
    return *this;
}

AverageONLeg::operator Leg() const {

    QL_REQUIRE(!notionals_.empty(), "No notional given for average overnight leg.");

    Leg cashflows;
    Date startDate;
    Date endDate;
    Date paymentDate;

    Calendar calendar;
    if (!paymentCalendar_.empty()) {
        calendar = paymentCalendar_;
    } else {
        calendar = schedule_.calendar();
    }

    Size numPeriods = schedule_.size() - 1;
    for (Size i = 0; i < numPeriods; ++i) {
        startDate = schedule_.date(i);
        endDate = schedule_.date(i + 1);
        paymentDate = calendar.adjust(endDate, paymentAdjustment_);

        boost::shared_ptr<AverageONIndexedCoupon> cashflow(new AverageONIndexedCoupon(
            paymentDate, detail::get(notionals_, i, notionals_.back()), startDate, endDate, overnightIndex_,
            detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), rateCutoff_, paymentDayCounter_));

        if (couponPricer_) {
            cashflow->setPricer(couponPricer_);
        }

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
