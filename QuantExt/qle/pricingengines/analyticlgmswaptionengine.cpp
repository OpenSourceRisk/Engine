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

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <boost/bind/bind.hpp>

#include <ostream>

namespace QuantExt {

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(model->parametrization()),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping),
      caching_(false) {
    registerWith(model);
    registerWith(c_);
}

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const Size ccy,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(model->irlgm1f(ccy)),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping),
      caching_(false) {
    registerWith(model);
    registerWith(c_);
}

AnalyticLgmSwaptionEngine::AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1f,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const FloatSpreadMapping floatSpreadMapping)
    : GenericEngine<Swaption::arguments, Swaption::results>(), p_(irlgm1f),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve), floatSpreadMapping_(floatSpreadMapping),
      caching_(false) {
    registerWith(c_);
}

void AnalyticLgmSwaptionEngine::enableCache(const bool lgm_H_constant, const bool lgm_alpha_constant) {
    caching_ = true;
    lgm_H_constant_ = lgm_H_constant;
    lgm_alpha_constant_ = lgm_alpha_constant;
    clearCache();
}

void AnalyticLgmSwaptionEngine::clearCache() {
    S_.clear();             // indicates that H / alpha independent variables are not yet computed
    Hj_.clear();            // indicates that H dependent variables not yet computed
    zetaex_ = Null<Real>(); // indicates that alpha dependent variables are not yet computed
}

Real AnalyticLgmSwaptionEngine::flatAmount(const Size k) const {
    Date reference = p_->termStructure()->referenceDate();
    QuantLib::ext::shared_ptr<IborIndex> index =
        arguments_.swap ? arguments_.swap->iborIndex() : arguments_.swapOis->overnightIndex();
    if (arguments_.swapOis) {
        auto on = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndexedCoupon>(floatingLeg_[k]);
        QL_REQUIRE(on, "AnalyticalLgmSwaptionEngine::calculate(): internal error, could not cast to "
                       "QuantLib::OvernightIndexedCoupon.");
        QL_REQUIRE(!on->valueDates().empty(),
                   "AnalyticalLgmSwaptionEngine::calculate(): internal error, no value dates in ois coupon.");
        Date v1 = std::max(reference, on->valueDates().front());
        Date v2 = std::max(v1 + 1, on->valueDates().back());
        Real rate;
        if (on->averagingMethod() == QuantLib::RateAveraging::Compound)
            rate = (c_->discount(v1) / c_->discount(v2) - 1.0) / index->dayCounter().yearFraction(v1, v2);
        else
            rate = std::log(c_->discount(v1) / c_->discount(v2)) / index->dayCounter().yearFraction(v1, v2);
        return floatingLeg_[k]->accrualPeriod() * nominal_ * rate;
    } else {
        if (IborCoupon::Settings::instance().usingAtParCoupons()) {
            // if par coupons are used, we mimick the fixing estimation in IborCoupon; we make
            // sure that the estimation period does not start in the past and we do not use
            // historical fixings
            Date fixingValueDate =
                index->fixingCalendar().advance(floatingLeg_[k]->fixingDate(), index->fixingDays(), Days);
            fixingValueDate = std::max(fixingValueDate, reference);
            auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(floatingLeg_[k]);
            QL_REQUIRE(cpn, "AnalyticalLgmSwaptionEngine::calculate(): coupon expected on underlying swap "
                            "floating leg, could not cast");
            Date nextFixingDate = index->fixingCalendar().advance(cpn->accrualEndDate(),
                                                                  -static_cast<Integer>(index->fixingDays()), Days);
            Date fixingEndDate = index->fixingCalendar().advance(nextFixingDate, index->fixingDays(), Days);
            fixingEndDate = std::max(fixingEndDate, fixingValueDate + 1);
            Real spanningTime = index->dayCounter().yearFraction(fixingValueDate, fixingEndDate);
            DiscountFactor disc1 = c_->discount(fixingValueDate);
            DiscountFactor disc2 = c_->discount(fixingEndDate);
            Real fixing = (disc1 / disc2 - 1.0) / spanningTime;
            return fixing * floatingLeg_[k]->accrualPeriod() * nominal_;
        } else {
            // if indexed coupons are used, we use a proper fixing, but make sure that the fixing
            // date is not in the past and we do not use a historical fixing for "today"
            auto flatIbor = QuantLib::ext::make_shared<IborIndex>(
                index->familyName() + " (no fixings)", index->tenor(), index->fixingDays(), index->currency(),
                index->fixingCalendar(), index->businessDayConvention(), index->endOfMonth(), index->dayCounter(), c_);
            Date fixingDate = flatIbor->fixingCalendar().adjust(std::max(floatingLeg_[k]->fixingDate(), reference));
            return flatIbor->fixing(fixingDate) * floatingLeg_[k]->accrualPeriod() * nominal_;
        }
    }
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

    if (!caching_ || S_.empty()) {

        Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

        QL_REQUIRE(
            arguments_.swap || arguments_.swapOis,
            "AnalyticalLgmSwaptionEngine::calculate(): internal error, expected either swap or swapOis to be set.");
        const Schedule& fixedSchedule =
            arguments_.swap ? arguments_.swap->fixedSchedule() : arguments_.swapOis->schedule();
        const Schedule& floatSchedule =
            arguments_.swap ? arguments_.swap->floatingSchedule() : arguments_.swapOis->schedule();

        j1_ = std::lower_bound(fixedSchedule.dates().begin(), fixedSchedule.dates().end(), expiry) -
              fixedSchedule.dates().begin();
        k1_ = std::lower_bound(floatSchedule.dates().begin(), floatSchedule.dates().end(), expiry) -
              floatSchedule.dates().begin();

        nominal_ = arguments_.swap ? arguments_.swap->nominal() : arguments_.swapOis->nominal();

        fixedLeg_.clear();
	floatingLeg_.clear();
	for(auto const& c: (arguments_.swap ? arguments_.swap->fixedLeg() : arguments_.swapOis->fixedLeg())) {
	    fixedLeg_.push_back(QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c));
            QL_REQUIRE(fixedLeg_.back(),
                       "AnalyticalLgmSwaptionEngine::calculate(): internal error, could not cast to FixedRateCoupon");
        }
	for(auto const& c: (arguments_.swap ? arguments_.swap->floatingLeg() : arguments_.swapOis->overnightLeg())) {
	    floatingLeg_.push_back(QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c));
            QL_REQUIRE(
                floatingLeg_.back(),
                "AnalyticalLgmSwaptionEngine::calculate(): internal error, could not cast to FloatingRateRateCoupon");
        }

        // compute S_i, i.e. equivalent fixed rate spreads compensating for
        // a) a possibly non-zero float spread and
        // b) a spread between the ibor indices forwarding curve and the
        //     discounting curve
        // here, we do not work with a spread corrections directly, but
        // with this multiplied by the nominal and accrual basis,
        // so S_i is really an amount correction.

        S_.resize(fixedLeg_.size() - j1_);
        for (Size i = 0; i < S_.size(); ++i) {
            S_[i] = 0.0;
        }
        S_m1 = 0.0;
        Size ratio =
            static_cast<Size>(static_cast<Real>(floatingLeg_.size()) / static_cast<Real>(fixedLeg_.size()) + 0.5);
        QL_REQUIRE(ratio >= 1, "floating leg's payment frequency must be equal or "
                               "higher than fixed leg's payment frequency in "
                               "analytic lgm swaption engine");

        if(floatSpreadMapping_ == simple) {
            Real annuity = 0.0;
            for (Size j = j1_; j < fixedLeg_.size(); ++j) {
                annuity += nominal_ * fixedLeg_[j]->accrualPeriod() * c_->discount(fixedLeg_[j]->date());
            }
            Real floatAmountMismatch = 0.0;
            for(Size k=k1_; k<floatingLeg_.size();++k) {
                floatAmountMismatch +=
                    (floatingLeg_[k]->amount() - flatAmount(k)) * c_->discount(floatingLeg_[k]->date());
            }
            for (Size j = j1_; j < fixedLeg_.size(); ++j) {
                S_[j] = fixedLeg_[j]->accrualPeriod() * nominal_ * floatAmountMismatch / annuity;
            }
        }

        Size k = k1_;
        // The method reduces the problem to a one curve configuration w.r.t. the discount curve and
        // apply a correction for the discount curve / forwarding curve spread. Furthermore the method
        // assumes that no historical fixings are present in the floating rate coupons.
        for (Size j = j1_; j < fixedLeg_.size(); ++j) {
            Real sum1 = 0.0, sum2 = 0.0;
            for (Size rr = 0; rr < ratio && k < floatingLeg_.size(); ++rr, ++k) {
                Real amount = Null<Real>();
		// same strategy as in VanillaSwap::setupArguments()
                try {
                    amount = floatingLeg_[k]->amount();
                } catch (...) {
                }
                Real lambda1, lambda2;
                if (floatSpreadMapping_ == proRata) {
                    // we do not use the exact pay dates but the ratio to determine
                    // the distance to the adjacent payment dates
                    lambda2 = static_cast<Real>(rr + 1) / static_cast<Real>(ratio);
                    lambda1 = 1.0 - lambda2;
                } else if (floatSpreadMapping_ == nextCoupon) {
                    lambda1 = 0.0;
                    lambda2 = 1.0;
                } else {
                    lambda1 = lambda2 = 0.0;
                }
                if (amount != Null<Real>()) {
                    Real correction = (amount - flatAmount(k)) * c_->discount(floatingLeg_[k]->date());
                    sum1 += lambda1 * correction;
                    sum2 += lambda2 * correction;
                } else {
                    // if no amount is given, we do not need a spread correction
                    // due to different forward / discounting curves since then
                    // no curve is attached to the swap's ibor index and so we
                    // assume a one curve setup;
                    // but we can still have a float spread that has to be converted
                    // into a fixed leg's payment
                    Real correction = nominal_ * floatingLeg_[k]->spread() * floatingLeg_[k]->accrualPeriod() *
                                      c_->discount(floatingLeg_[k]->date());
                    sum1 += lambda1 * correction;
                    sum2 += lambda2 * correction;
                }
            }
            if (j > j1_) {
                S_[j - j1_ - 1] += sum1 / c_->discount(fixedLeg_[j - 1]->date());
            } else {
                S_m1 += sum1 / c_->discount(floatingLeg_[k1_]->accrualStartDate());
            }
            S_[j - j1_] += sum2 / c_->discount(fixedLeg_[j]->date());
        }

        w_ = type == Option::Call ? -1.0 : 1.0;
        D0_ = c_->discount(floatingLeg_[k1_]->accrualStartDate());
        Dj_.resize(fixedLeg_.size() - j1_);
        for (Size j = j1_; j < fixedLeg_.size(); ++j) {
            Dj_[j - j1_] = c_->discount(fixedLeg_[j - j1_]->date());
        }
    }

    if (!caching_ || !lgm_H_constant_ || Hj_.empty()) {
        // it is a requirement that H' does not change its sign,
        // with u = -1.0 we handle the case H' < 0
        u_ = p_->Hprime(0.0) > 0.0 ? 1.0 : -1.0;

        H0_ = p_->H(p_->termStructure()->timeFromReference(floatingLeg_[k1_]->accrualStartDate()));
        Hj_.resize(fixedLeg_.size() - j1_);
        for (Size j = j1_; j < fixedLeg_.size(); ++j) {
            Hj_[j - j1_] = p_->H(p_->termStructure()->timeFromReference(fixedLeg_[j]->date()));
        }
    }

    if (!caching_ || !lgm_alpha_constant_ || zetaex_ == Null<Real>()) {
        zetaex_ = p_->zeta(p_->termStructure()->timeFromReference(expiry));
    }

    Brent b;
    Real yStar;
    try {
        yStar = b.solve(QuantLib::ext::bind(&AnalyticLgmSwaptionEngine::yStarHelper, this, QuantLib::ext::placeholders::_1), 1.0E-6,
                        0.0, 0.01);
    } catch (const std::exception& e) {
        std::ostringstream os;
        os << "AnalyticLgmSwaptionEngine: failed to compute yStar (" << e.what() << "), parameter details: [";
        Real tte = p_->termStructure()->timeFromReference(expiry);
        os << "tte=" << tte << ", vol=" << std::sqrt(zetaex_ / tte) << ", nominal=" << nominal_
           << ", d=" << D0_;
        for (Size j = 0; j < Dj_.size(); ++j)
            os << ", d" << j << "=" << Dj_[j];
        os << ", h=" << H0_;
        for (Size j = 0; j < Hj_.size(); ++j)
            os << ", h" << j << "=" << Hj_[j];
        for (Size i = j1_; i < fixedLeg_.size(); ++i) {
            os << ", cpn" << i << "=(" << QuantLib::io::iso_date(fixedLeg_[i]->accrualStartDate()) << ","
               << QuantLib::io::iso_date(fixedLeg_[i]->date()) << "," << fixedLeg_[i]->amount() << ")";
        }
        os << ", S=" << S_m1;
        for (Size j = 0; j < S_.size(); ++j) {
            os << ", S" << j << "=" << S_[j];
        }
        os << "]";
        QL_FAIL(os.str());
    }

    CumulativeNormalDistribution N;
    Real sqrt_zetaex = std::sqrt(zetaex_);
    Real sum = 0.0;
    for (Size j = j1_; j < fixedLeg_.size(); ++j) {
        sum += w_ * (fixedLeg_[j]->amount() - S_[j - j1_]) * Dj_[j - j1_] *
               N(u_ * w_ * (yStar + (Hj_[j - j1_] - H0_) * zetaex_) / sqrt_zetaex);
    }
    sum += -w_ * S_m1 * D0_ * N(u_ * w_ * yStar / sqrt_zetaex);
    sum += w_ * (nominal_ * Dj_.back() * N(u_ * w_ * (yStar + (Hj_.back() - H0_) * zetaex_) / sqrt_zetaex) -
                 nominal_ * D0_ * N(u_ * w_ * yStar / sqrt_zetaex));
    results_.value = sum;

    results_.additionalResults["fixedAmountCorrectionSettlement"] = S_m1;
    results_.additionalResults["fixedAmountCorrections"] = S_;

} // calculate

Real AnalyticLgmSwaptionEngine::yStarHelper(const Real y) const {
    Real sum = 0.0;
    for (Size j = j1_; j < fixedLeg_.size(); ++j) {
        sum += (fixedLeg_[j]->amount() - S_[j - j1_]) * Dj_[j - j1_] *
               std::exp(-(Hj_[j - j1_] - H0_) * y - 0.5 * (Hj_[j - j1_] - H0_) * (Hj_[j - j1_] - H0_) * zetaex_);
    }
    sum += -S_m1 * D0_;
    sum += Dj_.back() * nominal_ *
           std::exp(-(Hj_.back() - H0_) * y - 0.5 * (Hj_.back() - H0_) * (Hj_.back() - H0_) * zetaex_);
    sum -= D0_ * nominal_;
    return sum;
}

} // namespace QuantExt
