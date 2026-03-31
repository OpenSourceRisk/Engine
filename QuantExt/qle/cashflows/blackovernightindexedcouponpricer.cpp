/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

using std::vector;

namespace QuantExt {

void BlackOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
    gearing_ = coupon.gearing();
    index_ = ext::dynamic_pointer_cast<OvernightIndex>(coupon.index());
    if (!index_) {
        // check if the coupon was right
        const CappedFlooredOvernightIndexedCoupon* c =
            dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon);
        QL_REQUIRE(c, "BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
        // coupon was right, index is not
        QL_FAIL("BlackOvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon required");
    }
    swapletRate_ = coupon_->underlying()->rate();
    effectiveIndexFixing_ = coupon_->underlying()->effectiveIndexFixing();
    effectiveCapletVolatility_ = effectiveFloorletVolatility_ = Null<Real>();
}

Real BlackOvernightIndexedCouponPricer::optionletRateGlobal(Option::Type optionType, Real effStrike) const {
    Date lastRelevantFixingDate = coupon_->underlying()->fixingDate();
    if (lastRelevantFixingDate <= Settings::instance().evaluationDate()) {
        // the amount is determined
        Real a, b;
        if (optionType == Option::Call) {
            a = effectiveIndexFixing_;
            b = effStrike;
        } else {
            a = effStrike;
            b = effectiveIndexFixing_;
        }
        return gearing_ * std::max(a - b, 0.0);
    } else {
        // not yet determined, use Black model
        QL_REQUIRE(!capletVolatility().empty(), "BlackOvernightIndexedCouponPricer: missing optionlet volatility");
        const std::vector<Date>& fixingDates = coupon_->underlying()->fixingDates();
        QL_REQUIRE(!fixingDates.empty(), "BlackOvernightIndexedCouponPricer: empty fixing dates");
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Real shift = capletVolatility()->displacement();
        Real stdDev, strippedVol;
        Real effectiveTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
        if (capletVolatility()->useEffectiveVolatility()) {
            // vol input is effective, i.e. we use a plain black model
            strippedVol = capletVolatility()->volatility(coupon_->underlying()->fixingDateNoCutoff(), effStrike);
            stdDev = strippedVol * std::sqrt(effectiveTime);
        } else {
            // vol input is not effective:
            // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates,
            // section 6.3. the idea is to dampen the average volatility sigma between the fixing start and fixing end
            // date by a linear function going from (fixing start, 1) to (fixing end, 0)
            Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
            Real fixingEndTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
            Real T = std::max(fixingStartTime, 0.0);
            if (!close_enough(fixingEndTime, T))
                T += std::pow(fixingEndTime - T, 3.0) / std::pow(fixingEndTime - fixingStartTime, 2.0) / 3.0;
            strippedVol = capletVolatility()->volatility(
                std::max(fixingDates.front(), capletVolatility()->referenceDate() + 1), effStrike);
            stdDev = strippedVol * std::sqrt(T);
        }
        if (optionType == Option::Type::Call) {
            effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            strippedCapletVolatility_ = strippedVol;
        }
        else {
            effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);
            strippedFloorletVolatility_ = strippedVol;
        }
        Real fixing = shiftedLn ? blackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0, shift)
                                : bachelierBlackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0);
        return gearing_ * fixing;
    }
}

namespace {
Real cappedFlooredRate(Real r, Option::Type optionType, Real k) {
    if (optionType == Option::Call) {
        return std::min(r, k);
    } else {
        return std::max(r, k);
    }
}
} // namespace

Real BlackOvernightIndexedCouponPricer::optionletRateLocal(Option::Type optionType, Real effStrike) const {

    QL_REQUIRE(!capletVolatility()->useEffectiveVolatility(),
               "BlackAverageONIndexedCouponPricer::optionletRateLocal() does not support effective volatility input.");

    // We compute a rate and a rawRate such that
    // rate * tau * nominal is the amount of the coupon with locally (i.e. daily) capped / floored rates
    // rawRate * tau * nominal is the amount of the coupon without capping / flooring the rate
    // We will then return the difference between rate and rawRate (with the correct sign, see below)
    // as the option component of the coupon.

    // See CappedFlooredOvernightIndexedCoupon::effectiveCap(), Floor() for what is passed in as effStrike.
    // From this we back out the absolute strike at which the
    // - daily rate + spread (spread included) or the
    // - daily rate (spread excluded)
    // is capped / floored.

    Real absStrike = coupon_->underlying()->includeSpread() ? effStrike + coupon_->underlying()->spread() : effStrike;

    // This following code is inevitably quite similar to the plain ON coupon pricer code, possibly we can refactor
    // this, but as a first step it seems safer to add the full modified code explicitly here and leave the original
    // code alone.

    ext::shared_ptr<OvernightIndex> index = coupon_->underlying()->overnightIndex();

    const std::vector<Date>& fixingDates = coupon_->underlying()->fixingDates();
    const std::vector<Time>& dt = coupon_->underlying()->dt();

    Size n = dt.size();
    Size i = 0;
    QL_REQUIRE(coupon_->underlying()->rateCutoff() < n,
               "rate cutoff (" << coupon_->underlying()->rateCutoff()
                               << ") must be less than number of fixings in period (" << n << ")");
    Natural rco = coupon_->underlying()->rateCutoff();
    Size nCutoff = n - rco;

    Real compoundFactor = 1.0, compoundFactorRaw = 1.0;

    // Recall below, when we have a rate cut-off (aka lockout) of m > 0, the last m + 1 dates in fixingDates are the 
    // same, the last date in valueDates is the end of the underlying ON period corresponding to the lockout fixing 
    // date and the m + 1 dates in valueDates prior to the last date are the same and equal to the start of the 
    // underlying ON period corresponding to the lockout fixing date. So, with a rate cut-off of 2 for example, we have:
    // fixingDates = [...,       fd_{n-3},       fd_{n-3},       fd_{n-3}]
    // valueDates  = [..., VD_s(fd_{n-3}), VD_s(fd_{n-3}), VD_s(fd_{n-3}), VD_e(fd_{n-3})]
    // where VD_s(fd) and VD_e(fd) give the start and end dates of the underlying ON period given a fixing date fd.

    // already fixed part
    Date today = Settings::instance().evaluationDate();
    while (i < n && fixingDates[i] < today) {
        // rate must have been fixed
        Rate pastFixing = index->pastFixing(fixingDates[i]);
        QL_REQUIRE(pastFixing != Null<Real>(),
                   "Missing " << index->name() << " fixing for " << fixingDates[i]);
        if (coupon_->underlying()->includeSpread()) {
            pastFixing += coupon_->spread();
        }
        compoundFactor *= 1.0 + cappedFlooredRate(pastFixing, optionType, absStrike) * dt[i];
        compoundFactorRaw *= 1.0 + pastFixing * dt[i];
        ++i;
    }

    // today is a border case
    while (i < n && fixingDates[i] == today) {
        // might have been fixed
        try {
            Rate pastFixing = index->pastFixing(today);
            if (pastFixing != Null<Real>()) {
                if (coupon_->underlying()->includeSpread()) {
                    pastFixing += coupon_->spread();
                }
                compoundFactor *= 1.0 + cappedFlooredRate(pastFixing, optionType, absStrike) * dt[i];
                compoundFactorRaw *= 1.0 + pastFixing * dt[i];
                ++i;
            } else {
                break; // fall through and forecast
            }
        } catch (Error&) {
            break; // fall through and forecast
        }
    }

    // forward part, approximation by pricing a cap / floor in the middle of the future period
    const std::vector<Date>& dates = coupon_->underlying()->valueDates();
    const vector<Date>& intDates = coupon_->underlying()->interestDates();
    if (i < n) {
        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

        Real factor = 1.0;
        if (rco > 0) {
            // Can be on lockout date here if lockout date == today and there was no fixing for today. This is the
            // condition (rco > 0 && i == nCutoff - 1). Otherwise
            if (i < nCutoff - 1)
                factor = curve->discount(dates[i]) / curve->discount(dates[nCutoff - 1]);
        } else {
            // Don't make accomodations here for final interest date being a holiday, lookback w/o obs shift etc. like 
            // in the main ON pricers. It probably matters less here as we are making approximations in the pricing 
            // below but we may want to revisit if necessary.
            factor = curve->discount(dates[i]) / curve->discount(dates[n]);
        }

        // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
        if (rco > 0) {
            Rate onRate = (curve->discount(dates[nCutoff - 1]) / curve->discount(dates.back()) - 1.0) /
                index->dayCounter().yearFraction(dates[nCutoff-1], dates.back());
            for (Size j = nCutoff - 1; j < n; j++)
                factor *= 1.0 + onRate * dt[j];
        }

        // estimate the average daily rate over the future period (approximate the continuously compounded rate)
        Real tau = coupon_->dayCounter().yearFraction(dates[i], dates.back());
        Real averageRate = std::log(factor) / tau;

        // compute the value of a cap or floor with fixing in the middle of the future period
        // (but accounting for the rate cutoff here)
        Time midPoint =
            (capletVolatility()->timeFromReference(dates[i]) + capletVolatility()->timeFromReference(dates[nCutoff])) /
            2.0;
        Real stdDev = capletVolatility()->volatility(midPoint, effStrike) * std::sqrt(midPoint);
        Real shift = capletVolatility()->displacement();
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Rate cfValue = shiftedLn ? blackFormula(optionType, effStrike, averageRate, stdDev, 1.0, shift)
                                 : bachelierBlackFormula(optionType, effStrike, averageRate, stdDev, 1.0);
        Real effectiveTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
        if (optionType == Option::Type::Call)
            effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
        else
            effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);

        // add spread to average rate
        if (coupon_->underlying()->includeSpread()) {
            averageRate += coupon_->underlying()->spread();
        }

        // incorporate cap/floor into average rate
        Real averageRateRaw = averageRate;
        averageRate += optionType == Option::Call ? (-cfValue) : cfValue;

        // now assume the averageRate is the effective rate over the future period and update the compoundFactor
        // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE": set tau to avg value
        Natural numCalDays = intDates.back() - intDates[i];
        Real dailyTau = coupon_->underlying()->dayCounter().yearFraction(intDates[i], intDates.back()) / numCalDays;
        // now use formula (4) from the paper
        compoundFactor *= std::pow(1.0 + dailyTau * averageRate, static_cast<int>(numCalDays));
        compoundFactorRaw *= std::pow(1.0 + dailyTau * averageRateRaw, static_cast<int>(numCalDays));
    }

    Rate tau = !coupon_->underlying()->hasLookback() ? coupon_->accrualPeriod() :
        coupon_->dayCounter().yearFraction(intDates.front(), intDates.back());
    Rate rate = (compoundFactor - 1.0) / tau;
    Rate rawRate = (compoundFactorRaw - 1.0) / tau;

    rate *= coupon_->underlying()->gearing();
    rawRate *= coupon_->underlying()->gearing();

    if (!coupon_->underlying()->includeSpread()) {
        rate += coupon_->underlying()->spread();
        rawRate += coupon_->underlying()->spread();
    }

    // return optionletRate := r - rawRate, i.e. the option component only
    // (see CappedFlooredOvernightIndexedCoupon::rate() for the signs of the capletRate / flooredRate)

    return (optionType == Option::Call ? -1.0 : 1.0) * (rate - rawRate);
}

Rate BlackOvernightIndexedCouponPricer::swapletRate() const { return swapletRate_; }

Rate BlackOvernightIndexedCouponPricer::capletRate(Rate effectiveCap) const {
    return coupon_->localCapFloor() ? optionletRateLocal(Option::Call, effectiveCap)
                                    : optionletRateGlobal(Option::Call, effectiveCap);
}

Rate BlackOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor) const {
    return coupon_->localCapFloor() ? optionletRateLocal(Option::Put, effectiveFloor)
                                    : optionletRateGlobal(Option::Put, effectiveFloor);
}

Real BlackOvernightIndexedCouponPricer::swapletPrice() const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::swapletPrice() not provided");
}
Real BlackOvernightIndexedCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::capletPrice() not provided");
}
Real BlackOvernightIndexedCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("BlackOvernightIndexedCouponPricer::floorletPrice() not provided");
}

void BlackAverageONIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const CappedFlooredAverageONIndexedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "BlackAverageONIndexedCouponPricer: CappedFlooredAverageONIndexedCoupon required");
    gearing_ = coupon.gearing();
    index_ = ext::dynamic_pointer_cast<OvernightIndex>(coupon.index());
    if (!index_) {
        // check if the coupon was right
        const CappedFlooredAverageONIndexedCoupon* c =
            dynamic_cast<const CappedFlooredAverageONIndexedCoupon*>(&coupon);
        QL_REQUIRE(c, "BlackAverageONIndexedCouponPricer: CappedFlooredAverageONIndexedCoupon required");
        // coupon was right, index is not
        QL_FAIL("BlackAverageONIndexedCouponPricer: CappedFlooredAverageONIndexedCoupon required");
    }
    swapletRate_ = coupon_->underlying()->rate();
    forwardRate_ = (swapletRate_ - coupon_->underlying()->spread()) / coupon_->underlying()->gearing();
    effectiveCapletVolatility_ = effectiveFloorletVolatility_ = Null<Real>();
}

Real BlackAverageONIndexedCouponPricer::optionletRateGlobal(Option::Type optionType, Real effStrike) const {
    Date lastRelevantFixingDate = coupon_->underlying()->fixingDate();
    if (lastRelevantFixingDate <= Settings::instance().evaluationDate()) {
        // the amount is determined
        Real a, b;
        if (optionType == Option::Call) {
            a = forwardRate_;
            b = effStrike;
        } else {
            a = effStrike;
            b = forwardRate_;
        }
        return gearing_ * std::max(a - b, 0.0);
    } else {
        // not yet determined, use Black model
        QL_REQUIRE(!capletVolatility().empty(), "BlackAverageONIndexedCouponPricer: missing optionlet volatility");
        const std::vector<Date>& fixingDates = coupon_->underlying()->fixingDates();
        QL_REQUIRE(!fixingDates.empty(), "BlackAverageONIndexedCouponPricer: empty fixing dates");
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Real shift = capletVolatility()->displacement();
        Real stdDev, strippedVol;
        Real effectiveTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
        if (capletVolatility()->useEffectiveVolatility()) {
            // vol input is effective, i.e. we use a plain black model
            strippedVol = capletVolatility()->volatility(coupon_->underlying()->fixingDateNoCutoff(), effStrike);
            stdDev = strippedVol * std::sqrt(effectiveTime);
        } else {
            // vol input is not effective:
            // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates,
            // section 6.3. the idea is to dampen the average volatility sigma between the fixing start and fixing end
            // date by a linear function going from (fixing start, 1) to (fixing end, 0)
            Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
            Real fixingEndTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
            strippedVol = capletVolatility()->volatility(
                std::max(fixingDates.front(), capletVolatility()->referenceDate() + 1), effStrike);
            Real T = std::max(fixingStartTime, 0.0);
            if (!close_enough(fixingEndTime, T))
                T += std::pow(fixingEndTime - T, 3.0) / std::pow(fixingEndTime - fixingStartTime, 2.0) / 3.0;
            stdDev = strippedVol * std::sqrt(T);
        }
        if (optionType == Option::Type::Call) {
            effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            strippedCapletVolatility_ = strippedVol;
        }
        else {
            effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);
            strippedFloorletVolatility_ = strippedVol;
        }
        Real fixing = shiftedLn ? blackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0, shift)
                                : bachelierBlackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0);
        return gearing_ * fixing;
    }
}

Real BlackAverageONIndexedCouponPricer::optionletRateLocal(Option::Type optionType, Real effStrike) const {

    QL_REQUIRE(!capletVolatility()->useEffectiveVolatility(),
               "BlackAverageONIndexedCouponPricer::optionletRateLocal() does not support effective volatility input.");

    // We compute a rate and a rawRate such that
    // rate * tau * nominal is the amount of the coupon with locally (i.e. daily) capped / floored rates
    // rawRate * tau * nominal is the amount of the coupon without capping / flooring the rate
    // We will then return the difference between rate and rawRate (with the correct sign, see below)
    // as the option component of the coupon.

    // See CappedFlooredOvernightIndexedCoupon::effectiveCap(), Floor() for what is passed in as effStrike.
    // From this we back out the absolute strike at which the
    // - daily rate + spread (spread included) or the
    // - daily rate (spread excluded)
    // is capped / floored.

    Real absStrike = coupon_->includeSpread() ? effStrike + coupon_->underlying()->spread() : effStrike;

    // This following code is inevitably quite similar to the plain ON coupon pricer code, possibly we can refactor
    // this, but as a first step it seems safer to add the full modified code explicitly here and leave the original
    // code alone.

    ext::shared_ptr<OvernightIndex> index = coupon_->underlying()->overnightIndex();

    const std::vector<Date>& fixingDates = coupon_->underlying()->fixingDates();
    const std::vector<Time>& dt = coupon_->underlying()->dt();

    Size n = dt.size();
    Size i = 0;
    QL_REQUIRE(coupon_->underlying()->rateCutoff() < n,
               "rate cutoff (" << coupon_->underlying()->rateCutoff()
                               << ") must be less than number of fixings in period (" << n << ")");
    Natural rco = coupon_->underlying()->rateCutoff();
    Size nCutoff = n - rco;

    Real accumulatedRate = 0.0, accumulatedRateRaw = 0.0;

    // already fixed part
    Date today = Settings::instance().evaluationDate();
    while (i < n && fixingDates[i] < today) {
        // rate must have been fixed
        Rate pastFixing = index->pastFixing(fixingDates[i]);
        QL_REQUIRE(pastFixing != Null<Real>(),
                   "Missing " << index->name() << " fixing for " << fixingDates[i]);
        if (coupon_->includeSpread()) {
            pastFixing += coupon_->spread();
        }
        accumulatedRate += cappedFlooredRate(pastFixing, optionType, absStrike) * dt[i];
        accumulatedRateRaw += pastFixing * dt[i];
        ++i;
    }

    // today is a border case
    while (i < n && fixingDates[i] == today) {
        // might have been fixed
        try {
            Rate pastFixing = index->pastFixing(today);
            if (pastFixing != Null<Real>()) {
                if (coupon_->includeSpread()) {
                    pastFixing += coupon_->spread();
                }
                accumulatedRate += cappedFlooredRate(pastFixing, optionType, absStrike) * dt[i];
                accumulatedRateRaw += pastFixing * dt[i];
                ++i;
            } else {
                break; // fall through and forecast
            }
        } catch (Error&) {
            break; // fall through and forecast
        }
    }

    // forward part, approximation by pricing a cap / floor in the middle of the future period
    const std::vector<Date>& dates = coupon_->underlying()->valueDates();
    const vector<Date>& intDates = coupon_->underlying()->interestDates();
    if (i < n) {
        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

        Real factor = 1.0;
        if (rco > 0) {
            // Can be on lockout date here if lockout date == today and there was no fixing for today. This is the
            // condition (rco > 0 && i == nCutoff - 1). Otherwise
            if (i < nCutoff - 1)
                factor = curve->discount(dates[i]) / curve->discount(dates[nCutoff - 1]);
        } else {
            // Don't make accomodations here for final interest date being a holiday, lookback w/o obs shift etc. like
            // in the main ON pricers. It probably matters less here as we are making approximations in the pricing
            // below but we may want to revisit if necessary.
            factor = curve->discount(dates[i]) / curve->discount(dates[n]);
        }

        // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
        if (rco > 0) {
            Rate onRate = (curve->discount(dates[nCutoff - 1]) / curve->discount(dates.back()) - 1.0) /
                          index->dayCounter().yearFraction(dates[nCutoff-1], dates.back());
            for (Size j = nCutoff - 1; j < n; j++)
                factor *= 1.0 + onRate * dt[j];
        }

        // estimate the average daily rate over the future period (approximate the continuously compounded rate)
        Real tau = coupon_->dayCounter().yearFraction(dates[i], dates.back());
        Real averageRate = std::log(factor) / tau;

        // compute the value of a cap or floor with fixing in the middle of the future period
        // (but accounting for the rate cutoff here)
        Time midPoint =
            (capletVolatility()->timeFromReference(dates[i]) + capletVolatility()->timeFromReference(dates[nCutoff])) /
            2.0;
        Real stdDev = capletVolatility()->volatility(midPoint, effStrike) * std::sqrt(midPoint);
        Real shift = capletVolatility()->displacement();
        bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
        Rate cfValue = shiftedLn ? blackFormula(optionType, effStrike, averageRate, stdDev, 1.0, shift)
                                 : bachelierBlackFormula(optionType, effStrike, averageRate, stdDev, 1.0);

        Real effectiveTime = capletVolatility()->timeFromReference(coupon_->underlying()->fixingDateNoCutoff());
        if (optionType == Option::Type::Call)
            effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
        else
            effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);

        // add spread to average rate
        if (coupon_->includeSpread()) {
            averageRate += coupon_->underlying()->spread();
        }

        // incorporate cap/floor into average rate
        Real averageRateRaw = averageRate;
        averageRate += optionType == Option::Call ? (-cfValue) : cfValue;

        // now assume the averageRate is the effective rate over the future period and update the average rate
        // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE": set tau to avg value
        Natural numCalDays = intDates.back() - intDates[i];
        Real dailyTau = coupon_->underlying()->dayCounter().yearFraction(intDates[i], intDates.back()) / numCalDays;
        accumulatedRate += dailyTau * averageRate * static_cast<Real>(numCalDays);
        accumulatedRateRaw += dailyTau * averageRateRaw * static_cast<Real>(numCalDays);
    }

    Rate tau = !coupon_->underlying()->hasLookback() ? coupon_->accrualPeriod()
        : coupon_->dayCounter().yearFraction(intDates.front(), intDates.back());
    Rate rate = accumulatedRate / tau;
    Rate rawRate = accumulatedRateRaw / tau;

    rate *= coupon_->underlying()->gearing();
    rawRate *= coupon_->underlying()->gearing();

    if (!coupon_->includeSpread()) {
        rate += coupon_->underlying()->spread();
        rawRate += coupon_->underlying()->spread();
    }

    // return optionletRate := r - rawRate, i.e. the option component only
    // (see CappedFlooredAverageONIndexedCoupon::rate() for the signs of the capletRate / flooredRate)

    return (optionType == Option::Call ? -1.0 : 1.0) * (rate - rawRate);
}

Rate BlackAverageONIndexedCouponPricer::swapletRate() const { return swapletRate_; }

Rate BlackAverageONIndexedCouponPricer::capletRate(Rate effectiveCap) const {
    return coupon_->localCapFloor() ? optionletRateLocal(Option::Call, effectiveCap)
                                    : optionletRateGlobal(Option::Call, effectiveCap);
}

Rate BlackAverageONIndexedCouponPricer::floorletRate(Rate effectiveFloor) const {
    return coupon_->localCapFloor() ? optionletRateLocal(Option::Put, effectiveFloor)
                                    : optionletRateGlobal(Option::Put, effectiveFloor);
}

Real BlackAverageONIndexedCouponPricer::swapletPrice() const {
    QL_FAIL("BlackAverageONIndexedCouponPricer::swapletPrice() not provided");
}
Real BlackAverageONIndexedCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("BlackAverageONIndexedCouponPricer::capletPrice() not provided");
}
Real BlackAverageONIndexedCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("BlackAverageONIndexedCouponPricer::floorletPrice() not provided");
}

} // namespace QuantExt
