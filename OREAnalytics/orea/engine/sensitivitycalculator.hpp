/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file engine/sensitivitycalculator.hpp
    \brief sensitivity calculator
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/trade.hpp>
#include <orepbase/orea/engine/sensitivitystoragemanager.hpp>

using ore::data::Trade;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

namespace oreplus {
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
} // namespace oreplus
