/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/engine/sensitivitycalculator.hpp
    \brief sensitivity calculator
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/trade.hpp>
#include <orea/engine/sensitivitystoragemanager.hpp>

using ore::data::Trade;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

namespace ore {
namespace analytics {

//! SensitivityCalculator
/*! Calculates the sensitivities. The values are stored in the outputCubeNettingSet using the given storage manager */
class SensitivityCalculator : public ore::analytics::ValuationCalculator {
public:
    /*! Constructor */
    explicit SensitivityCalculator(const QuantLib::ext::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager)
        : sensitivityStorageManager_(sensitivityStorageManager) {}

    void calculate(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, QuantLib::Size tradeIndex,
                   const QuantLib::ext::shared_ptr<ore::analytics::SimMarket>& simMarket,
                   QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& outputCube,
                   QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                   QuantLib::Size sample, bool isCloseOut = false) override;

    void calculateT0(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, QuantLib::Size tradeIndex,
                     const QuantLib::ext::shared_ptr<ore::analytics::SimMarket>& simMarket,
                     QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& outputCube,
                     QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& outputCubeNettingSet) override;

    void init(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
              const QuantLib::ext::shared_ptr<ore::analytics::SimMarket>& simMarket) override {
        DLOG("init SensitivityCalculator");
    }

    void initScenario() override {}

private:
    const QuantLib::ext::shared_ptr<SensitivityStorageManager> sensitivityStorageManager_;
};

} // namespace analytics
} // namespace ore
