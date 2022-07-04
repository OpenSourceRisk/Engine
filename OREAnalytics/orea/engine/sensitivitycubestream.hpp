/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/engine/sensitivitycubestream.hpp
    \brief Class for streaming SensitivityRecords from a SensitivityCube
 */

#pragma once

#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/sensitivitystream.hpp>

#include <map>
#include <set>
#include <string>

namespace ore {
namespace analytics {

/*! Class for streaming SensitivityRecords from a SensitivityCube
 */
class SensitivityCubeStream : public SensitivityStream {
public:
    /*! Constructor providing the sensitivity \p cube and currency of the
        sensitivities
    */
    SensitivityCubeStream(const boost::shared_ptr<SensitivityCube>& cube, const std::string& currency);

    /*! Returns the next SensitivityRecord in the stream

        \warning the cube must not change during successive calls to next()!
    */
    SensitivityRecord next() override;

    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    //! Handle on the SensitivityCube
    boost::shared_ptr<SensitivityCube> cube_;
    //! Currency of the sensitivities in the SensitivityCube
    std::string currency_;

    //! Iterator to risk factor keys in the cube
    boost::bimap<RiskFactorKey, ore::analytics::SensitivityCube::FactorData>::const_iterator upRiskFactor_;
    std::map<RiskFactorKey, ore::analytics::SensitivityCube::FactorData>::const_iterator downRiskFactor_;
    //! Iterator to cross factors in the cube
    std::map<ore::analytics::SensitivityCube::crossPair,
             std::tuple<ore::analytics::SensitivityCube::FactorData, ore::analytics::SensitivityCube::FactorData,
                        QuantLib::Size>>::const_iterator itCrossPair_;
    //! Index of current trade Id in the cube
    std::map<std::string, QuantLib::Size>::const_iterator tradeIdx_;
    //! Can only compute gamma if the up and down risk factors align
    bool canComputeGamma_;
};

} // namespace analytics
} // namespace ore
