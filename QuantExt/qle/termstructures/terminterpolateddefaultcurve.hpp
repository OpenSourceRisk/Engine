/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file terminterpolateddefaultcurve.hpp
    \brief default curve interpolating between two term curves
*/

#pragma once

#include <ql/math/comparison.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {

class TermInterpolatedDefaultCurve : public SurvivalProbabilityStructure {
public:
    TermInterpolatedDefaultCurve(const Handle<DefaultProbabilityTermStructure>& c1,
                                 const Handle<DefaultProbabilityTermStructure>& c2, const Real alpha)
        : SurvivalProbabilityStructure(c1->dayCounter()), c1_(c1), c2_(c2), alpha_(alpha) {
        registerWith(c1_);
        registerWith(c2_);
    }
    Date maxDate() const override { return std::min(c1_->maxDate(), c2_->maxDate()); }
    Time maxTime() const override { return std::min(c1_->maxTime(), c2_->maxTime()); }
    const Date& referenceDate() const override { return c1_->referenceDate(); }
    Calendar calendar() const override { return c1_->calendar(); }
    Natural settlementDays() const override { return c1_->settlementDays(); }
    Probability survivalProbabilityImpl(Time t) const override {
        return std::pow(c1_->survivalProbability(t), alpha_) * std::pow(c2_->survivalProbability(t), 1.0 - alpha_);
    }

private:
    Handle<DefaultProbabilityTermStructure> c1_, c2_;
    Real alpha_;
};

} // namespace QuantExt
