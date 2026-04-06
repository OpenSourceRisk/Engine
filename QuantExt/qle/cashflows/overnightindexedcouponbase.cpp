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
 Copyright (C) 2026 AcadiaSoft, Inc.

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

#include <qle/cashflows/overnightindexedcouponbase.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <algorithm>
#include <iterator>

using namespace QuantLib;
using std::pair;
using std::tie;
using std::tuple;
using std::vector;

namespace QuantExt {

OvernightIndexedCouponBase::OvernightIndexedCouponBase(Type rateType, const Date& paymentDate, Real nominal,
    const Date& startDate, const Date& endDate, const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
    Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd, const DayCounter& dayCounter,
    bool telescopicValueDates, const Period& lookback, const Natural rateCutoff, const Natural fixingDays,
    const Date& rateComputationStartDate, const Date& rateComputationEndDate, bool observationShift,
    bool staleDatesCheck)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, fixingDays, overnightIndex, gearing, spread,
        refPeriodStart, refPeriodEnd, dayCounter, false),
      rateType_(rateType), telescopicDates_(telescopicValueDates), overnightIndex_(overnightIndex), lookback_(lookback),
      rateCutoff_(rateCutoff), rateComputationStartDate_(rateComputationStartDate),
      rateComputationEndDate_(rateComputationEndDate), observationShift_(observationShift),
      staleDatesCheck_(staleDatesCheck) {

    // Lookback was never intended to be positive i.e. it was designed to allow time to calculate the coupon before
    // a coupon payment date. QuantLib has it as Natural => non-negative but we won't change the interface now but just 
    // throw an error if anything other than a non-negative (- is applied to it) number of days is given.
    QL_REQUIRE(lookback.length() >= 0 && lookback.units() == Days,
        "OvernightIndexedCoupon: lookback (" << lookback << ") must be non-negative number of days.");

    // If we have no lookback, observation shift should be false if it isn't already.
    observationShift_ = observationShift_ && lookback.length() != 0;

    // Record if we have a rate computation period separate from the main coupon accrual period.
    separateRateCompPeriod_ = (rateComputationStartDate_ != Date() && rateComputationStartDate_ != startDate) ||
        (rateComputationEndDate_ != Date() && rateComputationEndDate_ != endDate);

    // Unadjusted interest start and end dates.
    Date intStart = rateComputationStartDate_ == Date() ? startDate : rateComputationStartDate_;
    Date intEnd = rateComputationEndDate_ == Date() ? endDate : rateComputationEndDate_;
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
    lastFixingDateNoCutoff_ = fixEnd;

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
        if (!observationShift_ && intStart != adjIntStart)
            interestDates_.front() = intStart;

        // Add interest end date.
        if (observationShift_) {
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
                interestDates_ = {observationShift_ ? lbStart : intStart};
            }

            // We need an overnight period stub at start here for one of two reasons:
            // 1. evaluation date is equal to first fixing date so want to allow attempt to look up fixing in pricer.
            // 2. if we have an underlying overnight period at the start whose start date is a holiday, the telescopic 
            //    formula is not accurate because period associated with fixing does not equal the interest period it 
            //    is being applied to. We add the extra date to allow the pricer to calculate the one forward rate 
            //    for this underlying overnight period.
            Date fixStartPlusOne = onFixCal.advance(fixStart, 1, Days, Following);
            if ((fixStart < fixEnd && (rateCutoff_ == 0 || fixStartPlusOne < rateCutOffStart)) &&
                (cachedEvalDate_ == fixStart || (intStart != adjIntStart))) {
                fixingDates_.push_back(fixStartPlusOne);
                valueDates_.push_back(overnightIndex->valueDate(fixingDates_.back()));
                if (!observationShift_) {
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

            // Note again that cachedEvalDate_ (CEVD) is the valuation date (VD) adjusted to the preceding good index 
            // business day if necessary i.e. CEVD is the latest fixing date <= VD. We need to add an extra fixing date 
            // so that the fixing date that starts the telescopic period is > VD. We do it in addScheduleDates if we 
            // can (i.e. if CEVD + 1 BD doesn't put us in the back stub). If not, it is covered by addTelescopeBackStub.
            bool addFixEnd = false;
            Date cachedEvalPlusOne = onFixCal.advance(cachedEvalDate_, 1, Days, Following);
            if (cachedEvalPlusOne < rateCutOffStart) {
                // If rco = 0, CEVD + 1 BD < fixEnd. If rco != 0, CEVD + 1 BD < rateCutOffStart.
                // We can add the extra fixing date here in addScheduleDates.
                addScheduleDates(cachedEvalPlusOne, fixStart, adjIntStart, lbStart);
                tsStartIdx_ = fixingDates_.size() - 1;
            } else if (cachedEvalPlusOne == rateCutOffStart) {
                // If rco = 0, CEVD + 1 BD == fixEnd:
                //   We set addFixEnd to true so that addTelescopeBackStub adds the extra fixing date.
                // If rco != 0, CEVD + 1 BD == rateCutOffStart.
                //   If rco != 0, addTelescopeBackStub adds from and incl. rateCutOffStart in any case.
                addScheduleDates(cachedEvalDate_, fixStart, adjIntStart, lbStart);
                addFixEnd = true;
                // intentionally leave tsStartIdx_ unset as in this case we will have all dates.
            } else {
                QL_FAIL("OvernightIndexedCoupon: failed to build dates schedules.");
            }

            QL_REQUIRE(!fixingDates_.empty(), "OvernightIndexedCoupon: no fixing dates generated.");

            // Add final dates.
            addTelescopeBackStub(fixEnd, rateCutOffStart, rcoIntStart, rcoLbStart, intEnd, adjIntEnd, lbEnd, addFixEnd);

            // Update interest start date if necessary.
            if (!observationShift_ && intStart != adjIntStart)
                interestDates_.front() = intStart;
        }

        checkForAllDates();
    }

    validateDates();
    populateAccruals();
}

const vector<Date>& OvernightIndexedCouponBase::fixingDates() const {
    if (staleDatesCheck_ && haveStaleDates())
        updateSchedules();
    return fixingDates_;
}

const vector<Time>& OvernightIndexedCouponBase::dt() const {
    if (staleDatesCheck_ && haveStaleDates())
        updateSchedules();
    return dt_;
}

const vector<Date>& OvernightIndexedCouponBase::valueDates() const {
    if (staleDatesCheck_ && haveStaleDates())
        updateSchedules();
    return valueDates_;
}

const vector<Date>& OvernightIndexedCouponBase::interestDates() const {
    if (staleDatesCheck_ && haveStaleDates())
        updateSchedules();
    return interestDates_;
}

bool OvernightIndexedCouponBase::canApplyTelescopic() const {
    return (!hasLookback() || observationShift()) && fixingDays_ == index_->fixingDays();
}

void OvernightIndexedCouponBase::performCalculations() const {
    // Turn off further stale date checks while we are performing the calculation.
    bool cachedStaleDatesCheck = staleDatesCheck_;
    if (staleDatesCheck_) {
        if (haveStaleDates())
            updateSchedules();
        staleDatesCheck_ = false;
    }

    if (rateType_ == Type::BrlCdi) {
        // If we have a BRL CDI coupon, do exactly what we were doing before the restructure of the QuantLib and
        // QuantExt overnight coupons and pricers until we decide on a course of action.
        FloatingRateCoupon::performCalculations();
    } else {
        additionalResults_.clear();
        Date upToDate = separateRateCompPeriod() ? interestDates_.back() : accrualEndDate_;
        std::tie(rate_, upToDateAdj_) = effectiveRate(upToDate);
    }

    // Restore value of staleDatesCheck_.
    staleDatesCheck_ = cachedStaleDatesCheck;
}

const vector<Rate>& OvernightIndexedCouponBase::indexFixings() const {
    if (staleDatesCheck_ && haveStaleDates())
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

Real OvernightIndexedCouponBase::amount() const {
    calculate();

    // If observation shift, we need to use the day count fraction from the shifted period.
    if (observationShift_ && !separateRateCompPeriod()) {
        return nominal() * rate_ * dayCounter().yearFraction(interestDates_.front(), upToDateAdj_);
    } else {
        return nominal() * rate_ * accrualPeriod();
    }
}

Real OvernightIndexedCouponBase::accruedAmount(const Date& d) const {
    // Note: no facility in OvernightIndexedCoupon ctor to pass in an ex-coupon date so we don't check
    // tradingExCoupon(d). Don't believe it applies for overnight indexed coupons in any case.
    if (d <= accrualStartDate_ || d > paymentDate_)
        return 0.0;

    // If we have a BRL CDI coupon, do exactly what we were doing before the restructure of the QuantLib and QuantExt
    // overnight coupons and pricers until we decide on a course of action.
    if (rateType_ == Type::BrlCdi)
        return FloatingRateCoupon::accruedAmount(d);

    Date upToDate = separateRateCompPeriod() ? std::min(d, interestDates_.back()) : std::min(d, accrualEndDate_);
    auto [rate, upToDateAdj] = effectiveRate(upToDate);
    if (observationShift_ && !separateRateCompPeriod()) {
        return nominal() * rate * dayCounter().yearFraction(interestDates_.front(), upToDateAdj);
    } else {
        return nominal() * rate * dayCounter().yearFraction(accrualStartDate_, upToDateAdj);
    }
}

void OvernightIndexedCouponBase::setTelescopicDates() {
    if (rateType_ == Type::Compounding)
        telescopicDates_ = telescopicDates_ && canApplyTelescopic();
}

bool OvernightIndexedCouponBase::haveStaleDates() const {
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

void OvernightIndexedCouponBase::updateSchedules() const {
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
            else if (onFixCal.isHoliday(accrualEndDate_))
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
            if (onFixCal.isHoliday(accrualStartDate_)) {
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

    // Check date schedule sizes, update n_ and dt_ and update the cached evaluation date.
    validateDates();
    populateAccruals();
    cachedEvalDate_ = evalDate;
}

void OvernightIndexedCouponBase::addScheduleDates(Date fixEnd, Date fixStart, Date intStart,
    Date lbStart, bool exclEnd) {
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
        if (observationShift_) {
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

void OvernightIndexedCouponBase::addRateCutoffDates(Date fixEnd,
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
        if (observationShift_) {
            interestDates_.push_back(rcoLbStart);
            rcoLbStart = onFixCal.advance(rcoLbStart, 1, Days, Following);
        } else {
            interestDates_.push_back(rcoIntStart);
            rcoIntStart = onFixCal.advance(rcoIntStart, 1, Days, Following);
        }
        rcoStart = onFixCal.advance(rcoStart, 1, Days, Following);
    }
}

void OvernightIndexedCouponBase::addTelescopeBackStub(Date fixEnd, Date rcoStart, Date rcoIntStart, Date rcoLbStart,
    const Date& intEnd, const Date& adjIntEnd, const Date& lbEnd, bool addFixEnd) {
    auto onFixCal = overnightIndex_->fixingCalendar();
    if (rateCutoff_ == 0) {
        // Add final dates.
        if (fixingDates_.back() < fixEnd && (!onFixCal.isBusinessDay(intEnd) || addFixEnd)) {
            // We need an overnight period stub here because everything does not collapse.
            fixingDates_.push_back(fixEnd);
            valueDates_.push_back(overnightIndex_->valueDate(fixEnd));
            valueDates_.push_back(onFixCal.advance(valueDates_.back(), 1, Days, Following));
            if (observationShift_) {
                interestDates_.push_back(*prev(valueDates_.end(), 2));
                interestDates_.push_back(valueDates_.back());
            } else {
                interestDates_.push_back(adjIntEnd);
                interestDates_.push_back(intEnd);
            }
        } else {
            if (observationShift_) {
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
        if (observationShift_)
            interestDates_.push_back(onFixCal.advance(interestDates_.back(), 1, Days, Following));
        else
            interestDates_.push_back(intEnd);
    }
}

void OvernightIndexedCouponBase::checkForAllDates() const {
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

void OvernightIndexedCouponBase::validateDates() const {
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
}

void OvernightIndexedCouponBase::populateAccruals() const {
    // Day count fractions for each overnight _interest_ period in the coupon. These are the daily periods from input
    // start date to input end date if observation shift is `false` and are the daily lookback periods corresponding to
    // the daily periods from input start to input end date if observation shift is `true` (and non-zero lookback).
    dt_.resize(n_);
    const DayCounter& dc = overnightIndex_->dayCounter();
    for (Size i = 0; i < n_; ++i)
        dt_[i] = dc.yearFraction(interestDates_[i], interestDates_[i + 1]);
}

OvernightCouponBuilder::OvernightCouponBuilder(
    OvernightIndexedCouponBase::Type rateType,
    const Date& paymentDate,
    Real nominal,
    const Date& startDate,
    const Date& endDate,
    const ext::shared_ptr<OvernightIndex>& overnightIndex)
    : rateType_(rateType),
      paymentDate_(paymentDate),
      nominal_(nominal),
      startDate_(startDate),
      endDate_(endDate),
      index_(overnightIndex) {

    using RateType = QuantExt::OvernightIndexedCouponBase::Type;
    auto brlCdiIndex = ext::dynamic_pointer_cast<BRLCdi>(overnightIndex);
    if (rateType_ == RateType::BrlCdi) {
        QL_REQUIRE(brlCdiIndex, "OvernightCouponBuilder: if rate type is BrlCdi, the index must be BRLCdi.");
    } else if (brlCdiIndex) {
        // Keep consistent with the constructors i.e. OvernightIndexedCoupon built with a BRL CDI index has its rate 
        // type set to BrlCdi. An averaging coupon with BRL CDI just proceeds as a normal AverageONIndexedCoupon.
        rateType_ = RateType::BrlCdi;
    }
}

OvernightCouponBuilder& OvernightCouponBuilder::withGearing(Real gearing) {
    gearing_ = gearing;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withSpread(Real spread) {
    spread_ = spread;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withRefPeriodStart(const Date& refPeriodStart) {
    refPeriodStart_ = refPeriodStart;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withRefPeriodEnd(const Date& refPeriodEnd) {
    refPeriodEnd_ = refPeriodEnd;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withDayCounter(const DayCounter& dayCounter) {
    dayCounter_ = dayCounter;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withTelescopicValueDates(bool telescopicValueDates) {
    telescopicValueDates_ = telescopicValueDates;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withIncludeSpread(bool includeSpread) {
    includeSpread_ = includeSpread;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withLookback(const Period& lookback) {
    lookback_ = lookback;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withRateCutoff(Natural rateCutoff) {
    rateCutoff_ = rateCutoff;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withFixingDays(Natural fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withRateComputationStart(const Date& rateComputationStart) {
    rateComputationStart_ = rateComputationStart;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withRateComputationEnd(const Date& rateComputationEnd) {
    rateComputationEnd_ = rateComputationEnd;
    return *this;
}

OvernightCouponBuilder& OvernightCouponBuilder::withObservationShift(bool observationShift) {
    observationShift_ = observationShift;
    return *this;
}

ext::shared_ptr<OvernightIndexedCouponBase> OvernightCouponBuilder::build() const {
    if (rateType_ == OvernightIndexedCouponBase::Type::Averaging) {
        return ext::make_shared<AverageONIndexedCoupon>(paymentDate_, nominal_, startDate_, endDate_, index_,
            gearing_, spread_, rateCutoff_, dayCounter_, lookback_, fixingDays_, rateComputationStart_,
            rateComputationEnd_, telescopicValueDates_, observationShift_);
    } else {
        return ext::make_shared<OvernightIndexedCoupon>(paymentDate_, nominal_, startDate_, endDate_, index_,
            gearing_, spread_, refPeriodStart_, refPeriodEnd_, dayCounter_, telescopicValueDates_, includeSpread_,
            lookback_, rateCutoff_, fixingDays_, rateComputationStart_, rateComputationEnd_, observationShift_);
    }
}

}
