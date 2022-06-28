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

#include <orea/cube/cubeinterpretation.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

Real RegularCubeInterpretation::getGenericValue(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                                Size sampleIdx, Size depth) const {
    if (flipViewXVA_) {
        return -cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    } else {
        return cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    }
}

Real RegularCubeInterpretation::getDefaultNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                              Size sampleIdx) const {
    return getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, npvIdx_);
}

Real RegularCubeInterpretation::getCloseOutNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                               Size sampleIdx) const {
    Size closeOutDateIdx = dateIdx + 1;
    return getGenericValue(cube, tradeIdx, closeOutDateIdx, sampleIdx, npvIdx_);
}

Real RegularCubeInterpretation::getMporFlows(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                             Size sampleIdx) const {
    Real aggMporFlowsVal = 0.0;
    try {
        aggMporFlowsVal = getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, mporFlowsIdx_);
    } catch (std::exception& e) {
        DLOG("Unable to retrieve MPOR flows for trade " << tradeIdx << ", date " << dateIdx << ", sample " << sampleIdx
                                                        << "; " << e.what());
    }
    return aggMporFlowsVal;
}

bool RegularCubeInterpretation::hasMporFlows(const boost::shared_ptr<NPVCube>& cube) const {
    return cube->depth() > mporFlowsIdx_;
}

Real RegularCubeInterpretation::getDefaultAggrionScenarioData(
    const boost::shared_ptr<AggregationScenarioData>& aggScenData, const AggregationScenarioDataType& dataType,
    Size dateIdx, Size sampleIdx, const std::string& qualifier) const {
    return aggScenData->get(dateIdx, sampleIdx, dataType, qualifier);
}

Real RegularCubeInterpretation::getCloseOutAggrionScenarioData(
    const boost::shared_ptr<AggregationScenarioData>& aggScenData, const AggregationScenarioDataType& dataType,
    Size dateIdx, Size sampleIdx, const std::string& qualifier) const {
    Size closeOutDateIdx = dateIdx + 1;
    return aggScenData->get(closeOutDateIdx, sampleIdx, dataType, qualifier);
}

Size RegularCubeInterpretation::getMporCalendarDays(const boost::shared_ptr<NPVCube>& cube, Size dateIdx) const {
    return cube->dates()[dateIdx + 1] - cube->dates()[dateIdx];
}

Real MporGridCubeInterpretation::getGenericValue(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                                 Size sampleIdx, Size depth) const {
    // it is assumed here that the user knows how to interpret the depth
    if (flipViewXVA_) {
        return -cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    } else {
        return cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    }
}

Real MporGridCubeInterpretation::getDefaultNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                               Size sampleIdx) const {
    return getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, defaultDateNpvIdx_);
}

Real MporGridCubeInterpretation::getCloseOutNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                                Size sampleIdx) const {
    Size closeOutDateIdx = dateIdx; // close-outs stored using depth
    return getGenericValue(cube, tradeIdx, closeOutDateIdx, sampleIdx, closeOutDateNpvIdx_);
}

Real MporGridCubeInterpretation::getMporFlows(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                              Size sampleIdx) const {
    Real aggMporFlowsVal = 0.0;
    try {
        aggMporFlowsVal = getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, mporFlowsIdx_);
    } catch (std::exception& e) {
        DLOG("Unable to retrieve MPOR flows for trade " << tradeIdx << ", date " << dateIdx << ", sample " << sampleIdx
                                                        << "; " << e.what());
    }
    return aggMporFlowsVal;
}

bool MporGridCubeInterpretation::hasMporFlows(const boost::shared_ptr<NPVCube>& cube) const {
    return cube->depth() > mporFlowsIdx_;
}

Real MporGridCubeInterpretation::getDefaultAggrionScenarioData(
    const boost::shared_ptr<AggregationScenarioData>& aggScenData, const AggregationScenarioDataType& dataType,
    Size dateIdx, Size sampleIdx, const std::string& qualifier) const {
    return aggScenData->get(dateIdx, sampleIdx, dataType, qualifier);
}

Real MporGridCubeInterpretation::getCloseOutAggrionScenarioData(
    const boost::shared_ptr<AggregationScenarioData>& aggScenData, const AggregationScenarioDataType& dataType,
    Size dateIdx, Size sampleIdx, const std::string& qualifier) const {
    QL_REQUIRE(dataType == AggregationScenarioDataType::Numeraire,
               "close out aggr scen data only available for numeraire");
    return 1.0;
}

Size MporGridCubeInterpretation::getMporCalendarDays(const boost::shared_ptr<NPVCube>& cube, Size dateIdx) const {
    QuantLib::Date dd = dateGrid_->valuationDates()[dateIdx];
    QuantLib::Date cd = dateGrid_->closeOutDates()[dateIdx];
    QL_REQUIRE(cd > dd,
               "close-out date (" << cd << ") must be greater than default date (" << dd << ") at index " << dateIdx);
    return (cd - dd);
}

} // namespace analytics
} // namespace ore
