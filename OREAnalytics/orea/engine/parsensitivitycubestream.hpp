/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/engine/parsensitivitycubestream.hpp
    \brief Class for streaming SensitivityRecords from a par sensitivity cube
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/zerotoparcube.hpp>

#include <string>

namespace ore {
namespace analytics {

/*! Class for streaming SensitivityRecords from a par sensitivity cube
 */
class ParSensitivityCubeStream : public ore::analytics::SensitivityStream {
public:
    /*! Constructor providing the sensitivity \p cube and currency of the sensitivities */
    ParSensitivityCubeStream(const QuantLib::ext::shared_ptr<ZeroToParCube>& cube, const std::string& currency);

    /*! Returns the next SensitivityRecord in the stream

        \warning the cube must not change during successive calls to next()!
    */
    ore::analytics::SensitivityRecord next() override;

    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    //! zero cube idx
    Size zeroCubeIdx_;
    //! Handle on the SensitivityCube
    QuantLib::ext::shared_ptr<ZeroToParCube> cube_;
    //! Currency of the sensitivities in the SensitivityCube
    std::string currency_;
    //! TradeId and index of current trade ID in the underlying cube
    std::map<std::string, QuantLib::Size>::const_iterator tradeIdx_;
    //! Par deltas for current trade ID
    std::map<ore::analytics::RiskFactorKey, QuantLib::Real> currentDeltas_;
    //! Iterator to current delta
    std::map<ore::analytics::RiskFactorKey, QuantLib::Real>::iterator itCurrent_;

    //! Shared initialisation
    void init();
};

} // namespace analytics
} // namespace ore
