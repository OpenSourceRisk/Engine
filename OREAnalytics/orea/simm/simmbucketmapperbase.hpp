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

/*! \file orea/simm/simmbucketmapperbase.hpp
    \brief Base SIMM class for mapping qualifiers to buckets
*/

#pragma once

#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/utilities.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/portfolio/referencedata.hpp>

#include <map>
#include <set>
#include <string>

namespace ore {
namespace analytics {

class BucketMapping {
public:
    BucketMapping(const std::string& bucket, const std::string& validFrom = "", const std::string& validTo = "", bool fallback = false)
        : bucket_(bucket), validFrom_(validFrom), validTo_(validTo), fallback_(fallback) {}
    const std::string& bucket() const { return bucket_; }
    const std::string& validFrom() const { return validFrom_; }
    const std::string& validTo() const { return validTo_; }
    bool fallback() const { return fallback_; }
    QuantLib::Date validToDate() const;
    QuantLib::Date validFromDate() const;
    std::string name() const;
private:
    std::string bucket_;
    std::string validFrom_;
    std::string validTo_;
    bool fallback_;
};

bool operator< (const BucketMapping &a, const BucketMapping &b);

class SimmBucketMapperBase : public SimmBucketMapper, public ore::data::XMLSerializable {
public:
    //! Default constructor that adds fixed known mappings
    SimmBucketMapperBase(
        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager = nullptr,
        const QuantLib::ext::shared_ptr<SimmBasicNameMapper>& nameMapper = nullptr);

    /*! Return the SIMM <em>bucket</em> for a given SIMM <em>RiskType</em> and
        SIMM <em>Qualifier</em>. An error is thrown if there is no <em>bucket</em>
        for the combination.
    */
    std::string bucket(const CrifRecord::RiskType& riskType, const std::string& qualifier) const override;

    //! Check if the given SIMM <em>RiskType</em> has a bucket structure.
    bool hasBuckets(const CrifRecord::RiskType& riskType) const override;

    //! Check if the given \p riskType and \p qualifier has a valid mapping
    bool has(const CrifRecord::RiskType& riskType, const std::string& qualifier,
             boost::optional<bool> fallback = boost::none) const override;

    //! \name Serialisation
    //@{
    ore::data::XMLNode* toXML(ore::data::XMLDocument&) const override;
    void fromXML(ore::data::XMLNode* node) override;
    //@}

    //! Add a single \p bucket mapping for \p qualifier with risk type \p riskType
    /*! \todo Not very convenient. If deemed useful, add more methods for adding
              mappings e.g. <code>addMappingRange</code> that adds a range of
              mappings at once
    */
    void addMapping(const CrifRecord::RiskType& riskType, const std::string& qualifier,
                    const std::string& bucket, const std::string& validFrom = "", const std::string& validTo = "", bool fallback = false) override;

    //! Set the Reference data manager
    void setSimmNameMapper(const QuantLib::ext::shared_ptr<SimmBasicNameMapper> nameMapper) { nameMapper_ = nameMapper; }

    //! Set the Reference data manager
    void setRefDataManger(const QuantLib::ext::shared_ptr<ore::data::BasicReferenceDataManager>& refDataManager) { refDataManager_ = refDataManager; }

    const std::set<FailedMapping>& failedMappings() const override { return failedMappings_; }

protected:
    /*! Simple logic for RiskType Risk_IRCurve. Qualifier is a currency code and
        this is checked here.
    */
    virtual std::string irBucket(const std::string& qualifier) const;

    //! Check the risk type before adding a mapping entry
    void checkRiskType(const CrifRecord::RiskType& riskType) const;

    /*! Map from SIMM <em>RiskType</em> to another map that holds the
        SIMM <em>Qualifier</em> to SIMM <em>bucket</em> mappings
    */
    std::map<CrifRecord::RiskType, std::map<std::string, std::set<BucketMapping>>> bucketMapping_;

    //! Set of SIMM risk types that have buckets
    std::set<CrifRecord::RiskType> rtWithBuckets_;

private:
    mutable std::map<std::pair<CrifRecord::RiskType, std::string>, std::string> cache_;

    //! Reset the SIMM bucket mapper i.e. clears all mappings and adds the initial hard-coded commodity mappings
    void reset();

    //! Reference data manager
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> refDataManager_;

    //! Simm Name Mapper
    QuantLib::ext::shared_ptr<SimmBasicNameMapper> nameMapper_;

    mutable std::set<FailedMapping> failedMappings_;
};

} // namespace analytics
} // namespace ore
