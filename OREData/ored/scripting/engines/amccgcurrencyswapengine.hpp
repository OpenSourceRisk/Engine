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

/*! \file amccgcurrencyswapengine.hpp
    \brief AMC CG currency swap engine
    \ingroup engines
*/

#pragma once

#include <ored/scripting/engines/amccgbaseengine.hpp>

#include <qle/instruments/currencyswap.hpp>

#include <ql/instruments/swap.hpp>

namespace ore {
namespace data {

class AmcCgCurrencySwapEngine : public QuantExt::CurrencySwap::engine, public AmcCgBaseEngine {
public:
    AmcCgCurrencySwapEngine(const std::vector<std::string>& ccys, const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                            const std::vector<Date>& simulationDates)
        : AmcCgBaseEngine(modelCg, simulationDates), ccys_(ccys) {
        registerWith(modelCg);
    }
    void calculate() const override;

private:
    std::vector<std::string> ccys_;
};

} // namespace data
} // namespace ore
