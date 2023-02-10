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
    \brief A Class to write a cube out to file
    \ingroup cube
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <map>
#include <orea/cube/npvcube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>
#include <ored/utilities/dategrid.hpp>
#include <string>

namespace ore {
using namespace data;
namespace analytics {

//! Allow for interpretation of how data is stored within cube and AggregationScenarioData
/*! \ingroup cube
 */
class CubeInterpretation {
public:
    virtual ~CubeInterpretation() {}

    //! Retrieve an arbitrary value from the Cube (user needs to know the precise location within depth axis)
    virtual Real getGenericValue(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx,
                                 Size depth) const = 0;

    //! Retrieve the default date NPV from the Cube
    virtual Real getDefaultNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                               Size sampleIdx) const = 0;

    //! Retrieve the close-out date NPV from the Cube
    virtual Real getCloseOutNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                                Size sampleIdx) const = 0;

    //! Retrieve the aggregate value of Margin Period of Risk cashflows from the Cube
    virtual Real getMporFlows(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx,
                              Size sampleIdx) const = 0;

    //! check whether mpor flows are available in the cube
    virtual bool hasMporFlows(const boost::shared_ptr<NPVCube>& cube) const = 0;

    //! Retrieve a (default date) simulated risk factor value from AggregationScenarioData
    virtual Real getDefaultAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx,
                                               Size sampleIdx, const std::string& qualifier = "") const = 0;

    //! Retrieve a (default date) simulated risk factor value from AggregationScenarioData
    virtual Real getCloseOutAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx,
                                                Size sampleIdx, const std::string& qualifier = "") const = 0;

    //! Number of Calendar Days between a given default date and corresponding close-out date
    virtual Size getMporCalendarDays(const boost::shared_ptr<NPVCube>& cube, Size dateIdx) const = 0;
};

//! A concrete implementation for regular (non-MPOR grid) storage style
/*! \ingroup cube
 */
class RegularCubeInterpretation : public CubeInterpretation {
public:
    //! ctor (default)
    explicit RegularCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData)
        : aggScenData_(aggScenData), npvIdx_(0), mporFlowsIdx_(1), flipViewXVA_(false) {}

    //! ctor
    RegularCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData, Size npvIdx,
                              Size mporFlowsIdx = 1)
        : aggScenData_(aggScenData), npvIdx_(npvIdx), mporFlowsIdx_(mporFlowsIdx), flipViewXVA_(false) {}

    //! ctor
    RegularCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData, bool flipViewXVA)
        : aggScenData_(aggScenData), npvIdx_(0), mporFlowsIdx_(1), flipViewXVA_(flipViewXVA) {}

    Real getGenericValue(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx,
                         Size depth) const override;

    Real getDefaultNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    Real getCloseOutNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    Real getMporFlows(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    bool hasMporFlows(const boost::shared_ptr<NPVCube>& cube) const override;

    Real getDefaultAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                       const std::string& qualifier = "") const override;

    Real getCloseOutAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                        const std::string& qualifier = "") const override;

    Size getMporCalendarDays(const boost::shared_ptr<NPVCube>& cube, Size dateIdx) const override;

private:
    boost::shared_ptr<AggregationScenarioData> aggScenData_;
    Size npvIdx_, mporFlowsIdx_;
    bool flipViewXVA_;
};

//! A concrete implementation for MPOR-style simulation grid and storage style
/*! \ingroup cube
 */
class MporGridCubeInterpretation : public CubeInterpretation {
public:
    //! ctor (default)
    MporGridCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData,
                               const boost::shared_ptr<DateGrid>& dateGrid)
        : aggScenData_(aggScenData), defaultDateNpvIdx_(0), closeOutDateNpvIdx_(1), mporFlowsIdx_(2),
          dateGrid_(dateGrid), flipViewXVA_(false) {}

    //! ctor
    MporGridCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData,
                               const boost::shared_ptr<DateGrid>& dateGrid, Size defaultDateNpvIdx,
                               Size closeOutDateNpvIdx, Size mporFlowsIdx = 2)
        : aggScenData_(aggScenData), defaultDateNpvIdx_(defaultDateNpvIdx), closeOutDateNpvIdx_(closeOutDateNpvIdx),
          mporFlowsIdx_(mporFlowsIdx), dateGrid_(dateGrid), flipViewXVA_(false) {}

    //! ctor
    MporGridCubeInterpretation(const boost::shared_ptr<AggregationScenarioData>& aggScenData,
                               const boost::shared_ptr<DateGrid>& dateGrid, bool flipViewXVA)
        : aggScenData_(aggScenData), defaultDateNpvIdx_(0), closeOutDateNpvIdx_(1), mporFlowsIdx_(2),
          dateGrid_(dateGrid), flipViewXVA_(flipViewXVA) {}

    Real getGenericValue(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx,
                         Size depth) const override;

    Real getDefaultNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    Real getCloseOutNpv(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    Real getMporFlows(const boost::shared_ptr<NPVCube>& cube, Size tradeIdx, Size dateIdx, Size sampleIdx) const override;

    bool hasMporFlows(const boost::shared_ptr<NPVCube>& cube) const override;

    Real getDefaultAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                       const std::string& qualifier = "") const override;

    Real getCloseOutAggregationScenarioData(const AggregationScenarioDataType& dataType, Size dateIdx, Size sampleIdx,
                                        const std::string& qualifier = "") const override;

    Size getMporCalendarDays(const boost::shared_ptr<NPVCube>& cube, Size dateIdx) const override;

private:
    boost::shared_ptr<AggregationScenarioData> aggScenData_;
    Size defaultDateNpvIdx_, closeOutDateNpvIdx_, mporFlowsIdx_;
    boost::shared_ptr<DateGrid> dateGrid_;
    bool flipViewXVA_;
};
} // namespace analytics
} // namespace ore
