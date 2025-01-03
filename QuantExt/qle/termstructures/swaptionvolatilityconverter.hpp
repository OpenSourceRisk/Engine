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

/*! \file qle/termstructures/swaptionvolatilityconverter.hpp
    \brief Convert swaption volatilities from one type to another
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>

#include <ql/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

//! swaption vol converter
Real convertSwaptionVolatility(const Date& asof, const Period& optionTenor, const Period& swapTenor,
                               const QuantLib::ext::shared_ptr<SwapIndex>& swapIndexBase,
                               const QuantLib::ext::shared_ptr<SwapIndex>& shortSwapIndexBase, const DayCounter volDayCounter,
                               const Real strikeSpread, const Real inputVol, const QuantLib::VolatilityType inputType,
                               const Real inputShift, const QuantLib::VolatilityType outputType,
                               const Real outputShift);

} // namespace QuantExt
