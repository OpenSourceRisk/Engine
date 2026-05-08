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
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/utilities/vectors.hpp>
#include <algorithm>
#include <iterator>

using std::pair;
using std::tuple;
using std::vector;
using RateType = QuantExt::OvernightIndexedCouponBase::Type;

namespace {

// A small helper to set the rate type correctly for the special case of a BRL CDI coupon.
RateType rateTypeFromIndex(const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& overnightIndex) {
    if (QuantLib::ext::dynamic_pointer_cast<QuantExt::BRLCdi>(overnightIndex))
        return RateType::BrlCdi;
    else
        return RateType::Compounding;
}

}

namespace QuantExt {

OvernightIndexedCoupon::OvernightIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate,
                                               const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing,
                                               Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd,
                                               const DayCounter& dayCounter, bool telescopicValueDates,
                                               bool includeSpread, const Period& lookback, const Natural rateCutoff,
                                               const Natural fixingDays, const Date& rateComputationStartDate,
                                               const Date& rateComputationEndDate, bool observationShift,
                                               bool staleDatesCheck)
    : OvernightIndexedCouponBase(rateTypeFromIndex(overnightIndex), paymentDate, nominal, startDate, endDate,
        overnightIndex, gearing, spread, refPeriodStart, refPeriodEnd, dayCounter, telescopicValueDates, lookback,
        rateCutoff, fixingDays, rateComputationStartDate, rateComputationEndDate, observationShift, staleDatesCheck),
        includeSpread_(includeSpread) {
    if (rateType() == RateType::BrlCdi)
        setPricer(ext::make_shared<BRLCdiCouponPricer>());
    else
        setPricer(ext::make_shared<OvernightIndexedCouponPricer>());
}

void OvernightIndexedCoupon::accept(AcyclicVisitor& v) {
    if (auto v1 = dynamic_cast<Visitor<OvernightIndexedCoupon>*>(&v))
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

Real OvernightIndexedCoupon::effectiveSpread() const {
    return !includeSpread_ ? spread() : oicPricer()->effectiveSpread();
}

Real OvernightIndexedCoupon::effectiveIndexFixing() const {
    return oicPricer()->effectiveIndexFixing();
}

pair<Rate, Date> OvernightIndexedCoupon::effectiveRate(const Date& d) const {
    return oicPricer()->effectiveRate(d);
}

ext::shared_ptr<OvernightIndexedCouponPricer> OvernightIndexedCoupon::oicPricer() const {
    // Any of the methods that depend on this should not be called by a BRL CDI coupon.
    QL_REQUIRE(rateType() != RateType::BrlCdi, "BRL CDI coupon does not use an OvernightIndexedCouponPricer.");
    auto fcp = pricer();
    QL_REQUIRE(fcp, "OvernightIndexedCoupon: FloatingRateCoupon pricer is null.");
    auto p = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer());
    QL_REQUIRE(p, "OvernightIndexedCoupon: expected an OvernightIndexedCouponPricer.");
    p->initialize(*this);
    return p;
}

// OvernightIndexedCouponPricer implementation
namespace {
    // Helper functions.

    // Determine the number of underlying overnight periods up to date `date`.
    Size numberPeriods(const Date& date, const vector<Date>& intDates)
    {
        // We determine the number of _original_ interest periods up to and including date. In other words, suppose 
        // that (i_0, i_1), (i_1, i_2), ..., (i_{n-1}, i_n) are the original interest periods with i_0 = input start 
        // date and i_n = input end date. Note that i_0 and i_n may be holidays but i_1, ..., i_{n-1} are all 
        // business days. If there is a lookback with observation shift then the interest period (i_{j-1}, i_{j}) 
        // corresponds to the lookback period (LB(i_{j-1}), LB(i_{j})) := (i^'_{j-1}, i^'_{j}) where 
        // LB(x) := x - lookback overnight index fixing BDs. Note i_0 is adjusted to preceding BD if necessary and 
        // i_n to following BD if necessary before applying the lookback. Obviously, if lookback is 0, then LB(x) = x.
        // These i^'_{j} are our coupon's interestDates_. So result is:
        // - LB(date) <= i^'_{0} => res = 0
        // - LB(date) \in (i^'_{0}, i^'_{1}] => res = 1
        // - ...
        // - LB(date) \in (i^'_{n-2}, i^'_{n-1}] => res = n - 1
        // - LB(date) > i^'_{n-1} => res = n
        auto it = std::lower_bound(intDates.begin(), intDates.end(), date);
        return it != intDates.end() ? std::distance(intDates.begin(), it) : intDates.size() - 1;
    }
}

void OvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
    QL_ENSURE(coupon_, "OvernightIndexedCouponPricer::initialize: expected an OvernightIndexedCoupon.");
}

tuple<Rate, Spread, Rate, Date> OvernightIndexedCouponPricer::compute(const Date& date) const
{
    // Variables needed in the calcs below.
    auto onFixCal = coupon_->index()->fixingCalendar();
    const vector<Date>& intDates = coupon_->interestDates();

    // See note in `numberPeriods` about this date.
    Date refDate = date;
    if (coupon_->observationShift())
        refDate = onFixCal.advance(onFixCal.adjust(refDate, Following), -coupon_->lookback(), Preceding);

    // Number of periods we will need to compound over - note the usage of refDate and not date.
    const Size numPeriods = numberPeriods(refDate, intDates);

    // If we are before the start of the first interest period, then return zeros.
    if (numPeriods == 0)
        return {0.0, 0.0, 0.0, refDate};

    // --- 1. Variable set-up ---
    const Date today = Settings::instance().evaluationDate();
    auto index = ext::dynamic_pointer_cast<OvernightIndex>(coupon_->index());
    const vector<Date>& fixDates = coupon_->fixingDates();
    const vector<Date>& valDates = coupon_->valueDates();
    const vector<Time>& dt = coupon_->dt();
    const Period& lookback = coupon_->lookback();
    const bool obsShift = coupon_->observationShift();
    const bool incSpread = coupon_->includeSpread();
    const Real spread = coupon_->spread();
    const Natural rco = coupon_->rateCutoff();
    Handle<YieldTermStructure> curve = index->forwardingTermStructure();
    const Date& cpnAccStart = coupon_->separateRateCompPeriod() ? intDates.front() : coupon_->accrualStartDate();
    const Date& cpnAccEnd = coupon_->separateRateCompPeriod() ? intDates.back() : coupon_->accrualEndDate();
    const DayCounter& indexDc = index->dayCounter();
    const ext::optional<Size> tsStartIdx = coupon_->telescopicStartIdx();

    // Compound factor with and without spread which will be calculated below.
    Real compFac = 1.0;
    Real compFacNoSpd = 1.0;
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

    auto updateCompFactors = [&](Rate onRate) {
        Time dcf = dt[currPeriodIdx];
        Real scale = currPeriodIdx < numPeriods - 1 ? 1.0 : brokenPeriodScale();
        if (incSpread) {
            compFacNoSpd *= 1.0 + onRate * scale * dcf;
            compFac *= 1.0 + (onRate + spread) * scale * dcf;
        } else {
            compFac *= 1.0 + onRate * scale * dcf;
        }
        currPeriodIdx++;
    };

    auto inRateCutoffPeriod = [&]() {
        return rco > 0 && fixDates.size() - rco - 1 <= currPeriodIdx;
    };

    auto onRate = [&](bool inRcoPeriod = false) {
        Date endValDate = inRcoPeriod ? valDates.back() : valDates[currPeriodIdx + 1];
        DiscountFactor startDiscount = curve->discount(valDates[currPeriodIdx]);
        DiscountFactor endDiscount = curve->discount(endValDate);
        Real factor = startDiscount / endDiscount;
        Time dcf = indexDc.yearFraction(valDates[currPeriodIdx], endValDate);
        return (factor - 1.0) / dcf;
    };

    auto onRateRcoInd = [&]() -> pair<Rate, bool> {
        bool inRcoPeriod = inRateCutoffPeriod();
        return {onRate(inRcoPeriod), inRcoPeriod};
    };

    auto applyTsFormula = [&](const Date& start, const Date& end) {
        Real factor = curve->discount(start) / curve->discount(end);
        if (incSpread) {
            compFacNoSpd *= factor;
            compFac *= factor;
            // Approximation from formula (1.5) in DAILY SPREAD CURVES AND ESTER at https://ssrn.com/abstract=3500090.
            Integer numCalDays = end - start;
            Real avgDailyDcf = indexDc.yearFraction(start, end) / numCalDays;
            compFac *= std::pow(1.0 + avgDailyDcf * spread, numCalDays);
        } else {
            compFac *= factor;
        }
    };

    auto applyTsFormulaWithStub = [&]() {
        // Get the value dates associated with the fixing date for the underlying overnight period that
        // contains `date`. We may need an extra stub below if `date` is not an index business day.
        Date adjDate = onFixCal.adjust(date, Preceding);
        Date valDateUndStart = lookback != 0 * Days ? onFixCal.advance(adjDate, -lookback, Preceding) : adjDate;

        // Piece from value date at start of telescopic period to valDateUndStart.
        applyTsFormula(std::min(valDates[currPeriodIdx], valDateUndStart), valDateUndStart);

        // If d is a holiday, compound the additional piece.
        if (!onFixCal.isBusinessDay(date)) {
            // Get the next business day after `date` and the value date associated with it.
            Date valDateUndEnd = onFixCal.advance(valDateUndStart, 1, Days, Following);
            Date nextDate = onFixCal.advance(adjDate, 1, Days, Following);
            Real scale = indexDc.yearFraction(adjDate, date) / indexDc.yearFraction(adjDate, nextDate);

            DiscountFactor startDisc = curve->discount(valDateUndStart);
            DiscountFactor endDisc = curve->discount(valDateUndEnd);
            Real factor = startDisc / endDisc;

            Time fullDcf = indexDc.yearFraction(valDateUndStart, valDateUndEnd);
            Rate onRate = (factor - 1.0) / fullDcf;

            if (incSpread) {
                compFacNoSpd *= 1.0 + onRate * scale * fullDcf;
                compFac *= 1.0 + (onRate + spread) * scale * fullDcf;
            } else {
                compFac *= 1.0 + onRate * scale * fullDcf;
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
        QL_REQUIRE(fixing != Null<Real>(), "OvernightIndexedCouponPricer: missing " << index->name() <<
            " fixing for fixing date " << fixDate << ".");
        // Check if in rate cut-off period before updating currPeriodIdx in call to updateCompFactors.
        if (inRateCutoffPeriod())
            rcoRate = fixing;
        updateCompFactors(fixing);
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
                updateCompFactors(fixing);
            }
        } catch (Error&) {
        }
    }

    // We need a valid curve after this point, unless we have the rate cut-off rate or gone through all periods.
    QL_REQUIRE((rcoRate || currPeriodIdx == numPeriods) || !curve.empty(), "OvernightIndexedCouponPricer: null term "
        "structure set for the instance of " << index->name() << " in the overnight index coupon.");

    // If we still have fixDates[currPeriodIdx] == today after previous if branch (and no rate cut-off rate), the 
    // fixing look up failed and we need to forecast today's fixing. We have the value dates for it, so we do it.
    if (!rcoRate && currPeriodIdx < numPeriods && fixDates[currPeriodIdx] == today) {
        auto [onRate, inRcoPeriod] = onRateRcoInd();
        updateCompFactors(onRate);
        if (inRcoPeriod)
            rcoRate = onRate;
    }

    // Fixing dates in the future.
    if (!rcoRate && !coupon_->canApplyTelescopic()) {
        // If can't apply telescopic formula, loop over remaining periods and forecast the fixings.
        while (currPeriodIdx < numPeriods) {
            auto [onRate, inRcoPeriod] = onRateRcoInd();
            updateCompFactors(onRate);
            if (inRcoPeriod) {
                rcoRate = onRate;
                break;
            }
        }
    } else if (!rcoRate) {
        while (currPeriodIdx < numPeriods) {
            const bool inTsPeriod = tsStartIdx && currPeriodIdx == tsStartIdx;
            if (!coupon_->telescopicDates()) {
                // Coupon does not have telescopic dates but we can apply telescopic formula to avoid forecasting each 
                // forward ON fixing.
                // - if first date is a holiday, we need to forecast the ON rate as telescopic formula including it 
                //   will give a sligthly inaccurate result.
                // - if in final period that we are accruing up to, forecast ON rate in case `date` is not a good 
                //   business day. This also covers the case of the last ON period where the coupon end date is a 
                //   holiday - we need to forecast there also as telescopic including it is slightly inaccurate.
                // - if in rate cut-off period, get the RCO ON rate and break to go to the RCO block at end.
                if ((currPeriodIdx == 0 && onFixCal.isHoliday(intDates.front())) ||
                    currPeriodIdx == numPeriods - 1 || inRateCutoffPeriod()) {
                    auto [onRate, inRcoPeriod] = onRateRcoInd();
                    updateCompFactors(onRate);
                    if (inRcoPeriod) {
                        rcoRate = onRate;
                        break;
                    }
                } else {
                    // Telescopic formula to either start of RCO period or start of last underlying ON period.
                    Size tsEndIdx = numPeriods - 1;
                    if (rco == 0) {
                        // If `date` that we are accruing up to is a good business day, avoid an ON forecast.
                        if (numPeriods == fixDates.size() && onFixCal.isBusinessDay(date))
                            tsEndIdx++;
                    } else {
                        tsEndIdx = std::min(tsEndIdx, fixDates.size() - rco - 1);
                    }
                    applyTsFormula(valDates[currPeriodIdx], valDates[tsEndIdx]);
                    currPeriodIdx = tsEndIdx;
                }
            } else if (currPeriodIdx == numPeriods - 1 && inTsPeriod) {
                // Telescopic formula and date d is in the period associated with the telescopic period.
                applyTsFormulaWithStub();
                currPeriodIdx++;
            } else if (currPeriodIdx < numPeriods - 1 && inTsPeriod) {
                // Telescopic formula but date d is not in the period associated with the telescopic period.
                // So can just apply the full factor.
                applyTsFormula(valDates[currPeriodIdx], valDates[currPeriodIdx + 1]);
                currPeriodIdx++;
            } else {
                // May have rate cut-off periods, final / initial stub period or telescopic period may have been 1D.
                auto [onRate, inRcoPeriod] = onRateRcoInd();
                updateCompFactors(onRate);
                if (inRcoPeriod) {
                    rcoRate = onRate;
                    break;
                }
            }
        }
    }

    // We may still have rate cut-off periods to cover.
    if (rcoRate) {
        while (currPeriodIdx < numPeriods) {
            updateCompFactors(*rcoRate);
        }
    }

    // Day count fraction for the compounding period using index day counter.
    Date upToDate = std::min(refDate, intDates.back());
    Time compDcf = indexDc.yearFraction(intDates.front(), upToDate);

    // If spread is not zero and not included in compouding and we have observation shift, users most likely expect the 
    // spread to be applied on the unshifted period. This is the assumption here which is why we need to scale.
    Spread adjSpread = spread;
    if (obsShift && !coupon_->separateRateCompPeriod() && !incSpread) {
        // So that we are left with unShiftedDcf x spread when we calculate the amount.
        Time unShiftedDcf = coupon_->dayCounter().yearFraction(coupon_->accrualStartDate(), date);
        adjSpread *= unShiftedDcf / compDcf;
    }

    // The final equivalent rate over the compounding period. There are a few scenarios:
    // 1. No obs shift and no separate rate computation period. This is the most standard case. First interest date 
    //    will be cpn accrual start date and upToDate will be the input `date`. In particular, if cpn dcf == index dcf 
    //    and no spread / gearing, the dcf will cancel and cpn amount = (cmpFac - 1.0) x Ntl
    // 2. Obs shift but no separate rate computation period. First interest date is first date in shifted period and 
    //    upToDate is the end of the shifted ON period containing `date`. In particular, if cpn dcf == index dcf 
    //    and no spread / gearing, the dcf will cancel and cpn amount = (cmpFac - 1.0) x Ntl where cmpFac is over the 
    //    shifted period as expected. As noted above, a non-included spread will be on the unshifted period.
    // 3. Separate rate computation period. First interest date is rate computation start date and could be anywhere. 
    //    In this case, the rate is just calculated over the computation period and the usual coupon dcf is applied 
    //    against it to get the amount.
    Rate rate = (compFac - 1.0) / compDcf;

    // This rate cannot be scaled as it is used in FallbackIborIndex.
    Rate swapletRate = !incSpread ? coupon_->gearing() * rate + adjSpread : coupon_->gearing() * rate;

    // Effective values that together give the OvernightIndexedCoupon amount.
    Spread effectiveSpread = !incSpread ? adjSpread : rate - (compFacNoSpd - 1.0) / compDcf;
    Rate effectiveIndexFixing = !incSpread ? rate : rate - effectiveSpread;
    if (obsShift && !coupon_->separateRateCompPeriod()) {
        Time unShiftedDcf = coupon_->dayCounter().yearFraction(coupon_->accrualStartDate(), date);
        effectiveIndexFixing *= compDcf / unShiftedDcf;
        if (incSpread)
            effectiveSpread *= compDcf / unShiftedDcf;
    }

    return {swapletRate, effectiveSpread, effectiveIndexFixing, upToDate};
}

Rate OvernightIndexedCouponPricer::swapletRate() const {
    return std::get<0>(rateSpreadFixing());
}

Rate OvernightIndexedCouponPricer::effectiveSpread() const {
    return std::get<1>(rateSpreadFixing());
}

Rate OvernightIndexedCouponPricer::effectiveIndexFixing() const {
    return std::get<2>(rateSpreadFixing());
}

std::pair<Rate, Date> OvernightIndexedCouponPricer::effectiveRate(const Date& date) const {
    auto res = compute(date);
    return {std::get<0>(res), std::get<3>(res)};
}

tuple<Rate, Spread, Rate> OvernightIndexedCouponPricer::rateSpreadFixing() const {
    Date d = coupon_->rateComputationEndDate() != Date() ?
        coupon_->rateComputationEndDate() : coupon_->accrualEndDate();
    auto res = compute(d);
    return {std::get<0>(res), std::get<1>(res), std::get<2>(res)};
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
    strippedCapletVolatility_ = p->strippedCapletVolatility();
    strippedFloorletVolatility_ = p->strippedFloorletVolatility();
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

Real CappedFlooredOvernightIndexedCoupon::strippedCapletVolatility() const {
    calculate();
    return strippedCapletVolatility_;
}

Real CappedFlooredOvernightIndexedCoupon::strippedFloorletVolatility() const {
    calculate();
    return strippedFloorletVolatility_;
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
    const Handle<OptionletVolatilityStructure>& v)
    : capletVol_(v) {
    registerWith(capletVol_);
}

Real CappedFlooredOvernightIndexedCouponPricer::effectiveCapletVolatility() const { return effectiveCapletVolatility_; }

Real CappedFlooredOvernightIndexedCouponPricer::effectiveFloorletVolatility() const {
    return effectiveFloorletVolatility_;
}

Real CappedFlooredOvernightIndexedCouponPricer::strippedCapletVolatility() const { return strippedCapletVolatility_; }

Real CappedFlooredOvernightIndexedCouponPricer::strippedFloorletVolatility() const {
    return strippedFloorletVolatility_;
}

Handle<OptionletVolatilityStructure> CappedFlooredOvernightIndexedCouponPricer::capletVolatility() const {
    return capletVol_;
}

// OvernightLeg implementation
OvernightLeg::OvernightLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& i)
    : schedule_(schedule), overnightIndex_(i), paymentCalendar_(schedule.calendar()), paymentAdjustment_(Following),
      paymentLag_(0), telescopicValueDates_(false), includeSpread_(false), lookback_(0 * Days), rateCutoff_(0),
      fixingDays_(Null<Size>()), nakedOption_(false), localCapFloor_(false), inArrears_(true), observationShift_(true),
      staleDatesCheck_(true) {}

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

OvernightLeg& OvernightLeg::withOvernightIndexedCouponPricer(
    const ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer) {
    couponPricer_ = couponPricer;
    return *this;
}

OvernightLeg& OvernightLeg::withCapFlooredOvernightIndexedCouponPricer(
    const QuantLib::ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& couponPricer) {
    capFlooredCouponPricer_ = couponPricer;
    return *this;
}

OvernightLeg& OvernightLeg::withObservationShift(bool observationShift) {
    observationShift_ = observationShift;
    return *this;
}

OvernightLeg& OvernightLeg::withStaleDatesCheck(bool staleDatesCheck) {
    staleDatesCheck_ = staleDatesCheck;
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
                rateComputationEndDate, observationShift_, staleDatesCheck_);
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
