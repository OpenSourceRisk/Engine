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

/*! \file orea/engine/sensitivityrecord.hpp
    \brief Struct for holding a sensitivity record
*/

#pragma once

#include <orea/scenario/scenario.hpp>

#include <string>

namespace ore {
namespace analytics {

/*! A container for holding sensitivity records.
    -# isPar is <code>true</code> for a par sensitivity, <code>false</code> for a zero sensitivity
    -# the currency member is the currency of the baseNpv, delta and gamma
    -# key_2 and desc_2 are empty except for cross gamma sensitivities
    -# for cross gamma, the sensitivity is in the gamma member
*/
struct SensitivityRecord {
    // Public members
    std::string tradeId;
    bool isPar;
    RiskFactorKey key_1;
    std::string desc_1;
    QuantLib::Real shift_1;
    RiskFactorKey key_2;
    std::string desc_2;
    QuantLib::Real shift_2;
    std::string currency;
    mutable QuantLib::Real baseNpv;
    mutable QuantLib::Real delta;
    mutable QuantLib::Real gamma;

    /*! Default ctor to prevent uninitialised variables
        Could use in class initialisation and avoid ctor but may be confusing
    */
    SensitivityRecord() : isPar(false), shift_1(0.0), shift_2(0.0), baseNpv(0.0), delta(0.0), gamma(0.0) {}

    //! Full ctor to allow braced initialisation
    SensitivityRecord(const std::string& tradeId, bool isPar, const RiskFactorKey& key_1, const std::string& desc_1,
                      QuantLib::Real shift_1, const RiskFactorKey& key_2, const std::string& desc_2,
                      QuantLib::Real shift_2, const std::string& currency, QuantLib::Real baseNpv, QuantLib::Real delta,
                      QuantLib::Real gamma)
        : tradeId(tradeId), isPar(isPar), key_1(key_1), desc_1(desc_1), shift_1(shift_1), key_2(key_2), desc_2(desc_2),
          shift_2(shift_2), currency(currency), baseNpv(baseNpv), delta(delta), gamma(gamma) {}

    /*! Comparison operators for SensitivityRecord
     */
    bool operator==(const SensitivityRecord& sr) const;
    bool operator!=(const SensitivityRecord& sr) const;
    bool operator<(const SensitivityRecord& sr) const;

    /*! This method will be used to denote the end of a stream of SensitivityRecord objects.
     */
    explicit operator bool() const;

    //! True if a SensitivityRecord is a cross gamma, otherwise false
    bool isCrossGamma() const;
};

//! Enable writing of a SensitivityRecord
std::ostream& operator<<(std::ostream& out, const SensitivityRecord& sr);

} // namespace analytics
} // namespace ore
