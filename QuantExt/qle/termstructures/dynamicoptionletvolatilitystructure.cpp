/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/termstructures/dynamicoptionletvolatilitystructure.hpp>

namespace QuantExt {
    DynamicOptionletVolatilityStructure::DynamicOptionletVolatilityStructure(
        const boost::shared_ptr<OptionletVolatilityStructure>& source, Natural settlementDays,
        const Calendar& calendar, ReactionToTimeDecay decayMode)
        : OptionletVolatilityStructure(settlementDays, calendar, source->businessDayConvention(), 
          source->dayCounter()), source_(source), decayMode_(decayMode), 
          originalReferenceDate_(source->referenceDate()), volatilityType_(source->volatilityType()), 
          displacement_(source->displacement()) {
        // Set extrapolation to source's extrapolation initially
        enableExtrapolation(source->allowsExtrapolation());
    }

    Rate DynamicOptionletVolatilityStructure::minStrike() const {
        return source_->minStrike();
    }

    Rate DynamicOptionletVolatilityStructure::maxStrike() const {
        return source_->maxStrike();
    }

    Date DynamicOptionletVolatilityStructure::maxDate() const {
        if (decayMode_ == ForwardForwardVariance) {
            return source_->maxDate();
        }

        if (decayMode_ == ConstantVariance) {
            return Date(std::min(Date::maxDate().serialNumber(), referenceDate().serialNumber() - 
                originalReferenceDate_.serialNumber() + source_->maxDate().serialNumber()));
        }

        QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
    }
    
    void DynamicOptionletVolatilityStructure::update() {
        TermStructure::update();
    }

    boost::shared_ptr<SmileSection> DynamicOptionletVolatilityStructure::smileSectionImpl(Time optionTime) const {
        // Again, what strikes do we chose? Should not need this in any case.
        QL_FAIL("Smile section not implemented for DynamicOptionletVolatilityStructure");
    }

    Volatility DynamicOptionletVolatilityStructure::volatilityImpl(Time optionTime, Rate strike) const {
        if (decayMode_ == ConstantVariance) {
            return source_->volatility(optionTime, strike);
        }
        
        // TODO: check validity of ForwardVariance option before using it.
        QL_REQUIRE(decayMode_ != ForwardForwardVariance, 
            "ForwardVariance not yet supported for DynamicOptionletVolatilityStructure");
        if (decayMode_ == ForwardForwardVariance) {
            Real timeToRef = source_->timeFromReference(referenceDate());
            Real varToRef = source_->blackVariance(timeToRef, strike);
            Real varToOptTime = source_->blackVariance(timeToRef + optionTime, strike);
            return std::sqrt((varToOptTime - varToRef) / optionTime);
        }

        QL_FAIL("Unexpected decay mode (" << decayMode_ << ")");
    }
}
