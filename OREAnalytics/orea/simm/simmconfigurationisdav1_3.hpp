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

/*! \file orea/simm/simmconfigurationisdav1_3.hpp
    \brief SIMM configuration for SIMM version R1.3 (3.29)
*/

#pragma once

#include <orea/simm/simmconfigurationisdav1_0.hpp>

namespace ore {
namespace analytics {

/*! Class giving the SIMM configuration as outlined in the document
    <em>ISDA SIMM Methodology, version R1.3 (based on v3.29: 1 April 2017).
        Effective Date: April 1, 2017.</em>

    This file used to be called simmconfigurationisdav329.hpp
    This class used to be called SimmConfiguration_ISDA_V329
*/
class SimmConfiguration_ISDA_V1_3 : public SimmConfiguration_ISDA_V1_0 {
public:
    SimmConfiguration_ISDA_V1_3(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const std::string& name = "SIMM ISDA V1_3 (1 April 2017)",
                                const std::string version = "1.3");
};

} // namespace analytics
} // namespace ore
