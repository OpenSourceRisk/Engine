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

#include <orea/cube/jointnpvsensicube.hpp>

#include <ql/errors.hpp>

#include <numeric>
#include <set>

namespace ore {
namespace analytics {

JointNPVSensiCube::JointNPVSensiCube(const QuantLib::ext::shared_ptr<NPVSensiCube>& cube1,
                                     const QuantLib::ext::shared_ptr<NPVSensiCube>& cube2, const std::set<std::string>& ids)
    : JointNPVSensiCube({cube1, cube2}, ids) {}

JointNPVSensiCube::JointNPVSensiCube(const std::vector<QuantLib::ext::shared_ptr<NPVSensiCube>>& cubes,
                                     const std::set<std::string>& ids)
    : NPVSensiCube(), cubes_(cubes) {

    // check we have at least one input cube

    QL_REQUIRE(!cubes.empty(), "JointNPVSensiCube: at least one cube must be given");

    // check that the dimensions are consistent

    for (Size i = 1; i < cubes.size(); ++i) {
        QL_REQUIRE(cubes_[i]->numDates() == cubes_[0]->numDates(),
                   "JointNPVSensiCube: numDates do not match for cube #"
                       << i << " (" << cubes[i]->numDates() << " vs. cube #0 (" << cubes_[0]->numDates() << ")");
        QL_REQUIRE(cubes_[i]->samples() == cubes_[0]->samples(),
                   "JointNPVSensiCube: samples do not match for cube #"
                       << i << " (" << cubes_[i]->samples() << " vs. cube #0 (" << cubes_[0]->samples() << ")");
        QL_REQUIRE(cubes_[i]->depth() == cubes_[0]->depth(), "JointNPVSensiCube: depth do not match for cube #"
                                                                 << i << " (" << cubes_[i]->depth() << " vs. cube #0 ("
                                                                 << cubes[0]->depth() << ")");
    }

    std::set<std::string> allIds;
    if (!ids.empty()) {
        // if ids are given, these define the ids in the result cube
        allIds = ids;
    } else {
        // otherwise the ids in the source cubes define the ids in the result cube
        for (Size i = 0; i < cubes_.size(); ++i) {
            for (auto const& [id, ignored] : cubes_[i]->idsAndIndexes()) {
                const auto& [ignored2, success] = allIds.insert(id);
                QL_REQUIRE(success,
                           "JointNPVSensiCube: input cubes have duplicate id '" << id << "', this is not allowed");
            }
        }
    }

    // build list of result cube ids
    Size pos = 0;
    for (const auto& id : allIds) {
        idIdx_[id] = pos++;
    }

    // populate cubeAndId_ vector which is the basis for the lookup
    cubeAndId_.resize(idIdx_.size());
    for (const auto& [id, jointPos] : idIdx_) {
        for (auto const& c : cubes_) {
            auto searchIt = c->idsAndIndexes().find(id);
            if (searchIt != c->idsAndIndexes().end()) {
                QL_REQUIRE(cubeAndId_[jointPos].first == nullptr,
                           "JointNPVSensiCube: input cubes have duplicate id '" << id << "', this is not allowed");
                cubeAndId_[jointPos] = std::make_pair(c, searchIt->second);
            }
        }
    }
}

Size JointNPVSensiCube::numIds() const { return idIdx_.size(); }

Size JointNPVSensiCube::numDates() const { return cubes_[0]->numDates(); }

Size JointNPVSensiCube::samples() const { return cubes_[0]->samples(); }

Size JointNPVSensiCube::depth() const { return cubes_[0]->depth(); }

const std::map<std::string, Size>& JointNPVSensiCube::idsAndIndexes() const { return idIdx_; }

const std::vector<QuantLib::Date>& JointNPVSensiCube::dates() const { return cubes_[0]->dates(); }

QuantLib::Date JointNPVSensiCube::asof() const { return cubes_[0]->asof(); }

const std::pair<QuantLib::ext::shared_ptr<NPVSensiCube>, Size>& JointNPVSensiCube::cubeAndId(Size id) const {
    QL_REQUIRE(id < cubeAndId_.size(),
               "JointNPVSensiCube: id (" << id << ") out of range, have " << cubeAndId_.size() << " ids");
    return cubeAndId_[id];
}

Real JointNPVSensiCube::getT0(Size id, Size depth) const {
    const auto& c = cubeAndId(id);
    return c.first->getT0(c.second, depth);
}

void JointNPVSensiCube::setT0(Real value, Size id, Size depth) {
    const auto& c = cubeAndId(id);
    c.first->setT0(value, c.second, depth);
}

Real JointNPVSensiCube::get(Size id, Size date, Size sample, Size depth) const {
    const auto& c = cubeAndId(id);
    return c.first->get(c.second, date, sample, depth);
}

void JointNPVSensiCube::set(Real value, Size id, Size date, Size sample, Size depth) {
    const auto& c = cubeAndId(id);
    c.first->set(value, c.second, date, sample, depth);
}

std::map<QuantLib::Size, QuantLib::Real> JointNPVSensiCube::getTradeNPVs(Size tradeIdx) const {
    const auto& c = cubeAndId(tradeIdx);
    return c.first->getTradeNPVs(c.second);
}

std::set<QuantLib::Size> JointNPVSensiCube::relevantScenarios() const {
    std::set<QuantLib::Size> tmp;
    for (auto const& c : cubes_) {
        auto r = c->relevantScenarios();
        tmp.insert(r.begin(), r.end());
    }
    return tmp;
}

void JointNPVSensiCube::remove(Size id) {
    const auto& c = cubeAndId(id);
    c.first->remove(c.second);
}

void JointNPVSensiCube::remove(Size id, Size sample) {
    const auto& c = cubeAndId(id);
    c.first->remove(c.second, sample);
}

} // namespace analytics
} // namespace ore
