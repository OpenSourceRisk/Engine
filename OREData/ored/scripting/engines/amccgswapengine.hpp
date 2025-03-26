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
    \brief MC LGM swap engines
    \ingroup engines
*/

#pragma once

#include <ored/scripting/engines/amccgbaseengine.hpp>

#include <ql/instruments/swap.hpp>

namespace ore {
namespace data {

class AmcCgSwapEngine : public QuantLib::GenericEngine<QuantLib::Swap::arguments, QuantLib::Swap::results>,
                        public AmcCgBaseEngine {
public:
    AmcCgSwapEngine(const std::string& ccy, const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                    const std::vector<Date>& simulationDates)
        : QuantLib::GenericEngine<QuantLib::Swap::arguments, QuantLib::Swap::results>(),
          AmcCgBaseEngine(modelCg, simulationDates), ccy_(ccy) {
        registerWith(modelCg);
    }

    void buildComputationGraph(const bool stickyCloseOutDateRun,
                               const bool reevaluateExerciseInStickyCloseOutDateRun) const override;
    void calculate() const override;

private:
    std::string ccy_;
};

} // namespace data
} // namespace ore
