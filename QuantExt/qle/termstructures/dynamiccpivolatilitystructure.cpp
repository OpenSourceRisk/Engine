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

#include <qle/termstructures/dynamiccpivolatilitystructure.hpp>

namespace QuantExt {
DynamicCPIVolatilitySurface::DynamicCPIVolatilitySurface(const QuantLib::ext::shared_ptr<CPIVolatilitySurface>& source,
                                                         ReactionToTimeDecay decayMode)
    : CPIVolatilitySurface(source->settlementDays(), source->calendar(), source->businessDayConvention(),
                           source->dayCounter(), source->observationLag(), source->frequency(),
                           source->indexIsInterpolated()),
      source_(source), decayMode_(decayMode), originalReferenceDate_(source->referenceDate()) {
    // Set extrapolation to source's extrapolation initially
    enableExtrapolation(source->allowsExtrapolation());
}

Rate DynamicCPIVolatilitySurface::minStrike() const { return source_->minStrike(); }

Rate DynamicCPIVolatilitySurface::maxStrike() const { return source_->maxStrike(); }

Date DynamicCPIVolatilitySurface::maxDate() const {
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

void DynamicCPIVolatilitySurface::update() { TermStructure::update(); }

Volatility DynamicCPIVolatilitySurface::volatilityImpl(Time length, Rate strike) const {
    if (decayMode_ == ConstantVariance) {
        // FIXME!
        Date m = referenceDate() + int(floor(365.25 * length)) * Days;
        return source_->volatility(m, strike, source_->observationLag());
    }

    // TODO: check validity of ForwardVariance option before using it.
    QL_REQUIRE(decayMode_ != ForwardForwardVariance,
               "ForwardVariance not yet supported for DynamicCPiVolatilityStructure");
    // if (decayMode_ == ForwardForwardVariance) {
    //     Volatility varToRef = source_->totalVariance(referenceDate(), strike, source_->observationLag());
    //     Volatility varToOptTime = source_->totalVariance(optionDate, strike, source_->observationLag());
    //     return std::sqrt((varToOptTime - varToRef) / timeFromReference(optionDate));
    // }

    QL_FAIL("Unexpected decay mode (" << decayMode_ << ")");
}
} // namespace QuantExt
