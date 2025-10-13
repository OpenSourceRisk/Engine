/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/analytichwswaptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/bind/bind.hpp>

#include <iostream>

namespace QuantExt {

AnalyticHwSwaptionEngine::AnalyticHwSwaptionEngine(const ext::shared_ptr<HwModel>& model,
                                                   const Handle<YieldTermStructure>& discountCurve)
    : GenericEngine<Swaption::arguments, Swaption::results>(), model_(model), p_(model->parametrization()),
      c_(discountCurve.empty() ? p_->termStructure() : discountCurve) {
    registerWith(model_);
    registerWith(c_);
}

double AnalyticHwSwaptionEngine::time(const QuantLib::Date& d) const {
    return p_->termStructure()->timeFromReference(d);
}

void AnalyticHwSwaptionEngine::calculate() const {

    // 1 determine a few indices, this is very similar to what we do in AnalyticLgmSwaptionEngine

    Date reference = p_->termStructure()->referenceDate();
    Date expiry = arguments_.exercise->dates().back();

    if (expiry <= reference) {
        // swaption is expired, possibly generated swap is not
        // valued by this engine, so we set the npv to zero
        results_.value = 0.0;
        return;
    }

    Option::Type type = arguments_.type == VanillaSwap::Payer ? Option::Call : Option::Put;

    QL_REQUIRE(arguments_.swap != nullptr,
               "AnalyticalLgmSwaptionEngine::calculate(): internal error, expected swap to be set.");
    const Schedule& fixedSchedule = arguments_.swap->fixedSchedule();
    const Schedule& floatSchedule = arguments_.swap->floatingSchedule();

    std::vector<QuantLib::ext::shared_ptr<FixedRateCoupon>> fixedLeg;
    std::vector<QuantLib::ext::shared_ptr<FloatingRateCoupon>> floatingLeg;

    for (auto const& c : arguments_.swap->fixedLeg()) {
        fixedLeg.push_back(QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c));
        QL_REQUIRE(fixedLeg.back(),
                   "AnalyticalLgmSwaptionEngine::calculate(): internal error, could not cast to FixedRateCoupon");
    }

    for (auto const& c : arguments_.swap->floatingLeg()) {
        floatingLeg.push_back(QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c));
        QL_REQUIRE(
            floatingLeg.back(),
            "AnalyticalLgmSwaptionEngine::calculate(): internal error, could not cast to FloatingRateRateCoupon");
    }

    Size j1 = std::lower_bound(fixedSchedule.dates().begin(), fixedSchedule.dates().end(), expiry) -
              fixedSchedule.dates().begin();

    QL_REQUIRE(j1 < fixedLeg.size(), "fixed leg's periods are all before expiry.");

    Size k1 = std::lower_bound(floatSchedule.dates().begin(), floatSchedule.dates().end(), expiry) -
              floatSchedule.dates().begin();

    QL_REQUIRE(k1 < floatingLeg.size(), "floating leg's periods are all before expiry.");

    // 2 populate some members that are used below

    fixedAccrualTimes_.clear();
    fixedAccrualFractions_.clear();
    fixedPaymentTimes_.clear();

    fixedAccrualTimes_.push_back(time(fixedLeg[j1]->accrualStartDate()));

    for (Size j = j1; j < fixedLeg.size(); ++j) {
        fixedAccrualTimes_.push_back(time(fixedLeg[j]->accrualEndDate()));
        fixedAccrualFractions_.push_back(fixedLeg[j]->accrualPeriod());
        fixedPaymentTimes_.push_back(time(fixedLeg[j]->date()));
    }

    // 3 calculate the t0 fixed leg annuity

    Real annuity = 0.0;
    for (Size j = j1; j < fixedLeg.size(); ++j) {
        annuity += fixedLeg[j]->accrualPeriod() * c_->discount(fixedLeg[j]->date());
    }

    annuity *= arguments_.swap->nominal();

    // 3 calculate the flat (single-curve, no float spread) and actual float lev npv

    Real floatLegNpv = 0.0;
    Real flatFloatLegNpv = 0.0;

    for (Size k = k1; k < floatingLeg.size(); ++k) {
        Real tmp = flatAmount(floatingLeg[k], c_);
        flatFloatLegNpv += tmp * c_->discount(floatingLeg[k]->date());
        floatLegNpv += floatingLeg[k]->amount() * c_->discount(floatingLeg[k]->date());
    }

    // 4 calculate an effective t0 fixed rate corrected by the actual / flat float leg npv

    Real fixedLegNpv = 0.0;
    for (Size j = j1; j < fixedLeg.size(); ++j) {
        fixedLegNpv += fixedLeg[j]->amount() * c_->discount(fixedLeg[j]->date());
    }

    Real effectiveFixedRate = (fixedLegNpv - (floatLegNpv - flatFloatLegNpv)) / annuity;

    // 5 calculate the effective fair swap rate

    Real effectiveFairSwapRate = flatFloatLegNpv / annuity;

    // 6 calculate the approximate variance of the swap rate, cf. Lemma 12.1.19 in Piterbarg

    auto integrand = [this](QuantLib::Real t) -> QuantLib::Real {
        Array tmp = p_->sigma_x(t) * q(t);
        return DotProduct(tmp, tmp);
    };

    SimpsonIntegral integrator(1E-10, 16);
    Real optionExpiryTime = model_->parametrization()->termStructure()->timeFromReference(expiry);

    Real variance = integrator(integrand, 0.0, optionExpiryTime);

    // 7 calculate the swaption npv

    results_.value =
        QuantLib::bachelierBlackFormula(type, effectiveFixedRate, effectiveFairSwapRate, std::sqrt(variance), annuity);
}

Array AnalyticHwSwaptionEngine::q(const Time t) const {

    // determinisitc approximation for x(t), simplest approach
    const Array x(p_->n(), 0.0);

    double A = 0.0;
    for (Size i = 0; i < fixedAccrualFractions_.size(); ++i) {
        A += fixedAccrualFractions_[i] * model_->discountBond(t, fixedPaymentTimes_[i], x, c_);
    }

    double S = (model_->discountBond(t, fixedAccrualTimes_[0], x, c_) -
                model_->discountBond(t, fixedAccrualTimes_.back(), x, c_)) /
               A;

    /* slight generalization of Piterbarg, 12.1.6.2, we can use payment times != accrual end times here,
       coming from the annuity calculation */

    Array sum(p_->n(), 0.0);
    for (Size i = 0; i < fixedAccrualFractions_.size(); ++i) {
        sum += fixedAccrualFractions_[i] * model_->discountBond(t, fixedPaymentTimes_[i], x, c_) *
               p_->g(t, fixedPaymentTimes_[i]);
    }

    // typo in Piterbarg, 12.1.6.2, formula q_j(t,x), the second term should be added, not subtracted

    return -(model_->discountBond(t, fixedAccrualTimes_[0], x, c_) * p_->g(t, fixedAccrualTimes_[0]) -
             model_->discountBond(t, fixedAccrualTimes_.back(), x, c_) * p_->g(t, fixedAccrualTimes_.back())) /
               A +
           S / A * sum;
}

} // namespace QuantExt
