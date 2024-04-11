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

/*! \file orea/cube/cubeinterpretation.hpp
    \brief class describing the layout of an npv cube and aggregation scenario data
    \ingroup cube
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>
#include <ored/utilities/dategrid.hpp>

#include <ql/handle.hpp>

#include <ql/shared_ptr.hpp>

#include <map>
#include <string>

namespace ore {
using namespace data;
namespace analytics {

//! Allow for interpretation of how data is stored within cube and AggregationScenarioData
/*! \ingroup cube
 */
class CubeInterpretation {
public:
    CubeInterpretation(const bool storeFlows, const bool withCloseOutLag,
                       const QuantLib::Handle<AggregationScenarioData>& aggregationScenarioData =
                           QuantLib::Handle<AggregationScenarioData>(),
                       const QuantLib::ext::shared_ptr<DateGrid>& dateGrid = nullptr, const Size storeCreditStateNPVs = 0,
                       const bool flipViewXVA = false);

    //! inspectors
    bool storeFlows() const;
    bool withCloseOutLag() const;
    const QuantLib::Handle<AggregationScenarioData>& aggregationScenarioData() const; // might be empty handle
    const QuantLib::ext::shared_ptr<DateGrid>& dateGrid() const;                              // might be nullptr
    Size storeCreditStateNPVs() const;
    bool flipViewXVA() const;

    //! npv cube depth that is at least required to work with this interpretation
    Size requiredNpvCubeDepth() const;

    //! indices in depth direction, might be Null<Size>() if not applicable
    Size defaultDateNpvIndex() const;
    Size closeOutDateNpvIndex() const;
    Size mporFlowsIndex() const;
    Size creditStateNPVsIndex() const;

    //! Retrieve an arbitrary value from the Cube (user needs to know the precise location within depth axis)
    Real getGenericValue(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx,
                         Size depth) const;

    //! Retrieve the default date NPV from the Cube
    Real getDefaultNpv(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const;

    //! Retrieve the close-out date NPV from the Cube
    Real getCloseOutNpv(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const;

    //! Retrieve the aggregate value of Margin Period of Risk positive cashflows from the Cube
    Real getMporPositiveFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                              Size sampleIdx) const;

    //! Retrieve the aggregate value of Margin Period of Risk negative cashflows from the Cube
    Real getMporNegativeFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, 
                              Size sampleIdx) const;
    //! Retrieve the aggregate value of Margin Period of Risk cashflows from the Cube
    Real getMporFlows(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const;

    //! Retrieve a (default date) simulated risk factor value from AggregationScenarioData
    Real getDefaultAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                           const std::string& qualifier = "") const;

    //! Retrieve a (default date) simulated risk factor value from AggregationScenarioData
    Real getCloseOutAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                            const std::string& qualifier = "") const;

    //! Number of Calendar Days between a given default date and corresponding close-out date
    Size getMporCalendarDays(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size dateIdx) const;

private:
    bool storeFlows_;
    bool withCloseOutLag_;
    QuantLib::Handle<AggregationScenarioData> aggregationScenarioData_;
    QuantLib::ext::shared_ptr<DateGrid> dateGrid_;
    Size storeCreditStateNPVs_;
    bool flipViewXVA_;

    Size requiredCubeDepth_;
    Size defaultDateNpvIndex_ = QuantLib::Null<Size>();
    Size closeOutDateNpvIndex_ = QuantLib::Null<Size>();
    Size mporFlowsIndex_ = QuantLib::Null<Size>();
    Size creditStateNPVsIndex_ = QuantLib::Null<Size>();
};

} // namespace analytics
} // namespace ore
