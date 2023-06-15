/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/structuredconfigurationwarning.hpp
    \brief Class for structured configuration warnings
    \ingroup portfolio
*/

#pragma once

#include <boost/algorithm/string.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

//! Utility classes for Structured warnings, contains the configuration type and ID (NettingSetId, CounterParty, etc.)
class StructuredConfigurationWarningMessage : public StructuredMessage {
public:
    StructuredConfigurationWarningMessage(const std::string& configurationType, const std::string& configurationId,
                                          const std::string& warningType, const std::string& warningWhat,
                                          const std::map<std::string, std::string>& subFields = {})
        : StructuredMessage(Category::Warning, Group::Configuration, warningWhat,
                            std::map<std::string, std::string>({{"warningType", warningType},
                                                                {"configurationType", configurationType},
                                                                {"configurationId", configurationId}})) {
        if (!subFields.empty())
            subFields_.insert(subFields.begin(), subFields.end());
    }
};

} // namespace data
} // namespace ore
