/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/indexparser.hpp
    \brief Map text representations to QuantLib/QuantExt types
    \ingroup utilities
*/


#include<ored/configuration/conventions.hpp>

#pragma once

namespace ore {
namespace data {

// Utility function to derive the inflation swap start date and curve observation lag from the as of date and
// convention. In general, we take this simply to be (as of date, Period()). However, for AU CPI for
// example, this is more complicated and we need to account for this here if the inflation swap conventions provide
// us with a publication schedule and tell us to roll on that schedule.
std::pair<QuantLib::Date, QuantLib::Period> getStartAndLag(const QuantLib::Date& asof,
                                                           const InflationSwapConvention& conv);


QuantLib::Date getInflationSwapStart(const QuantLib::Date& asof, const InflationSwapConvention& conv);






} // namespace data
} // namespace ore