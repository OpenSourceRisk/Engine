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

#include <qle/termstructures/swaptionvolconstantspread.hpp>

#include <iostream>

namespace QuantExt {

Volatility ConstantSpreadSmileSection::volatilityImpl(Rate strike) const {
    Real t = exerciseTime();
    Real s = section_->volatility(strike) - section_->volatility(atmStrike_);
    Real v = atm_->volatility(t, swapLength_, strike);
    return v + s;
}

QuantLib::ext::shared_ptr<SmileSection> SwaptionVolatilityConstantSpread::smileSectionImpl(Time optionTime,
                                                                                   Time swapLength) const {
    return QuantLib::ext::make_shared<ConstantSpreadSmileSection>(atm_, cube_, optionTime, swapLength);
}

Volatility SwaptionVolatilityConstantSpread::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    if (strike == Null<Real>())
        return atm_->volatility(optionTime, swapLength, 0.0);
    else {
        return smileSectionImpl(optionTime, swapLength)->volatility(strike);
    }
}

void SwaptionVolatilityConstantSpread::deepUpdate() {
    atm_->update();
    cube_->update();
    update();
}

} // namespace QuantExt
