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

#include <qle/cashflows/averageonindexedcouponpricer.hpp>

using std::pair;
using std::vector;

namespace QuantExt {

// Note: parts of the implementation here are copied from OvernightIndexedCouponPricer. It isn't great and it might be 
//      worth refactoring the two pricers to share more code. Leave this as a todo for now.

namespace {
// Determine the number of underlying overnight periods up to date `date`.
Size numberPeriods(const Date& date, const vector<Date>& intDates) {
    // Taken from OvernightIndexedCouponPricer. See explanation there.
    auto it = std::lower_bound(intDates.begin(), intDates.end(), date);
    return it != intDates.end() ? std::distance(intDates.begin(), it) : intDates.size() - 1;
}
}

void AverageONIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const AverageONIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "AverageONIndexedCouponPricer::initialize: expected an AverageONIndexedCoupon.");
}

Rate AverageONIndexedCouponPricer::swapletRate() const {
    Date d = coupon_->rateComputationEndDate() != Date() ?
        coupon_->rateComputationEndDate() : coupon_->accrualEndDate();
    return effectiveRate(d).first;
}

std::pair<Rate, Date> AverageONIndexedCouponPricer::effectiveRate(const Date& date) const {

    // Variables needed in the calcs below.
    auto onFixCal = coupon_->index()->fixingCalendar();
    const vector<Date>& intDates = coupon_->interestDates();

    // See note in `numberPeriods` about this date.
    Date refDate = date;
    if (coupon_->observationShift())
        refDate = onFixCal.advance(onFixCal.adjust(refDate, Following), -coupon_->lookback(), Preceding);

    // Number of periods we will need to average over. Note the usage of refDate and not date.
    const Size numPeriods = numberPeriods(refDate, intDates);

    // If we are before the start of the first interest period, then return zero.
    if (numPeriods == 0)
        return {0.0, refDate};

    // --- 1. Variable set-up ---
    const Date today = Settings::instance().evaluationDate();
    auto index = ext::dynamic_pointer_cast<OvernightIndex>(coupon_->index());
    const vector<Date>& fixDates = coupon_->fixingDates();
    const vector<Date>& valDates = coupon_->valueDates();
    const vector<Time>& dt = coupon_->dt();
    const Period& lookback = coupon_->lookback();
    const bool obsShift = coupon_->observationShift();
    const Natural rco = coupon_->rateCutoff();
    Handle<YieldTermStructure> curve = index->forwardingTermStructure();
    const Date& cpnAccStart = coupon_->separateRateCompPeriod() ? intDates.front() : coupon_->accrualStartDate();
    const Date& cpnAccEnd = coupon_->separateRateCompPeriod() ? intDates.back() : coupon_->accrualEndDate();
    const DayCounter& indexDc = index->dayCounter();
    const Natural indexFixDays = index->fixingDays();
    const Natural cpnFixDays = coupon_->fixingDays();
    const bool intValDatesAlign = (lookback.length() == 0 || obsShift) && cpnFixDays == indexFixDays;
    const ext::optional<Size> tsStartIdx = coupon_->telescopicStartIdx();

    // Average rate will be calculated below.
    Real avgRate = 0.0;
    Size currPeriodIdx = 0;
    // --- End of variable set-up ---

    // --- 2. Lambdas to help with the logic below ---
    auto brokenPeriodScale = [&]() {
        // Note: intDates[periodIdx] < refDate <= intDates[periodIdx + 1]
        //   <=> Unshifted(intDates[periodIdx]) < date <= Unshifted(intDates[periodIdx])
        if (!obsShift) {
            // No observation shift.
            if (date == intDates[currPeriodIdx + 1])
                // Full period, return 1.
                return 1.0;
            else
                // Broken period, calculate dcf from interest period start to date.
                return indexDc.yearFraction(intDates[currPeriodIdx], date) /
                    indexDc.yearFraction(intDates[currPeriodIdx], intDates[currPeriodIdx + 1]);
        } else {
            // Observation shift.
            const Date& intEnd = intDates[currPeriodIdx + 1];
            Date usIntEnd = currPeriodIdx == fixDates.size() - 1 ? cpnAccEnd :
                onFixCal.advance(intEnd, lookback, Following);
            if (date == usIntEnd) {
                // Full period, return 1.
                return 1.0;
            } else {
                // Broken period, scale dcf by portion of d in the unshifted period.
                const Date& intStart = intDates[currPeriodIdx];
                Date usIntStart = currPeriodIdx == 0 ? cpnAccStart :
                    onFixCal.advance(intStart, lookback, Following);
                return indexDc.yearFraction(usIntStart, date) / indexDc.yearFraction(usIntStart, usIntEnd);
            }
        }
    };

    auto updateAvgRate = [&](Rate onRate) {
        Time dcf = dt[currPeriodIdx];
        Real scale = currPeriodIdx < numPeriods - 1 ? 1.0 : brokenPeriodScale();
        avgRate += onRate * scale * dcf;
        currPeriodIdx++;
    };

    auto inRateCutoffPeriod = [&]() {
        return rco > 0 && fixDates.size() - rco - 1 <= currPeriodIdx;
    };

    auto onRate = [&](bool inRcoPeriod = false) {
        Date end = inRcoPeriod ? valDates.back() : valDates[currPeriodIdx + 1];
        const Date& start = valDates[currPeriodIdx];
        return (curve->discount(start) / curve->discount(end) - 1.0) / indexDc.yearFraction(start, end);
    };

    auto onRateRcoInd = [&]() -> pair<Rate, bool> {
        bool inRcoPeriod = inRateCutoffPeriod();
        return {onRate(inRcoPeriod), inRcoPeriod};
    };

    auto applyTakadaFormula = [&](Size startIdx, Size endIdx, bool checkGap = true) {
        if (startIdx >= endIdx)
            return;

        const Date& start = valDates[startIdx];
        const Date& end = valDates[endIdx];

        if (checkGap && onFixCal.advance(start, 1, Days, Following) == end) {
            if (intValDatesAlign) {
                avgRate += curve->discount(start) / curve->discount(end) - 1.0;
            } else {
                Time valDcf = indexDc.yearFraction(valDates[startIdx], valDates[endIdx]);
                auto onRate = (curve->discount(start) / curve->discount(end) - 1.0) / valDcf;
                avgRate += onRate * indexDc.yearFraction(intDates[startIdx], intDates[endIdx]);
            }
        } else {
            if (intValDatesAlign) {
                avgRate += log(curve->discount(start) / curve->discount(end));
            } else {
                Time valDcf = indexDc.yearFraction(start, end);
                Time intDcf = indexDc.yearFraction(intDates[startIdx], intDates[endIdx]);
                Real approx = log(curve->discount(start) / curve->discount(end));
                avgRate += approx * intDcf / valDcf;
            }
        }
    };

    auto applyTakadaFormulaWithStub = [&]() {
        // Get the value dates associated with the fixing date for the underlying overnight period that
        // contains `date`. We may need an extra stub below if `date` is not an index business day.
        Date adjDate = onFixCal.adjust(date, Preceding);
        Date valDateUndStart = lookback != 0 * Days ? onFixCal.advance(adjDate, -lookback, Preceding) : adjDate;
        Date intDateUndStart = obsShift ? valDateUndStart : adjDate;
        valDateUndStart = cpnFixDays != indexFixDays ?
            onFixCal.advance(valDateUndStart, -cpnFixDays, Days, Preceding) : valDateUndStart;

        // Piece from value date at start of telescopic period to valDateUndStart.
        if (valDates[currPeriodIdx] < valDateUndStart) {
            if (onFixCal.advance(valDates[currPeriodIdx], 1, Days, Following) == valDateUndStart) {
                if (intValDatesAlign) {
                    avgRate += curve->discount(valDates[currPeriodIdx]) / curve->discount(valDateUndStart) - 1.0;
                } else {
                    Time valDcf = indexDc.yearFraction(valDates[currPeriodIdx], valDateUndStart);
                    Rate onRate = (curve->discount(valDates[currPeriodIdx]) 
                        / curve->discount(valDateUndStart) - 1.0) / valDcf;
                    avgRate += onRate * indexDc.yearFraction(intDates[currPeriodIdx], intDateUndStart);
                }
            } else {
                if (intValDatesAlign) {
                    avgRate += log(curve->discount(valDates[currPeriodIdx]) / curve->discount(valDateUndStart));
                } else {
                    Time valDcf = indexDc.yearFraction(valDates[currPeriodIdx], valDateUndStart);
                    Time intDcf = indexDc.yearFraction(intDates[currPeriodIdx], intDateUndStart);
                    Real approx = log(curve->discount(valDates[currPeriodIdx]) / curve->discount(valDateUndStart));
                    avgRate += approx * intDcf / valDcf;
                }
            }
        }

        // If d is a holiday, add the additional stub piece.
        if (!onFixCal.isBusinessDay(date)) {
            Date valDateUndEnd = onFixCal.advance(valDateUndStart, 1, Days, Following);
            Time fullDcf = indexDc.yearFraction(valDateUndStart, valDateUndEnd);
            DiscountFactor startDisc = curve->discount(valDateUndStart);
            DiscountFactor endDisc = curve->discount(valDateUndEnd);
            Rate onRate = (startDisc / endDisc - 1.0) / fullDcf;
            if (obsShift) {
                Date nextDate = onFixCal.advance(adjDate, 1, Days, Following);
                Real scale = indexDc.yearFraction(adjDate, date) / indexDc.yearFraction(adjDate, nextDate);
                avgRate += onRate * scale * fullDcf;
            } else {
                avgRate += onRate * indexDc.yearFraction(adjDate, date);
            }
        }
    };
    // --- End of helper lambdas ---

    // --- 3. Start of main valuation loop ---
    // Evaluation date, EVD, is the threshold.
    // - fixing date < EVD: assume fixing is known. Not necessarily true e.g. valuing SOFR coupon on business day in 
    //                      Europe that is a SOFR US holiday, the fixing for the previous SOFR business day (i.e. 
    //                      fixing date < EVD) will not be available until 08:00 ET the next SOFR business day.
    // - fixing date = EVD: try to get fixing and use it, if not available forecast.
    // - fixing date > EVD: forecast.

    // If at any point we are in the rate cut-off period, we will use this variable to indicate it and also to store 
    // the rate. We will skip any further calculations below and perform the rate cut-off logic at the end.
    ext::optional<Rate> rcoRate;

    // Already fixed part with the caveat in the comment above.
    while (currPeriodIdx < numPeriods && fixDates[currPeriodIdx] < today) {
        const Date& fixDate = fixDates[currPeriodIdx];
        Rate fixing = index->pastFixing(fixDate);
        QL_REQUIRE(fixing != Null<Real>(), "AverageONIndexedCouponPricer: missing " << index->name() <<
            " fixing for fixing date " << fixDate << ".");
        // Check if in rate cut-off period before updating currPeriodIdx in call to updateAvgRate.
        if (inRateCutoffPeriod())
            rcoRate = fixing;
        updateAvgRate(fixing);
        // If we are in the rate cut-off period, remaining periods will be handled below.
        if (rcoRate)
            break;
    }

    // If the fixing date for the current period is today, try to get the fixing and use it. If it is not available, we
    // don't update the current period index and we will forecast it below.
    if (!rcoRate && currPeriodIdx < numPeriods && fixDates[currPeriodIdx] == today) {
        try {
            Rate fixing = index->pastFixing(today);
            if (fixing != Null<Real>()) {
                if (inRateCutoffPeriod())
                    rcoRate = fixing;
                updateAvgRate(fixing);
            }
        } catch (Error&) {
        }
    }

    // We need a valid curve after this point, unless we have the rate cut-off rate or gone through all periods.
    QL_REQUIRE((rcoRate || currPeriodIdx == numPeriods) || !curve.empty(), "AverageONIndexedCouponPricer: null term "
        "structure set for the instance of " << index->name() << " in the overnight index coupon.");

    // If we still have fixDates[currPeriodIdx] == today after previous if branch (and no rate cut-off rate), the 
    // fixing look up failed and we need to forecast today's fixing. We have the value dates for it, so we do it.
    if (!rcoRate && currPeriodIdx < numPeriods && fixDates[currPeriodIdx] == today) {
        auto [onRate, inRcoPeriod] = onRateRcoInd();
        updateAvgRate(onRate);
        if (inRcoPeriod)
            rcoRate = onRate;
    }

    // We need a valid approximation type now, unless we have the rate cut-off rate or gone through all periods.
    QL_REQUIRE((rcoRate || currPeriodIdx == numPeriods) || (approximationType_ == None || approximationType_ == Takada),
        "AverageONIndexedCouponPricer::effectiveRate: approximation type should be None or Takada.");

    // Fixing dates in the future.
    if (!rcoRate && coupon_->telescopicDates()) {
        // Telescopic dates so apply the Takada approximation (even if approximationType_ == None)
        while (currPeriodIdx < numPeriods) {
            const bool inTsPeriod = tsStartIdx && currPeriodIdx == tsStartIdx;
            if (currPeriodIdx == numPeriods - 1 && inTsPeriod) {
                // Telescopic formula and date d is in the period associated with the telescopic period.
                applyTakadaFormulaWithStub();
                currPeriodIdx++;
            } else if (currPeriodIdx < numPeriods - 1 && inTsPeriod) {
                // Telescopic formula but date d is not in the period associated with the telescopic period.
                // So can just apply Takada formula on the full period.
                applyTakadaFormula(currPeriodIdx, currPeriodIdx + 1, false);
                currPeriodIdx++;
            } else {
                // May have rate cut-off periods, final / initial stub period or telescopic period may have been 1D.
                auto [onRate, inRcoPeriod] = onRateRcoInd();
                updateAvgRate(onRate);
                if (inRcoPeriod) {
                    rcoRate = onRate;
                    break;
                }
            }
        }
    } else if (!rcoRate && approximationType_ == None) {
        // If we don't have telescopic dates and not using approximation, loop over remaining periods and forecast.
        while (currPeriodIdx < numPeriods) {
            auto [onRate, inRcoPeriod] = onRateRcoInd();
            updateAvgRate(onRate);
            if (inRcoPeriod) {
                rcoRate = onRate;
                break;
            }
        }
    } else if (!rcoRate) {
        // No telescopic dates, Takada approximation requested (and not in rate cut-off period).
        if (currPeriodIdx < numPeriods) {
            // If we haven't processed the first underlying overnight period and it starts on a holiday, we 
            // process it separately here.
            if (currPeriodIdx == 0 && onFixCal.isHoliday(cpnAccStart)) {
                auto [onRate, inRcoPeriod] = onRateRcoInd();
                updateAvgRate(onRate);
                if (inRcoPeriod)
                    rcoRate = onRate;
            }

            if (!rcoRate && currPeriodIdx < numPeriods) {
                // Index of the first period in the rate cut-off period or the last period if no rate cut-off.
                Size rcoPeriodIdx = fixDates.size() - rco - 1;
                if (rco == 0 && numPeriods == fixDates.size() && onFixCal.isHoliday(cpnAccEnd)) {
                    // If we are proessing all periods and the last day of the last underlying overnight period (i.e.
                    // coupon accrual end date) is a holiday, we need to apply approximation up to end of prior 
                    // underlying overnight period and then handle the final stub.
                    if (currPeriodIdx < numPeriods - 1) {
                        applyTakadaFormula(currPeriodIdx, numPeriods - 1);
                        currPeriodIdx = numPeriods - 1;
                    }
                    updateAvgRate(onRate(false));
                } else if (rco == 0 || numPeriods <= rcoPeriodIdx) {
                    // If rate cut-off and `date` is in a period before start of it or no rate cut-off, we just apply
                    // the Takada approximation up to `date`.
                    applyTakadaFormulaWithStub();
                    currPeriodIdx = numPeriods - 1;
                } else {
                    // `date` is in rate cut-off period, so apply Takada approximation up to the start of the rate
                    // cut-off period and then apply rate cut-off value below.
                    applyTakadaFormula(currPeriodIdx, rcoPeriodIdx);
                    currPeriodIdx = rcoPeriodIdx;
                    rcoRate = onRate(true);
                }
            }
        }
    }

    // We may still have rate cut-off periods to cover.
    if (rcoRate) {
        while (currPeriodIdx < numPeriods) {
            updateAvgRate(*rcoRate);
        }
    }

    // Day count fraction for the averaging period using index day counter.
    Date upToDate = std::min(refDate, intDates.back());
    Time avgPeriodDcf = indexDc.yearFraction(intDates.front(), upToDate);

    // If spread is not zero and we have observation shift, users most likely expect the spread to be applied on the 
    // unshifted period. This is the assumption here which is why we need to scale.
    Spread adjSpread = coupon_->spread();
    if (obsShift && !coupon_->separateRateCompPeriod()) {
        // So that we are left with unShiftedDcf x spread when we calculate the amount.
        Time unShiftedDcf = coupon_->dayCounter().yearFraction(coupon_->accrualStartDate(), date);
        adjSpread *= unShiftedDcf / avgPeriodDcf;
    }

    // Give the final result
    Rate rate = coupon_->gearing() * avgRate / avgPeriodDcf + adjSpread;
    return {rate, upToDate};
}

}
