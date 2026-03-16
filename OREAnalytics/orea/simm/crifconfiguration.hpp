/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

/*! \file orea/simm/crifconfiguration.hpp
    \brief CRIF configuration interface
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/time/period.hpp>
#include <string>

namespace ore {
namespace analytics {

class CrifConfiguration {
public:    
    virtual ~CrifConfiguration() {};

    //virtual bool isValidSensitivity(const ore::analytics::RiskFactorKey::KeyType& rfkey) const = 0;

    //! Returns the SIMM configuration name
    virtual const std::string& name() const = 0;    

    //! Returns the SIMM configuration version
    virtual const std::string& version() const = 0;

    /*! Return the CRIF <em>bucket</em> name for the given risk type \p rt
        and \p qualifier

        \warning Throws an error if there are no buckets for the risk type \p rt
    */
    virtual std::string bucket(const ore::analytics::CrifRecord::RiskType& rt, const std::string& qualifier) const = 0;

    virtual bool hasBucketMapping(const ore::analytics::CrifRecord::RiskType& rt, const std::string& qualifier) const = 0;

    //! Returns the SIMM bucket mapper used by the configuration
    virtual const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper() const = 0;

    /*! Return the CRIF <em>Label2</em> value for the given interest rate index
        \p irIndex. For interest rate indices, this is the CRIF sub curve name
        e.g. 'Libor1m', 'Libor3m' etc.
    */
    virtual std::string label2(const QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>& irIndex) const;

    /*! Return the CRIF <em>Label2</em> value for the given Libor tenor
        \p p. This is the CRIF sub curve name, e.g. 'Libor1m', 'Libor3m' etc.
    */
    virtual std::string label2(const QuantLib::Period& p) const;
};
} // namespace analytics
} // namespace ore