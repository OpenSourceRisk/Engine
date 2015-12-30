/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

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

#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/math/solvers1d/brent.hpp>

#include <boost/bind.hpp>

namespace QuantExt {

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(
    const boost::shared_ptr<XAssetModel> &model, const Size ccy,
    const Handle<YieldTermStructure> &discountCurve)
    : GenericModelEngine<XAssetModel, Swaption::arguments, Swaption::results>(
          model),
      ccy_(ccy), discountCurve_(discountCurve) {
    if (!discountCurve_.empty())
        registerWith(discountCurve_);
}

void AnalyticLgmSwaptionEngine::calculate() const {

    const IrLgm1fParametrization *const p = model_->irlgm1f(ccy_);
    Handle<YieldTermStructure> c =
        discountCurve_.empty() ? p->termStructure() : discountCurve_;

    QL_REQUIRE(arguments_.settlementType == Settlement::Physical,
               "cash-settled swaptions are not supported ...");

    Date reference = p->termStructure()->referenceDate();

    Date expiry = arguments_.exercise->dates().back();

    if (expiry <= reference) {
        // swaption is expired, possibly generated swap is not
        // valued by this engine, so we set the npv to zero
        results_.value = 0.0;
        return;
    }

    VanillaSwap swap = *arguments_.swap;
    Option::Type type =
        arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;
    Schedule fixedSchedule = swap.fixedSchedule();
    Schedule floatSchedule = swap.floatingSchedule();

    j1_ = std::lower_bound(fixedSchedule.dates().begin(),
                           fixedSchedule.dates().end(), expiry) -
          fixedSchedule.dates().begin();
    k1_ = std::lower_bound(floatSchedule.dates().begin(),
                           floatSchedule.dates().end(), expiry) -
          floatSchedule.dates().begin();

    // compute S_i, i.e. equivalent fixed rate spreads compensating for
    // a) a possibly non-zero float spread and
    // b) a spread between the ibor indices forwarding curve and the
    //     discounting curve
    // here, we do not work with a spread correction directly, but
    // multiply this implicitly with the nominal and fixed rate, so
    // S_i is really an amount correction rather.

    S_.resize(arguments_.fixedCoupons.size() - j1_);
    Size ratio = static_cast<Size>(
        static_cast<Real>(arguments_.floatingCoupons.size()) /
            static_cast<Real>(arguments_.fixedCoupons.size()) +
        0.5);
    QL_REQUIRE(ratio >= 1, "floating leg's payment frequency must be equal or "
                           "higher than fixed leg's payment frequency in "
                           "analytic lgm swaption engine");

    Size k = k1_;
    boost::shared_ptr<IborIndex> flatIbor = swap.iborIndex()->clone(c);
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        Real sum = 0.0;
        for (Size rr = 0; rr < ratio && k < arguments_.floatingCoupons.size();
             ++rr, ++k) {
            Real amount = arguments_.floatingCoupons[k];
            if (amount != Null<Real>()) {
                // if no amount is given, we do not need a spread correction
                // since then no curve is attached to the swap's ibor index
                // and so we assume a one curve setup.
                Real flatAmount =
                    flatIbor->fixing(arguments_.floatingFixingDates[k]) *
                    arguments_.floatingAccrualTimes[k];
                sum += (amount - flatAmount) *
                       c->discount(arguments_.floatingPayDates[k]);
            }
        }
        S_[j - j1_] = sum / c->discount(arguments_.fixedPayDates[j]);
    }

    // do the actual pricing

    zetaex_ = p->zeta(p->termStructure()->timeFromReference(expiry));
    H0_ = p->H(p->termStructure()->timeFromReference(
        arguments_.floatingResetDates[k1_]));
    D0_ = c->discount(arguments_.floatingResetDates[k1_]);
    Hj_.resize(arguments_.fixedCoupons.size() - j1_);
    Dj_.resize(arguments_.fixedCoupons.size() - j1_);
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        Hj_[j - j1_] = p->H(
            p->termStructure()->timeFromReference(arguments_.fixedPayDates[j]));
        Dj_[j - j1_] = c->discount(arguments_.fixedCoupons[j - j1_]);
    }

    Brent b;
    Real yStar =
        b.solve(boost::bind(&AnalyticLgmSwaptionEngine::yStarHelper, this, _1),
                1.0E-6, 0.0, 0.01);

    QuantExt::CumulativeNormalDistribution N;
    Real sqrt_zetaex = std::sqrt(zetaex_);
    Real sum = 0.0;
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        sum += (arguments_.fixedCoupons[j] - S_[j - j1_]) * Dj_[j - j1_] *
               (type == Option::Call
                    ? N((yStar + (Hj_[j - j1_] - H0_) * zetaex_) / sqrt_zetaex)
                    : 1.0 - N((yStar + (Hj_[j - j1_] - H0_) * zetaex_) /
                              sqrt_zetaex));
    }
    sum += Dj_.back() *
               (type == Option::Call
                    ? N((yStar + (Hj_.back() - H0_) * zetaex_) / sqrt_zetaex)
                    : 1.0 - N((yStar + (Hj_.back() - H0_) * zetaex_) /
                              sqrt_zetaex)) -
           D0_ * (type == Option::Call ? N(yStar / sqrt_zetaex)
                                       : 1.0 - N(yStar / sqrt_zetaex));

    results_.value = sum;

    results_.additionalResults["fixedAmountCorrections"] = S_;

} // calculate

Real AnalyticLgmSwaptionEngine::yStarHelper(const Real y) const {
    Real sum = 0.0;
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        sum +=
            (arguments_.fixedCoupons[j] - S_[j - j1_]) * Dj_[j - j1_] *
            std::exp(-(Hj_[j - j1_] - H0_) * y -
                     0.5 * (Hj_[j - 1] - H0_) * (Hj_[j - 1] - H0_) * zetaex_);
    }
    sum += Dj_.back() *
           std::exp(-(Hj_.back() - H0_) * y -
                    0.5 * (Hj_.back() - H0_) * (Hj_.back() - H0_) * zetaex_);
    sum -= D0_;
    return sum;
}

} // namespace QuantExt
