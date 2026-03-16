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

/*! \file orea/simm/simmconfigurationisdav1_0.hpp
    \brief SIMM configuration for SIMM version R1.0 (v3.15)
*/

/*! This file used to be called simmconfigurationisdav315.hpp
    This class used to be called SimmConfiguration_ISDA_V315
*/

#pragma once

#include <orea/simm/simmconfigurationbase.hpp>

namespace ore {
namespace analytics {

class SimmConfiguration_ISDA_V1_0 : public SimmConfigurationBase {
public:
    SimmConfiguration_ISDA_V1_0(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const std::string& name = "SIMM ISDA V1_0 (7 April 2016)",
                                const std::string version = "1.0");
};

} // namespace analytics
} // namespace ore
