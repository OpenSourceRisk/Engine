/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file mclgmswapengine.hpp
    \brief MC LGM swap engines
    \ingroup engines
*/

#pragma once

#include <ored/scripting/engines/amccgpricingengine.hpp>

#include <ql/instruments/swap.hpp>

namespace ore {
namespace data {

class AmcCgBaseEngine : public AmcCgPricingEngine {
public:
    AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg) {}
    std::string npvName() const override;
    void buildComputationGraph() const override;
    void calculate() const;

protected:
    QuantLib::ext::shared_ptr<ModelCG> modelCg_;

    // input data from the derived pricing engines, to be set in these engines
    mutable std::vector<Leg> leg_;
    mutable std::vector<std::string> currency_;
    mutable std::vector<bool> payer_;
    mutable QuantLib::ext::shared_ptr<Exercise> exercise_; // may be empty, if underlying is the actual trade
    mutable Settlement::Type optionSettlement_ = Settlement::Physical;
    mutable bool includeSettlementDateFlows_ = false;
};

} // namespace data
} // namespace ore
