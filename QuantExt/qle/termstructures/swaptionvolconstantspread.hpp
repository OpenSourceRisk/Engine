/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/swaptionvolconstantspread.hpp
    \brief swaption cube that combines an ATM matrix and vol spreads from a cube
    \ingroup termstructures
*/

#ifndef quantext_swaptionvolcubeconstantspread_hpp
#define quantext_swaptionvolcubeconstantspread_hpp

#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>

#include <boost/shared_ptr.hpp>

#include <iostream>

using namespace QuantLib;

namespace QuantExt {

namespace {
class ConstantSpreadSmileSection : public SmileSection {
public:
    ConstantSpreadSmileSection(const Handle<SwaptionVolatilityStructure>& atm,
                               const Handle<SwaptionVolatilityStructure>& cube, const Real optionTime,
                               const Real swapLength)
        : SmileSection(optionTime, DayCounter(), atm->volatilityType(),
                       atm->volatilityType() == ShiftedLognormal ? atm->shift(optionTime, swapLength) : 0.0),
          atm_(atm), cube_(cube), swapLength_(swapLength), section_(cube_->smileSection(optionTime, swapLength_)),
          atmStrike_(section_->atmLevel()) {}
    Rate minStrike() const { return cube_->minStrike(); }
    Rate maxStrike() const { return cube_->maxStrike(); }
    Rate atmLevel() const { return Null<Real>(); }

protected:
    Volatility volatilityImpl(Rate strike) const {
        Real t = exerciseTime();
        Real s = section_->volatility(strike) - section_->volatility(atmStrike_);
        Real v = atm_->volatility(t, swapLength_, strike);
        return v + s;
    }

private:
    const Handle<SwaptionVolatilityStructure> atm_, cube_;
    const Real swapLength_;
    const boost::shared_ptr<SmileSection> section_;
    const Real atmStrike_;
};
} // anonymous namespace

//! Swaption cube that combines an ATM matrix and vol spreads from a cube
/*! Notice that the TS has a floating reference date and accesses the source TS only via
 their time-based volatility methods.

 \warning the given atm vol structure should be strike independent, this is not checked
 \warning the given cube must provide smile sections that provide an ATM level

 \ingroup termstructures
*/
class SwaptionVolatilityConstantSpread : public SwaptionVolatilityStructure {
public:
    SwaptionVolatilityConstantSpread(const Handle<SwaptionVolatilityStructure>& atm,
                                     const Handle<SwaptionVolatilityStructure>& cube)
        : SwaptionVolatilityStructure(0, atm->calendar(), atm->businessDayConvention(), atm->dayCounter()), atm_(atm),
          cube_(cube) {
        enableExtrapolation(atm->allowsExtrapolation());
        registerWith(atm_);
        registerWith(cube_);
    }

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return atm_->dayCounter(); }
    Date maxDate() const { return atm_->maxDate(); }
    Time maxTime() const { return atm_->maxTime(); }
    const Date& referenceDate() const { return atm_->referenceDate(); }
    Calendar calendar() const { return atm_->calendar(); }
    Natural settlementDays() const { return atm_->settlementDays(); }
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const { return cube_->minStrike(); }
    Rate maxStrike() const { return cube_->maxStrike(); }
    //@}
    //! \name SwaptionVolatilityStructure interface
    //@{
    const Period& maxSwapTenor() const { return atm_->maxSwapTenor(); }
    VolatilityType volatilityType() const { return atm_->volatilityType(); }
    //@}
    const Handle<SwaptionVolatilityStructure>& atmVol() { return atm_; }
    const Handle<SwaptionVolatilityStructure>& cube() { return cube_; }

protected:
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const {
        return boost::make_shared<ConstantSpreadSmileSection>(atm_, cube_, optionTime, swapLength);
    }

    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
        return smileSectionImpl(optionTime, swapLength)->volatility(strike);
    }

private:
    Handle<SwaptionVolatilityStructure> atm_, cube_;
};

} // namespace QuantExt

#endif
