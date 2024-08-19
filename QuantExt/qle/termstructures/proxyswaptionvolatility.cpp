/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/termstructures/atmadjustedsmilesection.hpp>
#include <qle/termstructures/proxyswaptionvolatility.hpp>
#include <qle/utilities/time.hpp>

namespace QuantExt {

using namespace QuantLib;

ProxySwaptionVolatility::ProxySwaptionVolatility(const QuantLib::Handle<SwaptionVolatilityStructure>& baseVol,
                                                 QuantLib::ext::shared_ptr<SwapIndex> baseSwapIndexBase,
                                                 QuantLib::ext::shared_ptr<SwapIndex> baseShortSwapIndexBase,
                                                 QuantLib::ext::shared_ptr<SwapIndex> targetSwapIndexBase,
                                                 QuantLib::ext::shared_ptr<SwapIndex> targetShortSwapIndexBase)
    : SwaptionVolatilityStructure(baseVol->businessDayConvention(), baseVol->dayCounter()), baseVol_(baseVol),
      baseSwapIndexBase_(baseSwapIndexBase), baseShortSwapIndexBase_(baseShortSwapIndexBase),
      targetSwapIndexBase_(targetSwapIndexBase), targetShortSwapIndexBase_(targetShortSwapIndexBase) {
    enableExtrapolation(baseVol->allowsExtrapolation());
}

QuantLib::ext::shared_ptr<SmileSection> ProxySwaptionVolatility::smileSectionImpl(Time optionTime, Time swapLength) const {
    // imply option date from option time
    Date optionDate = lowerDate(optionTime, referenceDate(), dayCounter());
    // imply swap tenor from swap length
    Period swapTenor = tenorFromLength(swapLength);
    return smileSectionImpl(optionDate, swapTenor);
}

QuantLib::ext::shared_ptr<SmileSection> ProxySwaptionVolatility::smileSectionImpl(const Date& optionDate,
                                                                          const Period& swapTenor) const {
    Real baseAtmLevel =
        swapTenor > baseShortSwapIndexBase_->tenor()
            ? baseSwapIndexBase_->clone(swapTenor)->fixing(baseSwapIndexBase_->fixingCalendar().adjust(optionDate))
            : baseShortSwapIndexBase_->clone(swapTenor)->fixing(
                  baseShortSwapIndexBase_->fixingCalendar().adjust(optionDate));
    Real targetAtmLevel =
        swapTenor > targetShortSwapIndexBase_->tenor()
            ? targetSwapIndexBase_->clone(swapTenor)->fixing(targetSwapIndexBase_->fixingCalendar().adjust(optionDate))
            : targetShortSwapIndexBase_->clone(swapTenor)->fixing(
                  targetShortSwapIndexBase_->fixingCalendar().adjust(optionDate));
    return QuantLib::ext::make_shared<AtmAdjustedSmileSection>(baseVol_->smileSection(optionDate, swapTenor, true),
                                                       baseAtmLevel, targetAtmLevel);
}

Volatility ProxySwaptionVolatility::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    return smileSection(optionTime, swapLength)->volatility(strike);
}

} // namespace QuantExt
