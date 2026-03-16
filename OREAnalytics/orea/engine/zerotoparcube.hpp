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

/*! \file orea/engine/zerotoparcube.hpp
    \brief Class for converting zero sensitivities to par sensitivities
*/

#pragma once

#include <map>
#include <string>

#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>

namespace ore {
namespace analytics {

//! ZeroToParCube class
/*! Takes a cube of zero sensitivities, a par sensitivity converter and can return the
    par deltas for a given trade ID from the cube.
 */
class ZeroToParCube {
public:
    //! Constructor
    ZeroToParCube(const QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>& zeroCube,
                  const QuantLib::ext::shared_ptr<ParSensitivityConverter>& parConverter,
                  const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {},
                  const bool continueOnError = false);
    //! Another Constructor!
    ZeroToParCube(const std::vector<QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>>& zeroCubes,
                  const QuantLib::ext::shared_ptr<ParSensitivityConverter>& parConverter,
                  const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {},
                  const bool continueOnError = false);

    //! Inspectors
    //@{
    //! Underlying zero sensitivity cubes
    const std::vector<QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>>& zeroCubes() const { return zeroCubes_; }
    //! Par converter object
    const QuantLib::ext::shared_ptr<ParSensitivityConverter>& parConverter() const { return parConverter_; }
    //! The par risk factor types that are disabled for this instance of ZeroToParCube.
    const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled() const { return typesDisabled_; }
    //@}

    //! Return the non-zero par deltas for the given \p tradeId
    std::map<ore::analytics::RiskFactorKey, QuantLib::Real> parDeltas(const std::string& tradeId) const;

    //! Return the non-zero par deltas for the given cube and trade index
    std::map<ore::analytics::RiskFactorKey, QuantLib::Real> parDeltas(QuantLib::Size cubeIdx,
                                                                      QuantLib::Size tradeIdx) const;

private:
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>> zeroCubes_;
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter_;
    std::map<ore::analytics::RiskFactorKey, Size> factorToIndex_;

    //! Set of risk factor types available for par conversion but that are disabled for this instance of ZeroToParCube.
    std::set<ore::analytics::RiskFactorKey::KeyType> typesDisabled_;
    const bool continueOnError_;
};

} // namespace analytics
} // namespace ore
