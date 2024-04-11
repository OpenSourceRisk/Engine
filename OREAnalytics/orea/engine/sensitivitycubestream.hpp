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
    /*! Constructor providing the sensitivity \p cube and currency of the sensitivities */
    SensitivityCubeStream(const QuantLib::ext::shared_ptr<SensitivityCube>& cube, const std::string& currency);
    /*! Constructor providing the sensitivity \p cubes and currency of the sensitivities */
    SensitivityCubeStream(const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& cubes, const std::string& currency);

    /*! Returns the next SensitivityRecord in the stream

        \warning the cube must not change during successive calls to next()!
    */
    SensitivityRecord next() override;

    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    void updateForNewTrade();

    //! Handle on the SensitivityCubes
    std::vector<QuantLib::ext::shared_ptr<SensitivityCube>> cubes_;
    //! Currency of the sensitivities in the SensitivityCubes
    std::string currency_;

    //! Current cube index in vector
    Size currentCubeIdx_;

    //! Current delta risk factor keys to process and iterators
    std::set<RiskFactorKey> currentDeltaKeys_;
    std::set<std::pair<RiskFactorKey,RiskFactorKey>> currentCrossGammaKeys_;

    std::set<RiskFactorKey>::const_iterator currentDeltaKey_;
    std::set<std::pair<RiskFactorKey,RiskFactorKey>>::const_iterator currentCrossGammaKey_;

    //! Current trade iterator
    std::map<std::string, QuantLib::Size>::const_iterator tradeIdx_;

    //! Can only compute gamma if the up and down risk factors align
    bool canComputeGamma_;
};

} // namespace analytics
} // namespace ore
