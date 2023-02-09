/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/cube/cube_io.hpp
    \brief load / save cubes and agg scen data from / to disk
    \ingroup cube
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

namespace ore {
namespace analytics {

boost::shared_ptr<NPVCube> loadCube(const std::string& filename, const bool doublePrecision = false);
void saveCube(const std::string& filename, const NPVCube& cube, const bool doublePrecision = false);

boost::shared_ptr<AggregationScenarioData> loadAggregationScenarioData(const std::string& filename);
void saveAggregationScenarioData(const std::string& filename, const AggregationScenarioData& cube);

} // namespace analytics
} // namespace ore
