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

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

AverageONIndexedCoupon::AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate,
                                               const boost::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
                                               Spread spread, Natural rateCutoff, const DayCounter& dayCounter,
                                               const Period& lookback, const Size fixingDays,
                                               const Date& rateComputationStartDate, const Date& rateComputationEndDate)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, overnightIndex, gearing, spread, Date(),
                         Date(), dayCounter, false),
      rateCutoff_(rateCutoff), lookback_(lookback), rateComputationStartDate_(rateComputationStartDate),
      rateComputationEndDate_(rateComputationEndDate) {

    Date valueStart = rateComputationStartDate_ == Null<Date>() ? startDate : rateComputationStartDate_;
    Date valueEnd = rateComputationEndDate_ == Null<Date>() ? endDate : rateComputationEndDate_;
    if (lookback != 0 * Days) {
        BusinessDayConvention bdc = lookback.length() > 0 ? Preceding : Following;
        valueStart = overnightIndex->fixingCalendar().advance(valueStart, -lookback, bdc);
        valueEnd = overnightIndex->fixingCalendar().advance(valueEnd, -lookback, bdc);
    }

    // Populate the value dates.
    Schedule sch = MakeSchedule()
                       .from(valueStart)
                       .to(valueEnd)
                       .withTenor(1 * Days)
                       .withCalendar(overnightIndex->fixingCalendar())
                       .withConvention(overnightIndex->businessDayConvention())
                       .backwards();
    valueDates_ = sch.dates();
    QL_ENSURE(valueDates_.size() >= 2 + rateCutoff_, "degenerate schedule");

    // Populate the fixing dates.
    numPeriods_ = valueDates_.size() - 1;
    if (FloatingRateCoupon::fixingDays() == 0) {
        fixingDates_ = std::vector<Date>(valueDates_.begin(), valueDates_.end() - 1);
    } else {
        fixingDates_.resize(numPeriods_);
        for (Size i = 0; i < numPeriods_; ++i)
            fixingDates_[i] = overnightIndex->fixingCalendar().advance(
                valueDates_[i], -static_cast<Integer>(FloatingRateCoupon::fixingDays()), Days, Preceding);
    }

    // Populate the accrual periods.
    dt_.resize(numPeriods_);
    for (Size i = 0; i < numPeriods_; ++i)
        dt_[i] = dayCounter.yearFraction(valueDates_[i], valueDates_[i + 1]);

    // check that rate cutoff is < number of fixing dates
    QL_REQUIRE(rateCutoff_ < numPeriods_, "rate cutoff (" << rateCutoff_
                                                          << ") must be less than number of fixings in period ("
                                                          << numPeriods_ << ")");
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
    : schedule_(schedule), overnightIndex_(i), paymentAdjustment_(Following), paymentLag_(0),
      paymentCalendar_(Calendar()), rateCutoff_(0), lookback_(0 * Days), fixingDays_(Null<Size>()), inArrears_(true) {}

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

AverageONLeg& AverageONLeg::withPaymentLag(Natural lag) {
    paymentLag_ = lag;
    return *this;
}

AverageONLeg& AverageONLeg::withLookback(const Period& lookback) {
    lookback_ = lookback;
    return *this;
}

AverageONLeg& AverageONLeg::withFixingDays(const Size fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

AverageONLeg& AverageONLeg::withInArrears(const bool inArrears) {
    inArrears_ = inArrears;
    return *this;
}

AverageONLeg& AverageONLeg::withLastRecentPeriod(const boost::optional<Period>& lastRecentPeriod) {
    lastRecentPeriod_ = lastRecentPeriod;
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

    Calendar calendar = schedule_.calendar();

    Date refStart, start, refEnd, end;
    Date paymentDate;

    Size n = schedule_.size() - 1;
    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);
        paymentDate = paymentCalendar_.advance(end, paymentLag_, Days, paymentAdjustment_);

        // determine refStart and refEnd

        if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1))
            refStart = calendar.adjust(end - schedule_.tenor(), paymentAdjustment_);
        if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1))
            refEnd = calendar.adjust(start + schedule_.tenor(), paymentAdjustment_);

        // Determine the rate computation start and end date as
        // - the coupon start and end date, if in arrears, and
        // - the previous coupon start and end date, if in advance.
        // In addition, adjust the start date, if a last recent period is given.

        Date rateComputationStartDate, rateComputationEndDate;
        if (!inArrears_) {
            // in arrears fixing (i.e. the "classic" case)
            rateComputationStartDate = start;
            rateComputationEndDate = end;
        } else {
            // handle in advance fixing
            if (i > 0) {
                // if there is a previous period, we take that
                rateComputationStartDate = schedule_.date(i - 1);
                rateComputationEndDate = schedule_.date(i);
            } else {
                // otherwise we construct the previous period
                rateComputationEndDate = start;
                if (schedule_.hasTenor())
                    rateComputationStartDate = calendar.adjust(start - schedule_.tenor(), Preceding);
                else
                    rateComputationStartDate = calendar.adjust(start - (end - start), Preceding);
            }
        }

        if (lastRecentPeriod_) {
            rateComputationStartDate = calendar.advance(rateComputationEndDate, -*lastRecentPeriod_);
        }

        // build coupon

        if (close_enough(detail::get(gearings_, i, 1.0), 0.0)) {
            // fixed coupon
            cashflows.push_back(boost::make_shared<FixedRateCoupon>(paymentDate, detail::get(notionals_, i, 1.0),
                                                                    detail::get(spreads_, i, 0.0), paymentDayCounter_,
                                                                    start, end, refStart, refEnd));
        } else {
            // floating coupon
            auto cpn = boost::make_shared<AverageONIndexedCoupon>(
                paymentDate, detail::get(notionals_, i, notionals_.back()), start, end, overnightIndex_,
                detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), rateCutoff_, paymentDayCounter_,
                lookback_, fixingDays_, rateComputationStartDate, rateComputationEndDate);
            if (couponPricer_) {
                cpn->setPricer(couponPricer_);
            }
            cashflows.push_back(cpn);
        }
    }
    return cashflows;
}

} // namespace QuantExt
