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

#include <qle/models/exactbachelierimpliedvolatility.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>

#include <ql/pricingengines/blackformula.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

Real convertSwaptionVolatility(const Date& asof, const Period& optionTenor, const Period& swapTenor,
                               const boost::shared_ptr<SwapIndex>& swapIndexBase,
                               const boost::shared_ptr<SwapIndex>& shortSwapIndexBase, const DayCounter volDayCounter,
                               const Real strikeSpread, const Real inputVol, const QuantLib::VolatilityType inputType,
                               const Real inputShift, const QuantLib::VolatilityType outputType,
                               const Real outputShift) {

    // do we need a conversion at all?

    if (inputType == outputType &&
        (inputType == QuantLib::VolatilityType::Normal || QuantLib::close_enough(inputShift, outputShift))) {
        return inputVol;
    }

    // check we have the swap index bases

    QL_REQUIRE(swapIndexBase != nullptr, "convertSwaptionVolatility(): swapIndexBase is null");
    QL_REQUIRE(shortSwapIndexBase != nullptr, "convertSwaptionVolatility(): swapIndexBase is null");

    // determine the option date and time to expiry

    Date optionDate = swapIndexBase->fixingCalendar().advance(asof, optionTenor, Following);
    Real timeToExpiry = volDayCounter.yearFraction(asof, optionDate);

    // determine the ATM strike

    Real atmStrike = swapTenor <= shortSwapIndexBase->tenor() ? shortSwapIndexBase->clone(swapTenor)->fixing(optionDate)
                                                              : swapIndexBase->clone(swapTenor)->fixing(optionDate);

    // convert input vol to premium

    Option::Type otmOptionType = strikeSpread < 0.0 ? Option::Put : Option::Call;

    Real forwardPremium;
    if (inputType == QuantLib::VolatilityType::Normal) {
        forwardPremium = bachelierBlackFormula(otmOptionType, atmStrike + strikeSpread, atmStrike,
                                               inputVol * std::sqrt(timeToExpiry));
    } else {
        if (atmStrike + strikeSpread < -inputShift)
            forwardPremium = 0.0;
        else
            forwardPremium = blackFormula(otmOptionType, atmStrike + strikeSpread, atmStrike,
                                          inputVol * std::sqrt(timeToExpiry), 1.0, inputShift);
    }

    // convert permium back to vol

    Real outputVol;
    if (outputType == QuantLib::VolatilityType::Normal) {
        outputVol = exactBachelierImpliedVolatility(otmOptionType, atmStrike + strikeSpread, atmStrike, timeToExpiry,
                                                    forwardPremium);
    } else {
        outputVol = blackFormulaImpliedStdDev(otmOptionType, atmStrike + strikeSpread, atmStrike, forwardPremium, 1.0,
                                              outputShift) /
                    std::sqrt(timeToExpiry);
    }

    // return the result

    return outputVol;
}

} // namespace QuantExt
