/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/ratehelpers.hpp
    \brief rate helper related utilities.
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/termstructures/bootstraphelper.hpp>

namespace QuantExt {

QuantLib::Date determineLatestRelevantDate(const std::vector<QuantLib::Leg>& legs,
                                           const std::vector<bool>& includeIndexEstimationEndDate = {});

QuantLib::Date determinePillarDate(const QuantLib::Pillar::Choice pillarChoice, const QuantLib::Date& maturityDate,
                                   const QuantLib::Date& latestRelevantDate);

} // namespace QuantExt
