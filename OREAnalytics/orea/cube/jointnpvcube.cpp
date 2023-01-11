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

JointNPVCube::JointNPVCube(const boost::shared_ptr<NPVCube>& cube1, const boost::shared_ptr<NPVCube>& cube2,
                           const std::vector<std::string>& ids, const bool requireUniqueIds)
    : JointNPVCube({cube1, cube2}, ids, requireUniqueIds) {}

JointNPVCube::JointNPVCube(const std::vector<boost::shared_ptr<NPVCube>>& cubes, const std::vector<std::string>& ids,
                           const bool requireUniqueIds)
    : NPVCube(), cubes_(cubes) {

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

    // check that the input ids (if given) are unique

    std::set<std::string> tmp(ids.begin(), ids.end());
    QL_REQUIRE(tmp.size() == ids.size(), "JoingNPVCube: given input ids (" << ids.size() << ") are not unique (got "
                                                                           << tmp.size() << " unique id)");

    // build list of result cube ids

    if (!ids.empty()) {
        // if ids are given, these define the order in the result cube
        ids_ = ids;
    } else {
        // otherwise the ids in the source cubes define the order in the result cube
        for (Size i = 0; i < cubes_.size(); ++i) {
            const std::vector<std::string>& tmp = cubes_[i]->ids();
            for (auto const& id : tmp) {
                auto f = std::find(ids_.begin(), ids_.end(), id);
                QL_REQUIRE(!requireUniqueIds || f == ids_.end(),
                           "JointNPVCube: input cubes have duplicate id '"
                               << id << "', but unique ids in the input cubes are required");
                if (f == ids_.end())
                    ids_.push_back(id);
            }
        }
    }

    // populate cubeAndId_ vector which is the basis for the lookup

    cubeAndId_.resize(ids_.size());
    for (Size i = 0; i < ids_.size(); ++i) {
        for (auto const& c : cubes_) {
            const std::vector<std::string>& tmp = c->ids();
            for (Size j = 0; j < tmp.size(); ++j) {
                if (ids_[i] == tmp[j])
                    cubeAndId_[i].insert(std::make_pair(c, j));
            }
        }
        // internal consistency checks
        QL_REQUIRE(cubeAndId_[i].size() >= 1,
                   "JointNPVCube: internal error, got no input cubes for id '" << ids_[i] << "'");
        QL_REQUIRE(!requireUniqueIds || cubeAndId_[i].size() == 1,
                   "JointNPVCube: internal error, got more than one input cube for id '"
                       << ids_[i] << "', but unique input ids qre required");
    }
}

Size JointNPVCube::numIds() const { return ids_.size(); }

Size JointNPVCube::numDates() const { return cubes_[0]->numDates(); }

Size JointNPVCube::samples() const { return cubes_[0]->samples(); }

Size JointNPVCube::depth() const { return cubes_[0]->depth(); }

const std::vector<std::string>& JointNPVCube::ids() const { return ids_; }

const std::vector<QuantLib::Date>& JointNPVCube::dates() const { return cubes_[0]->dates(); }

QuantLib::Date JointNPVCube::asof() const { return cubes_[0]->asof(); }

std::set<std::pair<boost::shared_ptr<NPVCube>, Size>> JointNPVCube::cubeAndId(Size id) const {
    QL_REQUIRE(id < cubeAndId_.size(),
               "JointNPVCube: id (" << id << ") out of range, have " << cubeAndId_.size() << " ids");
    return cubeAndId_[id];
}

Real JointNPVCube::getT0(Size id, Size depth) const {
    Real sum = 0.0;
    for (auto const& p : cubeAndId(id))
        sum += p.first->getT0(p.second, depth);
    return sum;
}

void JointNPVCube::setT0(Real value, Size id, Size depth) {
    auto c = cubeAndId(id);
    QL_REQUIRE(c.size() == 1,
               "JointNPVCube::setT0(): not allowed, because id '" << id << "' occurs in more than one input cube");
    (*c.begin()).first->setT0(value, (*c.begin()).second, depth);
}

Real JointNPVCube::get(Size id, Size date, Size sample, Size depth) const {
    Real sum = 0.0;
    for (auto const& p : cubeAndId(id))
        sum += p.first->get(p.second, date, sample, depth);
    return sum;
}

void JointNPVCube::set(Real value, Size id, Size date, Size sample, Size depth) {
    auto c = cubeAndId(id);
    QL_REQUIRE(c.size() == 1,
               "JointNPVCube::set(): not allowed, because id '" << id << "' occurs in more than one input cube");
    (*c.begin()).first->set(value, (*c.begin()).second, date, sample, depth);
}

} // namespace analytics
} // namespace ore
