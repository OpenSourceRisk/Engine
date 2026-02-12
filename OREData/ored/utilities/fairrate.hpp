/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/fairrate.hpp
    \brief Fair rate utility for multi-leg swaps
    \ingroup utilities
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace ore {
namespace data {

/*! Select the reference leg index for fair-rate computation.

    Rules:
    1. If a fixed leg is present, use the first fixed leg.
    2. If multiple fixed legs are present, only the first is the reference;
       the caller should exclude the remaining fixed legs from fairRate().
    3. If no fixed leg is present, use the first floating leg with a
       non-zero spread. If none has a spread, use the first floating leg.

    \param legData   The LegData descriptors (used for leg-type inspection).
    \return pair(referenceLegIndex, indicesOfLegsToExclude)
*/
std::pair<QuantLib::Size, std::vector<QuantLib::Size>>
selectReferenceLeg(const std::vector<LegData>& legData);

/*! Compute the fair rate of a collection of swap legs.

    If the reference leg is fixed, returns the par fixed rate.
    If the reference leg is floating, returns the fair spread.

    \param legs             Vector of cashflow legs.
    \param isPayer          Pay (true) / receive (false) indicator per leg.
    \param discountCurves   Discount curve per leg for NPV/BPS calculation.
    \param fxSpotToBase     FX spot per leg converting leg-ccy NPV to base ccy.
                            Use 1.0 for all legs in a single-currency swap.
    \param referenceLegIdx  Index of the leg versus which the fair rate is expressed.

    \return The fair fixed rate (if reference leg is fixed) or the fair spread
            (if reference leg is floating).

    \pre All vectors have the same size.
    \pre referenceLegIdx < legs.size().
    \pre The reference leg contains at least one future Coupon.
*/
QuantLib::Real fairRate(const std::vector<QuantLib::Leg>& legs,
                        const std::vector<bool>& isPayer,
                        const std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>>& discountCurves,
                        const std::vector<QuantLib::Real>& fxSpotToBase,
                        QuantLib::Size referenceLegIdx);

} // namespace data
} // namespace ore
