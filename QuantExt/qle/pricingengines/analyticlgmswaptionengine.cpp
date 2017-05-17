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

#include <ql/indexes/iborindex.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <boost/bind.hpp>

namespace QuantExt {

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(model->parametrization()),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping) {
    registerWith(model);
    registerWith(c_);
}

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const boost::shared_ptr<CrossAssetModel>& model, const Size ccy,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(model->irlgm1f(ccy)),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping) {
    registerWith(model);
    registerWith(c_);
}

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const boost::shared_ptr<IrLgm1fParametrization> irlgm1f,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(irlgm1f),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping) {
    registerWith(c_);
}

void AnalyticLgmSwaptionEngine::calculate() const {

    QL_REQUIRE(arguments_.settlementType == Settlement::Physical, "cash-settled swaptions are not supported ...");

    Date reference = p_->termStructure()->referenceDate();

    Date expiry = arguments_.exercise->dates().back();

    if (expiry <= reference) {
        // swaption is expired, possibly generated swap is not
        // valued by this engine, so we set the npv to zero
        results_.value = 0.0;
        return;
    }

    VanillaSwap swap = *arguments_.swap;
    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;
    Schedule fixedSchedule = swap.fixedSchedule();
    Schedule floatSchedule = swap.floatingSchedule();

    j1_ = std::lower_bound(fixedSchedule.dates().begin(), fixedSchedule.dates().end(), expiry) -
          fixedSchedule.dates().begin();
    k1_ = std::lower_bound(floatSchedule.dates().begin(), floatSchedule.dates().end(), expiry) -
          floatSchedule.dates().begin();

    // compute S_i, i.e. equivalent fixed rate spreads compensating for
    // a) a possibly non-zero float spread and
    // b) a spread between the ibor indices forwarding curve and the
    //     discounting curve
    // here, we do not work with a spread corrections directly, but
    // with this multiplied by the nominal and accrual basis,
    // so S_i is really an amount correction.

    S_.resize(arguments_.fixedCoupons.size() - j1_);
    for (Size i = 0; i < S_.size(); ++i) {
        S_[i] = 0.0;
    }
    S_m1 = 0.0;
    Size ratio = static_cast<Size>(
        static_cast<Real>(arguments_.floatingCoupons.size()) / static_cast<Real>(arguments_.fixedCoupons.size()) + 0.5);
    QL_REQUIRE(ratio >= 1, "floating leg's payment frequency must be equal or "
                           "higher than fixed leg's payment frequency in "
                           "analytic lgm swaption engine");

    Size k = k1_;
    boost::shared_ptr<IborIndex> flatIbor = swap.iborIndex()->clone(c_);
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        Real sum1 = 0.0, sum2 = 0.0;
        for (Size rr = 0; rr < ratio && k < arguments_.floatingCoupons.size(); ++rr, ++k) {
            Real amount = arguments_.floatingCoupons[k];
            Real lambda1 = 0.0, lambda2 = 1.0;
            if (floatSpreadMapping_ == proRata) {
                // we do not use the exact pay dates but the ratio to determine
                // the distance to the adjacent payment dates
                lambda2 = static_cast<Real>(rr + 1) / static_cast<Real>(ratio);
                lambda1 = 1.0 - lambda2;
            }
            if (amount != Null<Real>()) {
                Real flatAmount = flatIbor->fixing(arguments_.floatingFixingDates[k]) *
                                  arguments_.floatingAccrualTimes[k] * arguments_.nominal;
                Real correction = (amount - flatAmount) * c_->discount(arguments_.floatingPayDates[k]);
                sum1 += lambda1 * correction;
                sum2 += lambda2 * correction;
            } else {
                // if no amount is given, we do not need a spread correction
                // due to different forward / discounting curves since then
                // no curve is attached to the swap's ibor index and so we
                // assume a one curve setup;
                // but we can still have a float spread that has to be converted
                // into a fixed leg's payment
                Real correction = arguments_.nominal * arguments_.floatingSpreads[k] *
                                  arguments_.floatingAccrualTimes[k] * c_->discount(arguments_.floatingPayDates[k]);
                sum1 += lambda1 * correction;
                sum2 += lambda2 * correction;
            }
        }
        if (j > j1_) {
            S_[j - j1_ - 1] += sum1 / c_->discount(arguments_.fixedPayDates[j - 1]);
        } else {
            S_m1 += sum1 / c_->discount(arguments_.floatingResetDates[k1_]);
        }
        S_[j - j1_] += sum2 / c_->discount(arguments_.fixedPayDates[j]);
    }

    Real w = type == Option::Call ? -1.0 : 1.0;

    // it is a requirement that H' does not change its sign,
    // with u = -1.0 we handle the case H' < 0
    Real u = p_->Hprime(0.0) > 0.0 ? 1.0 : -1.0;

    // do the actual pricing

    zetaex_ = p_->zeta(p_->termStructure()->timeFromReference(expiry));
    H0_ = p_->H(p_->termStructure()->timeFromReference(arguments_.floatingResetDates[k1_]));
    D0_ = c_->discount(arguments_.floatingResetDates[k1_]);
    Hj_.resize(arguments_.fixedCoupons.size() - j1_);
    Dj_.resize(arguments_.fixedCoupons.size() - j1_);
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        Hj_[j - j1_] = p_->H(p_->termStructure()->timeFromReference(arguments_.fixedPayDates[j]));
        Dj_[j - j1_] = c_->discount(arguments_.fixedPayDates[j - j1_]);
    }

    Brent b;
    Real yStar;
    try {
        yStar = b.solve(boost::bind(&AnalyticLgmSwaptionEngine::yStarHelper, this, _1), 1.0E-6, 0.0, 0.01);
    } catch (const std::exception& e) {
        QL_FAIL("AnalyticLgmSwaptionEngine, failed to compute yStar, " << e.what());
    }

    CumulativeNormalDistribution N;
    Real sqrt_zetaex = std::sqrt(zetaex_);
    Real sum = 0.0;
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        sum += w * (arguments_.fixedCoupons[j] - S_[j - j1_]) * Dj_[j - j1_] *
               N(u * w * (yStar + (Hj_[j - j1_] - H0_) * zetaex_) / sqrt_zetaex);
    }
    sum += -w * S_m1 * D0_ * N(u * w * yStar / sqrt_zetaex);
    sum += w * (arguments_.nominal * Dj_.back() * N(u * w * (yStar + (Hj_.back() - H0_) * zetaex_) / sqrt_zetaex) -
                arguments_.nominal * D0_ * N(u * w * yStar / sqrt_zetaex));
    results_.value = sum;

    results_.additionalResults["fixedAmountCorrectionSettlement"] = S_m1;
    results_.additionalResults["fixedAmountCorrections"] = S_;

} // calculate

Real AnalyticLgmSwaptionEngine::yStarHelper(const Real y) const {
    Real sum = 0.0;
    for (Size j = j1_; j < arguments_.fixedCoupons.size(); ++j) {
        sum += (arguments_.fixedCoupons[j] - S_[j - j1_]) * Dj_[j - j1_] *
               std::exp(-(Hj_[j - j1_] - H0_) * y - 0.5 * (Hj_[j - j1_] - H0_) * (Hj_[j - j1_] - H0_) * zetaex_);
    }
    sum += -S_m1 * D0_;
    sum += Dj_.back() * arguments_.nominal *
           std::exp(-(Hj_.back() - H0_) * y - 0.5 * (Hj_.back() - H0_) * (Hj_.back() - H0_) * zetaex_);
    sum -= D0_ * arguments_.nominal;
    return sum;
}

} // namespace QuantExt
