/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/simmcreditqualifiermapping.hpp
    \brief mapping of SIMM credit qualifiers
    \ingroup portfolio
*/

#pragma once

#include <string>

namespace ore {
namespace data {

struct SimmCreditQualifierMapping {
    SimmCreditQualifierMapping() {}
    SimmCreditQualifierMapping(const std::string& targetQualifier, const std::string& creditGroup)
        : targetQualifier(targetQualifier), creditGroup(creditGroup) {}
    SimmCreditQualifierMapping(std::string&& targetQualifier, std::string&& creditGroup)
        : targetQualifier(targetQualifier), creditGroup(creditGroup) {}
    std::string targetQualifier;
    std::string creditGroup; // only required for CreditNonQ
};

} // namespace data
} // namespace ore
