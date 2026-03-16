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

/*! \file qle/utilities/fairrate.hpp
    \brief Fair rate utility for multi-leg swaps
    \ingroup utilities
*/

#pragma once

#include <utility>
#include <vector>

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

/*! Compute the fair rate of a collection of swap legs.

    The reference leg is selected internally according to the following rules:
    1. If a fixed leg is present, use the first fixed leg.
    2. If multiple fixed legs are present, only the first is used as reference.
    3. If no fixed leg is present, use the first floating leg with payer = true
    4. If none has payer = true, use the first floating leg.

    If the reference leg is fixed, the first return component is the par fixed rate.
    If the reference leg is floating, the first return component is the fair spread.

    \param legs             Vector of cashflow legs.
    \param isPayer          Pay (true) / receive (false) indicator per leg.
    \param discountCurves   Discount curve per leg for NPV/BPS calculation,
                            or a single curve used for all legs.
    \param fxSpotToBase     FX spot per leg converting leg-ccy NPV to base ccy.
                            If empty, all FX spots are assumed to be 1.0.

    \return pair(fairRate, spreadCorrection)

    \pre All vectors have the same size.
    \pre discountCurves.size() == legs.size() or discountCurves.size() == 1.
    \pre fxSpotToBase.size() == legs.size() or fxSpotToBase.empty().
*/
std::pair<QuantLib::Real, QuantLib::Real>
fairRate(const std::vector<QuantLib::Leg>& legs,
         const std::vector<bool>& isPayer,
         const std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>>& discountCurves,
         const std::vector<QuantLib::Real>& fxSpotToBase = {});

/*! Compute fair forward and spread correction from pre-aggregated fixed/floating components.


    fairForward = -(floatingNpv - floatingSpreadNpv) / fixedBps
    spreadCorrection = floatingSpreadNpv / |fixedBps|

    If fixedBps is zero (or very close), both return values are 0.

    \return pair(fairForward, spreadCorrection)
*/
std::pair<QuantLib::Real, QuantLib::Real>
fairRateFromNpvBps(QuantLib::Real fixedBps,
                   QuantLib::Real floatingNpv,
                   QuantLib::Real floatingSpreadNpv);

} // namespace QuantExt
