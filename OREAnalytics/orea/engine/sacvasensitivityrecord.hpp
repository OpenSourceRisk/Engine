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

/*! \file orea/engine/cvasensitivityrecord.hpp
    \brief Struct for holding a sensitivity record
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/cvascenario.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <string>

namespace ore {
namespace analytics {

enum class CvaType { CvaAggregate, CvaHedge};

/*! A container for holding cva sensitivity records.
*/
struct SaCvaSensitivityRecord {
    // Public members
    std::string nettingSetId;
    CvaRiskFactorKey::KeyType riskType;
    CvaType cvaType;
    CvaRiskFactorKey::MarginType marginType;
    std::string riskFactor;
    std::string bucket;
    mutable QuantLib::Real value;

    /*! Default ctor to prevent uninitialised variables
        Could use in class initialisation and avoid ctor but may be confusing
    */
    SaCvaSensitivityRecord() : value(0.0) {}

    //! Full ctor to allow braced initialisation
    SaCvaSensitivityRecord(const std::string& nettingSetId, const CvaRiskFactorKey::KeyType& riskType, const CvaType& cvaType, const CvaRiskFactorKey::MarginType& marginType,
                        const std::string& riskFactor, const std::string& bucket,
                      QuantLib::Real value)
        : nettingSetId(nettingSetId), riskType(riskType), cvaType(cvaType), marginType(marginType), 
        riskFactor(riskFactor), bucket(bucket), value(value) {}

    /*! Comparison operators for CvaSensitivityRecord
     */
    bool operator==(const SaCvaSensitivityRecord& sr) const;
    bool operator<(const SaCvaSensitivityRecord& sr) const;
};

//! Enable writing of a CvaSensitivityRecord
std::ostream& operator<<(std::ostream& out, const SaCvaSensitivityRecord& sr);
std::ostream& operator<<(std::ostream& out, const CvaType& mt);

CvaType parseCvaType(const std::string& mt);

struct CvaNettingSetTag {};
struct CvaRiskTypeTag {};
struct CvaRiskFactorTag {};

typedef boost::multi_index_container<
    SaCvaSensitivityRecord,
    boost::multi_index::indexed_by<
        // The CvaSensi record itself and its '<' operator define a unique index
        boost::multi_index::ordered_unique<boost::multi_index::identity<SaCvaSensitivityRecord>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<CvaNettingSetTag>,
            boost::multi_index::composite_key<
                SaCvaSensitivityRecord, boost::multi_index::member<SaCvaSensitivityRecord, std::string, &SaCvaSensitivityRecord::nettingSetId>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<CvaRiskTypeTag>,
            boost::multi_index::composite_key<
                SaCvaSensitivityRecord, boost::multi_index::member<SaCvaSensitivityRecord, std::string, &SaCvaSensitivityRecord::nettingSetId>,
                boost::multi_index::member<SaCvaSensitivityRecord, CvaRiskFactorKey::KeyType, &SaCvaSensitivityRecord::riskType>,
                boost::multi_index::member<SaCvaSensitivityRecord, CvaRiskFactorKey::MarginType, &SaCvaSensitivityRecord::marginType>
                >>,
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<CvaRiskFactorTag>,
            boost::multi_index::composite_key<
                SaCvaSensitivityRecord, boost::multi_index::member<SaCvaSensitivityRecord, std::string, &SaCvaSensitivityRecord::nettingSetId>,
                boost::multi_index::member<SaCvaSensitivityRecord, CvaRiskFactorKey::KeyType, &SaCvaSensitivityRecord::riskType>,
                boost::multi_index::member<SaCvaSensitivityRecord, std::string, &SaCvaSensitivityRecord::bucket>,
                boost::multi_index::member<SaCvaSensitivityRecord, CvaRiskFactorKey::MarginType, &SaCvaSensitivityRecord::marginType>,
                boost::multi_index::member<SaCvaSensitivityRecord, std::string, &SaCvaSensitivityRecord::riskFactor>,
                boost::multi_index::member<SaCvaSensitivityRecord, CvaType, &SaCvaSensitivityRecord::cvaType>
                >>
    >>
    SaCvaNetSensitivities;

} // namespace analytics
} // namespace ore
