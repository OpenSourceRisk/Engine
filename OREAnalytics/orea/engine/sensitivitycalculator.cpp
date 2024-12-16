/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepbase/orea/engine/sensitivitycalculator.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace ore::analytics;

namespace oreplus {
namespace analytics {

void SensitivityCalculator::calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                      const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                      QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                      QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date,
                                      Size dateIndex, Size sample, bool isCloseOut) {
    if (isCloseOut)
        return;
    sensitivityStorageManager_->addSensitivities(outputCubeNettingSet, trade, simMarket, dateIndex, sample);
}

void SensitivityCalculator::calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                        QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                        QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) {
    sensitivityStorageManager_->addSensitivities(outputCubeNettingSet, trade, simMarket);
}

} // namespace analytics
} // namespace oreplus
