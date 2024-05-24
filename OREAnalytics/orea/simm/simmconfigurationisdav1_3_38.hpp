/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

/*! \file orea/simm/simmconfigurationisdav1_3_38.hpp
    \brief SIMM configuration for SIMM version 1.3.38
*/

#pragma once

#include <orea/simm/simmconfigurationbase.hpp>

namespace ore {
namespace analytics {

/*! Class giving the SIMM configuration as outlined in the document
    <em>ISDA SIMM Methodology, version R1.3 (based on v3.29: 1 April 2017).
        Effective Date: April 1, 2017.</em>

    This file used to be called simmconfigurationisdav338.hpp
    This class used to be called SimmConfiguration_ISDA_V338

*/
//  Do not have documentation for this class. The values are copied during refactor.

//! Class giving the SIMM configuration for v1.3.38
class SimmConfiguration_ISDA_V1_3_38 : public SimmConfigurationBase {
public:
    SimmConfiguration_ISDA_V1_3_38(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                   const std::string& name = "SIMM ISDA V1_3_38 (24 May 2017)",
                                   const std::string version = "1.3.38");

    //! Return the SIMM <em>Label2</em> value for the given interest rate index
    std::string label2(const QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>& irIndex) const override;

    //! Add SIMM <em>Label2</em> values under certain circumstances.
    void addLabels2(const CrifRecord::RiskType& rt, const std::string& label_2) override;
};

} // namespace analytics
} // namespace ore
