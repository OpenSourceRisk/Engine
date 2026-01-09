/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#pragma once

#include <qle/models/crossassetmodel.hpp>
#include <qle/math/randomvariable.hpp>
#include <ql/types.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <vector>
#include <functional>

namespace QuantExt {

//! Holds information required to price a cashflow in an MC simulation
//! It contains the information about the cashflow like its payment time, 
//! the required simulation times, the currency index, whether it is a payer or receiver
//! its currency and leg and cashflow number.
//! The amountCalculator function calculates the cashflow amount under a simulated market state
//! Monte Carlo pricing engines can use this pricing function to price the cashflows under scenarios
//! without needing to know the details of the cashflow itself. 
//! Its used to convert Cashflow objects into a form that is suitable for MC pricing engines.
struct McCashflowInfo {
    Size legNo = Null<Size>(), cfNo = Null<Size>();
    Real payTime = Null<Real>();
    Real exIntoCriterionTime = Null<Real>();
    Size payCcyIndex = Null<Size>();
    bool payer = false;
    std::vector<Real> simulationTimes;
    std::vector<std::vector<Size>> modelIndices;
    std::function<RandomVariable(const Size n, const std::vector<std::vector<const RandomVariable*>>&)>
        amountCalculator;

    McCashflowInfo(QuantLib::ext::shared_ptr<CashFlow> flow, const Currency& payCcy, const bool payer, const Size legNo,
                 const Size cfNo, const Handle<CrossAssetModel>& model, const std::vector<LgmVectorised>& lgmVectorized,
                 const bool exerciseIntoIncludeSameDayFlows = true, const double tinyTime = 1e-10,
                 const Size cfOnCpnMaxSimTimes = 1, const Period cfOnCpnAddSimTimesCutoff = Period());
};

} // namespace QuantExt