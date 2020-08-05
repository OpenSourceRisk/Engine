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
#include <orea/engine/sensitivitystoragemanager.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/trade.hpp>

using ore::data::Trade;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

namespace ore {
namespace analytics {

//! SensitivityCalculator
/*! Calculates the sensitivities. The values are stored in the outputCubeNettingSet using the given storage manager */
class SensitivityCalculator : public ValuationCalculator {
public:
    /*! Constructor */
    explicit SensitivityCalculator(const boost::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager)
        : sensitivityStorageManager_(sensitivityStorageManager) {}

    virtual void calculate(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                           const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube,
                           boost::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                           Size sample, bool isCloseOut = false);

    virtual void calculateT0(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                             const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube,
                             boost::shared_ptr<NPVCube>& outputCubeNettingSet);

private:
    const boost::shared_ptr<SensitivityStorageManager> sensitivityStorageManager_;
};

} // namespace analytics
} // namespace ore
