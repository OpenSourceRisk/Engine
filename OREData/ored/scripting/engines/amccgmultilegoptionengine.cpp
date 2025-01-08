/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/scripting/engines/amccgmultilegoptionengine.hpp>

namespace ore {
namespace data {

void AmcCgMultiLegOptionEngine::buildComputationGraph() const { AmcCgBaseEngine::buildComputationGraph(); }

void AmcCgMultiLegOptionEngine::calculate() const {
    leg_ = arguments_.legs;
    currency_.clear();
    std::transform(arguments_.currency.begin(), arguments_.currency.end(), std::back_inserter(currency_),
                   [](const Currency& c) { return c.code(); });
    payer_ = arguments_.payer;
    exercise_ = arguments_.exercise;
    optionSettlement_ = arguments_.settlementType;
    cashSettlementDates_ = arguments_.settlementDates;

    AmcCgBaseEngine::calculate();
}

} // namespace data
} // namespace ore
