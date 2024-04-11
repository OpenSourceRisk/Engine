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

/*! \file orea/simm/simmconcentration.hpp
    \brief Abstract base class for retrieving SIMM concentration thresholds
*/

#pragma once

#include <orea/simm/simmconfiguration.hpp>
#include <orea/simm/simmcalibration.hpp>
#include <ql/qldefines.hpp>
#include <ql/types.hpp>

#include <string>

namespace ore {
namespace analytics {

class SimmConcentration {
public:
    //! Destructor
    virtual ~SimmConcentration() {}

    /*! Return the SIMM <em>concentration threshold</em> for a given SIMM
        <em>RiskType</em> and SIMM <em>Qualifier</em>.
     */
    virtual QuantLib::Real threshold(const CrifRecord::RiskType& riskType,
                                     const std::string& qualifier) const = 0;
};

class SimmConcentrationBase : public SimmConcentration {
public:
    //SimmConcentrationBase(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
    //                      const QuantLib::ext::shared_ptr<SimmCalibration>& simmCalibration = nullptr);

    /*! Default ctor. May need to generalise if units of threshold quotation
        change significantly
    */
    SimmConcentrationBase() : units_(1000000) {}

    /*! Early versions of SIMM did not have concentration thresholds assigned.
        This base class just returns the maximum real number i.e. effectively
        no concentration threshold
     */
    QuantLib::Real threshold(const CrifRecord::RiskType& riskType, const std::string& qualifier) const override {
        return QL_MAX_REAL;
    }

protected:
    //! The units of quotation of the threshold amount e.g. $1MM
    QuantLib::Real units_;

    /*! Map from SIMM <em>RiskType</em> to another map that holds the
        SIMM concentration threshold <em>bucket</em> to threshold value mappings
    */
    std::map<CrifRecord::RiskType, QuantLib::Real> flatThresholds_;

    /*! Map from SIMM <em>RiskType</em> to another map that holds the
        SIMM concentration threshold <em>bucket</em> to threshold value mappings
    */
    std::map<CrifRecord::RiskType, std::map<std::string, QuantLib::Real>> bucketedThresholds_;

    /*! Map defining the currency groupings for IR concentration thresholds i.e. key is the
        category and value is the set of currencies in that category.
    */
    std::map<std::string, std::set<std::string>> irCategories_;

    /*! Map defining the currency groupings for concentration thresholds i.e. key is the
        category and value is the set of currencies in that category.
    */
    std::map<std::string, std::set<std::string>> fxCategories_;

    //! Maps SIMM qualifiers to SIMM buckets
    QuantLib::ext::shared_ptr<SimmBucketMapper> simmBucketMapper_;

    /*! Shared threshold implementation for derived classes to call
     */
    QuantLib::Real thresholdImpl(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                 const CrifRecord::RiskType& riskType, const std::string& qualifier) const;

    //! Return concentration threshold for <em>Risk_FXVol</em> given the \p fxPair
    QuantLib::Real fxVolThreshold(const std::string& fxPair) const;

    //! Find the concentration threshold category of the \p qualifier
    std::string category(const std::string& qualifier,
                         const std::map<std::string, std::set<std::string>>& categories) const;
};

} // namespace analytics
} // namespace ore
