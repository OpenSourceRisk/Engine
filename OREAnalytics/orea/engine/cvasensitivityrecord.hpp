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

/*! \file orea/engine/sensitivityrecord.hpp
    \brief Struct for holding a sensitivity record
*/

#pragma once

#include <orea/scenario/cvascenario.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <string>

namespace ore {
namespace analytics {

/*! A container for holding cva sensitivity records.
    -# the currency member is the currency of the baseCva and delta
*/
struct CvaSensitivityRecord {
    // Public members
    std::string nettingSetId;
    CvaRiskFactorKey key;
    ore::analytics::ShiftType shiftType;
    QuantLib::Real shiftSize;
    std::string currency;
    mutable QuantLib::Real baseCva;
    mutable QuantLib::Real delta;

    /*! Default ctor to prevent uninitialised variables
        Could use in class initialisation and avoid ctor but may be confusing
    */
    CvaSensitivityRecord() : shiftSize(0.0), baseCva(0.0), delta(0.0) {}

    //! Full ctor to allow braced initialisation
    CvaSensitivityRecord(const std::string& nettingSetId, const CvaRiskFactorKey& key,
                         ore::analytics::ShiftType shiftType, QuantLib::Real shiftSize, const std::string& currency,
                         QuantLib::Real baseCva, QuantLib::Real delta)
        : nettingSetId(nettingSetId), key(key), shiftType(shiftType), shiftSize(shiftSize), currency(currency),
          baseCva(baseCva), delta(delta) {}

    /*! Comparison operators for SensitivityRecord
        */
    bool operator==(const CvaSensitivityRecord& sr) const;
    bool operator!=(const CvaSensitivityRecord& sr) const;
    bool operator<(const CvaSensitivityRecord& sr) const;

    /*! This method will be used to denote the end of a stream of SensitivityRecord objects.
        */
    explicit operator bool() const;
};

//! Enable writing of a SensitivityRecord
std::ostream& operator<<(std::ostream& out, const CvaSensitivityRecord& sr);

} // namespace analytics
} // namespace ore
