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


class NPVCubeWithMetaData  {
public:
    NPVCubeWithMetaData() {}
    NPVCubeWithMetaData(const QuantLib::ext::shared_ptr<NPVCube>& cube,
                        const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
                        const QuantLib::ext::optional<bool>& storeFlows,
                        const QuantLib::ext::optional<Size>& storeCreditStateNPVs)
        : cube_(cube), scenarioGeneratorData_(scenarioGeneratorData), storeFlows_(storeFlows), storeCreditStateNPVs_(storeCreditStateNPVs) {};

    //getters
    const QuantLib::ext::shared_ptr<NPVCube>& cube() const { return cube_; }
    const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData() const { return scenarioGeneratorData_; }
    const QuantLib::ext::optional<bool>& storeFlows() const { return storeFlows_; }
    const QuantLib::ext::optional<Size>& storeCreditStateNPVs() const { return storeCreditStateNPVs_; }

    //setters
    void setCube(const QuantLib::ext::shared_ptr<NPVCube>& cube) { cube_ = cube; }
    void setScenarioGeneratorData(const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& data) {
        scenarioGeneratorData_ = data;
    }
    void setStoreFlows(bool storeFlows) { storeFlows_ = storeFlows; }
    void storeCreditStateNPVs(Size depth) { storeCreditStateNPVs_ = depth; }

private:
    QuantLib::ext::shared_ptr<NPVCube> cube_;
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::optional<bool> storeFlows_;
    QuantLib::ext::optional<Size> storeCreditStateNPVs_;
};

QuantLib::ext::shared_ptr<NPVCubeWithMetaData> loadCube(const std::string& filename);
void saveCube(const std::string& filename, const NPVCubeWithMetaData& cube);

QuantLib::ext::shared_ptr<AggregationScenarioData> loadAggregationScenarioData(const std::string& filename);
void saveAggregationScenarioData(const std::string& filename, const AggregationScenarioData& cube);

} // namespace analytics
} // namespace ore
