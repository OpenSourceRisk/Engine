/*
 Copyright (C) 2025 Acadia Inc
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

/*! \file ored/utilities/credit.hpp
    \brief helper functions related to credit products
    \ingroup utilities
*/
#pragma once
#include <string_view>

namespace ore {
namespace data {

inline bool isIndexCDS(std::string_view redCode) { return redCode.size() == 13 && redCode.substr(0, 3) == "RED"; }

} // namespace data
} // namespace ore
