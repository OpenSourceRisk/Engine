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

CubeInterpretation::CubeInterpretation(const bool storeFlows, const bool withCloseOutLag,
                                       const QuantLib::Handle<AggregationScenarioData>& aggregationScenarioData,
                                       const QuantLib::ext::shared_ptr<DateGrid>& dateGrid, const Size storeCreditStateNPVs,
                                       const bool flipViewXVA)
    : storeFlows_(storeFlows), withCloseOutLag_(withCloseOutLag), aggregationScenarioData_(aggregationScenarioData),
      dateGrid_(dateGrid), storeCreditStateNPVs_(storeCreditStateNPVs), flipViewXVA_(flipViewXVA) {

    // determine required cube depth and layout

    requiredCubeDepth_ = 1;
    defaultDateNpvIndex_ = 0;

    if (withCloseOutLag_) {
        closeOutDateNpvIndex_ = requiredCubeDepth_++;
        QL_REQUIRE(dateGrid_ != nullptr, "CubeInterpretation: dateGrid is required when withCloseOutLag is true");
    }

    if (storeFlows_) {
        mporFlowsIndex_ = requiredCubeDepth_;
        requiredCubeDepth_ += 2;
    }

    if (storeCreditStateNPVs_ > 0) {
        creditStateNPVsIndex_ = requiredCubeDepth_;
        requiredCubeDepth_ += storeCreditStateNPVs_;
    }
}

bool CubeInterpretation::storeFlows() const { return storeFlows_; }

bool CubeInterpretation::withCloseOutLag() const { return withCloseOutLag_; }

Size CubeInterpretation::storeCreditStateNPVs() const { return storeCreditStateNPVs_; }

const QuantLib::Handle<AggregationScenarioData>& CubeInterpretation::aggregationScenarioData() const {
    return aggregationScenarioData_;
}

const QuantLib::ext::shared_ptr<DateGrid>& CubeInterpretation::dateGrid() const { return dateGrid_; }

bool CubeInterpretation::flipViewXVA() const { return flipViewXVA_; }

Size CubeInterpretation::requiredNpvCubeDepth() const { return requiredCubeDepth_; }

Size CubeInterpretation::defaultDateNpvIndex() const { return defaultDateNpvIndex_; }
Size CubeInterpretation::closeOutDateNpvIndex() const { return closeOutDateNpvIndex_; }
Size CubeInterpretation::mporFlowsIndex() const { return mporFlowsIndex_; }
Size CubeInterpretation::creditStateNPVsIndex() const { return creditStateNPVsIndex_; }

Real CubeInterpretation::getGenericValue(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                         Size sampleIdx, Size depth) const {
    if (flipViewXVA_) {
        return -cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    } else {
        return cube->get(tradeIdx, dateIdx, sampleIdx, depth);
    }
}

Real CubeInterpretation::getDefaultNpv(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                       Size sampleIdx) const {
    return getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, defaultDateNpvIndex_);
}

Real CubeInterpretation::getCloseOutNpv(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                        Size sampleIdx) const {
    if (withCloseOutLag_)
        return getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, closeOutDateNpvIndex_) /
               getCloseOutAggregationScenarioData(AggregationScenarioDataType::Numeraire, dateIdx, sampleIdx);
    else
        return getGenericValue(cube, tradeIdx, dateIdx + 1, sampleIdx, defaultDateNpvIndex_);
}

Real CubeInterpretation::getMporPositiveFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                             Size sampleIdx) const {
    if (mporFlowsIndex_ == QuantLib::Null<Size>())
        return 0.0;
    Real aggMporFlowsVal = 0.0;
    try {
        aggMporFlowsVal = getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, mporFlowsIndex_);
    } catch (std::exception& e) {
        DLOG("Unable to retrieve MPOR flows for trade " << tradeIdx << ", date " << dateIdx << ", sample " << sampleIdx
                                                        << "; " << e.what());
    }
    return aggMporFlowsVal;
}

Real CubeInterpretation::getMporNegativeFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                             Size sampleIdx) const {
    if (mporFlowsIndex_ == QuantLib::Null<Size>())
        return 0.0;
    Real aggMporFlowsVal = 0.0;
    try {
        aggMporFlowsVal = getGenericValue(cube, tradeIdx, dateIdx, sampleIdx, mporFlowsIndex_ + 1);
    } catch (std::exception& e) {
        DLOG("Unable to retrieve MPOR flows for trade " << tradeIdx << ", date " << dateIdx << ", sample " << sampleIdx
                                                        << "; " << e.what());
    }
    return aggMporFlowsVal;
}

Real CubeInterpretation::getMporFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                      Size sampleIdx) const {
    return getMporPositiveFlows(cube, tradeIdx, dateIdx, sampleIdx) + getMporNegativeFlows(cube, tradeIdx, dateIdx, sampleIdx) ;
}

Real CubeInterpretation::getDefaultAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx,
                                                           Size sampleIdx, const std::string& qualifier) const {
    QL_REQUIRE(!aggregationScenarioData_.empty(),
               "CubeInterpretation::getDefaultAggregationScenarioData(): no aggregation scenario data given");
    return aggregationScenarioData_->get(dateIdx, sampleIdx, dataType, qualifier);
}

Real CubeInterpretation::getCloseOutAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx,
                                                            Size sampleIdx, const std::string& qualifier) const {
    if (withCloseOutLag_) {
        QL_REQUIRE(dataType == AggregationScenarioDataType::Numeraire,
                   "close out aggr scen data only available for numeraire");
        // this is an approximation
        return getDefaultAggregationScenarioData(dataType, dateIdx, sampleIdx, qualifier);
    } else {
        QL_REQUIRE(!aggregationScenarioData_.empty(), "CubeInterpretation::getCloseOutAggregationScenarioData(): no "
                                                      "aggregation scenario data given");
        return aggregationScenarioData_->get(dateIdx + 1, sampleIdx, dataType, qualifier);
    }
}

Size CubeInterpretation::getMporCalendarDays(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size dateIdx) const {
    if (withCloseOutLag_) {
        QuantLib::Date dd = dateGrid_->valuationDates()[dateIdx];
        QuantLib::Date cd = dateGrid_->closeOutDates()[dateIdx];
        QL_REQUIRE(cd > dd, "close-out date (" << cd << ") must be greater than default date (" << dd << ") at index "
                                               << dateIdx);
        return (cd - dd);
    } else {
        return cube->dates()[dateIdx + 1] - cube->dates()[dateIdx];
    }
}

} // namespace analytics
} // namespace ore
