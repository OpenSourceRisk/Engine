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

/*! \file orea/cube/sensitivitycube.hpp
    \brief wrapper class for an npvCube and a vector of scenarioDescriotions for use in sensitivity generation
    \ingroup Cube
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/cube/npvcube.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace analytics {

class SensitivityCube {

public:

    SensitivityCube() {}
    
    SensitivityCube(boost::shared_ptr<NPVCube>& cube, boost::shared_ptr<vector<ShiftScenarioGenerator::ScenarioDescription>>& scenarioDescriptions, 
        const boost::shared_ptr<ore::data::Portfolio>& portfolio):
            cube_(cube), scenarioDescriptions_(scenarioDescriptions) {
        for (Size i=0; i<scenarioDescriptions->size(); i++) {
            ShiftScenarioGenerator::ScenarioDescription desc = (*scenarioDescriptions)[i];
            std::tuple<ShiftScenarioGenerator::ScenarioDescription::Type, string, string> triple(desc.type(), desc.factor1(), desc.factor2()); 
            factorIndices_[triple] = i;
        }
        for (Size i=0; i<portfolio->size(); i++) {
            tradeIndices_[portfolio->trades()[i]->id()] = i;
        }
    }

    Real baseNPV(Size tradeIdx) const { return cube_->getT0(tradeIdx, 0);}
    Real baseNPV(string tradeId) const {
        Size tradeIdx = getTradeIndex(tradeId);
        return cube_->getT0(tradeIdx, 0);
    }
    
    Real upNPV(Size tradeIdx, string factor) const {
       return cubeNPV(tradeIdx, ShiftScenarioGenerator::ScenarioDescription::Type::Up, factor, "");
    }

    Real downNPV(Size tradeIdx, string factor) const {
       return cubeNPV(tradeIdx, ShiftScenarioGenerator::ScenarioDescription::Type::Down, factor, "");
    }

    Real crossNPV(Size tradeIdx, string factor1, string factor2) const{
        return cubeNPV(tradeIdx, ShiftScenarioGenerator::ScenarioDescription::Type::Cross, factor1, factor2);
    }

    Real getNPV(Size i, Size j) const { return cube_->get(i, 0, j, 0);}

    //the scenarioDescriptions_ indices match the cube_ indices for a given type/factor
    Size getFactorIndex(ShiftScenarioGenerator::ScenarioDescription::Type type, string factor1, string factor2) const {
        std::tuple<ShiftScenarioGenerator::ScenarioDescription::Type, string, string> triple(type, factor1, factor2);
        auto it = factorIndices_.find(triple);
        return it->second;
    };

    Size getTradeIndex(string tradeId) const {
        auto it = tradeIndices_.find(tradeId);
        return it->second;
    }

    
    boost::shared_ptr<NPVCube>& npvCube() { return cube_;}
    boost::shared_ptr<vector<ShiftScenarioGenerator::ScenarioDescription>>& scenDesc() { return  scenarioDescriptions_;}
    
private:
    Real cubeNPV(Size& tradeIdx, ShiftScenarioGenerator::ScenarioDescription::Type type, string factor1, string factor2 = "") const {
        Size index = getFactorIndex(type, factor1, factor2);
        return cube_->get(tradeIdx, 0, index, 0);
    }


    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<vector<ShiftScenarioGenerator::ScenarioDescription>> scenarioDescriptions_;

    std::map<string, Size> tradeIndices_;
    std::map<tuple<ShiftScenarioGenerator::ScenarioDescription::Type, string, string>, Size> factorIndices_;

};
} // namespace analytics
} // namespace ore
