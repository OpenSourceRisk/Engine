/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orea/engine/sensitivitycalculator.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
using namespace data;
namespace analytics {

void SensitivityCalculator::calculate(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                                      const boost::shared_ptr<SimMarket>& simMarket,
                                      boost::shared_ptr<NPVCube>& outputCube,
                                      boost::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date,
                                      Size dateIndex, Size sample, bool isCloseOut) {
    sensitivityStorageManager_->addSensitivities(outputCubeNettingSet, trade, simMarket, dateIndex, sample);
}

void SensitivityCalculator::calculateT0(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                                        const boost::shared_ptr<SimMarket>& simMarket,
                                        boost::shared_ptr<NPVCube>& outputCube,
                                        boost::shared_ptr<NPVCube>& outputCubeNettingSet) {
    sensitivityStorageManager_->addSensitivities(outputCubeNettingSet, trade, simMarket);
}

} // namespace analytics
} // namespace ore
