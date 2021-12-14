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

#include <qle/termstructures/blackvolconstantspread.hpp>

using namespace QuantLib;

namespace QuantExt {

BlackVolatilityConstantSpread::BlackVolatilityConstantSpread(const Handle<BlackVolTermStructure>& atm,
                                                             const Handle<BlackVolTermStructure>& surface)
    : BlackVolTermStructure(0, atm->calendar(), atm->businessDayConvention(), atm->dayCounter()), atm_(atm),
      surface_(surface) {
    enableExtrapolation(atm->allowsExtrapolation());
    registerWith(atm);
    registerWith(surface);
}

DayCounter BlackVolatilityConstantSpread::dayCounter() const { 
    return atm_->dayCounter(); 
}

Date BlackVolatilityConstantSpread::maxDate() const { 
    return atm_->maxDate(); 
}

Time BlackVolatilityConstantSpread::maxTime() const { 
    return atm_->maxTime(); 
}

const Date& BlackVolatilityConstantSpread::referenceDate() const { 
    return atm_->referenceDate(); 
}

Calendar BlackVolatilityConstantSpread::calendar() const { 
    return atm_->calendar(); 
}

Natural BlackVolatilityConstantSpread::settlementDays() const { 
    return atm_->settlementDays(); 
}

Rate BlackVolatilityConstantSpread::minStrike() const { 
    return surface_->minStrike(); 
}

Rate BlackVolatilityConstantSpread::maxStrike() const { 
    return surface_->maxStrike(); 
}

void BlackVolatilityConstantSpread::deepUpdate() {
    atm_->update();
    update();
}

Volatility BlackVolatilityConstantSpread::blackVolImpl(Time t, Rate strike) const {
    Real s = surface_->blackVol(t, strike, true) - surface_->blackVol(t, Null<Real>(), true);
    Real v = atm_->blackVol(t, Null<Real>(), true);
    return v + s;
}

Real BlackVolatilityConstantSpread::blackVarianceImpl(Time t, Real strike) const {
    Real vol = blackVolImpl(t, strike);
    return vol * vol * t;
}


} // QuantExt