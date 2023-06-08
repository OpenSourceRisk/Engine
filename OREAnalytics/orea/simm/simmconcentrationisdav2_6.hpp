/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/
/*! \file orea/simm/simmconcentrationisdav2_6.hpp
    \brief SIMM concentration thresholds for SIMM version 2.6
*/

#pragma once

#include <orea/simm/simmconcentration.hpp>

#include <map>
#include <orea/simm/simmbucketmapper.hpp>
#include <set>
#include <string>

namespace ore {
namespace analytics {

/*! Class giving the SIMM concentration thresholds as outlined in the document
    <em>ISDA SIMM Methodology, version 2.6.
        Effective Date: December 3, 2022.</em>
*/
class SimmConcentration_ISDA_V2_6 : public SimmConcentrationBase {
public:
    //! Default constructor that adds fixed known mappings
    SimmConcentration_ISDA_V2_6(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper);

    /*! Return the SIMM <em>concentration threshold</em> for a given SIMM
        <em>RiskType</em> and SIMM <em>Qualifier</em>.

        \warning If the risk type is not covered <code>QL_MAX_REAL</code> is
                 returned i.e. no concentration threshold
     */
    QuantLib::Real threshold(const SimmConfiguration::RiskType& riskType, const std::string& qualifier) const override;

private:
    //! Help getting SIMM buckets from SIMM qualifiers
    boost::shared_ptr<SimmBucketMapper> simmBucketMapper_;
};

} // namespace analytics
} // namespace ore
