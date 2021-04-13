/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file adjusteddefaultcurve.hpp
    \brief default curve with SP(t) = exp(-int_0^t m * h(s) ds), with a multiplier m and source curve defining h(s)
    \ingroup models
*/

#pragma once

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

class AdjustedDefaultCurve : public SurvivalProbabilityStructure {
public:
    AdjustedDefaultCurve(const Handle<DefaultProbabilityTermStructure>& source, const Handle<Quote>& multiplier)
        : SurvivalProbabilityStructure(source->dayCounter()), source_(source), multiplier_(multiplier) {
        registerWith(source);
        registerWith(multiplier);
        enableExtrapolation(source->allowsExtrapolation());
    }
    Date maxDate() const override { return source_->maxDate(); }
    const Date& referenceDate() const override { return source_->referenceDate(); }

protected:
    Real survivalProbabilityImpl(Time t) const override {
        return std::pow(source_->survivalProbability(t), multiplier_->value());
    }
    void update() override { SurvivalProbabilityStructure::update(); }
    const Handle<DefaultProbabilityTermStructure> source_;
    const Handle<Quote> multiplier_;
};

} // namespace QuantExt
