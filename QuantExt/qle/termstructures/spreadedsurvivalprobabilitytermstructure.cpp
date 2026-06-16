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

#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>

namespace QuantExt {

SpreadedSurvivalProbabilityTermStructure::SpreadedSurvivalProbabilityTermStructure(
    const Handle<DefaultProbabilityTermStructure>& referenceCurve, const std::vector<Time>& times,
    const std::vector<Handle<Quote>>& spreads, const Extrapolation extrapolation,
    const YieldCurveRollDown yieldCurveRollDown)
    : SurvivalProbabilityStructure(0, referenceCurve->calendar(), referenceCurve->dayCounter()),
      referenceCurve_(referenceCurve), times_(times), spreads_(spreads), data_(times.size(), 1.0),
      extrapolation_(extrapolation), yieldCurveRollDown_(yieldCurveRollDown) {
    QL_REQUIRE(times_.size() > 1, "at least two times required");
    QL_REQUIRE(times_.size() == spreads_.size(), "size of time and quote vectors do not match");
    QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]);
    for (Size i = 0; i < spreads_.size(); ++i) {
        registerWith(spreads_[i]);
    }
    interpolation_ = QuantLib::ext::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    interpolation_->enableExtrapolation();
    registerWith(Settings::instance().evaluationDate());
    registerWith(referenceCurve_);
}

void SpreadedSurvivalProbabilityTermStructure::update() {
    LazyObject::update();
    TermStructure::update();
}

void SpreadedSurvivalProbabilityTermStructure::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!spreads_[i].empty(),
                   "SpreadedSurvivalProbabilityTermStructure: quote at index " << i << " is empty");
        data_[i] = spreads_[i]->value();
        QL_REQUIRE(data_[i] > 0,
                   "SpreadedSurvivalProbabilityTermStructure: invalid value " << data_[i] << " at index " << i);
    }
    interpolation_->update();
}

Probability SpreadedSurvivalProbabilityTermStructure::survivalProbabilityImpl(Time t) const {
    calculate();
    if (QuantLib::close_enough(referenceCurve()->survivalProbability(t), 0.0))
        return 0.0;

    Real baseSurvProb;
    if (referenceDate() == referenceCurve_->referenceDate()) {
        baseSurvProb = referenceCurve_->survivalProbability(t);
    } else {
        if (yieldCurveRollDown_ == YieldCurveRollDown::ConstantDiscounts) {
            baseSurvProb = referenceCurve_->survivalProbability(t);
        } else if (yieldCurveRollDown_ == YieldCurveRollDown::ForwardForward) {
            Time t0 = referenceCurve_->timeFromReference(referenceDate());
            baseSurvProb = referenceCurve_->survivalProbability(t + t0) / referenceCurve_->survivalProbability(t0);
        } else {
            QL_FAIL("SpreadedSurvivalProbabilityTermStructure::survivalProbabilityImpl(): yield curve rolldown not "
                    "handled, internal error.");
        }
    }

    if (t <= this->times_.back())
        return baseSurvProb * (*interpolation_)(t, true);
    // flat fwd extrapolation
    Real tMax = this->times_.back();
    Real dMax = this->data_.back();
    if (extrapolation_ == Extrapolation::flatFwd) {
        Real instFwdMax = -(*interpolation_).derivative(tMax) / dMax;
        return baseSurvProb * dMax * std::exp(-instFwdMax * (t - tMax));
    } else {
        return baseSurvProb * std::pow(dMax, t / tMax);
    }
}

Date SpreadedSurvivalProbabilityTermStructure::maxDate() const { return referenceCurve_->maxDate(); }

std::vector<Time> SpreadedSurvivalProbabilityTermStructure::times() const { return times_; }

Handle<DefaultProbabilityTermStructure> SpreadedSurvivalProbabilityTermStructure::referenceCurve() const {
    return referenceCurve_;
}
} // namespace QuantExt
