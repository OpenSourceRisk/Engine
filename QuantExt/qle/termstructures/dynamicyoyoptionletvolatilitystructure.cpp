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

#include <qle/termstructures/dynamicyoyoptionletvolatilitystructure.hpp>

namespace QuantExt {
DynamicYoYOptionletVolatilitySurface::DynamicYoYOptionletVolatilitySurface(
    const QuantLib::ext::shared_ptr<YoYOptionletVolatilitySurface>& source, ReactionToTimeDecay decayMode)
    : YoYOptionletVolatilitySurface(source->settlementDays(), source->calendar(), source->businessDayConvention(),
                                    source->dayCounter(), source->observationLag(), source->frequency(),
                                    source->indexIsInterpolated(), source->volatilityType(), source->displacement()),
      source_(source), decayMode_(decayMode), originalReferenceDate_(source->referenceDate()) {
    // Set extrapolation to source's extrapolation initially
    enableExtrapolation(source->allowsExtrapolation());
}

Rate DynamicYoYOptionletVolatilitySurface::minStrike() const { return source_->minStrike(); }

Rate DynamicYoYOptionletVolatilitySurface::maxStrike() const { return source_->maxStrike(); }

Date DynamicYoYOptionletVolatilitySurface::maxDate() const {
    if (decayMode_ == ForwardForwardVariance) {
        return source_->maxDate();
    }

    if (decayMode_ == ConstantVariance) {
        return Date(std::min(Date::maxDate().serialNumber(), referenceDate().serialNumber() -
                                                                 originalReferenceDate_.serialNumber() +
                                                                 source_->maxDate().serialNumber()));
    }

    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

Volatility DynamicYoYOptionletVolatilitySurface::volatilityImpl(Time optionTime, Rate strike) const {
    if (decayMode_ == ConstantVariance) {
        return source_->volatility(optionTime, strike);
    }

    // TODO: check validity of ForwardVariance option before using it.
    QL_REQUIRE(decayMode_ != ForwardForwardVariance,
               "ForwardVariance not yet supported for DynamicYoYOptionletVolatilityStructure");
    if (decayMode_ == ForwardForwardVariance) {
        // FIXME
        Real t0 = timeFromBase(referenceDate());
        Volatility varToRef = source_->volatility(t0, strike) * source_->volatility(t0, strike) * t0;
        Volatility varToOptTime =
            source_->volatility(optionTime, strike) * source_->volatility(optionTime, strike) * optionTime;
        return std::sqrt((varToOptTime - varToRef) / (optionTime - t0));
    }

    QL_FAIL("Unexpected decay mode (" << decayMode_ << ")");
}
} // namespace QuantExt
