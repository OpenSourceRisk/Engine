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

/*! \file amccgswapengine.hpp
    \brief AMC CG swap engine
    \ingroup engines
*/

#include <ored/scripting/engines/amccgswapengine.hpp>

#include <ql/exercise.hpp>

namespace ore {
namespace data {

void AmcCgSwapEngine::buildComputationGraph() const {
    calculate();
    AmcCgBaseEngine::buildComputationGraph();
}

void AmcCgSwapEngine::calculate() const {
    leg_ = arguments_.legs;
    currency_ = std::vector<std::string>(leg_.size(), ccy_);
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    exercise_ = nullptr;
    AmcCgBaseEngine::calculate();
}

} // namespace data
} // namespace ore
