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

#ifndef ored_fixings_i
#define ored_fixings_i

// Fixing struct and applyFixings are already declared in ored_loader.i;
// this file adds the free functions from fixings.hpp.

%include ored_loader.i

namespace ore {
namespace data {

//! Write a set of Fixing records into the QuantLib index manager's fixing history
void applyFixings(const std::set<ore::data::Fixing>& fixings);

} // namespace data
} // namespace ore

#endif
