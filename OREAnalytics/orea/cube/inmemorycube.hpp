/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file orea/cube/inmemorycube.hpp
    \brief A cube implementation that stores the cube in memory
    \ingroup cube
*/

#pragma once

#include <orea/cube/inmemorycubeopt.hpp>

namespace ore {
namespace analytics {

// redirecting to new implementation which has a better memory footprint...

template <typename T> using InMemoryCubeBase = InMemoryCubeOpt<T>;
template <typename T> using InMemoryCubeN = InMemoryCubeOpt<T>;
template <typename T> using InMemoryCube1 = InMemoryCubeOpt<T>;

using SinglePrecisionInMemoryCube = InMemoryCubeOpt<float>;
using SinglePrecisionInMemoryCubeN = InMemoryCubeOpt<float>;
using DoublePrecisionInMemoryCube = InMemoryCubeOpt<double>;
using DoublePrecisionInMemoryCubeN = InMemoryCubeOpt<double>;

} // namespace analytics
} // namespace ore
