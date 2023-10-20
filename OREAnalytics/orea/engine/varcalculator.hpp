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

/*! \file engine/varcalculator.hpp
    \brief Base class for a var calculation
    \ingroup engine
*/

#pragma once

#include <ored/report/report.hpp>
#include <ored/utilities/timeperiod.hpp>

#include <qle/math/covariancesalvage.hpp>

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

#include <map>
#include <set>

namespace ore {
namespace analytics {
using QuantLib::Array;
using QuantLib::Matrix;

//! VaR Calculator
class VarCalculator {
public:
    VarCalculator() {}
    virtual ~VarCalculator() {}

    virtual QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true, 
        const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) = 0;
};

} // namespace analytics
} // namespace ore
