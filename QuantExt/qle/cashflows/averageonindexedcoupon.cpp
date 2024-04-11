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
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

// average on indexed coupon implementation

AverageONIndexedCoupon::AverageONIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate,
                                               const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
                                               Spread spread, Natural rateCutoff, const DayCounter& dayCounter,
                                               const Period& lookback, const Size fixingDays,
                                               const Date& rateComputationStartDate, const Date& rateComputationEndDate,
                                               const bool telescopicValueDates)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, overnightIndex, gearing, spread, Date(),
                         Date(), dayCounter, false),
      overnightIndex_(overnightIndex), rateCutoff_(rateCutoff), lookback_(lookback),
      rateComputationStartDate_(rateComputationStartDate), rateComputationEndDate_(rateComputationEndDate) {

    Date valueStart = rateComputationStartDate_ == Null<Date>() ? startDate : rateComputationStartDate_;
    Date valueEnd = rateComputationEndDate_ == Null<Date>() ? endDate : rateComputationEndDate_;
    if (lookback != 0 * Days) {
        BusinessDayConvention bdc = lookback.length() > 0 ? Preceding : Following;
        valueStart = overnightIndex->fixingCalendar().advance(valueStart, -lookback, bdc);
        valueEnd = overnightIndex->fixingCalendar().advance(valueEnd, -lookback, bdc);
    }

    // Populate the value dates.
    Date tmpEndDate = valueEnd;
    if(telescopicValueDates) {
	// same optimization as in OvernightIndexedCoupon
        Date evalDate = Settings::instance().evaluationDate();
        tmpEndDate = overnightIndex->fixingCalendar().advance(std::max(valueStart, evalDate), 7, Days, Following);
        tmpEndDate = std::min(tmpEndDate, valueEnd);
    }

    Schedule sch = MakeSchedule()
                       .from(valueStart)
                       .to(tmpEndDate)
                       .withTenor(1 * Days)
                       .withCalendar(overnightIndex->fixingCalendar())
                       .withConvention(overnightIndex->businessDayConvention())
                       .backwards();
    valueDates_ = sch.dates();

    if (telescopicValueDates) {
        // build optimised value dates schedule: back stub
        // contains at least two dates and enough periods to cover rate cutoff
        Date tmp2 = overnightIndex->fixingCalendar().adjust(valueEnd, overnightIndex->businessDayConvention());
        Date tmp1 = overnightIndex->fixingCalendar().advance(tmp2, -std::max<Size>(rateCutoff_, 1), Days, Preceding);
        while (tmp1 <= tmp2) {
            if (tmp1 > valueDates_.back())
                valueDates_.push_back(tmp1);
            tmp1 = overnightIndex->fixingCalendar().advance(tmp1, 1, Days, Following);
        }
    }

    QL_ENSURE(valueDates_.size() >= 2 + rateCutoff_, "degenerate schedule");

    // the first and last value date should be the unadjusted input value dates
    if (valueDates_.front() != valueStart)
        valueDates_.front() = valueStart;
    if (valueDates_.back() != valueEnd)
        valueDates_.back() = valueEnd;

    numPeriods_ = valueDates_.size() - 1;

    QL_REQUIRE(valueDates_[0] != valueDates_[1],
               "internal error: first two value dates of on coupon are equal: " << valueDates_[0]);
    QL_REQUIRE(valueDates_[numPeriods_] != valueDates_[numPeriods_ - 1],
               "internal error: last two value dates of on coupon are equal: " << valueDates_[numPeriods_]);

    // Populate the fixing dates.
    fixingDates_.resize(numPeriods_);
    for (Size i = 0; i < numPeriods_; ++i)
        fixingDates_[i] = overnightIndex->fixingCalendar().advance(
            valueDates_[i], -static_cast<Integer>(FloatingRateCoupon::fixingDays()), Days, Preceding);

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

// capped floored average on coupon implementation

void CappedFlooredAverageONIndexedCoupon::alwaysForwardNotifications() {
    LazyObject::alwaysForwardNotifications();
    underlying_->alwaysForwardNotifications();
}

void CappedFlooredAverageONIndexedCoupon::deepUpdate() {
    update();
    underlying_->deepUpdate();
}

void CappedFlooredAverageONIndexedCoupon::performCalculations() const {
    QL_REQUIRE(underlying_->pricer(), "pricer not set");
    Rate swapletRate = nakedOption_ ? 0.0 : underlying_->rate();
    if (floor_ != Null<Real>() || cap_ != Null<Real>())
        pricer()->initialize(*this);
    Rate floorletRate = 0.;
    if (floor_ != Null<Real>())
        floorletRate = pricer()->floorletRate(effectiveFloor());
    Rate capletRate = 0.;
    if (cap_ != Null<Real>())
        capletRate = (nakedOption_ && floor_ == Null<Real>() ? -1.0 : 1.0) * pricer()->capletRate(effectiveCap());
    rate_ = swapletRate + floorletRate - capletRate;
    auto p = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageONIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "CapFlooredAverageONIndexedCoupon::performCalculations(): internal error, could not cast to "
                  "CapFlooredAverageONIndexedCouponPricer");
    effectiveCapletVolatility_ = p->effectiveCapletVolatility();
    effectiveFloorletVolatility_ = p->effectiveFloorletVolatility();
}

Rate CappedFlooredAverageONIndexedCoupon::cap() const { return gearing_ > 0.0 ? cap_ : floor_; }

Rate CappedFlooredAverageONIndexedCoupon::floor() const { return gearing_ > 0.0 ? floor_ : cap_; }

Rate CappedFlooredAverageONIndexedCoupon::rate() const {
    calculate();
    return rate_;
}

Rate CappedFlooredAverageONIndexedCoupon::convexityAdjustment() const { return underlying_->convexityAdjustment(); }

Rate CappedFlooredAverageONIndexedCoupon::effectiveCap() const {
    if (cap_ == Null<Real>())
        return Null<Real>();
    /* We have four cases dependent on localCapFloor_ and includeSpread. Notation in the formulas:
       g         gearing,
       s         spread,
       A         coupon amount,
       f_i       daily fixings,
       \tau_i    daily accrual fractions,
       \tau      coupon accrual fraction,
       C         cap rate
       F         floor rate
    */
    if (localCapFloor_) {
        if (includeSpread()) {
            // A = \cdot \frac{\sum (\tau_i \min ( \max ( f_i + s , F), C))}{\tau}
            return cap_ - underlying_->spread();
        } else {
            // A = g \cdot \frac{\sum (\tau_i \min ( \max ( f_i , F), C))}{\tau} + s
            return cap_;
        }
    } else {
        if (includeSpread()) {
            // A = \min \left( \max \left( \frac{\sum (\tau_i f_i)}{\tau} + s, F \right), C \right)
            return (cap_ / gearing() - underlying_->spread());
        } else {
            // A = \min \left( \max \left( g \cdot \frac{\sum (\tau_i f_i)}{\tau} + s, F \right), C \right)
            return (cap_ - underlying_->spread()) / gearing();
        }
    }
}

Rate CappedFlooredAverageONIndexedCoupon::effectiveFloor() const {
    if (floor_ == Null<Real>())
        return Null<Real>();
    if (localCapFloor_) {
        if (includeSpread()) {
            return floor_ - underlying_->spread();
        } else {
            return floor_;
        }
    } else {
        if (includeSpread()) {
            return (floor_ - underlying_->spread());
        } else {
            return (floor_ - underlying_->spread()) / gearing();
        }
    }
}

Real CappedFlooredAverageONIndexedCoupon::effectiveCapletVolatility() const {
    calculate();
    return effectiveCapletVolatility_;
}

Real CappedFlooredAverageONIndexedCoupon::effectiveFloorletVolatility() const {
    calculate();
    return effectiveFloorletVolatility_;
}

void CappedFlooredAverageONIndexedCoupon::accept(AcyclicVisitor& v) {
    Visitor<CappedFlooredAverageONIndexedCoupon>* v1 = dynamic_cast<Visitor<CappedFlooredAverageONIndexedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

CappedFlooredAverageONIndexedCoupon::CappedFlooredAverageONIndexedCoupon(
    const ext::shared_ptr<AverageONIndexedCoupon>& underlying, Real cap, Real floor, bool nakedOption,
    bool localCapFloor, bool includeSpread)
    : FloatingRateCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                         underlying->accrualEndDate(), underlying->fixingDays(), underlying->index(),
                         underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                         underlying->referencePeriodEnd(), underlying->dayCounter(), false),
      underlying_(underlying), cap_(cap), floor_(floor), nakedOption_(nakedOption), localCapFloor_(localCapFloor),
      includeSpread_(includeSpread) {
    QL_REQUIRE(!includeSpread_ || close_enough(underlying_->gearing(), 1.0),
               "CappedFlooredAverageONIndexedCoupon: if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");
    registerWith(underlying_);
    if (nakedOption_)
        underlying_->alwaysForwardNotifications();
}

// capped floored average on coupon pricer base class implementation

CapFlooredAverageONIndexedCouponPricer::CapFlooredAverageONIndexedCouponPricer(
    const Handle<OptionletVolatilityStructure>& v, const bool effectiveVolatilityInput)
    : capletVol_(v), effectiveVolatilityInput_(effectiveVolatilityInput) {
    registerWith(capletVol_);
}

bool CapFlooredAverageONIndexedCouponPricer::effectiveVolatilityInput() const { return effectiveVolatilityInput_; }

Real CapFlooredAverageONIndexedCouponPricer::effectiveCapletVolatility() const { return effectiveCapletVolatility_; }

Real CapFlooredAverageONIndexedCouponPricer::effectiveFloorletVolatility() const {
    return effectiveFloorletVolatility_;
}

Handle<OptionletVolatilityStructure> CapFlooredAverageONIndexedCouponPricer::capletVolatility() const {
    return capletVol_;
}

// average on leg implementation

AverageONLeg::AverageONLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<OvernightIndex>& i)
    : schedule_(schedule), overnightIndex_(i), paymentAdjustment_(Following), paymentLag_(0),
      telescopicValueDates_(false), paymentCalendar_(schedule.calendar()), rateCutoff_(0), lookback_(0 * Days),
      fixingDays_(Null<Size>()), includeSpread_(false), nakedOption_(false), localCapFloor_(false), inArrears_(true) {}

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

AverageONLeg& AverageONLeg::withTelescopicValueDates(bool telescopicValueDates) {
    telescopicValueDates_ = telescopicValueDates;
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

AverageONLeg& AverageONLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

AverageONLeg& AverageONLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

AverageONLeg& AverageONLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

AverageONLeg& AverageONLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

AverageONLeg& AverageONLeg::includeSpreadInCapFloors(bool includeSpread) {
    includeSpread_ = includeSpread;
    return *this;
}

AverageONLeg& AverageONLeg::withNakedOption(const bool nakedOption) {
    nakedOption_ = nakedOption;
    return *this;
}

AverageONLeg& AverageONLeg::withLocalCapFloor(const bool localCapFloor) {
    localCapFloor_ = localCapFloor;
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

AverageONLeg& AverageONLeg::withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar) {
    lastRecentPeriodCalendar_ = lastRecentPeriodCalendar;
    return *this;
}

AverageONLeg& AverageONLeg::withPaymentDates(const std::vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

AverageONLeg&
AverageONLeg::withAverageONIndexedCouponPricer(const QuantLib::ext::shared_ptr<AverageONIndexedCouponPricer>& couponPricer) {
    couponPricer_ = couponPricer;
    return *this;
}

AverageONLeg& AverageONLeg::withCapFlooredAverageONIndexedCouponPricer(
    const QuantLib::ext::shared_ptr<CapFlooredAverageONIndexedCouponPricer>& couponPricer) {
    capFlooredCouponPricer_ = couponPricer;
    return *this;
}

AverageONLeg::operator Leg() const {

    QL_REQUIRE(!notionals_.empty(), "No notional given for average overnight leg.");

    Leg cashflows;

    Calendar calendar = schedule_.calendar();
    Calendar paymentCalendar = paymentCalendar_;

    if (calendar.empty())
        calendar = paymentCalendar;
    if (calendar.empty())
        calendar = WeekendsOnly();
    if (paymentCalendar.empty())
        paymentCalendar = calendar;

    Date refStart, start, refEnd, end;
    Date paymentDate;

    Size n = schedule_.size() - 1;

    // Initial consistency checks
    if (!paymentDates_.empty()) {
        QL_REQUIRE(paymentDates_.size() == n, "Expected the number of explicit payment dates ("
                                                  << paymentDates_.size()
                                                  << ") to equal the number of calculation periods ("
                                                  << n << ")");
    }

    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);

        // If explicit payment dates provided, use them.
        if (!paymentDates_.empty()) {
            paymentDate = paymentDates_[i];
        } else {
            paymentDate = paymentCalendar.advance(end, paymentLag_, Days, paymentAdjustment_);
        }

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
        if (inArrears_) {
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
                if (schedule_.hasTenor() && schedule_.tenor() != 0 * Days)
                    rateComputationStartDate = calendar.adjust(start - schedule_.tenor(), Preceding);
                else
                    rateComputationStartDate = calendar.adjust(start - (end - start), Preceding);
            }
        }

        if (lastRecentPeriod_) {
            rateComputationStartDate = (lastRecentPeriodCalendar_.empty() ? calendar : lastRecentPeriodCalendar_)
                                           .advance(rateComputationEndDate, -*lastRecentPeriod_);
        }

        // build coupon

        if (close_enough(detail::get(gearings_, i, 1.0), 0.0)) {
            // fixed coupon
            cashflows.push_back(QuantLib::ext::make_shared<FixedRateCoupon>(paymentDate, detail::get(notionals_, i, 1.0),
                                                                    detail::get(spreads_, i, 0.0), paymentDayCounter_,
                                                                    start, end, refStart, refEnd));
        } else {
            // floating coupon
            auto cpn = QuantLib::ext::make_shared<AverageONIndexedCoupon>(
                paymentDate, detail::get(notionals_, i, notionals_.back()), start, end, overnightIndex_,
                detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), rateCutoff_, paymentDayCounter_,
                lookback_, fixingDays_, rateComputationStartDate, rateComputationEndDate, telescopicValueDates_);
            if (couponPricer_) {
                cpn->setPricer(couponPricer_);
            }
            Real cap = detail::get(caps_, i, Null<Real>());
            Real floor = detail::get(floors_, i, Null<Real>());
            if (cap == Null<Real>() && floor == Null<Real>()) {
                cashflows.push_back(cpn);
            } else {
                auto cfCpn = ext::make_shared<CappedFlooredAverageONIndexedCoupon>(cpn, cap, floor, nakedOption_,
                                                                                   localCapFloor_, includeSpread_);
                if (capFlooredCouponPricer_) {
                    cfCpn->setPricer(capFlooredCouponPricer_);
                }
                cashflows.push_back(cfCpn);
            }
        }
    }
    return cashflows;
}

} // namespace QuantExt
