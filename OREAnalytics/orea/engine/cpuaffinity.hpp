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

/*! \file orea/engine/cpuaffinity.hpp
    \brief helpers to assign worker threads to cpus
    \ingroup engine
*/

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

std::vector<std::size_t> getCpuIds(std::size_t nThreads, const std::string& logPrefix);

void setThreadCpuAffinity(int id, const std::vector<std::size_t>& cpuIds, const std::string& logPrefix);

} // namespace analytics
} // namespace ore
