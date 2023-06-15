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

/*! \file orea/simm/simmbucketmapper.hpp
    \brief Abstract base class for classes that map SIMM qualifiers to buckets
*/

#pragma once

#include <orea/simm/simmconfiguration.hpp>
#include <ql/time/date.hpp>

#include <string>

namespace ore {
namespace analytics {

class SimmBucketMapper {
public:
    struct FailedMapping {
        std::string name, lookupName;
        SimmConfiguration::RiskType riskType, lookupRiskType;
    };

    //! Destructor
    virtual ~SimmBucketMapper() {}

    /*! Return the SIMM <em>bucket</em> for a given SIMM <em>RiskType</em> and
        SIMM <em>Qualifier</em> (using valid mappings only)

        \warning An error is thrown if there is no <em>bucket</em>
                 for the combination.
     */
    virtual std::string bucket(const SimmConfiguration::RiskType& riskType, const std::string& qualifier) const = 0;

    //! Check if the given SIMM <em>RiskType</em> has a bucket structure.
    virtual bool hasBuckets(const SimmConfiguration::RiskType& riskType) const = 0;

    //! Check if the given \p riskType and \p qualifier has a mapping (which is valid, and matches the fallback flag if given)
    virtual bool has(const SimmConfiguration::RiskType& riskType, const std::string& qualifier, boost::optional<bool> fallback = boost::none) const = 0;

    //! Add a single \p bucket mapping for \p qualifier with risk type \p riskType
    /*! \todo Not very convenient. If deemed useful, add more methods for adding
              mappings e.g. <code>addMappingRange</code> that adds a range of
              mappings at once
    */
    virtual void addMapping(const SimmConfiguration::RiskType& riskType, const std::string& qualifier,
                            const std::string& bucket, const std::string& validFrom = "", const std::string& validTo = "", bool fallback = false) = 0;

    virtual const std::set<FailedMapping>& failedMappings() const = 0;
};

inline bool operator<(const SimmBucketMapper::FailedMapping& a, const SimmBucketMapper::FailedMapping& b) {
    return std::make_tuple(a.name, a.lookupName, a.riskType, a.lookupRiskType) <
           std::make_tuple(b.name, b.lookupName, b.riskType, b.lookupRiskType);
}

} // namespace analytics
} // namespace ore
