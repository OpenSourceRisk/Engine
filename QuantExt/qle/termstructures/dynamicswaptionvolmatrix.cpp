/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>

#include <ql/termstructures/volatility/flatsmilesection.hpp>

namespace QuantExt {

DynamicSwaptionVolatilityMatrix::DynamicSwaptionVolatilityMatrix(
    const boost::shared_ptr<SwaptionVolatilityStructure> &source,
    Natural settlementDays, const Calendar &calendar,
    ReactionToTimeDecay decayMode)
    : SwaptionVolatilityStructure(settlementDays, calendar,
                                  source->businessDayConvention(),
                                  source->dayCounter()),
      source_(source), decayMode_(decayMode),
      originalReferenceDate_(source->referenceDate()) {}

const Period &DynamicSwaptionVolatilityMatrix::maxSwapTenor() const {
    return source_->maxSwapTenor();
}

boost::shared_ptr<SmileSection>
DynamicSwaptionVolatilityMatrix::smileSectionImpl(Time optionTime,
                                                  Time swapLength) const {
    // dummy strike, just as in SwaptionVolatilityMatrix
    return boost::make_shared<FlatSmileSection>(
        optionTime, volatilityImpl(optionTime, swapLength, 0.05),
        source_->dayCounter(), Null<Real>(), source_->volatilityType(),
        shiftImpl(optionTime, swapLength));
}

Volatility DynamicSwaptionVolatilityMatrix::volatilityImpl(Time optionTime,
                                                           Time swapLength,
                                                           Rate strike) const {
    if (decayMode_ == ForwardForwardVariance) {
        Real tf = source_->timeFromReference(referenceDate());
        Real shift1 = 0.0, shift2 = 0.0;
        if (source_->volatilityType() == ShiftedLognormal) {
            shift2 = source_->shift(tf + optionTime, swapLength);
            shift1 = source_->shift(tf, swapLength);
        }
        return std::sqrt(
            (source_->blackVariance(tf + optionTime, swapLength, strike) *
                 (1.0 + shift2) -
             source_->blackVariance(tf, swapLength, strike) * (1.0 + shift1)) /
            ((1.0 + this->shift(optionTime, swapLength)) * optionTime));
    }
    if (decayMode_ == ConstantVariance) {
        return source_->volatility(optionTime, swapLength, strike);
    }
    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

Real DynamicSwaptionVolatilityMatrix::shiftImpl(Time optionTime,
                                                Time swapLength) const {
    if (source_->volatilityType() == Normal) {
        return 0.0;
    }
    if (decayMode_ == ForwardForwardVariance) {
        Real tf = source_->timeFromReference(referenceDate());
        return source_->shift(tf + optionTime, swapLength);
    }
    if (decayMode_ == ConstantVariance) {
        return source_->shift(optionTime, swapLength);
    }
    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

Real DynamicSwaptionVolatilityMatrix::minStrike() const {
    return source_->minStrike();
}

Real DynamicSwaptionVolatilityMatrix::maxStrike() const {
    return source_->maxStrike();
}

Date DynamicSwaptionVolatilityMatrix::maxDate() const {
    if (decayMode_ == ForwardForwardVariance) {
        return source_->maxDate();
    }
    if (decayMode_ == ConstantVariance) {
        return Date(std::min(Date::maxDate().serialNumber(),
                             referenceDate().serialNumber() -
                                 originalReferenceDate_.serialNumber() +
                                 source_->maxDate().serialNumber()));
    }
    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

void DynamicSwaptionVolatilityMatrix::update() {
    SwaptionVolatilityStructure::update();
}

} // namespace QuantExt
