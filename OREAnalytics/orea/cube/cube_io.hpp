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
#include <orea/scenario/scenariogeneratordata.hpp>

namespace ore {
namespace analytics {

/*! We save / load the npv cube data toegther with some meta data that is used to set up the CubeInterpretation. This is
    to ensure that the cube interpretation is consistent with the cube that we load from disk. The meta data overwrites
    the config in ore.xml / simulation.xml. All meta data is optional, i.e. if not given in the cube file, the original
    config will be used. */

struct NPVCubeWithMetaData {
    QuantLib::ext::shared_ptr<NPVCube> cube;
    // all of the following members are optional
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData;
    boost::optional<bool> storeFlows;
    boost::optional<Size> storeCreditStateNPVs;
};

NPVCubeWithMetaData loadCube(const std::string& filename, const bool doublePrecision = false);
void saveCube(const std::string& filename, const NPVCubeWithMetaData& cube, const bool doublePrecision = false);

QuantLib::ext::shared_ptr<AggregationScenarioData> loadAggregationScenarioData(const std::string& filename);
void saveAggregationScenarioData(const std::string& filename, const AggregationScenarioData& cube);

} // namespace analytics
} // namespace ore
