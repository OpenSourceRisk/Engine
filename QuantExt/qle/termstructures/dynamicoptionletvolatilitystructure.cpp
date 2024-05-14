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

#include <qle/termstructures/dynamicoptionletvolatilitystructure.hpp>

namespace QuantExt {
DynamicOptionletVolatilityStructure::DynamicOptionletVolatilityStructure(
    const QuantLib::ext::shared_ptr<OptionletVolatilityStructure>& source, Natural settlementDays, const Calendar& calendar,
    ReactionToTimeDecay decayMode)
    : OptionletVolatilityStructure(settlementDays, calendar, source->businessDayConvention(), source->dayCounter()),
      source_(source), decayMode_(decayMode), originalReferenceDate_(source->referenceDate()),
      volatilityType_(source->volatilityType()), displacement_(source->displacement()) {
    QL_REQUIRE(decayMode_ != ForwardForwardVariance,
               "ForwardVariance not yet supported for DynamicOptionletVolatilityStructure");
    enableExtrapolation(source->allowsExtrapolation());
}

Rate DynamicOptionletVolatilityStructure::minStrike() const { return source_->minStrike(); }

Rate DynamicOptionletVolatilityStructure::maxStrike() const { return source_->maxStrike(); }

Date DynamicOptionletVolatilityStructure::maxDate() const {
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

void DynamicOptionletVolatilityStructure::update() { TermStructure::update(); }

QuantLib::ext::shared_ptr<SmileSection> DynamicOptionletVolatilityStructure::smileSectionImpl(Time optionTime) const {
    return source_->smileSection(optionTime);
}

Volatility DynamicOptionletVolatilityStructure::volatilityImpl(Time optionTime, Rate strike) const {
    return source_->volatility(optionTime, strike);
    // tentative ForwardForwardVariance implementation:
    // if (decayMode_ == ForwardForwardVariance) {
    //     Real timeToRef = source_->timeFromReference(referenceDate());
    //     Real varToRef = source_->blackVariance(timeToRef, strike);
    //     Real varToOptTime = source_->blackVariance(timeToRef + optionTime, strike);
    //     return std::sqrt((varToOptTime - varToRef) / optionTime);
    // }
}
} // namespace QuantExt
