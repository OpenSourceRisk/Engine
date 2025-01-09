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

#include <orea/engine/sensitivitycalculator.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
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
} // namespace ore
