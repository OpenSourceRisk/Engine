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

#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <ql/instruments/vanillaswap.hpp>

namespace QuantExt {

namespace detail {

template <class Key> Real& getMapEntry(std::map<Key, Real>& map, const Key& key) {
    auto f = map.find(key);
    if (f == map.end()) {
        return map.insert(std::make_pair(key, 0.0)).first->second;
    } else {
        return f->second;
    }
}

NpvDeltaGammaCalculator::NpvDeltaGammaCalculator(Handle<YieldTermStructure> discountCurve, const Real payer, Real& npv,
                                                 Real& bps, const bool computeDelta, const bool computeGamma,
                                                 const bool computeBPS, std::map<Date, Real>& deltaDiscount,
                                                 std::map<Date, Real>& deltaForward, std::map<Date, Real>& deltaBPS,
                                                 std::map<Date, Real>& gammaDiscount,
                                                 std::map<std::pair<Date, Date>, Real>& gammaForward,
                                                 std::map<std::pair<Date, Date>, Real>& gammaDscFwd,
                                                 std::map<Date, Real>& gammaBPS, Real& fxLinkedForeignNpv,
                                                 const bool excludeSimpleCashFlowsFromSensis, Real& simpleCashFlowNpv)
    : discountCurve_(discountCurve), payer_(payer), npv_(npv), bps_(bps), computeDelta_(computeDelta),
      computeGamma_(computeGamma), computeBPS_(computeBPS), deltaDiscount_(deltaDiscount), deltaForward_(deltaForward),
      deltaBPS_(deltaBPS), gammaDiscount_(gammaDiscount), gammaForward_(gammaForward), gammaDscFwd_(gammaDscFwd),
      gammaBPS_(gammaBPS), fxLinkedForeignNpv_(fxLinkedForeignNpv),
      excludeSimpleCashFlowsFromSensis_(excludeSimpleCashFlowsFromSensis), simpleCashFlowNpv_(simpleCashFlowNpv) {}

void NpvDeltaGammaCalculator::visit(CashFlow& c) {
    Real dsc = discountCurve_->discount(c.date());
    Real a = payer_ * c.amount() * dsc;
    npv_ += a;
    Real t = discountCurve_->timeFromReference(c.date());
    if (computeDelta_) {
        getMapEntry(deltaDiscount_, c.date()) += -t * a;
    }
    if (computeGamma_) {
        getMapEntry(gammaDiscount_, c.date()) += t * t * a;
    }
}

void NpvDeltaGammaCalculator::visit(SimpleCashFlow& c) {
    if (excludeSimpleCashFlowsFromSensis_) {
        // even when excluding the cf from the sensis we want to colect their npv contribution, but in a separate field
        Real dsc = discountCurve_->discount(c.date());
        Real a = payer_ * c.amount() * dsc;
        simpleCashFlowNpv_ += a;
        return;
    }
    visit(static_cast<CashFlow&>(c));
}

void NpvDeltaGammaCalculator::visit(FixedRateCoupon& c) {
    Real dsc = discountCurve_->discount(c.date());
    Real a = payer_ * c.amount() * dsc;
    npv_ += a;
    Real t = discountCurve_->timeFromReference(c.date());
    if (computeDelta_) {
        getMapEntry(deltaDiscount_, c.date()) += -t * a;
    }
    if (computeGamma_) {
        getMapEntry(gammaDiscount_, c.date()) += t * t * a;
    }
    if (computeBPS_) {
        Real tau = c.accrualPeriod();
        bps_ += payer_ * c.nominal() * tau * dsc;
        if (computeDelta_) {
            getMapEntry(deltaBPS_, c.date()) += -t * payer_ * c.nominal() * tau * dsc;
        }
        if (computeGamma_) {
            getMapEntry(gammaBPS_, c.date()) += t * t * payer_ * c.nominal() * tau * dsc;
        }
    }
}

void NpvDeltaGammaCalculator::processIborCoupon(FloatingRateCoupon& c) {
    Real dsc = discountCurve_->discount(c.date());
    Real a = payer_ * c.amount() * dsc;
    npv_ += a;
    Date d3 = c.date();
    Real t3 = discountCurve_->timeFromReference(d3);
    if (computeDelta_) {
        getMapEntry(deltaDiscount_, d3) += -t3 * a;
    }
    if (computeGamma_) {
        getMapEntry(gammaDiscount_, d3) += t3 * t3 * a;
    }
    if (computeBPS_) {
        Real tau = c.accrualPeriod();
        bps_ += payer_ * tau * c.nominal() * dsc;
        if (computeDelta_) {
            getMapEntry(deltaBPS_, d3) += -t3 * payer_ * tau * c.nominal() * dsc;
        }
        if (computeGamma_) {
            getMapEntry(gammaBPS_, d3) += t3 * t3 * payer_ * tau * c.nominal() * dsc;
        }
    }
    Date fixing = c.fixingDate();

    // is it actually a floating rate coupon?
    if (fixing > discountCurve_->referenceDate() ||
        (fixing == discountCurve_->referenceDate() && c.index()->pastFixing(fixing) == Null<Real>())) {
        Date d1 = c.index()->valueDate(fixing);
        Date d2;
        if (IborCoupon::Settings::instance().usingAtParCoupons() && fixing <= c.accrualStartDate()) {
            // par coupon approximation
            Date nextFixingDate =
                c.index()->fixingCalendar().advance(c.accrualEndDate(), -static_cast<Integer>(c.fixingDays()), Days);
            d2 = c.index()->fixingCalendar().advance(nextFixingDate, c.fixingDays(), Days);
        } else if (IborCoupon::Settings::instance().usingAtParCoupons()) {
            // in arreas
            d2 = c.index()->maturityDate(d1);
        } else {
            // use indexed coupon
            d2 = c.index()->maturityDate(d1);
        }
       
        // if the coupon is degenerated we exit early
        if (d2 <= d1)
            return;

        Real t1 = discountCurve_->timeFromReference(d1);
        Real t2 = discountCurve_->timeFromReference(d2);
        Real r = payer_ * dsc *
                 (c.nominal() * c.accrualPeriod() *
                  (c.gearing() / c.index()->dayCounter().yearFraction(d1, d2) - c.spread()));
        if (computeDelta_) {
            getMapEntry(deltaForward_, d1) += -t1 * (a + r);
            getMapEntry(deltaForward_, d2) += t2 * (a + r);
        }
        if (computeGamma_) {
            getMapEntry(gammaForward_, std::make_pair(d1, d1)) += t1 * t1 * (a + r);
            getMapEntry(gammaForward_, std::make_pair(d2, d2)) += t2 * t2 * (a + r);
            getMapEntry(gammaForward_, std::make_pair(d1, d2)) += -2.0 * t1 * t2 * (a + r);
            getMapEntry(gammaDscFwd_, std::make_pair(d3, d1)) += t1 * t3 * (a + r);
            getMapEntry(gammaDscFwd_, std::make_pair(d3, d2)) += -t2 * t3 * (a + r);
        }
    }
} // process IborCoupon

void NpvDeltaGammaCalculator::visit(IborCoupon& c) { processIborCoupon(c); }

void NpvDeltaGammaCalculator::visit(FloatingRateFXLinkedNotionalCoupon& c) {
    // only add to the foreign npv (and hence effectively to the fx spot delta) if the fx rate is not yet fixed
    
     if (c.fxFixingDate() > discountCurve_->referenceDate() ||
        (c.fxFixingDate() == discountCurve_->referenceDate() &&
         c.fxIndex()->pastFixing(c.fxFixingDate()) == Null<Real>())) {
        Real tmp = c.fxIndex()->forecastFixing(0.0);
        fxLinkedForeignNpv_ += payer_ * c.amount() * discountCurve_->discount(c.date()) / tmp;
    }
    processIborCoupon(c);
}

void NpvDeltaGammaCalculator::visit(FXLinkedCashFlow& c) {
    Real dsc = discountCurve_->discount(c.date());
    Real a = payer_ * c.amount() * dsc;
    npv_ += a;
    Real t = discountCurve_->timeFromReference(c.date());
    if (computeDelta_) {
        getMapEntry(deltaDiscount_, c.date()) += -t * a;
    }
    if (computeGamma_) {
        getMapEntry(gammaDiscount_, c.date()) += t * t * a;
    }
    // only add to the foreign npv (and hence effectively to the fx spot delta) if the fx rate is not yet fixed
    if (c.fxFixingDate() > discountCurve_->referenceDate() ||
        (c.fxFixingDate() == discountCurve_->referenceDate() &&
         c.fxIndex()->pastFixing(c.fxFixingDate()) == Null<Real>())) {
        Real tmp = c.fxIndex()->forecastFixing(0.0);
        Real b = a / tmp;
        fxLinkedForeignNpv_ += b;
    }
}

void NpvDeltaGammaCalculator::visit(QuantExt::OvernightIndexedCoupon& c)
{
    processIborCoupon(c);
}

std::vector<Real> rebucketDeltas(const std::vector<Time>& deltaTimes, const std::map<Date, Real>& deltaRaw,
				 const Date& referenceDate, const DayCounter& dc, const bool linearInZero) {
    std::vector<Real> delta(deltaTimes.size(), 0.0);
    for (std::map<Date, Real>::const_iterator i = deltaRaw.begin(); i != deltaRaw.end(); ++i) {
        Real t = dc.yearFraction(referenceDate, i->first);
        Size b = std::upper_bound(deltaTimes.begin(), deltaTimes.end(), t) - deltaTimes.begin();
        if (b == 0) {
            delta[0] += i->second;
        } else if (b == deltaTimes.size()) {
            delta.back() += i->second;
        } else {
            Real tmp = (deltaTimes[b] - t) / (deltaTimes[b] - deltaTimes[b - 1]);
            if (linearInZero) {
                delta[b - 1] += i->second * tmp;
                delta[b] += i->second * (1.0 - tmp);
            } else {
                delta[b - 1] += i->second * tmp * deltaTimes[b - 1] / t;
                delta[b] += i->second * (1.0 - tmp) * deltaTimes[b] / t;
            }
        }
    }
    return delta;
}

Matrix rebucketGammas(const std::vector<Time>& gammaTimes, const std::map<Date, Real>& gammaDscRaw,
		      std::map<std::pair<Date, Date>, Real>& gammaForward,
		      std::map<std::pair<Date, Date>, Real>& gammaDscFwd, const bool forceFullMatrix,
		      const Date& referenceDate, const DayCounter& dc, const bool linearInZero) {
    int n = gammaTimes.size();
    // if we have two curves c1, c2, the matrix contains c1-c1, c1-c2, c2-c1, c2-c2 blocks
    Size n2 = !forceFullMatrix && gammaForward.empty() && gammaDscFwd.empty() ? n : 2 * n;
    Matrix gamma(n2, n2, 0.0);
    // pure dsc
    for (std::map<Date, Real>::const_iterator i = gammaDscRaw.begin(); i != gammaDscRaw.end(); ++i) {
        Real t = dc.yearFraction(referenceDate, i->first);
        int b = (int)(std::upper_bound(gammaTimes.begin(), gammaTimes.end(), t) - gammaTimes.begin());
        if (b == 0) {
            gamma[0][0] += i->second;
        } else if (b == n) {
            gamma[n - 1][n - 1] += i->second;
        } else {
            Real tmp = (gammaTimes[b] - t) / (gammaTimes[b] - gammaTimes[b - 1]);
            if (linearInZero) {
                gamma[b - 1][b - 1] += i->second * tmp * tmp;
                gamma[b - 1][b] += i->second * (1.0 - tmp) * tmp;
                gamma[b][b - 1] += i->second * tmp * (1.0 - tmp);
                gamma[b][b] += i->second * (1.0 - tmp) * (1.0 - tmp);
            } else {
                gamma[b - 1][b - 1] += i->second * tmp * tmp * gammaTimes[b - 1] * gammaTimes[b - 1] / (t * t);
                gamma[b - 1][b] += i->second * (1.0 - tmp) * tmp * gammaTimes[b] * gammaTimes[b - 1] / (t * t);
                gamma[b][b - 1] += i->second * tmp * (1.0 - tmp) * gammaTimes[b - 1] * gammaTimes[b] / (t * t);
                gamma[b][b] += i->second * (1.0 - tmp) * (1.0 - tmp) * gammaTimes[b] * gammaTimes[b] / (t * t);
            }
        }
    }
    // dsc-fwd
    if (!gammaDscFwd.empty()) {
        Matrix gammadf(n, n, 0.0);
        for (std::map<std::pair<Date, Date>, Real>::const_iterator i = gammaDscFwd.begin(); i != gammaDscFwd.end();
             ++i) {
            Real t1 = dc.yearFraction(referenceDate, i->first.first);
            Real t2 = dc.yearFraction(referenceDate, i->first.second);
            int b1 = (int)(std::upper_bound(gammaTimes.begin(), gammaTimes.end(), t1) - gammaTimes.begin());
            int b2 = (int)(std::upper_bound(gammaTimes.begin(), gammaTimes.end(), t2) - gammaTimes.begin());
            Real w1 = 0.0, w2 = 0.0;
            int i1 = 0, i2 = 0;
            if (b1 == 0) {
                w1 = 1.0;
                i1 = 0;
            } else if (b1 == n) {
                w1 = 0.0;
                i1 = n - 2;
            } else {
                w1 = (gammaTimes[b1] - t1) / (gammaTimes[b1] - gammaTimes[b1 - 1]);
                i1 = b1 - 1;
            }
            if (b2 == 0) {
                w2 = 1.0;
                i2 = 0;
            } else if (b2 == n) {
                w2 = 0.0;
                i2 = n - 2;
            } else {
                w2 = (gammaTimes[b2] - t2) / (gammaTimes[b2] - gammaTimes[b2 - 1]);
                i2 = b2 - 1;
            }
            if (i1 >= 0) {
                if (i2 < n - 1)
                    gammadf[i1][i2 + 1] += w1 * (1.0 - w2) * i->second *
                                           (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2] / (t1 * t2));
                if (i2 >= 0) {
                    gammadf[i1][i2] += w1 * w2 * i->second *
                                       (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2 - 1] / (t1 * t2));
                }
            }
            if (i2 >= 0 && i1 < n - 1) {
                gammadf[i1 + 1][i2] += (1.0 - w1) * w2 * i->second *
                                       (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2 - 1] / (t1 * t2));
            }
            if (i1 < n - 1 && i2 < n - 1) {
                gammadf[i1 + 1][i2 + 1] += (1.0 - w1) * (1.0 - w2) * i->second *
                                           (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2] / (t1 * t2));
            }
        }
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                gamma[i][n + j] = gamma[n + j][i] = gammadf[i][j];
            }
        }
    }
    // fwd-fwd
    for (std::map<std::pair<Date, Date>, Real>::const_iterator i = gammaForward.begin(); i != gammaForward.end(); ++i) {
        Real t1 = dc.yearFraction(referenceDate, i->first.first);
        Real t2 = dc.yearFraction(referenceDate, i->first.second);
        int b1 = (int)(std::upper_bound(gammaTimes.begin(), gammaTimes.end(), t1) - gammaTimes.begin());
        int b2 = (int)(std::upper_bound(gammaTimes.begin(), gammaTimes.end(), t2) - gammaTimes.begin());
        Real tmp = 0.5 * i->second;
        Real w1 = 0.0, w2 = 0.0;
        int i1 = 0, i2 = 0;
        if (b1 == 0) {
            w1 = 1.0;
            i1 = 0;
        } else if (b1 == n) {
            w1 = 0.0;
            i1 = n - 2;
        } else {
            w1 = (gammaTimes[b1] - t1) / (gammaTimes[b1] - gammaTimes[b1 - 1]);
            i1 = b1 - 1;
        }
        if (b2 == 0) {
            w2 = 1.0;
            i2 = 0;
        } else if (b2 == n) {
            w2 = 0.0;
            i2 = n - 2;
        } else {
            w2 = (gammaTimes[b2] - t2) / (gammaTimes[b2] - gammaTimes[b2 - 1]);
            i2 = b2 - 1;
        }
        if (i1 >= 0) {
            if (i2 < n - 1) {
                gamma[n + i1][n + i2 + 1] +=
                    w1 * (1.0 - w2) * tmp * (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2] / (t1 * t2));
                gamma[n + i2 + 1][n + i1] +=
                    w1 * (1.0 - w2) * tmp * (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2] / (t1 * t2));
            }
            if (i2 >= 0) {
                gamma[n + i1][n + i2] +=
                    w1 * w2 * tmp * (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2 - 1] / (t1 * t2));
                gamma[n + i2][n + i1] +=
                    w1 * w2 * tmp * (linearInZero ? 1.0 : gammaTimes[b1 - 1] * gammaTimes[b2 - 1] / (t1 * t2));
            }
        }
        if (i2 >= 0 && i1 < n - 1) {
            gamma[n + i1 + 1][n + i2] +=
                (1.0 - w1) * w2 * tmp * (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2 - 1] / (t1 * t2));
            gamma[n + i2][n + i1 + 1] +=
                (1.0 - w1) * w2 * tmp * (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2 - 1] / (t1 * t2));
        }
        if (i1 < n - 1 && i2 < n - 1) {
            gamma[n + i1 + 1][n + i2 + 1] +=
                (1.0 - w1) * (1.0 - w2) * tmp * (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2] / (t1 * t2));
            gamma[n + i2 + 1][n + i1 + 1] +=
                (1.0 - w1) * (1.0 - w2) * tmp * (linearInZero ? 1.0 : gammaTimes[b1] * gammaTimes[b2] / (t1 * t2));
        }
    }

    return gamma;
} // rebucketGammas

} // namespace detail

DiscountingSwapEngineDeltaGamma::DiscountingSwapEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                                 const std::vector<Time>& bucketTimes,
                                                                 const bool computeDelta, const bool computeGamma,
                                                                 const bool computeBPS, const bool linearInZero)
    : discountCurve_(discountCurve), bucketTimes_(bucketTimes), computeDelta_(computeDelta),
      computeGamma_(computeGamma), computeBPS_(computeBPS), linearInZero_(linearInZero) {
    registerWith(discountCurve_);
    QL_REQUIRE(!bucketTimes_.empty() || (!computeDelta && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

void DiscountingSwapEngineDeltaGamma::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    // compute npv and raw deltas

    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();
    results_.legNPV.resize(arguments_.legs.size());
    if (computeBPS_)
        results_.legBPS.resize(arguments_.legs.size());

    std::map<Date, Real> deltaDiscountRaw, deltaForwardRaw, gammaDiscountRaw;
    std::map<std::pair<Date, Date>, Real> gammaForwardRaw, gammaDscFwdRaw;

    std::vector<std::vector<Real>> deltaBPS;
    std::vector<Matrix> gammaBPS;

    Real empty = 0.0;

    for (Size j = 0; j < arguments_.legs.size(); ++j) {
        Real npv = 0.0, bps = 0.0;
        std::map<Date, Real> deltaBPSRaw, gammaBPSRaw; // these results are per leg
        detail::NpvDeltaGammaCalculator calc(discountCurve_, arguments_.payer[j], npv, bps, computeDelta_,
                                             computeGamma_, computeBPS_, deltaDiscountRaw, deltaForwardRaw, deltaBPSRaw,
                                             gammaDiscountRaw, gammaForwardRaw, gammaDscFwdRaw, gammaBPSRaw, empty,
                                             false, empty);
        Leg& leg = arguments_.legs[j];
        for (Size i = 0; i < leg.size(); ++i) {
            CashFlow& cf = *leg[i];
            if (cf.date() <= discountCurve_->referenceDate()) {
                continue;
            }
            cf.accept(calc);
        }
        results_.legNPV[j] = npv;
        results_.value += npv;
        if (computeBPS_) {
            std::map<std::pair<Date, Date>, Real> empty;
            results_.legBPS[j] += bps;
            // bps delta and gamma are per leg, i.e. we have to compute delta and gamma already here,
            // the result vectors are then set below where the rest of the delta / gamma computation is done
            std::vector<Real> tmp = detail::rebucketDeltas(bucketTimes_, deltaBPSRaw, discountCurve_->referenceDate(),
                                                           discountCurve_->dayCounter(), linearInZero_);
            deltaBPS.push_back(tmp);
            Matrix tmp2 =
                detail::rebucketGammas(bucketTimes_, gammaBPSRaw, empty, empty, false, discountCurve_->referenceDate(),
                                       discountCurve_->dayCounter(), linearInZero_);
            gammaBPS.push_back(tmp2);
        }
    }

    // convert raw deltas to given bucketing structure

    results_.additionalResults["bucketTimes"] = bucketTimes_;

    if (computeDelta_) {
        std::vector<Real> deltaDiscount =
            detail::rebucketDeltas(bucketTimes_, deltaDiscountRaw, discountCurve_->referenceDate(),
                                   discountCurve_->dayCounter(), linearInZero_);
        std::vector<Real> deltaForward =
            detail::rebucketDeltas(bucketTimes_, deltaForwardRaw, discountCurve_->referenceDate(),
                                   discountCurve_->dayCounter(), linearInZero_);

        results_.additionalResults["deltaDiscount"] = deltaDiscount;
        results_.additionalResults["deltaForward"] = deltaForward;
        if (computeBPS_) {
            results_.additionalResults["deltaBPS"] = deltaBPS;
        }
    }

    // convert raw gammas to given bucketing structure

    if (computeGamma_) {
        Matrix gamma =
            detail::rebucketGammas(bucketTimes_, gammaDiscountRaw, gammaForwardRaw, gammaDscFwdRaw, true,
                                   discountCurve_->referenceDate(), discountCurve_->dayCounter(), linearInZero_);
        results_.additionalResults["gamma"] = gamma;
        if (computeBPS_) {
            results_.additionalResults["gammaBPS"] = gammaBPS;
        }
    }

} // calculate()

} // namespace QuantExt
