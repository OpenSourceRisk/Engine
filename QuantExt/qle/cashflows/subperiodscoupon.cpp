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

#include <ql/indexes/interestrateindex.hpp>
#include <ql/time/schedule.hpp>
#include <ql/utilities/vectors.hpp>

#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

QLESubPeriodsCoupon::QLESubPeriodsCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                                   const boost::shared_ptr<InterestRateIndex>& index, Type type,
                                   BusinessDayConvention convention, Spread spread, const DayCounter& dayCounter,
                                   bool includeSpread, Real gearing)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, index->fixingDays(), index, gearing, spread, Date(),
                         Date(), dayCounter, false),
      type_(type), includeSpread_(includeSpread) {

    // Populate the value dates.
    Schedule sch = MakeSchedule()
                       .from(startDate)
                       .to(endDate)
                       .withTenor(index->tenor())
                       .withCalendar(index->fixingCalendar())
                       .withConvention(convention)
                       .withTerminationDateConvention(convention)
                       .backwards();
    valueDates_ = sch.dates();
    QL_ENSURE(valueDates_.size() >= 2, "Degenerate schedule.");

    // Populate the fixing dates.
    numPeriods_ = valueDates_.size() - 1;
    if (index->fixingDays() == 0) {
        fixingDates_ = std::vector<Date>(valueDates_.begin(), valueDates_.end() - 1);
    } else {
        fixingDates_.resize(numPeriods_);
        for (Size i = 0; i < numPeriods_; ++i)
            fixingDates_[i] = index->fixingDate(valueDates_[i]);
    }

    // Populate the accrual periods.
    accrualFractions_.resize(numPeriods_);
    for (Size i = 0; i < numPeriods_; ++i) {
        accrualFractions_[i] = dayCounter.yearFraction(valueDates_[i], valueDates_[i + 1]);
    }
}

const std::vector<Rate>& QLESubPeriodsCoupon::indexFixings() const {

    fixings_.resize(numPeriods_);

    for (Size i = 0; i < numPeriods_; ++i) {
        fixings_[i] = index_->fixing(fixingDates_[i]);
    }

    return fixings_;
}

void QLESubPeriodsCoupon::accept(AcyclicVisitor& v) {
    Visitor<QLESubPeriodsCoupon>* v1 = dynamic_cast<Visitor<QLESubPeriodsCoupon>*>(&v);
    if (v1 != 0) {
        v1->visit(*this);
    } else {
        FloatingRateCoupon::accept(v);
    }
}

QLESubPeriodsLeg::QLESubPeriodsLeg(const Schedule& schedule, const boost::shared_ptr<InterestRateIndex>& index)
    : schedule_(schedule), index_(index), notionals_(std::vector<Real>(1, 1.0)), paymentAdjustment_(Following),
      paymentCalendar_(Calendar()), type_(QLESubPeriodsCoupon::Compounding) {}

QLESubPeriodsLeg& QLESubPeriodsLeg::withNotional(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withGearing(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withSpread(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withPaymentCalendar(const Calendar& calendar) {
    paymentCalendar_ = calendar;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::withType(QLESubPeriodsCoupon::Type type) {
    type_ = type;
    return *this;
}

QLESubPeriodsLeg& QLESubPeriodsLeg::includeSpread(bool includeSpread) {
    includeSpread_ = includeSpread;
    return *this;
}

QLESubPeriodsLeg::operator Leg() const {

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
    if (numPeriods == 0)
        return cashflows;

    startDate = schedule_.date(0);
    for (Size i = 0; i < numPeriods; ++i) {
        endDate = schedule_.date(i + 1);
        paymentDate = calendar.adjust(endDate, paymentAdjustment_);
        // the sub periods coupon might produce degenerated schedules, in this
        // case we just join the current period with the next one
        // we catch all QL exceptions here, although we should only pick the one
        // that is thrown in case of a degenerated schedule, but there is no way
        // of identifying it except parsing the exception text, which isn't a
        // clean solution either
        try {
            boost::shared_ptr<QLESubPeriodsCoupon> cashflow(
                new QLESubPeriodsCoupon(paymentDate, detail::get(notionals_, i, notionals_.back()), startDate, endDate,
                                     index_, type_, paymentAdjustment_, detail::get(spreads_, i, 0.0),
                                     paymentDayCounter_, includeSpread_, detail::get(gearings_, i, 1.0)));

            cashflows.push_back(cashflow);
            startDate = endDate;
        } catch (const QuantLib::Error&) {
        }
    }

    boost::shared_ptr<QLESubPeriodsCouponPricer> pricer(new QLESubPeriodsCouponPricer);
    QuantExt::setCouponPricer(cashflows, pricer);

    return cashflows;
}
} // namespace QuantExt
