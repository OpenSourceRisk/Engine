/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/curvepillar.hpp
    \brief Variant of Periods, explicit dates and future continuations to be used for pillar selection in simmmarket and
   sceneriodata
*/

#pragma once

#include <ored/marketdata/expiry.hpp>

namespace ore {
namespace data {
typedef std::variant<QuantLib::Date, QuantLib::Period, FutureContinuationExpiry> CurvePillar;

CurvePillar parseCurvePillar(const std::string& str);
} // namespace data
} // namespace ore