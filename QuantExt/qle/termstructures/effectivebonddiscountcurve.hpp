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

/*! \file effectivebonddiscountcurve.hpp */

#pragma once

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! effective bond discount curve
class EffectiveBondDiscountCurve : public YieldTermStructure {
public:
    EffectiveBondDiscountCurve(const Handle<YieldTermStructure>& reference,
                               const Handle<DefaultProbabilityTermStructure>& credit, const Handle<Quote>& secSpread,
                               const Handle<Quote>& rr)
        : YieldTermStructure(reference->dayCounter()), reference_(reference), credit_(credit), secSpread_(secSpread),
          rr_(rr) {
        registerWith(reference_);
        registerWith(credit_);
        registerWith(secSpread_);
        registerWith(rr_);
    }
    Date maxDate() const override;
    const Date& referenceDate() const override;

protected:
    Real discountImpl(Time t) const override;
    Handle<YieldTermStructure> reference_;
    Handle<DefaultProbabilityTermStructure> credit_;
    Handle<Quote> secSpread_, rr_;
};

// inline

inline Date EffectiveBondDiscountCurve::maxDate() const { return reference_->maxDate(); }

inline const Date& EffectiveBondDiscountCurve::referenceDate() const { return reference_->referenceDate(); }

inline Real EffectiveBondDiscountCurve::discountImpl(Time t) const {
    Real d = reference_->discount(t);
    if (!credit_.empty()) {
        Real rr = rr_.empty() ? 0.0 : rr_->value();
        d *= std::pow(credit_->survivalProbability(t), (1.0 - rr));
    }
    if (!secSpread_.empty()) {
        d *= std::exp(-secSpread_->value() * t);
    }
    return d;
}

} // namespace QuantExt
