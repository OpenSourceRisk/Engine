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

/*! \file orea/engine/sensitivitycubestream.hpp
    \brief Class for streaming CvaSensitivityRecords
 */

#pragma once

#include <orea/scenario/cvascenario.hpp>
#include <orea/engine/cvasensitivityrecord.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/cube/npvsensicube.hpp>

#include <map>
#include <set>
#include <string>

namespace ore {
namespace analytics {

/*! Class for streaming SensitivityRecords from a SensitivityCube
    */
class CvaSensitivityCubeStream {
public:
    /*! Constructor providing the sensitivity \p cube and currency of the
        sensitivities
    */
    CvaSensitivityCubeStream(const QuantLib::ext::shared_ptr<ore::analytics::NPVSensiCube>& cube,
                             const std::vector<CvaRiskFactorKey>& descriptions,
                             const std::set<std::string>& nettingSetIds,
                             const std::vector<std::pair<ore::analytics::ShiftType, QuantLib::Real>>& shifts,
                             const std::vector<QuantLib::Period>& cdsGrid,
                             const std::map<std::string, std::vector<QuantLib::Real>>& cdsSensis,
                             std::pair<ore::analytics::ShiftType, QuantLib::Real>& cdsShift,
                             const std::map<std::string, std::string>& counterpartyMap, const std::string& currency);

    /*! Returns the next SensitivityRecord in the stream

        \warning the cube must not change during successive calls to next()!
    */
    CvaSensitivityRecord next();

    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset();

private:
    //! Handle on the SensitivityCube
    QuantLib::ext::shared_ptr<ore::analytics::NPVSensiCube> cube_;
    std::vector<CvaRiskFactorKey> descriptions_;
    std::set<std::string> nettingSetIds_;
    std::vector<std::pair<ore::analytics::ShiftType, QuantLib::Real>> shifts_;
    std::vector<QuantLib::Period> cdsGrid_;
    std::map<std::string, std::vector<QuantLib::Real>> cdsSensis_;
    std::pair<ore::analytics::ShiftType, QuantLib::Real> cdsShift_;
    std::map<std::string, std::string> counterpartyMap_;
    //! Currency of the sensitivities in the SensitivityCube
    std::string currency_;

    
    QuantLib::Size scenarioIdx_;
    QuantLib::Size nettingIdx_;
    QuantLib::Size cdsGridIdx_;
    std::set<std::string>::const_iterator nettingSetIdIter_;
};

} // namespace analytics
} // namespace ore
