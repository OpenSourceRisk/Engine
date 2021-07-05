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

#include <ql/instruments/vanillaswap.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>

namespace QuantExt {

RandomVariable LgmVectorised::numeraire(const Time t, const RandomVariable& x,
                                        const Handle<YieldTermStructure> discountCurve) const {
    QL_REQUIRE(t >= 0.0, "t (" << t << ") >= 0 required in LGM::numeraire");
    RandomVariable Ht(x.size(), p_->H(t));
    return exp(Ht * x + RandomVariable(x.size(), 0.5 * p_->zeta(t)) * Ht * Ht) /
           RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(t) : discountCurve->discount(t)));
}

RandomVariable LgmVectorised::discountBond(const Time t, const Time T, const RandomVariable& x,
                                           Handle<YieldTermStructure> discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return RandomVariable(x.size(), 1.0);
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in LGM::discountBond");
    RandomVariable Ht(x.size(), p_->H(t));
    RandomVariable HT(x.size(), p_->H(T));
    return RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(T) / p_->termStructure()->discount(t)
                                                 : discountCurve->discount(T) / discountCurve->discount(t))) *
           exp(-(HT - Ht) * x - RandomVariable(x.size(), 0.5 * p_->zeta(t)) * (HT * HT - Ht * Ht));
}

RandomVariable LgmVectorised::reducedDiscountBond(const Time t, const Time T, const RandomVariable& x,
                                                  const Handle<YieldTermStructure> discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return RandomVariable(x.size(), 1.0) / numeraire(t, x, discountCurve);
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in LGM::reducedDiscountBond");
    RandomVariable HT(x.size(), p_->H(T));
    return RandomVariable(x.size(),
                          (discountCurve.empty() ? p_->termStructure()->discount(T) : discountCurve->discount(T))) *
           exp(-HT * x - RandomVariable(x.size(), 0.5 * p_->zeta(t)) * HT * HT);
}

RandomVariable LgmVectorised::fixing(const boost::shared_ptr<InterestRateIndex>& index, const Date& fixingDate,
                                     const Time t, const RandomVariable& x) const {

    // handle case where fixing is deterministic
    Date today = Settings::instance().evaluationDate();
    if (fixingDate <= today)
        return RandomVariable(x.size(), index->fixing(today));

    // handle stochastic fixing
    if (auto ibor = boost::dynamic_pointer_cast<IborIndex>(index)) {
        Date d1 = ibor->valueDate(fixingDate);
        Date d2 = ibor->maturityDate(d1);
        Time T1 = p_->termStructure()->timeFromReference(d1);
        Time T2 = p_->termStructure()->timeFromReference(d2);
        Time dt = ibor->dayCounter().yearFraction(d1, d2);
        // reducedDiscountBond is faster to compute, there we use this here instead of discountBond
        RandomVariable disc1 = reducedDiscountBond(t, T1, x, ibor->forwardingTermStructure());
        RandomVariable disc2 = reducedDiscountBond(t, T2, x, ibor->forwardingTermStructure());
        return (disc1 / disc2 - RandomVariable(x.size(), 1.0)) / RandomVariable(x.size(), dt);
    } else if (auto swap = boost::dynamic_pointer_cast<SwapIndex>(index)) {
        auto swapDiscountCurve =
            swap->exogenousDiscount() ? swap->discountingTermStructure() : swap->forwardingTermStructure();
        auto underlying = swap->underlyingSwap(fixingDate);
        RandomVariable numerator(x.size(), 0.0), denominator(x.size(), 0.0);
        for (auto const& c : underlying->floatingLeg()) {
            auto cpn = boost::dynamic_pointer_cast<IborCoupon>(c);
            QL_REQUIRE(cpn, "LgmVectorised::fixing(): excepted ibor coupon");
            Date fixingValueDate =
                swap->iborIndex()->fixingCalendar().advance(cpn->fixingDate(), swap->iborIndex()->fixingDays(), Days);
            Date fixingEndDate = cpn->fixingEndDate();
            Time T1 = p_->termStructure()->timeFromReference(fixingValueDate);
            Time T2 = p_->termStructure()->timeFromReference(fixingEndDate); // accounts for QL_INDEXED_COUPON
            Time T3 = p_->termStructure()->timeFromReference(cpn->date());
            RandomVariable disc1 = reducedDiscountBond(t, T1, x, swap->forwardingTermStructure());
            RandomVariable disc2 = reducedDiscountBond(t, T2, x, swap->forwardingTermStructure());
            Real adjFactor = cpn->dayCounter().yearFraction(cpn->accrualStartDate(), cpn->accrualEndDate(),
                                                            cpn->referencePeriodStart(), cpn->referencePeriodEnd()) /
                             swap->iborIndex()->dayCounter().yearFraction(fixingValueDate, fixingEndDate);
            RandomVariable tmp = disc1 / disc2 - RandomVariable(x.size(), 1.0);
            if (!QuantLib::close_enough(adjFactor, 1.0)) {
                tmp *= RandomVariable(x.size(), adjFactor);
            }
            numerator += tmp * reducedDiscountBond(t, T3, x, swapDiscountCurve);
        }
        for (auto const& c : underlying->fixedLeg()) {
            auto cpn = boost::dynamic_pointer_cast<FixedRateCoupon>(c);
            QL_REQUIRE(cpn, "LgmVectorised::fixing(): excepted ibor coupon");
            Date d = cpn->date();
            Time T = p_->termStructure()->timeFromReference(d);
            denominator +=
                reducedDiscountBond(t, T, x, swapDiscountCurve) * RandomVariable(x.size(), cpn->accrualPeriod());
        }
        return numerator / denominator;
    } else {
        QL_FAIL("LgmVectorised::fixing(): index ('" << index->name() << "') must be ibor or swap index");
    }
}

} // namespace QuantExt
