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

#include <qle/models/lgmvectorised.hpp>

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/instruments/vanillaswap.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>

namespace QuantExt {

RandomVariable LgmVectorised::numeraire(const Time t, const RandomVariable& x,
                                        const Handle<YieldTermStructure>& discountCurve) const {
    QL_REQUIRE(t >= 0.0, "t (" << t << ") >= 0 required in LGMVectorised::numeraire");
    RandomVariable Ht(x.size(), p_->H(t));
    return exp(Ht * x + RandomVariable(x.size(), 0.5 * p_->zeta(t)) * Ht * Ht) /
           RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(t) : discountCurve->discount(t)));
}

RandomVariable LgmVectorised::discountBond(const Time t, const Time T, const RandomVariable& x,
                                           const Handle<YieldTermStructure>& discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return RandomVariable(x.size(), 1.0);
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in LGMVectorised::discountBond");
    RandomVariable Ht(x.size(), p_->H(t));
    RandomVariable HT(x.size(), p_->H(T));
    return RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(T) / p_->termStructure()->discount(t)
                                                 : discountCurve->discount(T) / discountCurve->discount(t))) *
           exp(-(HT - Ht) * x - RandomVariable(x.size(), 0.5 * p_->zeta(t)) * (HT * HT - Ht * Ht));
}

RandomVariable LgmVectorised::reducedDiscountBond(const Time t, const Time T, const RandomVariable& x,
                                                  const Handle<YieldTermStructure>& discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return RandomVariable(x.size(), 1.0) / numeraire(t, x, discountCurve);
    QL_REQUIRE(T >= t && t >= 0.0,
               "T(" << T << ") >= t(" << t << ") >= 0 required in LGMVectorised::reducedDiscountBond");
    RandomVariable HT(x.size(), p_->H(T));
    return RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(T) : discountCurve->discount(T))) *
           exp(-HT * x - RandomVariable(x.size(), 0.5 * p_->zeta(t)) * HT * HT);
}

RandomVariable LgmVectorised::discountBondOption(Option::Type type, const Real K, const Time t, const Time S,
                                                 const Time T, const RandomVariable& x,
                                                 const Handle<YieldTermStructure>& discountCurve) const {
    QL_REQUIRE(T > S && S >= t && t >= 0.0,
               "T(" << T << ") > S(" << S << ") >= t(" << t << ") >= 0 required in LGMVectorised::discountBondOption");
    RandomVariable w(x.size(), type == Option::Call ? 1.0 : -1.0);
    RandomVariable pS = discountBond(t, S, x, discountCurve);
    RandomVariable pT = discountBond(t, T, x, discountCurve);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S, SSRN: https://ssrn.com/abstract=2246054
    RandomVariable sigma(x.size(), std::sqrt(p_->zeta(t)) * (p_->H(T) - p_->H(S)));
    RandomVariable dp =
        log(pT / (RandomVariable(x.size(), K) * pS)) / sigma + RandomVariable(x.size(), 0.5) * sigma * sigma;
    RandomVariable dm = dp - sigma;
    return w * (pT * normalCdf(w * dp) - pS * RandomVariable(x.size(), K) * normalCdf(w * dm));
}

RandomVariable LgmVectorised::fixing(const QuantLib::ext::shared_ptr<InterestRateIndex>& index, const Date& fixingDate,
                                     const Time t, const RandomVariable& x) const {

    // handle case where fixing is deterministic

    Date today = Settings::instance().evaluationDate();
    if (fixingDate <= today)
        return RandomVariable(x.size(), index->fixing(fixingDate));

    // handle stochastic fixing

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(index)) {

        // Ibor Index

        Date d1 = ibor->valueDate(fixingDate);
        Date d2 = ibor->maturityDate(d1);
        Time T1 = std::max(t, p_->termStructure()->timeFromReference(d1));
        Time T2 = std::max(T1, p_->termStructure()->timeFromReference(d2));
        Time dt = ibor->dayCounter().yearFraction(d1, d2);
        // reducedDiscountBond is faster to compute, there we use this here instead of discountBond
        RandomVariable disc1 = reducedDiscountBond(t, T1, x, ibor->forwardingTermStructure());
        RandomVariable disc2 = reducedDiscountBond(t, T2, x, ibor->forwardingTermStructure());
        return (disc1 / disc2 - RandomVariable(x.size(), 1.0)) / RandomVariable(x.size(), dt);
    } else if (auto swap = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(index)) {

        // Swap Index

        auto swapDiscountCurve =
            swap->exogenousDiscount() ? swap->discountingTermStructure() : swap->forwardingTermStructure();
        Leg floatingLeg, fixedLeg;
        if (auto ois = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedSwapIndex>(index)) {
            auto underlying = ois->underlyingSwap(fixingDate);
            floatingLeg = underlying->overnightLeg();
            fixedLeg = underlying->fixedLeg();
        } else {
            auto underlying = swap->underlyingSwap(fixingDate);
            floatingLeg = underlying->floatingLeg();
            fixedLeg = underlying->fixedLeg();
        }
        RandomVariable numerator(x.size(), 0.0), denominator(x.size(), 0.0);
        for (auto const& c : floatingLeg) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c)) {
                Date fixingValueDate = swap->iborIndex()->fixingCalendar().advance(
                    cpn->fixingDate(), swap->iborIndex()->fixingDays(), Days);
                Date fixingEndDate = cpn->fixingEndDate();
                Time T1 = std::max(t, p_->termStructure()->timeFromReference(fixingValueDate));
                Time T2 = std::max(
                    T1, p_->termStructure()->timeFromReference(fixingEndDate)); // accounts for QL_INDEXED_COUPON
                Time T3 = std::max(T2, p_->termStructure()->timeFromReference(cpn->date()));
                RandomVariable disc1 = reducedDiscountBond(t, T1, x, swap->forwardingTermStructure());
                RandomVariable disc2 = reducedDiscountBond(t, T2, x, swap->forwardingTermStructure());
                Real adjFactor =
                    cpn->dayCounter().yearFraction(cpn->accrualStartDate(), cpn->accrualEndDate(),
                                                   cpn->referencePeriodStart(), cpn->referencePeriodEnd()) /
                    swap->iborIndex()->dayCounter().yearFraction(fixingValueDate, fixingEndDate);
                RandomVariable tmp = disc1 / disc2 - RandomVariable(x.size(), 1.0);
                if (!QuantLib::close_enough(adjFactor, 1.0)) {
                    tmp *= RandomVariable(x.size(), adjFactor);
                }
                numerator += tmp * reducedDiscountBond(t, T3, x, swapDiscountCurve);
            } else if (auto cpn = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCoupon>(c)) {
                Date start = cpn->valueDates().front();
                Date end = cpn->valueDates().back();
                Time T1 = std::max(t, p_->termStructure()->timeFromReference(start));
                Time T2 = std::max(T1, p_->termStructure()->timeFromReference(end));
                Time T3 = std::max(T2, p_->termStructure()->timeFromReference(cpn->date()));
                RandomVariable disc1 = reducedDiscountBond(t, T1, x, swap->forwardingTermStructure());
                RandomVariable disc2 = reducedDiscountBond(t, T2, x, swap->forwardingTermStructure());
                Real adjFactor =
                    cpn->dayCounter().yearFraction(cpn->accrualStartDate(), cpn->accrualEndDate(),
                                                   cpn->referencePeriodStart(), cpn->referencePeriodEnd()) /
                    swap->iborIndex()->dayCounter().yearFraction(start, end);
                RandomVariable tmp;
                if (cpn->averagingMethod() == RateAveraging::Compound) {
                    tmp = disc1 / disc2 - RandomVariable(x.size(), 1.0);
                } else if (cpn->averagingMethod() == RateAveraging::Simple) {
                    tmp = QuantExt::log(disc1 / disc2);
                } else {
                    QL_FAIL("LgmVectorised::fixing(): RateAveraging '"
                            << static_cast<int>(cpn->averagingMethod())
                            << "' not handled - internal error, contact dev.");
                }
                if (!QuantLib::close_enough(adjFactor, 1.0)) {
                    tmp *= RandomVariable(x.size(), adjFactor);
                }
                numerator += tmp * reducedDiscountBond(t, T3, x, swapDiscountCurve);
            } else {
                QL_FAIL("LgmVectorised::fixing(): expected ibor coupon");
            }
        }
        for (auto const& c : fixedLeg) {
            auto cpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c);
            QL_REQUIRE(cpn, "LgmVectorised::fixing(): expected fixed coupon");
            Date d = cpn->date();
            Time T = std::max(t, p_->termStructure()->timeFromReference(d));
            denominator +=
                reducedDiscountBond(t, T, x, swapDiscountCurve) * RandomVariable(x.size(), cpn->accrualPeriod());
        }
        return numerator / denominator;

    } else {
        QL_FAIL("LgmVectorised::fixing(): index ('" << index->name() << "') must be ibor or swap index");
    }
}

RandomVariable LgmVectorised::compoundedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                               const std::vector<Date>& fixingDates,
                                               const std::vector<Date>& valueDates, const std::vector<Real>& dt,
                                               const Natural rateCutoff, const bool includeSpread, const Real spread,
                                               const Real gearing, const Period lookback, Real cap, Real floor,
                                               const bool localCapFloor, const bool nakedOption, const Time t,
                                               const RandomVariable& x) const {

    QL_REQUIRE(!includeSpread || QuantLib::close_enough(gearing, 1.0),
               "LgmVectorised::compoundedOnRate(): if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");

    QL_REQUIRE(rateCutoff < dt.size(), "LgmVectorised::compoundedOnRate(): rate cutoff ("
                                           << rateCutoff << ") must be less than number of fixings in period ("
                                           << dt.size() << ")");

    /* We allow the observation time t to be later than the value dates for which to project ON fixings.
       In this case we project the period from the first (future) value date to the last value date starting
       from t, but use the actual portion of the underlying curve.
       As a refinement, we might consider to scale x down to the volatility corresponding to the first future
       value date as well (TODO) - this is all experimental and an approximation to meet the requirements of
       an 1D backward solver, i.e. to be able to price e.g. Bermudan OIS swaptions in an efficient way. */

    // the following is similar to the code in the overnight index coupon pricer

    Size i = 0, n = dt.size();
    Size nCutoff = n - rateCutoff;
    Real compoundFactor = 1.0, compoundFactorWithoutSpread = 1.0;

    Date today = Settings::instance().evaluationDate();

    while (i < n && fixingDates[std::min(i, nCutoff)] < today) {
        Rate pastFixing = IndexManager::instance().getHistory(index->name())[fixingDates[std::min(i, nCutoff)]];
        QL_REQUIRE(pastFixing != Null<Real>(), "LgmVectorised::compoundedOnRate(): Missing "
                                                   << index->name() << " fixing for "
                                                   << fixingDates[std::min(i, nCutoff)]);
        if (includeSpread) {
            compoundFactorWithoutSpread *= (1.0 + pastFixing * dt[i]);
            pastFixing += spread;
        }
        compoundFactor *= (1.0 + pastFixing * dt[i]);
        ++i;
    }

    if (i < n && fixingDates[std::min(i, nCutoff)] == today) {
        Rate pastFixing = IndexManager::instance().getHistory(index->name())[fixingDates[std::min(i, nCutoff)]];
        if (pastFixing != Null<Real>()) {
            if (includeSpread) {
                compoundFactorWithoutSpread *= (1.0 + pastFixing * dt[i]);
                pastFixing += spread;
            }
            compoundFactor *= (1.0 + pastFixing * dt[i]);
            ++i;
        }
    }

    RandomVariable compoundFactorLgm(x.size(), compoundFactor),
        compoundFactorWithoutSpreadLgm(x.size(), compoundFactorWithoutSpread);

    if (i < n) {
        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(),
                   "LgmVectorised::compoundedOnRate(): null term structure set to this instance of " << index->name());

        DiscountFactor startDiscount = curve->discount(valueDates[i]);
        DiscountFactor endDiscount = curve->discount(valueDates[std::max(nCutoff, i)]);

        if (nCutoff < n) {
            DiscountFactor discountCutoffDate =
                curve->discount(valueDates[nCutoff] + 1) / curve->discount(valueDates[nCutoff]);
            endDiscount *= std::pow(discountCutoffDate, valueDates[n] - valueDates[nCutoff]);
        }

        // the times associated to the projection on the T0 curve

        Real T1 = p_->termStructure()->timeFromReference(valueDates[i]);
        Real T2 = p_->termStructure()->timeFromReference(valueDates[n]);

        // the times we use for the projection in the LGM model, if t > T1 they are displaced by (t-T1)

        Real T1_lgm = T1, T2_lgm = T2;
        if (t > T1) {
            T1_lgm += t - T1;
            T2_lgm += t - T1;
        }

        // the discount factors estimated in the lgm model

        RandomVariable disc1 = reducedDiscountBond(t, T1_lgm, x, curve);
        RandomVariable disc2 = reducedDiscountBond(t, T2_lgm, x, curve);

        // apply a correction to the discount factors

        disc1 *= RandomVariable(x.size(), startDiscount / curve->discount(T1_lgm));
        disc2 *= RandomVariable(x.size(), endDiscount / curve->discount(T2_lgm));

        // continue with the usual computation

        compoundFactorLgm *= disc1 / disc2;

        if (includeSpread) {
            compoundFactorWithoutSpreadLgm *= disc1 / disc2;
            Real tau =
                index->dayCounter().yearFraction(valueDates[i], valueDates.back()) / (valueDates.back() - valueDates[i]);
            compoundFactorLgm *= RandomVariable(
                x.size(), std::pow(1.0 + tau * spread, static_cast<int>(valueDates.back() - valueDates[i])));
        }
    }

    Rate tau = index->dayCounter().yearFraction(valueDates.front(), valueDates.back());
    RandomVariable rate = (compoundFactorLgm - RandomVariable(x.size(), 1.0)) / RandomVariable(x.size(), tau);
    RandomVariable swapletRate = RandomVariable(x.size(), gearing) * rate;
    RandomVariable effectiveSpread, effectiveIndexFixing;
    if (!includeSpread) {
        swapletRate += RandomVariable(x.size(), spread);
        effectiveSpread = RandomVariable(x.size(), spread);
        effectiveIndexFixing = rate;
    } else {
        effectiveSpread =
            rate - (compoundFactorWithoutSpreadLgm - RandomVariable(x.size(), 1.0)) / RandomVariable(x.size(), tau);
        effectiveIndexFixing = rate - effectiveSpread;
    }

    if (cap == Null<Real>() && floor == Null<Real>())
        return swapletRate;

    // handle cap / floor - we compute the intrinsic value only

    if (gearing < 0.0) {
        std::swap(cap, floor);
    }

    if (nakedOption)
        swapletRate = RandomVariable(x.size(), 0.0);

    RandomVariable floorletRate(x.size(), 0.0);
    RandomVariable capletRate(x.size(), 0.0);

    if (floor != Null<Real>()) {
        // ignore localCapFloor, treat as global
        RandomVariable effectiveStrike =
            (RandomVariable(x.size(), floor) - effectiveSpread) / RandomVariable(x.size(), gearing);
        floorletRate = RandomVariable(x.size(), gearing) *
                       max(RandomVariable(x.size(), 0.0), effectiveStrike - effectiveIndexFixing);
    }

    if (cap != Null<Real>()) {
        RandomVariable effectiveStrike =
            (RandomVariable(x.size(), cap) - effectiveSpread) / RandomVariable(x.size(), gearing);
        capletRate = RandomVariable(x.size(), gearing) *
                     max(RandomVariable(x.size(), 0.0), effectiveIndexFixing - effectiveStrike);
        if (nakedOption && floor == Null<Real>())
            capletRate = -capletRate;
    }

    return swapletRate + floorletRate - capletRate;
}

RandomVariable LgmVectorised::averagedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                             const std::vector<Date>& fixingDates, const std::vector<Date>& valueDates,
                                             const std::vector<Real>& dt, const Natural rateCutoff,
                                             const bool includeSpread, const Real spread, const Real gearing,
                                             const Period lookback, Real cap, Real floor, const bool localCapFloor,
                                             const bool nakedOption, const Time t, const RandomVariable& x) const {

    QL_REQUIRE(!includeSpread || QuantLib::close_enough(gearing, 1.0),
               "LgmVectorised::averageOnRate(): if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");

    QL_REQUIRE(rateCutoff < dt.size(), "LgmVectorised::averageOnRate(): rate cutoff ("
                                           << rateCutoff << ") must be less than number of fixings in period ("
                                           << dt.size() << ")");

    /* Same comment on t as in compoundedOnRate() above applies here */

    // the following is similar to the code in the overnight index coupon pricer

    Size i = 0, n = dt.size();
    Size nCutoff = n - rateCutoff;
    Real accumulatedRate = 0.0;

    Date today = Settings::instance().evaluationDate();

    while (i < n && fixingDates[std::min(i, nCutoff)] < today) {
        Rate pastFixing = IndexManager::instance().getHistory(index->name())[fixingDates[std::min(i, nCutoff)]];
        QL_REQUIRE(pastFixing != Null<Real>(), "LgmVectorised::averageOnRate(): Missing "
                                                   << index->name() << " fixing for "
                                                   << fixingDates[std::min(i, nCutoff)]);
        accumulatedRate += pastFixing * dt[i];
        ++i;
    }

    if (i < n && fixingDates[std::min(i, nCutoff)] == today) {
        Rate pastFixing = IndexManager::instance().getHistory(index->name())[fixingDates[std::min(i, nCutoff)]];
        if (pastFixing != Null<Real>()) {
            accumulatedRate += pastFixing * dt[i];
            ++i;
        }
    }

    RandomVariable accumulatedRateLgm(x.size(), accumulatedRate);

    if (i < n) {
        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(),
                   "LgmVectorised::averageOnRate(): null term structure set to this instance of " << index->name());

        DiscountFactor startDiscount = curve->discount(valueDates[i]);
        DiscountFactor endDiscount = curve->discount(valueDates[std::max(nCutoff, i)]);

        if (nCutoff < n) {
            DiscountFactor discountCutoffDate =
                curve->discount(valueDates[nCutoff] + 1) / curve->discount(valueDates[nCutoff]);
            endDiscount *= std::pow(discountCutoffDate, valueDates[n] - valueDates[nCutoff]);
        }

        // the times associated to the projection on the T0 curve

        Real T1 = p_->termStructure()->timeFromReference(valueDates[i]);
        Real T2 = p_->termStructure()->timeFromReference(valueDates[n]);

        // the times we use for the projection in the LGM model, if t > T1 they are displaced by (t-T1)

        Real T1_lgm = T1, T2_lgm = T2;
        if (t > T1) {
            T1_lgm += t - T1;
            T2_lgm += t - T1;
        }

        // the discount factors estimated in the lgm model

        RandomVariable disc1 = reducedDiscountBond(t, T1_lgm, x, curve);
        RandomVariable disc2 = reducedDiscountBond(t, T2_lgm, x, curve);

        // apply a correction to the discount factors

        disc1 *= RandomVariable(x.size(), startDiscount / curve->discount(T1_lgm));
        disc2 *= RandomVariable(x.size(), endDiscount / curve->discount(T2_lgm));

        // continue with the usual computation

        accumulatedRateLgm += log(disc1 / disc2);
    }

    Rate tau = index->dayCounter().yearFraction(valueDates.front(), valueDates.back());
    RandomVariable rate =
        RandomVariable(x.size(), gearing / tau) * accumulatedRateLgm + RandomVariable(x.size(), spread);

    if (cap == Null<Real>() && floor == Null<Real>())
        return rate;

    // handle cap / floor - we compute the intrinsic value only

    if (gearing < 0.0) {
        std::swap(cap, floor);
    }

    RandomVariable forwardRate = (rate - RandomVariable(x.size(), spread)) / RandomVariable(x.size(), gearing);
    RandomVariable floorletRate(x.size(), 0.0);
    RandomVariable capletRate(x.size(), 0.0);

    if (nakedOption)
        rate = RandomVariable(x.size(), 0.0);

    if (floor != Null<Real>()) {
        // ignore localCapFloor, treat as global
        RandomVariable effectiveStrike = RandomVariable(x.size(), (floor - spread) / gearing);
        floorletRate =
            RandomVariable(x.size(), gearing) * max(RandomVariable(x.size(), 0.0), effectiveStrike - forwardRate);
    }

    if (cap != Null<Real>()) {
        RandomVariable effectiveStrike = RandomVariable(x.size(), (cap - spread) / gearing);
        capletRate =
            RandomVariable(x.size(), gearing) * max(RandomVariable(x.size(), 0.0), forwardRate - effectiveStrike);
        if (nakedOption && floor == Null<Real>())
            capletRate = -capletRate;
    }

    return rate + floorletRate - capletRate;
}

RandomVariable LgmVectorised::averagedBmaRate(const QuantLib::ext::shared_ptr<BMAIndex>& index,
                                              const std::vector<Date>& fixingDates, const Date& accrualStartDate,
                                              const Date& accrualEndDate, const bool includeSpread, const Real spread,
                                              const Real gearing, Real cap, Real floor, const bool nakedOption,
                                              const Time t, const RandomVariable& x) const {

    // similar to AverageBMACouponPricer

    Natural cutoffDays = 0;
    Date startDate = accrualStartDate - cutoffDays;
    Date endDate = accrualEndDate - cutoffDays;
    Date d1 = startDate, d2 = startDate;

    QL_REQUIRE(!fixingDates.empty(), "LgmVectorised::averagedBmaRate(): fixing date list empty");
    QL_REQUIRE(index->valueDate(fixingDates.front()) <= startDate,
               "LgmVectorised::averagedBmaRate(): first fixing date valid after period start");
    QL_REQUIRE(index->valueDate(fixingDates.back()) >= endDate,
               "LgmVectorised::averagedBmaRate(): last fixing date valid before period end");

    Handle<YieldTermStructure> curve = index->forwardingTermStructure();
    QL_REQUIRE(!curve.empty(),
               "LgmVectorised::averagedBmaRate(): null term structure set to this instance of " << index->name());

    Date today = Settings::instance().evaluationDate();

    RandomVariable avgBMA(x.size(), 0.0);

    for (Size i = 0; i < fixingDates.size() - 1; ++i) {
        Date valueDate = index->valueDate(fixingDates[i]);
        Date nextValueDate = index->valueDate(fixingDates[i + 1]);
        if (fixingDates[i] >= endDate || valueDate >= endDate)
            break;
        if (fixingDates[i + 1] < startDate || nextValueDate <= startDate)
            continue;

        d2 = std::min(nextValueDate, endDate);
        RandomVariable fixing;
        if (fixingDates[i] <= today) {
            // past fixing or forecast today's fixing on T0 curve (which is ok, since model independent)
            fixing = RandomVariable(x.size(), index->fixing(fixingDates[i]));
        } else {

            Date start = index->fixingCalendar().advance(fixingDates[i], 1, Days);
            Date end = index->maturityDate(start);
            DiscountFactor startDiscount = curve->discount(start);
            DiscountFactor endDiscount = curve->discount(end);

            // the times associated to the projection on the T0 curve

            Real T1 = p_->termStructure()->timeFromReference(start);
            Real T2 = p_->termStructure()->timeFromReference(end);

            // the times we use for the projection in the LGM model, if t > T1 they are displaced by (t-T1)

            Real T1_lgm = T1, T2_lgm = T2;
            if (t > T1) {
                T1_lgm += t - T1;
                T2_lgm += t - T1;
            }

            // the discount factors estimated in the lgm model

            RandomVariable disc1 = reducedDiscountBond(t, T1_lgm, x, curve);
            RandomVariable disc2 = reducedDiscountBond(t, T2_lgm, x, curve);

            // apply a correction to the discount factors

            disc1 *= RandomVariable(x.size(), startDiscount / curve->discount(T1_lgm));
            disc2 *= RandomVariable(x.size(), endDiscount / curve->discount(T2_lgm));

            // estimate the fixing

            fixing = (disc1 / disc2 - RandomVariable(x.size(), 1.0)) /
                     RandomVariable(x.size(), index->dayCounter().yearFraction(start, end));
        }

        avgBMA += fixing * RandomVariable(x.size(), (d2 - d1));
        d1 = d2;
    }

    avgBMA *= RandomVariable(x.size(), gearing / (endDate - startDate));
    avgBMA += RandomVariable(x.size(), spread);

    if (cap == Null<Real>() && floor == Null<Real>())
        return avgBMA;

    // handle cap / floor - we compute the intrinsic value only

    if (gearing < 0.0) {
        std::swap(cap, floor);
    }

    RandomVariable forwardRate = (avgBMA - RandomVariable(x.size(), spread)) / RandomVariable(x.size(), gearing);
    RandomVariable floorletRate(x.size(), 0.0);
    RandomVariable capletRate(x.size(), 0.0);

    if (nakedOption)
        avgBMA = RandomVariable(x.size(), 0.0);

    if (floor != Null<Real>()) {
        // ignore localCapFloor, treat as global
        RandomVariable effectiveStrike = RandomVariable(x.size(), (floor - spread) / gearing);
        floorletRate =
            RandomVariable(x.size(), gearing) * max(RandomVariable(x.size(), 0.0), effectiveStrike - forwardRate);
    }

    if (cap != Null<Real>()) {
        RandomVariable effectiveStrike = RandomVariable(x.size(), (cap - spread) / gearing);
        capletRate =
            RandomVariable(x.size(), gearing) * max(RandomVariable(x.size(), 0.0), forwardRate - effectiveStrike);
        if (nakedOption && floor == Null<Real>())
            capletRate = -capletRate;
    }

    return avgBMA + floorletRate - capletRate;
}

RandomVariable LgmVectorised::subPeriodsRate(const QuantLib::ext::shared_ptr<InterestRateIndex>& index,
                                             const std::vector<Date>& fixingDates, const Time t,
                                             const RandomVariable& x) const {

    return fixing(index, fixingDates.front(), t, x);
}

} // namespace QuantExt
