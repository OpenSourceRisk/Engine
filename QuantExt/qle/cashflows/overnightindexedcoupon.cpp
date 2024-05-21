/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/utilities/vectors.hpp>

using std::vector;

namespace QuantExt {

// OvernightIndexedCoupon implementation

OvernightIndexedCoupon::OvernightIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate,
                                               const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
                                               Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd,
                                               const DayCounter& dayCounter, bool telescopicValueDates,
                                               bool includeSpread, const Period& lookback, const Natural rateCutoff,
                                               const Natural fixingDays, const Date& rateComputationStartDate,
                                               const Date& rateComputationEndDate)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, overnightIndex, gearing, spread,
                         refPeriodStart, refPeriodEnd, dayCounter, false),
      overnightIndex_(overnightIndex), includeSpread_(includeSpread), lookback_(lookback), rateCutoff_(rateCutoff),
      rateComputationStartDate_(rateComputationStartDate), rateComputationEndDate_(rateComputationEndDate) {

    Date valueStart = rateComputationStartDate_ == Null<Date>() ? startDate : rateComputationStartDate_;
    Date valueEnd = rateComputationEndDate_ == Null<Date>() ? endDate : rateComputationEndDate_;
    if (lookback != 0 * Days) {
        BusinessDayConvention bdc = lookback.length() > 0 ? Preceding : Following;
        valueStart = overnightIndex->fixingCalendar().advance(valueStart, -lookback, bdc);
        valueEnd = overnightIndex->fixingCalendar().advance(valueEnd, -lookback, bdc);
    }

    // value dates
    Date tmpEndDate = valueEnd;

    /* For the coupon's valuation only the first and last future valuation
       dates matter, therefore we can avoid to construct the whole series
       of valuation dates, a front and back stub will do. However notice
       that if the global evaluation date moves forward it might run past
       the front stub of valuation dates we build here (which incorporates
       a grace period of 7 business after the evaluation date). This will
       lead to false coupon projections (see the warning the class header). */

    if (telescopicValueDates) {
        // build optimised value dates schedule: front stub goes
        // from start date to max(evalDate,valueStart) + 7bd
        Date evalDate = Settings::instance().evaluationDate();
        tmpEndDate = overnightIndex->fixingCalendar().advance(std::max(valueStart, evalDate), 7, Days, Following);
        tmpEndDate = std::min(tmpEndDate, valueEnd);
    }
    Schedule sch = MakeSchedule()
                       .from(valueStart)
                       // .to(valueEnd)
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

    n_ = valueDates_.size() - 1;

    QL_REQUIRE(valueDates_[0] != valueDates_[1],
               "internal error: first two value dates of on coupon are equal: " << valueDates_[0]);
    QL_REQUIRE(valueDates_[n_] != valueDates_[n_ - 1],
               "internal error: last two value dates of on coupon are equal: " << valueDates_[n_]);

    // fixing dates
    fixingDates_.resize(n_);
    for (Size i = 0; i < n_; ++i)
        fixingDates_[i] = overnightIndex->fixingCalendar().advance(
            valueDates_[i], -static_cast<Integer>(FloatingRateCoupon::fixingDays()), Days, Preceding);

    // accrual (compounding) periods
    dt_.resize(n_);
    const DayCounter& dc = overnightIndex->dayCounter();
    for (Size i = 0; i < n_; ++i)
        dt_[i] = dc.yearFraction(valueDates_[i], valueDates_[i + 1]);

    setPricer(ext::shared_ptr<FloatingRateCouponPricer>(new OvernightIndexedCouponPricer));

    // check that rate cutoff is < number of fixing dates
    QL_REQUIRE(rateCutoff_ < n_,
               "rate cutoff (" << rateCutoff_ << ") must be less than number of fixings in period (" << n_ << ")");
}

const vector<Rate>& OvernightIndexedCoupon::indexFixings() const {
    fixings_.resize(n_);
    Size i;
    for (i = 0; i < n_ - rateCutoff_; ++i) {
        fixings_[i] = index_->fixing(fixingDates_[i]);
    }
    Rate cutoffFixing = fixings_[i - 1];
    while (i < n_) {
        fixings_[i] = cutoffFixing;
        i++;
    }
    return fixings_;
}

void OvernightIndexedCoupon::accept(AcyclicVisitor& v) {
    Visitor<OvernightIndexedCoupon>* v1 = dynamic_cast<Visitor<OvernightIndexedCoupon>*>(&v);
    if (v1 != 0) {
        v1->visit(*this);
    } else {
        FloatingRateCoupon::accept(v);
    }
}

Real OvernightIndexedCoupon::effectiveSpread() const {
    if (!includeSpread_)
        return spread();
    auto p = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "OvernightIndexedCoupon::effectiveSpread(): expected OvernightIndexedCouponPricer");
    p->initialize(*this);
    return p->effectiveSpread();
}

Real OvernightIndexedCoupon::effectiveIndexFixing() const {
    auto p = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "OvernightIndexedCoupon::effectiveSpread(): expected OvernightIndexedCouponPricer");
    p->initialize(*this);
    return p->effectiveIndexFixing();
}

// OvernightIndexedCouponPricer implementation

void OvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
    QL_ENSURE(coupon_, "wrong coupon type");
}

void OvernightIndexedCouponPricer::compute() const {
    ext::shared_ptr<OvernightIndex> index = ext::dynamic_pointer_cast<OvernightIndex>(coupon_->index());

    const vector<Date>& fixingDates = coupon_->fixingDates();
    const vector<Time>& dt = coupon_->dt();

    Size n = dt.size();
    Size i = 0;
    QL_REQUIRE(coupon_->rateCutoff() < n, "rate cutoff (" << coupon_->rateCutoff()
                                                          << ") must be less than number of fixings in period (" << n
                                                          << ")");
    Size nCutoff = n - coupon_->rateCutoff();

    Real compoundFactor = 1.0, compoundFactorWithoutSpread = 1.0;

    // already fixed part
    Date today = Settings::instance().evaluationDate();
    while (i < n && fixingDates[std::min(i, nCutoff)] < today) {
        // rate must have been fixed
        Rate pastFixing = index->pastFixing(fixingDates[std::min(i, nCutoff)]);
        QL_REQUIRE(pastFixing != Null<Real>(),
                   "Missing " << index->name() << " fixing for " << fixingDates[std::min(i, nCutoff)]);
        if (coupon_->includeSpread()) {
            compoundFactorWithoutSpread *= (1.0 + pastFixing * dt[i]);
            pastFixing += coupon_->spread();
        }
        compoundFactor *= (1.0 + pastFixing * dt[i]);
        ++i;
    }

    // today is a border case
    if (i < n && fixingDates[std::min(i, nCutoff)] == today) {
        // might have been fixed
        try {
            Rate pastFixing = index->pastFixing(fixingDates[std::min(i, nCutoff)]);
            if (pastFixing != Null<Real>()) {
                if (coupon_->includeSpread()) {
                    compoundFactorWithoutSpread *= (1.0 + pastFixing * dt[i]);
                    pastFixing += coupon_->spread();
                }
                compoundFactor *= (1.0 + pastFixing * dt[i]);
                ++i;
            } else {
                ; // fall through and forecast
            }
        } catch (Error&) {
            ; // fall through and forecast
        }
    }

    // forward part using telescopic property in order
    // to avoid the evaluation of multiple forward fixings
    const vector<Date>& dates = coupon_->valueDates();
    if (i < n) {
        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

        // handle the part until the rate cutoff (might be empty, i.e. startDiscount = endDiscount)
        DiscountFactor startDiscount = curve->discount(dates[i]);
        DiscountFactor endDiscount = curve->discount(dates[std::max(nCutoff, i)]);

        // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
        if (nCutoff < n) {
            // forward discount factor for one calendar day on the cutoff date
            DiscountFactor discountCutoffDate = curve->discount(dates[nCutoff] + 1) / curve->discount(dates[nCutoff]);
            // keep the above forward discount factor constant during the cutoff period
            endDiscount *= std::pow(discountCutoffDate, dates[n] - dates[nCutoff]);
        }

        compoundFactor *= startDiscount / endDiscount;

        if (coupon_->includeSpread()) {
            compoundFactorWithoutSpread *= startDiscount / endDiscount;
            // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE":
            // set tau to an average value
            Real tau = index->dayCounter().yearFraction(dates[i], dates.back()) / (dates.back() - dates[i]);
            // now use formula (4) from the paper
            compoundFactor *= std::pow(1.0 + tau * coupon_->spread(), static_cast<int>(dates.back() - dates[i]));
        }
    }

    Rate tau = index->dayCounter().yearFraction(dates.front(), dates.back());
    Rate rate = (compoundFactor - 1.0) / tau;
    swapletRate_ = coupon_->gearing() * rate;
    if (!coupon_->includeSpread()) {
        swapletRate_ += coupon_->spread();
        effectiveSpread_ = coupon_->spread();
        effectiveIndexFixing_ = rate;
    } else {
        effectiveSpread_ = rate - (compoundFactorWithoutSpread - 1.0) / tau;
        effectiveIndexFixing_ = rate - effectiveSpread_;
    }
}

Rate OvernightIndexedCouponPricer::swapletRate() const {
    compute();
    return swapletRate_;
}

Rate OvernightIndexedCouponPricer::effectiveSpread() const {
    compute();
    return effectiveSpread_;
}

Rate OvernightIndexedCouponPricer::effectiveIndexFixing() const {
    compute();
    return effectiveIndexFixing_;
}

// CappedFlooredOvernightIndexedCoupon implementation

CappedFlooredOvernightIndexedCoupon::CappedFlooredOvernightIndexedCoupon(
    const ext::shared_ptr<OvernightIndexedCoupon>& underlying, Real cap, Real floor, bool nakedOption,
    bool localCapFloor)
    : FloatingRateCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                         underlying->accrualEndDate(), underlying->fixingDays(), underlying->index(),
                         underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                         underlying->referencePeriodEnd(), underlying->dayCounter(), false),
      underlying_(underlying), nakedOption_(nakedOption), localCapFloor_(localCapFloor) {

    QL_REQUIRE(!underlying_->includeSpread() || close_enough(underlying_->gearing(), 1.0),
               "CappedFlooredOvernightIndexedCoupon: if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");

    if (!localCapFloor) {
        if (gearing_ > 0.0) {
            cap_ = cap;
            floor_ = floor;
        } else {
            cap_ = floor;
            floor_ = cap;
        }
    } else {
        cap_ = cap;
        floor_ = floor;
    }
    if (cap_ != Null<Real>() && floor_ != Null<Real>()) {
        QL_REQUIRE(cap_ >= floor, "cap level (" << cap_ << ") less than floor level (" << floor_ << ")");
    }
    registerWith(underlying_);
    if (nakedOption_)
        underlying_->alwaysForwardNotifications();
}

void CappedFlooredOvernightIndexedCoupon::alwaysForwardNotifications() {
    LazyObject::alwaysForwardNotifications();
    underlying_->alwaysForwardNotifications();
}

void CappedFlooredOvernightIndexedCoupon::deepUpdate() {
    update();
    underlying_->deepUpdate();
}

void CappedFlooredOvernightIndexedCoupon::performCalculations() const {
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
    auto p = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "CappedFlooredOvernightIndexedCoupon::performCalculations(): internal error, could not cast to "
                  "CappedFlooredOvernightIndexedCouponPricer");
    effectiveCapletVolatility_ = p->effectiveCapletVolatility();
    effectiveFloorletVolatility_ = p->effectiveFloorletVolatility();
}

Rate CappedFlooredOvernightIndexedCoupon::cap() const { return gearing_ > 0.0 ? cap_ : floor_; }

Rate CappedFlooredOvernightIndexedCoupon::floor() const { return gearing_ > 0.0 ? floor_ : cap_; }

Rate CappedFlooredOvernightIndexedCoupon::rate() const {
    calculate();
    return rate_;
}

Rate CappedFlooredOvernightIndexedCoupon::convexityAdjustment() const { return underlying_->convexityAdjustment(); }

Rate CappedFlooredOvernightIndexedCoupon::effectiveCap() const {
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
        if (underlying_->includeSpread()) {
            // A = g \cdot \frac{\prod (1 + \tau_i \min ( \max ( f_i + s , F), C)) - 1}{\tau}
            return cap_ - underlying_->spread();
        } else {
            // A = g \cdot \frac{\prod (1 + \tau_i \min ( \max ( f_i , F), C)) - 1}{\tau} + s
            return cap_;
        }
    } else {
        if (underlying_->includeSpread()) {
            // A = \min \left( \max \left( g \cdot \frac{\prod (1 + \tau_i(f_i + s)) - 1}{\tau}, F \right), C \right)
            return (cap_ / gearing() - underlying_->effectiveSpread());
        } else {
            // A = \min \left( \max \left( g \cdot \frac{\prod (1 + \tau_i f_i) - 1}{\tau} + s, F \right), C \right)
            return (cap_ - underlying_->effectiveSpread()) / gearing();
        }
    }
}

Rate CappedFlooredOvernightIndexedCoupon::effectiveFloor() const {
    if (floor_ == Null<Real>())
        return Null<Real>();
    if (localCapFloor_) {
        if (underlying_->includeSpread()) {
            return floor_ - underlying_->spread();
        } else {
            return floor_;
        }
    } else {
        if (underlying_->includeSpread()) {
            return (floor_ - underlying_->effectiveSpread());
        } else {
            return (floor_ - underlying_->effectiveSpread()) / gearing();
        }
    }
}

Real CappedFlooredOvernightIndexedCoupon::effectiveCapletVolatility() const {
    calculate();
    return effectiveCapletVolatility_;
}

Real CappedFlooredOvernightIndexedCoupon::effectiveFloorletVolatility() const {
    calculate();
    return effectiveFloorletVolatility_;
}

void CappedFlooredOvernightIndexedCoupon::accept(AcyclicVisitor& v) {
    Visitor<CappedFlooredOvernightIndexedCoupon>* v1 = dynamic_cast<Visitor<CappedFlooredOvernightIndexedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

// CappedFlooredOvernightIndexedCouponPricer implementation (this is the base class only)

CappedFlooredOvernightIndexedCouponPricer::CappedFlooredOvernightIndexedCouponPricer(
    const Handle<OptionletVolatilityStructure>& v, const bool effectiveVolatilityInput)
    : capletVol_(v), effectiveVolatilityInput_(effectiveVolatilityInput) {
    registerWith(capletVol_);
}

bool CappedFlooredOvernightIndexedCouponPricer::effectiveVolatilityInput() const { return effectiveVolatilityInput_; }

Real CappedFlooredOvernightIndexedCouponPricer::effectiveCapletVolatility() const { return effectiveCapletVolatility_; }

Real CappedFlooredOvernightIndexedCouponPricer::effectiveFloorletVolatility() const {
    return effectiveFloorletVolatility_;
}

Handle<OptionletVolatilityStructure> CappedFlooredOvernightIndexedCouponPricer::capletVolatility() const {
    return capletVol_;
}

// OvernightLeg implementation

OvernightLeg::OvernightLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& i)
    : schedule_(schedule), overnightIndex_(i), paymentCalendar_(schedule.calendar()), paymentAdjustment_(Following),
      paymentLag_(0), telescopicValueDates_(false), includeSpread_(false), lookback_(0 * Days), rateCutoff_(0),
      fixingDays_(Null<Size>()), nakedOption_(false), localCapFloor_(false), inArrears_(true) {}

OvernightLeg& OvernightLeg::withNotionals(Real notional) {
    notionals_ = vector<Real>(1, notional);
    return *this;
}

OvernightLeg& OvernightLeg::withNotionals(const vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

OvernightLeg& OvernightLeg::withPaymentDayCounter(const DayCounter& dc) {
    paymentDayCounter_ = dc;
    return *this;
}

OvernightLeg& OvernightLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

OvernightLeg& OvernightLeg::withPaymentCalendar(const Calendar& cal) {
    paymentCalendar_ = cal;
    return *this;
}

OvernightLeg& OvernightLeg::withPaymentLag(Natural lag) {
    paymentLag_ = lag;
    return *this;
}

OvernightLeg& OvernightLeg::withGearings(Real gearing) {
    gearings_ = vector<Real>(1, gearing);
    return *this;
}

OvernightLeg& OvernightLeg::withGearings(const vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

OvernightLeg& OvernightLeg::withSpreads(Spread spread) {
    spreads_ = vector<Spread>(1, spread);
    return *this;
}

OvernightLeg& OvernightLeg::withSpreads(const vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

OvernightLeg& OvernightLeg::withTelescopicValueDates(bool telescopicValueDates) {
    telescopicValueDates_ = telescopicValueDates;
    return *this;
}

OvernightLeg& OvernightLeg::includeSpread(bool includeSpread) {
    includeSpread_ = includeSpread;
    return *this;
}

OvernightLeg& OvernightLeg::withLookback(const Period& lookback) {
    lookback_ = lookback;
    return *this;
}

OvernightLeg& OvernightLeg::withRateCutoff(const Natural rateCutoff) {
    rateCutoff_ = rateCutoff;
    return *this;
}

OvernightLeg& OvernightLeg::withFixingDays(const Natural fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

OvernightLeg& OvernightLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

OvernightLeg& OvernightLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

OvernightLeg& OvernightLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

OvernightLeg& OvernightLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

OvernightLeg& OvernightLeg::withNakedOption(const bool nakedOption) {
    nakedOption_ = nakedOption;
    return *this;
}

OvernightLeg& OvernightLeg::withLocalCapFloor(const bool localCapFloor) {
    localCapFloor_ = localCapFloor;
    return *this;
}

OvernightLeg& OvernightLeg::withInArrears(const bool inArrears) {
    inArrears_ = inArrears;
    return *this;
}

OvernightLeg& OvernightLeg::withLastRecentPeriod(const boost::optional<Period>& lastRecentPeriod) {
    lastRecentPeriod_ = lastRecentPeriod;
    return *this;
}

OvernightLeg& OvernightLeg::withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar) {
    lastRecentPeriodCalendar_ = lastRecentPeriodCalendar;
    return *this;
}

OvernightLeg& OvernightLeg::withPaymentDates(const std::vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

OvernightLeg&
OvernightLeg::withOvernightIndexedCouponPricer(const QuantLib::ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer) {
    couponPricer_ = couponPricer;
    return *this;
}

OvernightLeg& OvernightLeg::withCapFlooredOvernightIndexedCouponPricer(
    const QuantLib::ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& couponPricer) {
    capFlooredCouponPricer_ = couponPricer;
    return *this;
}

OvernightLeg::operator Leg() const {

    QL_REQUIRE(!notionals_.empty(), "no notional given for compounding overnight leg");

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
            cashflows.push_back(QuantLib::ext::make_shared<FixedRateCoupon>(
                paymentDate, detail::get(notionals_, i, 1.0), detail::effectiveFixedRate(spreads_, caps_, floors_, i),
                paymentDayCounter_, start, end, refStart, refEnd));
        } else {
            // floating coupon
            auto cpn = ext::make_shared<OvernightIndexedCoupon>(
                paymentDate, detail::get(notionals_, i, 1.0), start, end, overnightIndex_,
                detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), refStart, refEnd, paymentDayCounter_,
                telescopicValueDates_, includeSpread_, lookback_, rateCutoff_, fixingDays_, rateComputationStartDate,
                rateComputationEndDate);
            if (couponPricer_) {
                cpn->setPricer(couponPricer_);
            }
            Real cap = detail::get(caps_, i, Null<Real>());
            Real floor = detail::get(floors_, i, Null<Real>());
            if (cap == Null<Real>() && floor == Null<Real>()) {
                cashflows.push_back(cpn);
            } else {
                auto cfCpn = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(cpn, cap, floor, nakedOption_,
                                                                                   localCapFloor_);
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
