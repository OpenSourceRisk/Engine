/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <orea/cube/jointnpvcube.hpp>

#include <ql/errors.hpp>

#include <numeric>
#include <set>

namespace ore {
namespace analytics {

JointNPVCube::JointNPVCube(const QuantLib::ext::shared_ptr<NPVCube>& cube1, const QuantLib::ext::shared_ptr<NPVCube>& cube2,
                           const std::set<std::string>& ids, const bool requireUniqueIds,
                           const std::function<Real(Real a, Real x)>& accumulator, const Real accumulatorInit)
    : JointNPVCube({cube1, cube2}, ids, requireUniqueIds, accumulator, accumulatorInit) {}

JointNPVCube::JointNPVCube(const std::vector<QuantLib::ext::shared_ptr<NPVCube>>& cubes, const std::set<std::string>& ids,
                           const bool requireUniqueIds, const std::function<Real(Real a, Real x)>& accumulator,
                           const Real accumulatorInit)
    : NPVCube(), cubes_(cubes), accumulator_(accumulator), accumulatorInit_(accumulatorInit) {

    // check we have at least one input cube

    QL_REQUIRE(!cubes.empty(), "JointNPVCube: at least one cube must be given");

    // check that the dimensions are consistent

    for (Size i = 1; i < cubes.size(); ++i) {
        QL_REQUIRE(cubes_[i]->numDates() == cubes_[0]->numDates(),
                   "JointNPVCube: numDates do not match for cube #"
                       << i << " (" << cubes[i]->numDates() << " vs. cube #0 (" << cubes_[0]->numDates() << ")");
        QL_REQUIRE(cubes_[i]->samples() == cubes_[0]->samples(),
                   "JointNPVCube: samples do not match for cube #" << i << " (" << cubes_[i]->samples()
                                                                   << " vs. cube #0 (" << cubes_[0]->samples() << ")");
        QL_REQUIRE(cubes_[i]->depth() == cubes_[0]->depth(), "JointNPVCube: depth do not match for cube #"
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
                QL_REQUIRE(!requireUniqueIds || success,
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
                cubeAndId_[jointPos].insert(std::make_pair(c, searchIt->second));
            }
        }
        // internal consistency checks
        QL_REQUIRE(cubeAndId_[jointPos].size() >= 1,
                   "JointNPVCube: internal error, got no input cubes for id '" << id << "'");
        QL_REQUIRE(!requireUniqueIds || cubeAndId_[jointPos].size() == 1,
                   "JointNPVCube: internal error, got more than one input cube for id '"
                       << id << "', but unique input ids qre required");
    }
}

Size JointNPVCube::numIds() const { return idIdx_.size(); }

Size JointNPVCube::numDates() const { return cubes_[0]->numDates(); }

Size JointNPVCube::samples() const { return cubes_[0]->samples(); }

Size JointNPVCube::depth() const { return cubes_[0]->depth(); }

const std::map<std::string, Size>& JointNPVCube::idsAndIndexes() const { return idIdx_; }

const std::vector<QuantLib::Date>& JointNPVCube::dates() const { return cubes_[0]->dates(); }

QuantLib::Date JointNPVCube::asof() const { return cubes_[0]->asof(); }

std::set<std::pair<QuantLib::ext::shared_ptr<NPVCube>, Size>> JointNPVCube::cubeAndId(Size id) const {
    QL_REQUIRE(id < cubeAndId_.size(),
               "JointNPVCube: id (" << id << ") out of range, have " << cubeAndId_.size() << " ids");
    return cubeAndId_[id];
}

Real JointNPVCube::getT0(Size id, Size depth) const {
    auto cids = cubeAndId(id);
    if (cids.size() == 1)
        return cids.begin()->first->getT0(cids.begin()->second, depth);
    Real tmp = accumulatorInit_;
    for (auto const& p : cids)
        tmp = accumulator_(tmp, p.first->getT0(p.second, depth));
    return tmp;
}

void JointNPVCube::setT0(Real value, Size id, Size depth) {
    auto c = cubeAndId(id);
    QL_REQUIRE(c.size() == 1,
               "JointNPVCube::setT0(): not allowed, because id '" << id << "' occurs in more than one input cube");
    (*c.begin()).first->setT0(value, (*c.begin()).second, depth);
}

Real JointNPVCube::get(Size id, Size date, Size sample, Size depth) const {
    auto cids = cubeAndId(id);
    if (cids.size() == 1)
        return cids.begin()->first->get(cids.begin()->second, date, sample, depth);
    Real tmp = accumulatorInit_;
    for (auto const& p : cids)
        tmp = accumulator_(tmp, p.first->get(p.second, date, sample, depth));
    return tmp;
}

void JointNPVCube::set(Real value, Size id, Size date, Size sample, Size depth) {
    auto c = cubeAndId(id);
    QL_REQUIRE(c.size() == 1,
               "JointNPVCube::set(): not allowed, because id '" << id << "' occurs in more than one input cube");
    (*c.begin()).first->set(value, (*c.begin()).second, date, sample, depth);
}

} // namespace analytics
} // namespace ore
