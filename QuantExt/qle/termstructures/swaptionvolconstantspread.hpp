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

#include <ql/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class ConstantSpreadSmileSection : public SmileSection {
public:
    ConstantSpreadSmileSection(const Handle<SwaptionVolatilityStructure>& atm,
                               const Handle<SwaptionVolatilityStructure>& cube, const Real optionTime,
                               const Real swapLength)
        : SmileSection(optionTime, DayCounter(), atm->volatilityType(),
                       atm->volatilityType() == ShiftedLognormal ? atm->shift(optionTime, swapLength) : 0.0),
          atm_(atm), cube_(cube), swapLength_(swapLength), section_(cube_->smileSection(optionTime, swapLength_)),
          atmStrike_(section_->atmLevel()) {
        registerWith(atm_);
        registerWith(cube_);
    }
    Rate minStrike() const override { return cube_->minStrike(); }
    Rate maxStrike() const override { return cube_->maxStrike(); }
    Rate atmLevel() const override { return Null<Real>(); }

protected:
    Volatility volatilityImpl(Rate strike) const override;

private:
    const Handle<SwaptionVolatilityStructure> atm_, cube_;
    const Real swapLength_;
    const QuantLib::ext::shared_ptr<SmileSection> section_;
    const Real atmStrike_;
};

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
    DayCounter dayCounter() const override { return atm_->dayCounter(); }
    Date maxDate() const override { return atm_->maxDate(); }
    Time maxTime() const override { return atm_->maxTime(); }
    const Date& referenceDate() const override { return atm_->referenceDate(); }
    Calendar calendar() const override { return atm_->calendar(); }
    Natural settlementDays() const override { return atm_->settlementDays(); }
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const override { return cube_->minStrike(); }
    Rate maxStrike() const override { return cube_->maxStrike(); }
    //@}
    //! \name SwaptionVolatilityStructure interface
    //@{
    const Period& maxSwapTenor() const override { return atm_->maxSwapTenor(); }
    VolatilityType volatilityType() const override { return atm_->volatilityType(); }
    //@}
    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}
    const Handle<SwaptionVolatilityStructure>& atmVol() { return atm_; }
    const Handle<SwaptionVolatilityStructure>& cube() { return cube_; }

protected:
    QuantLib::ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;

    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const override;

private:
    Handle<SwaptionVolatilityStructure> atm_, cube_;
};

} // namespace QuantExt

#endif
