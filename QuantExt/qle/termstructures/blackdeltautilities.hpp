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

/*! \file qle/termstructures/blackdeltautilities.hpp
    \brief utilities to calculate strikes from deltas and atm strikes on smiles
    \ingroup termstructures
*/

#pragma once

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

// get a strike from a delta on an existing vol smile
Real getStrikeFromDelta(Option::Type optionType, Real delta, DeltaVolQuote::DeltaType dt, Real spot, Real domDiscount,
                        Real forDiscount, QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy = 1E-6,
                        Size maxIterations = 1000);

// get an atm strike on an existing vol smile
Real getAtmStrike(DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at, Real spot, Real domDiscount, Real forDiscount,
                  QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy = 1E-6,
                  Size maxIterations = 1000);

} // namespace QuantExt
