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
#include <algorithm>
#include <iterator>

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
                                               const Date& rateComputationEndDate, bool applyObservationShift)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, overnightIndex, gearing, spread,
                         refPeriodStart, refPeriodEnd, dayCounter, false),
      overnightIndex_(overnightIndex), includeSpread_(includeSpread), lookback_(lookback), rateCutoff_(rateCutoff),
      rateComputationStartDate_(rateComputationStartDate), rateComputationEndDate_(rateComputationEndDate),
      applyObservationShift_(applyObservationShift), telescopicDates_(telescopicValueDates) {

    // Lookback was never intended to be positive i.e. it was designed to allow time to calculate the coupon before
    // a coupon payment date. QuantLib has it as Natural => non-negative but we won't change the interface now but just 
    // throw an error if anything other than a non-negative (- is applied to it) number of days is given.
    QL_REQUIRE(lookback.length() >= 0 && lookback.units() == Days,
        "OvernightIndexedCoupon: lookback (" << lookback << ") must be non-negative number of days.");

    // If we have no lookback, observation shift should be false if it isn't already.
    applyObservationShift_ = applyObservationShift_ && lookback.length() != 0;

    // Unadjusted interest start and end dates.
    Date intStart = rateComputationStartDate_ == Null<Date>() ? startDate : rateComputationStartDate_;
    Date intEnd = rateComputationEndDate_ == Null<Date>() ? endDate : rateComputationEndDate_;
    QL_REQUIRE(intStart < intEnd, "OvernightIndexedCoupon: start date ("
        << intStart << ") must be earlier than end date (" << intEnd << ")");

    setTelescopicDates();
    auto onFixCal = overnightIndex->fixingCalendar();
    cachedEvalDate_ = onFixCal.adjust(Settings::instance().evaluationDate(), Preceding);

    // Anchor date for generating the interest dates schedule.
    // Start date of first overnight period and start date of last overnight period.
    Date adjIntStart = onFixCal.adjust(intStart, Preceding);
    Date adjIntEnd = onFixCal.isBusinessDay(intEnd) ? onFixCal.advance(intEnd, -1, Days, Preceding)
                                                    : onFixCal.adjust(intEnd, Preceding);

    // Corresponding anchor lookback dates.
    Date lbStart = lookback != 0 * Days ? onFixCal.advance(adjIntStart, -lookback, Preceding) : adjIntStart;
    Date lbEnd = lookback != 0 * Days ? onFixCal.advance(adjIntEnd, -lookback, Preceding) : adjIntEnd;

    // Corresponding anchor fixing dates.
    Integer frcFixDays = fixingDays_ != 0 ? static_cast<Integer>(fixingDays_) : 0;
    Date fixStart = fixingDays_ != 0 ? onFixCal.advance(lbStart, -frcFixDays, Days, Preceding) : lbStart;
    Date fixEnd = fixingDays_ != 0 ? onFixCal.advance(lbEnd, -frcFixDays, Days, Preceding) : lbEnd;

    // If a rate cut-off is in effect, note the date at which it starts and corresponding lb & interest dates.
    Integer rateCutOffDays = rateCutoff_ != 0 ? static_cast<Integer>(rateCutoff_) : 0;
    Date rateCutOffStart = rateCutoff_ != 0 ? onFixCal.advance(fixEnd, -rateCutOffDays, Days, Preceding) : fixEnd;
    Date rcoIntStart = rateCutoff_ != 0 ? onFixCal.advance(adjIntEnd, -rateCutOffDays, Days, Preceding) : adjIntEnd;
    Date rcoLbStart = rateCutoff_ != 0 ? onFixCal.advance(lbEnd, -rateCutOffDays, Days, Preceding) : lbEnd;
    QL_REQUIRE(fixStart <= rateCutOffStart, "OvernightIndexedCoupon: rate cut-off fixing date (" <<
        rateCutOffStart << ") cannot be earlier than the first fixing date (" << fixStart << ").");
    lastFixingDate_ = rateCutOffStart;

    if (!telescopicDates_ || cachedEvalDate_ >= rateCutOffStart) {

        // Build full dates schedule.
        size_t reserveSize = static_cast<size_t>(fixEnd - fixStart + 1);
        valueDates_.reserve(reserveSize);
        fixingDates_.reserve(reserveSize);
        interestDates_.reserve(reserveSize);

        if (rateCutoff_ == 0) {
            // Add fixing, interest and value dates up to and including fixing end date.
            addScheduleDates(fixEnd, fixStart, adjIntStart, lbStart);
        } else {
            // Add fixing, interest and value dates up to and excluding rate cut-off date.
            addScheduleDates(rateCutOffStart, fixStart, adjIntStart, lbStart, true);
            // Add rate cut-off dates up to and including fixing end date.
            addRateCutoffDates(fixEnd, rateCutOffStart, rcoIntStart, rcoLbStart);
        }

        QL_REQUIRE(!fixingDates_.empty(), "OvernightIndexedCoupon: no fixing dates generated!");

        // Update interest start date if necessary.
        if (!applyObservationShift_ && intStart != adjIntStart)
            interestDates_.front() = intStart;

        // Add interest end date.
        if (applyObservationShift_) {
            interestDates_.push_back(onFixCal.advance(interestDates_.back(), 1, Days, Following));
        } else {
            QL_REQUIRE(intEnd > interestDates_.back(), "OvernightIndexedCoupon: expected interest end date " <<
                intEnd << " to be greater than last generated interest date " << interestDates_.back() << ".");
            interestDates_.push_back(intEnd);
        }

        // Add final value date. For consistency, even with rate cut-off, the final value date should be the end date 
        // of the underlying 1D overnight coupon period. In other words, with rate cut-off = 3 for example, we will 
        // have fixingDates_ = {..., t_f, t_f, t_f}, valueDates_ = {..., VD(t_f), VD(t_f), VD(t_f), VD(t_f) + 1BD}
        valueDates_.push_back(onFixCal.advance(valueDates_.back(), 1, Days, Following));

    } else {
        // Build telescopic dates schedule (telescopic dates allowed and cachedEvalDate_ < rateCutOffStart).

        if (cachedEvalDate_ <= fixStart) {
            // Always have this set of start dates if evaluation date is on or before first fixing date.
            if (rateCutoff_ == 0 || fixStart < rateCutOffStart) {
                fixingDates_ = {fixStart};
                valueDates_ = {overnightIndex->valueDate(fixStart)};
                interestDates_ = {applyObservationShift_ ? lbStart : intStart};
            }

            // We need an overnight period stub at start here for one of two reasons:
            // 1. evaluation date is equal to first fixing date so want to allow attempt to look up fixing in pricer.
            // 2. if we have an underlying overnight period at the start whose start date is a holiday, the telescopic 
            //    formula is not accurate because period associated with fixing does not equal the interest period it 
            //    is being applied to. We add the extra date to allow the pricer to calculate the one forward rate 
            //    for this underlying overnight period.
            Date fixStartPlusOne = onFixCal.advance(fixStart, 1, Days, Following);
            if ((fixStart < fixEnd && (rateCutoff_ == 0 || fixStartPlusOne < rateCutOffStart)) &&
                (cachedEvalDate_ == fixStart || (!applyObservationShift_ && intStart != adjIntStart))) {
                fixingDates_.push_back(fixStartPlusOne);
                valueDates_.push_back(overnightIndex->valueDate(fixingDates_.back()));
                if (!applyObservationShift_) {
                    if (intStart != adjIntStart)
                        interestDates_.push_back(onFixCal.adjust(intStart, Following));
                    else
                        interestDates_.push_back(onFixCal.advance(intStart, 1, Days, Following));
                } else {
                    interestDates_.push_back(onFixCal.advance(lbStart, 1, Days, Following));
                }
            }

            // If have rate cut-off and rate cut-off start is (<, not allowed, or) equal to fixStart, we will not have 
            // added any fixings yet and all dates will be frozen rate cut-off dates => tsStartIdx_ unset in this case.
            if (!fixingDates_.empty())
                tsStartIdx_ = fixingDates_.size() - 1;

            // Add final dates.
            addTelescopeBackStub(fixEnd, rateCutOffStart, rcoIntStart, rcoLbStart, intEnd, adjIntEnd, lbEnd);

        } else {

            // cachedEvalDate_ \in (fixStart, rateCutOffStart).
            size_t reserveSize = static_cast<size_t>(cachedEvalDate_ - fixStart + 1);
            valueDates_.reserve(reserveSize);
            fixingDates_.reserve(reserveSize);
            interestDates_.reserve(reserveSize);

            // Add fixing, interest and value dates up to and including the fixing date corresponding to 
            // cachedEvalDate_.
            addScheduleDates(cachedEvalDate_, fixStart, adjIntStart, lbStart);
            QL_REQUIRE(!fixingDates_.empty(), "OvernightIndexedCoupon: no fixing dates generated!");
            tsStartIdx_ = fixingDates_.size() - 1;

            // Add final dates.
            addTelescopeBackStub(fixEnd, rateCutOffStart, rcoIntStart, rcoLbStart, intEnd, adjIntEnd, lbEnd);

            // Update interest start date if necessary.
            if (!applyObservationShift_ && intStart != adjIntStart)
                interestDates_.front() = intStart;
        }

        checkForAllDates();
    }

    QL_ENSURE(valueDates_.size() >= 2, "OvernightIndexedCoupon: degenerate schedule, only have " <<
        valueDates_.size() << " value date(s).");

    // Number of overnight periods in the coupon.
    n_ = valueDates_.size() - 1;

    QL_ENSURE(valueDates_.size() == interestDates_.size(), "OvernightIndexedCoupon: mismatch in value dates and " <<
        " interest dates schedule sizes: " << valueDates_.size() << " vs. " << interestDates_.size() << ".");
    QL_ENSURE(n_ == fixingDates_.size(), "OvernightIndexedCoupon: size of fixing dates (" <<
        fixingDates_.size() << ") should equal size of value dates (" << valueDates_.size() << ") - 1.");
    QL_REQUIRE(rateCutoff_ < n_, "Number of rate cut-off days (" << rateCutoff_ <<
        ") must be less than the number of fixings (" << n_ << ").");

    populateAccruals();

    // Set the pricer.
    setPricer(ext::make_shared<OvernightIndexedCouponPricer>());
}

const vector<Date>& OvernightIndexedCoupon::fixingDates() const {
    if (haveStaleDates())
        updateSchedules();
    return fixingDates_;
}

const vector<Time>& OvernightIndexedCoupon::dt() const {
    if (haveStaleDates())
        updateSchedules();
    return dt_;
}

const vector<Date>& OvernightIndexedCoupon::valueDates() const {
    if (haveStaleDates())
        updateSchedules();
    return valueDates_;
}

const vector<Date>& OvernightIndexedCoupon::interestDates() const {
    if (haveStaleDates())
        updateSchedules();
    return interestDates_;
}

void OvernightIndexedCoupon::performCalculations() const {
    if (haveStaleDates())
        updateSchedules();
    FloatingRateCoupon::performCalculations();
}

const vector<Rate>& OvernightIndexedCoupon::indexFixings() const {
    if (haveStaleDates())
        updateSchedules();

    fixings_.resize(n_);

    // Fixings up to rate cut-off (if any).
    for (Size i = 0; i < n_ - rateCutoff_; ++i) {
        fixings_[i] = index_->fixing(fixingDates_[i]);
    }

    // Copy fixings from rate cut-off fixing (if any).
    auto it = prev(fixings_.end(), rateCutoff_ + 1);
    fill(it, fixings_.end(), *it);

    return fixings_;
}

Real OvernightIndexedCoupon::accruedAmount(const Date& d) const {
    if (d <= accrualStartDate_ || d > paymentDate_)
        return 0.0;
    else if (tradingExCoupon(d))
        return nominal() * effectiveRate(d) * accruedPeriod(d);
    else
        return nominal() * effectiveRate(std::min(d, accrualEndDate_)) * accruedPeriod(d);
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
    return !includeSpread_ ? spread() : oicPricer()->effectiveSpread();
}

Real OvernightIndexedCoupon::effectiveIndexFixing() const {
    return oicPricer()->effectiveIndexFixing();
}

void OvernightIndexedCoupon::setTelescopicDates() {
    // Can apply telescopic formula if either of the following hold:
    // 1. no lookback and fixingDays_ align with overnight index fixing days.
    // 2. have lookback, obs shift is true and fixingDays_ align with overnight index fixing days.
    telescopicDates_ = telescopicDates_ &&
        ((!hasLookback() || applyObservationShift_) && fixingDays_ == index_->fixingDays());
}

bool OvernightIndexedCoupon::haveStaleDates() const {
    // If we don't have telescopic dates, always have full schedule => dates don't change and are not stale.
    if (!telescopicDates_)
        return false;

    Date evalDate = overnightIndex_->fixingCalendar().adjust(Settings::instance().evaluationDate(), Preceding);

    if (evalDate == cachedEvalDate_)
        return false;

    if (evalDate > cachedEvalDate_) {
        // Checks for avoiding adding dates.
        // If we already have all dates OR
        // If eval date is before start of existing telescopic period, can't add dates.
        if (!tsStartIdx_ || evalDate < fixingDates_[*tsStartIdx_]) {
            cachedEvalDate_ = evalDate;
            return false;
        }
    } else {
        // evalDate < cachedEvalDate_: checks for avoiding removing dates.
        // If we have all dates and evalDate is on or after last possible fixing date OR
        // If we have a telescopic period and evalDate is on or after start of telescopic period, can't remove dates.
        if ((!tsStartIdx_ && lastFixingDate_ <= evalDate) || (tsStartIdx_ && fixingDates_[*tsStartIdx_] <= evalDate)) {
            cachedEvalDate_ = evalDate;
            return false;
        }
    }

    return true;
}

void OvernightIndexedCoupon::updateSchedules() const {
    auto onFixCal = overnightIndex_->fixingCalendar();
    Date evalDate = onFixCal.adjust(Settings::instance().evaluationDate(), Preceding);

    if (evalDate > cachedEvalDate_) {
        // Additional safety check to avoid duplicating dates at end of schedule.
        Date stopDate = std::min(evalDate, onFixCal.advance(lastFixingDate_, -1, Days, Preceding));
        if(*tsStartIdx_ < fixingDates_.size() - 1) {
            stopDate = std::min(stopDate, onFixCal.advance(fixingDates_[*tsStartIdx_ + 1], -2, Days, Preceding));
        }

        // We should not hit this point because of checkForAllDates() and use of tsStartIdx_ but add it in case.
        if (fixingDates_[*tsStartIdx_] > stopDate) {
            cachedEvalDate_ = evalDate;
            tsStartIdx_.reset();
            return;
        }

        vector<Date> addtlFixingDates{onFixCal.advance(fixingDates_[*tsStartIdx_], 1, Days, Following)};
        vector<Date> addtlValueDates{overnightIndex_->valueDate(addtlFixingDates.back())};
        vector<Date> addtlInterestDates{onFixCal.advance(interestDates_[*tsStartIdx_], 1, Days, Following)};

        // May not be much to be gained by this but do it anyway.
        size_t reserveSize = static_cast<size_t>(evalDate - fixingDates_[*tsStartIdx_]);
        addtlFixingDates.reserve(reserveSize);
        addtlValueDates.reserve(reserveSize);
        addtlInterestDates.reserve(reserveSize);

        // Build up additional dates to be added.
        while (addtlFixingDates.back() <= stopDate) {
            addtlFixingDates.push_back(onFixCal.advance(addtlFixingDates.back(), 1, Days, Following));
            addtlValueDates.push_back(onFixCal.advance(addtlValueDates.back(), 1, Days, Following));
            addtlInterestDates.push_back(onFixCal.advance(addtlInterestDates.back(), 1, Days, Following));
        }

        // Add the additional dates.
        fixingDates_.insert(next(fixingDates_.begin(), *tsStartIdx_ + 1),
            addtlFixingDates.begin(), addtlFixingDates.end());
        valueDates_.insert(next(valueDates_.begin(), *tsStartIdx_ + 1),
            addtlValueDates.begin(), addtlValueDates.end());
        interestDates_.insert(next(interestDates_.begin(), *tsStartIdx_ + 1),
            addtlInterestDates.begin(), addtlInterestDates.end());

        // Update index of start of telescopic period.
        *tsStartIdx_ += addtlFixingDates.size();

        // Check if we have all dates and update tsStartIdx_ if we do.
        checkForAllDates();

    } else {
        // Need to remove fixing dates and associated value and interest dates.
        // Note: only get called if have all fixings and evalDate < last fixing OR have a telescopic period and 
        //       evalDate < fixingDates_[*tsStartIdx_].
        vector<Date>::iterator itFixDelTo;
        if (!tsStartIdx_) {
            // If have all dates:
            if (rateCutoff_ != 0)
                // erase up to but not incl. start of rate cut-off.
                itFixDelTo = std::prev(fixingDates_.end(), rateCutoff_ + 1);
            else if (onFixCal.isHoliday(interestDates_.back()))
                // erase up to but not incl. start of back stub for holiday.
                itFixDelTo = prev(fixingDates_.end());
            else
                // erase up to and incl. last fixing date.
                itFixDelTo = fixingDates_.end();
        } else {
            // If have a telescopic period, erase up to and including start of telescopic period.
            itFixDelTo = next(fixingDates_.begin(), *tsStartIdx_ + 1);
        }

        // evalDate is a valid business day on the fixing calendar (adjusted preceding if true eval date not)
        // - if evalDate < first fixing date: just need to keep first fixing date unless holiday front stub.
        // - if evalDate == fixing_date_{i}, need to keep up to fixing_date_{i+1}.
        // Note from above, we either have:
        // 1. all fixings and evalDate < last fixing OR
        // 2. evalDate < fixingDates_[*tsStartIdx_]
        // => upper_bound != fixingDates_.end() => can add 1, may be end().
        vector<Date>::iterator itFixDelFrom;
        if (evalDate < fixingDates_.front()) {
            if (!applyObservationShift_ && onFixCal.isHoliday(interestDates_.front())) {
                itFixDelFrom = next(fixingDates_.begin(), std::min(fixingDates_.size(), static_cast<size_t>(2)));
            } else {
                itFixDelFrom = next(fixingDates_.begin());
            }
        } else {
            itFixDelFrom = next(upper_bound(fixingDates_.begin(), fixingDates_.end(), evalDate));
        }

        // If we are not able to remove any dates, return.
        if (itFixDelFrom >= itFixDelTo) {
            cachedEvalDate_ = evalDate;
            return;
        }

        // Remove the dates.
        auto viIdxFrom = distance(fixingDates_.begin(), itFixDelFrom);
        auto viIdxTo = distance(fixingDates_.begin(), itFixDelTo);
        fixingDates_.erase(itFixDelFrom, itFixDelTo);
        valueDates_.erase(valueDates_.begin() + viIdxFrom, valueDates_.begin() + viIdxTo);
        interestDates_.erase(interestDates_.begin() + viIdxFrom, interestDates_.begin() + viIdxTo);

        // Update index of start of telescopic period.
        tsStartIdx_ = viIdxFrom - 1;
    }

    // Update the dt_ values and update the cached evaluation date.
    populateAccruals();
    cachedEvalDate_ = evalDate;
}

Rate OvernightIndexedCoupon::effectiveRate(const Date& d) const {
    QL_FAIL("Not implemented yet!");
    // Should be "oicPricer()->effectiveRate(d);"
}

ext::shared_ptr<OvernightIndexedCouponPricer> OvernightIndexedCoupon::oicPricer() const {
    auto fcp = pricer();
    QL_REQUIRE(fcp, "OvernightIndexedCoupon: FloatingRateCoupon pricer is null.");
    auto p = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "OvernightIndexedCoupon: expected an OvernightIndexedCouponPricer.");
    p->initialize(*this);
    return p;
}

void OvernightIndexedCoupon::addScheduleDates(Date fixEnd, Date fixStart, Date intStart, Date lbStart, bool exclEnd)
{
    bool fixSameAsLb = fixingDays_ == 0;
    bool valueSameAsLb = fixSameAsLb && fixingDays_ == overnightIndex_->fixingDays();
    bool lbDatesUpdating = false;
    auto onFixCal = overnightIndex_->fixingCalendar();
    fixStart = fixingDates_.empty() ? fixStart : std::max(fixStart, fixingDates_.back());
    fixEnd = exclEnd ? onFixCal.advance(fixEnd, -1, Days, Preceding) : fixEnd;

    while (fixStart <= fixEnd) {
        // Fixing dates.
        fixingDates_.push_back(fixStart);
        fixStart = onFixCal.advance(fixStart, 1, Days, Following);

        // Interest dates.
        if (applyObservationShift_) {
            if (fixSameAsLb) {
                interestDates_.push_back(fixingDates_.back());
            } else {
                interestDates_.push_back(lbStart);
                lbStart = onFixCal.advance(lbStart, 1, Days, Following);
                lbDatesUpdating = true;
            }
        } else {
            interestDates_.push_back(intStart);
            intStart = onFixCal.advance(intStart, 1, Days, Following);
        }

        // Value dates associated with the fixing dates.
        if (valueSameAsLb) {
            if (lbDatesUpdating) {
                valueDates_.push_back(lbStart);
            } else {
                valueDates_.push_back(lbStart);
                lbStart = onFixCal.advance(lbStart, 1, Days, Following);
            }
        } else {
            valueDates_.push_back(overnightIndex_->valueDate(fixingDates_.back()));
        }
    }
}

void OvernightIndexedCoupon::addRateCutoffDates(Date fixEnd,
    Date rcoStart, Date rcoIntStart, Date rcoLbStart, bool exclEnd)
{
    // From rate cut-off start to end (if we have a rate cut-off <=> rateCutOffStart < fixEnd).
    auto onFixCal = overnightIndex_->fixingCalendar();
    Date frozenDate = rcoStart;
    Date frozenValueDate = overnightIndex_->valueDate(frozenDate);
    fixEnd = exclEnd ? onFixCal.advance(fixEnd, -1, Days, Preceding) : fixEnd;

    while (rcoStart <= fixEnd) {
        fixingDates_.push_back(frozenDate);
        valueDates_.push_back(frozenValueDate);
        if (applyObservationShift_) {
            interestDates_.push_back(rcoLbStart);
            rcoLbStart = onFixCal.advance(rcoLbStart, 1, Days, Following);
        } else {
            interestDates_.push_back(rcoIntStart);
            rcoIntStart = onFixCal.advance(rcoIntStart, 1, Days, Following);
        }
        rcoStart = onFixCal.advance(rcoStart, 1, Days, Following);
    }
}

void OvernightIndexedCoupon::addTelescopeBackStub(Date fixEnd, Date rcoStart, Date rcoIntStart, Date rcoLbStart,
                                                  const Date& intEnd, const Date& adjIntEnd, const Date& lbEnd) {
    auto onFixCal = overnightIndex_->fixingCalendar();
    if (rateCutoff_ == 0) {
        // Add final dates.
        if (fixingDates_.back() < fixEnd && (!applyObservationShift_ && !onFixCal.isBusinessDay(intEnd))) {
            // We need an overnight period stub here because everything does not collapse.
            fixingDates_.push_back(fixEnd);
            valueDates_.push_back(overnightIndex_->valueDate(fixEnd));
            valueDates_.push_back(onFixCal.advance(valueDates_.back(), 1, Days, Following));
            interestDates_.push_back(adjIntEnd);
            interestDates_.push_back(intEnd);
        } else {
            if (applyObservationShift_) {
                valueDates_.push_back(onFixCal.advance(lbEnd, 1, Days, Following));
                interestDates_.push_back(valueDates_.back());
            } else {
                valueDates_.push_back(onFixCal.adjust(intEnd, Following));
                interestDates_.push_back(intEnd);
            }
        }
    } else {
        // Add rate cut-off dates up to and including final fixing end date.
        addRateCutoffDates(fixEnd, rcoStart, rcoIntStart, rcoLbStart);
        valueDates_.push_back(onFixCal.advance(valueDates_.back(), 1, Days, Following));
        if (applyObservationShift_)
            interestDates_.push_back(onFixCal.advance(interestDates_.back(), 1, Days, Following));
        else
            interestDates_.push_back(intEnd);
    }
}

void OvernightIndexedCoupon::checkForAllDates() const {
    // In rare cases, all dates will be lockout dates and tsStartIdx_ will still be null.
    if (!tsStartIdx_)
        return;

    // After adding dates to telescopic period, we may have added all dates => we should set tsStartIdx_ to null.
    Date nextFixDt = overnightIndex_->fixingCalendar().advance(fixingDates_[*tsStartIdx_], 1, Days, Following);
    if (nextFixDt > lastFixingDate_ ||
        (*tsStartIdx_ < fixingDates_.size() - 1 && fixingDates_[*tsStartIdx_ + 1] == nextFixDt)) {
        tsStartIdx_.reset();
    }
}

void OvernightIndexedCoupon::populateAccruals() const {
    // Day count fractions for each overnight _interest_ period in the coupon. These are the daily periods from input
    // start date to input end date if observation shift is `false` and are the daily lookback periods corresponding to
    // the daily periods from input start to input end date if observation shift is `true` (and non-zero lookback).
    dt_.resize(n_);
    const DayCounter& dc = overnightIndex_->dayCounter();
    for (Size i = 0; i < n_; ++i)
        dt_[i] = dc.yearFraction(interestDates_[i], interestDates_[i + 1]);
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

OvernightLeg& OvernightLeg::withLastRecentPeriod(const QuantLib::ext::optional<Period>& lastRecentPeriod) {
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
